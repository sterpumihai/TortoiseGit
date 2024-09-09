﻿// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2014, 2016-2024 TortoiseGit
// Copyright (C) the libgit2 contributors. All rights reserved.
//               - based on libgit2/src/transports/ssh.c

// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//

#include "str.h"
#include "win32/utf-conv.h"
#include "process.h"
#include "../../ext/libgit2/src/libgit2/transports/smart.h"
#include "system-call.h"
#include "ssh-wintunnel.h"

#define OWNING_SUBTRANSPORT(s) ((ssh_subtransport *)(s)->parent.subtransport)

static const char cmd_uploadpack[] = "git-upload-pack";
static const char cmd_receivepack[] = "git-receive-pack";

typedef struct {
	git_smart_subtransport_stream parent;
	COMMAND_HANDLE commandHandle;
	const char *cmd;
	char *url;
} ssh_stream;

typedef struct {
	git_smart_subtransport parent;
	transport_smart *owner;
	ssh_stream *current_stream;
	char *cmd_uploadpack;
	char *cmd_receivepack;
	const LPWSTR *pEnv;
	LPWSTR sshtoolpath;
} ssh_subtransport;

static int ssh_stream_read(
	git_smart_subtransport_stream *stream,
	char *buffer,
	size_t buf_size,
	size_t *bytes_read)
{
	ssh_stream *s = (ssh_stream *)stream;

	if (command_read_stdout(&s->commandHandle, buffer, buf_size, bytes_read))
		return -1;

	return 0;
}

static int ssh_stream_write(
	git_smart_subtransport_stream *stream,
	const char *buffer,
	size_t len)
{
	ssh_stream *s = (ssh_stream *)stream;

	if (command_write(&s->commandHandle, buffer, len))
		return -1;

	return 0;
}

static void ssh_stream_free(git_smart_subtransport_stream *stream)
{
	ssh_stream *s = (ssh_stream *)stream;
	ssh_subtransport *t = OWNING_SUBTRANSPORT(s);

	DWORD exitcode = command_close(&s->commandHandle);
	if (s->commandHandle.errBuf) {
		if (exitcode && exitcode != MAXDWORD) {
			if (!git_str_oom(s->commandHandle.errBuf) && git_str_len(s->commandHandle.errBuf))
				git_error_set(GIT_ERROR_SSH, "Command exited non-zero (%ld) and returned:\n%s", exitcode, s->commandHandle.errBuf->ptr);
			else
				git_error_set(GIT_ERROR_SSH, "Command exited non-zero: %ld", exitcode);
		}
		git_str_dispose(s->commandHandle.errBuf);
		git__free(s->commandHandle.errBuf);
	}

	t->current_stream = NULL;

	git__free(s->url);
	git__free(s);
}

static int ssh_stream_alloc(
	ssh_subtransport *t,
	const char *url,
	const char *cmd,
	git_smart_subtransport_stream **stream)
{
	ssh_stream *s;
	git_str *errBuf;

	assert(stream);

	s = git__calloc(sizeof(ssh_stream), 1);
	if (!s) {
		git_error_set_oom();
		return -1;
	}

	errBuf = git__calloc(sizeof(git_buf), 1);
	if (!errBuf) {
		git__free(s);
		git_error_set_oom();
		return -1;
	}

	s->parent.subtransport = &t->parent;
	s->parent.read = ssh_stream_read;
	s->parent.write = ssh_stream_write;
	s->parent.free = ssh_stream_free;

	s->cmd = cmd;

	s->url = git__strdup(url);
	if (!s->url) {
		git__free(errBuf);
		git__free(s);
		git_error_set_oom();
		return -1;
	}

	command_init(&s->commandHandle);
	s->commandHandle.errBuf = errBuf;

	*stream = &s->parent;
	return 0;
}

