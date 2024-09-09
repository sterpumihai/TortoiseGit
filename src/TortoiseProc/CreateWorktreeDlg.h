﻿// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2022-2024 - TortoiseGit

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

#include "StandAloneDlg.h"
#include "ChooseVersion.h"

// CCreateWorktreeDlg dialog

class CCreateWorktreeDlg : public CResizableStandAloneDialog
	, public CChooseVersion
{
	DECLARE_DYNAMIC(CCreateWorktreeDlg)

public:
	CCreateWorktreeDlg(CWnd* pParent = nullptr); // standard constructor
	virtual ~CCreateWorktreeDlg();

	// Dialog Data
	enum
	{
		IDD = IDD_WORKTREE_CREATE
	};

	BOOL m_bCheckout = BST_CHECKED;
	BOOL m_bForce = BST_UNCHECKED;
	BOOL m_bDetach = BST_UNCHECKED;

	BOOL m_bBranch = BST_UNCHECKED;
	CString m_sNewBranch;

	CString m_sWorktreePath;

protected:
	void DoDataExchange(CDataExchange* pDX) override; // DDX/DDV support
	BOOL OnInitDialog() override;

	CHOOSE_EVENT_RADIO();

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedWorkdirDirBrowse();
	afx_msg void OnBnClickedCheckBranch();
	afx_msg void OnBnClickedCheckDetach();
	afx_msg void OnCbnEditchangeComboboxexVersion();

	void OnVersionChanged() override;
	afx_msg void OnDestroy();
};
