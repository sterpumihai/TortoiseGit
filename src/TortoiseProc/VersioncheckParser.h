﻿// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2013-2018, 2020-2024 - TortoiseGit

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
#pragma once
#include "..\..\ext\simpleini\SimpleIni.h"

class CVersioncheckParser
{
public:
	CVersioncheckParser();
	~CVersioncheckParser();

	CVersioncheckParser(const CVersioncheckParser&) = delete;
	CVersioncheckParser& operator=(const CVersioncheckParser&) = delete;

	bool Load(const CString& filename, CString& err);

	struct Version
	{
		CString version;
		CString version_for_filename;
		CString version_languagepacks;
		unsigned int major = 0;
		unsigned int minor = 0;
		unsigned int micro = 0;
		unsigned int build = 0;
	};
	Version GetTortoiseGitVersion();

	CString GetTortoiseGitInfoText();
	CString GetTortoiseGitInfoTextURL();
	CString GetTortoiseGitIssuesURL();
	bool GetTortoiseGitHasChangelog();
	CString GetTortoiseGitChangelogURL();
	bool GetTortoiseGitIsDirectDownloadable();
	CString GetTortoiseGitBaseURL();
	bool GetTortoiseGitIsHotfix();
	CString GetTortoiseGitMainfilename();

	struct LanguagePack
	{
		CString m_PackName;
		CString m_LangName;
		DWORD m_LocaleID;
		CString m_LangCode;

		CString m_filename;
	};
	using LANGPACK_VECTOR = std::vector<LanguagePack>;
	LANGPACK_VECTOR GetTortoiseGitLanguagePacks();

private:
	CString		GetTortoiseGitLanguagepackFilenameTemplate();
	CString		GetStringValue(const CString& section, const CString& entry);

	CSimpleIni	m_versioncheckfile;

	Version		m_version;
};