static int wcstristr(const wchar_t *heystack, const wchar_t *needle)
{
	const wchar_t *end;
	size_t lenNeedle = wcslen(needle);
	size_t lenHeystack = wcslen(heystack);
	if (lenNeedle > lenHeystack)
		return 0;

	end = heystack + lenHeystack - lenNeedle;
	while (heystack <= end) {
		if (!wcsnicmp(heystack, needle, lenNeedle))
			return 1;
		++heystack;
	}

	return 0;
}

/* based on netops.c code of libgit2, needed by extract_url_parts, see comment there */
#define hex2c(c) ((c | 32) % 39 - 9)
static char* unescape(char *str)
{
	int x, y;
	int len = (int)strlen(str);

	for (x = y = 0; str[y]; ++x, ++y) {
		if ((str[x] = str[y]) == '%') {
			if (y < len - 2 && isxdigit(str[y + 1]) && isxdigit(str[y + 2])) {
				str[x] = (hex2c(str[y + 1]) << 4) + hex2c(str[y + 2]);
				y += 2;
			}
		}
	}
	str[x] = '\0';
	return str;
}

static int _git_ssh_setup_tunnel(
	ssh_subtransport *t,
	const char *url,
	const char *gitCmd,
	git_smart_subtransport_stream **stream)
{
	size_t i;
	ssh_stream *s;
	wchar_t *ssh = t->sshtoolpath;
	wchar_t *wideParams = NULL;
	wchar_t *cmd = NULL;
	git_net_url parsed_url = GIT_NET_URL_INIT;
	git_str params = GIT_STR_INIT;
	int isPutty;
	size_t length;

	*stream = NULL;
	if (ssh_stream_alloc(t, url, gitCmd, stream) < 0) {
		git_error_set_oom();
		return -1;
	}

	s = (ssh_stream *)*stream;

	if (git_net_url_parse_standard_or_scp(&parsed_url, url) < 0)
		goto on_error;

	if (!ssh) {
		git_error_set(GIT_ERROR_SSH, "No GIT_SSH tool configured");
		goto on_error;
	}

	isPutty = wcstristr(ssh, L"plink");
	if (parsed_url.port) {
		if (isPutty)
			git_str_printf(&params, " -P %s", parsed_url.port);
		else
			git_str_printf(&params, " -p %s", parsed_url.port);
	}
	if (isPutty && !wcstristr(ssh, L"tortoiseplink")) {
		git_str_puts(&params, " -batch");
	}

	if (git_process__is_cmdline_option(parsed_url.username)) {
		git_error_set(GIT_ERROR_NET, "cannot start ssh: username '%s' is ambiguous with command-line option", parsed_url.username);
		return -1;
	} else if (git_process__is_cmdline_option(parsed_url.host)) {
		git_error_set(GIT_ERROR_NET, "cannot start ssh: host '%s' is ambiguous with command-line option", parsed_url.host);
		return -1;
	} else if (git_process__is_cmdline_option(parsed_url.path)) {
		git_error_set(GIT_ERROR_NET, "cannot start ssh: path '%s' is ambiguous with command-line option", parsed_url.path);
		return -1;
	}

	if (parsed_url.username)
		git_str_printf(&params, " \"%s@%s\" ", parsed_url.username, parsed_url.host);
	else
		git_str_printf(&params, " \"%s\" ", parsed_url.host);

	git_str_printf(&params, "%s \"%s\"", gitCmd, parsed_url.path);
	if (git_str_oom(&params)) {
		git_error_set_oom();
		goto on_error;
	}

	if (git_utf8_to_16_alloc(&wideParams, params.ptr) < 0) {
		git_error_set_oom();
		goto on_error;
	}
	git_str_dispose(&params);

	length = wcslen(ssh) + wcslen(wideParams) + 3;
	cmd = git__calloc(length, sizeof(wchar_t));
	if (!cmd) {
		git_error_set_oom();
		goto on_error;
	}

	wcscat_s(cmd, length, L"\"");
	wcscat_s(cmd, length, ssh);
	wcscat_s(cmd, length, L"\"");
	wcscat_s(cmd, length, wideParams);

	if (command_start(cmd, &s->commandHandle, t->pEnv, isPutty ? CREATE_NEW_CONSOLE : DETACHED_PROCESS))
		goto on_error;

	git__free(wideParams);
	git__free(cmd);
	t->current_stream = s;
	git_net_url_dispose(&parsed_url);

	return 0;

on_error:
	t->current_stream = NULL;

	if (*stream)
		ssh_stream_free(*stream);

	git_str_dispose(&params);

	git__free(wideParams);

	git__free(cmd);

	git_net_url_dispose(&parsed_url);

	return -1;
}

static int ssh_uploadpack_ls(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	const char *cmd = t->cmd_uploadpack ? t->cmd_uploadpack : cmd_uploadpack;

	if (_git_ssh_setup_tunnel(t, url, cmd, stream) < 0)
		return -1;

	return 0;
}

static int ssh_uploadpack(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	GIT_UNUSED(url);

	if (t->current_stream) {
		*stream = &t->current_stream->parent;
		return 0;
	}

	git_error_set(GIT_ERROR_NET, "Must call UPLOADPACK_LS before UPLOADPACK");
	return -1;
}

static int ssh_receivepack_ls(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	const char *cmd = t->cmd_receivepack ? t->cmd_receivepack : cmd_receivepack;

	if (_git_ssh_setup_tunnel(t, url, cmd, stream) < 0)
		return -1;

	return 0;
}

static int ssh_receivepack(
	ssh_subtransport *t,
	const char *url,
	git_smart_subtransport_stream **stream)
{
	GIT_UNUSED(url);

	if (t->current_stream) {
		*stream = &t->current_stream->parent;
		return 0;
	}

	git_error_set(GIT_ERROR_NET, "Must call RECEIVEPACK_LS before RECEIVEPACK");
	return -1;
}

static int _ssh_action(
	git_smart_subtransport_stream **stream,
	git_smart_subtransport *subtransport,
	const char *url,
	git_smart_service_t action)
{
	ssh_subtransport *t = (ssh_subtransport *) subtransport;

	switch (action) {
		case GIT_SERVICE_UPLOADPACK_LS:
			return ssh_uploadpack_ls(t, url, stream);

		case GIT_SERVICE_UPLOADPACK:
			return ssh_uploadpack(t, url, stream);

		case GIT_SERVICE_RECEIVEPACK_LS:
			return ssh_receivepack_ls(t, url, stream);

		case GIT_SERVICE_RECEIVEPACK:
			return ssh_receivepack(t, url, stream);
	}

	*stream = NULL;
	return -1;
}

static int _ssh_close(git_smart_subtransport *subtransport)
{
	ssh_subtransport *t = (ssh_subtransport *) subtransport;

	assert(!t->current_stream);

	return 0;
}

static void _ssh_free(git_smart_subtransport *subtransport)
{
	ssh_subtransport *t = (ssh_subtransport *) subtransport;

	assert(!t->current_stream);

	git__free(t->cmd_uploadpack);
	git__free(t->cmd_receivepack);
	git__free(t->sshtoolpath);
	git__free(t);
}

int git_smart_subtransport_ssh_wintunnel(
	git_smart_subtransport **out, git_transport *owner, LPCWSTR sshtoolpath, const LPWSTR *pEnv)
{
	ssh_subtransport *t;

	assert(out);

	t = git__calloc(sizeof(ssh_subtransport), 1);
	if (!t) {
		git_error_set_oom();
		return -1;
	}

	t->sshtoolpath = _wcsdup(sshtoolpath);
	t->pEnv = pEnv;

	t->owner = (transport_smart *)owner;
	t->parent.action = _ssh_action;
	t->parent.close = _ssh_close;
	t->parent.free = _ssh_free;

	*out = (git_smart_subtransport *) t;
	return 0;
}
