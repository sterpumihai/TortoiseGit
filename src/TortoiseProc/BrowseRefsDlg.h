﻿// TortoiseGit - a Windows shell extension for easy version control

// Copyright (C) 2009-2024 - TortoiseGit

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
#include "resource.h"
#include "StandAloneDlg.h"
#include "FilterEdit.h"
#include "ResizableColumnsListCtrl.h"
#include "gittype.h"
#include "HistoryCombo.h"
#include "GestureEnabledControl.h"

const int gPickRef_Head		= 1;
const int gPickRef_Tag		= 2;
const int gPickRef_Remote	= 4;
const int gPickRef_All		= gPickRef_Head | gPickRef_Tag | gPickRef_Remote;
const int gPickRef_NoTag	= gPickRef_All & ~gPickRef_Tag;

class CBrowseRefsDlgFilter;

class CShadowTree
{
public:
	using TShadowTreeMap = std::map<CString, CShadowTree>;

	CShadowTree() = default;

	CShadowTree*	GetNextSub(CString& nameLeft, bool bCreateIfNotExist);

	bool			IsLeaf()const {return m_ShadowTree.empty();}
	CString			GetRefName()const
	{
		if (!m_pParent)
			return m_csRefName;
		return m_pParent->GetRefName()+"/"+m_csRefName;
	}

	/**
	 * from = refs/heads, refname = refs/heads/master => true
	 * from = refs/heads, refname = refs/heads => true
	 * from = refs/heads, refname = refs/headsh => false
	 * from = refs/heads/, refname = refs/heads/master => true
	 * from = refs/heads/, refname = refs/heads => false
	 */
	bool			IsFrom(const wchar_t* from)const
	{
		CString name = GetRefName();
		int len = static_cast<int>(wcslen(from));
		if (from[len - 1] != '/' && wcsncmp(name, from, len) == 0)
		{
			if (len == name.GetLength())
				return true;
			if (len < name.GetLength() && name[len] == '/')
				return true;
			return false;
		}

		return wcsncmp(name, from, len) == 0;
	}

	CString			GetRefsHeadsName() const
	{
		return GetRefName().Mid(static_cast<int>(wcslen(L"refs/heads/")));
	}

	CShadowTree*	FindLeaf(CString partialRefName);

	CString			m_csRefName;
	CString			m_csUpstream;
	CString			m_csRefHash;
	CTime			m_csAuthorDate;
	CString			m_csCommitter;
	CTime			m_csCommitterDate;
	CString			m_csAuthor;
	CString			m_csSubject;
	CString			m_csDescription;

	HTREEITEM		m_hTree = nullptr;

	TShadowTreeMap	m_ShadowTree;
	CShadowTree*	m_pParent = nullptr;
};
using VectorPShadowTree = std::vector<CShadowTree*>;

class CBrowseRefsDlg : public CResizableStandAloneDialog
{
	DECLARE_DYNAMIC(CBrowseRefsDlg)

public:
	CBrowseRefsDlg(CString cmdPath, CWnd* pParent = nullptr);   // standard constructor
	virtual ~CBrowseRefsDlg();

	enum eCmd
	{
		eCmd_ViewLog = WM_APP,
		eCmd_AddRemote,
		eCmd_ManageRemotes,
		eCmd_CreateBranch,
		eCmd_CreateTag,
		eCmd_DeleteAllTags,
		eCmd_DeleteBranch,
		eCmd_DeleteRemoteBranch,
		eCmd_DeleteTag,
		eCmd_ShowReflog,
		eCmd_Diff,
		eCmd_Fetch,
		eCmd_Switch,
		eCmd_Merge,
		eCmd_Rename,
		eCmd_RepoBrowser,
		eCmd_DeleteRemoteTag,
		eCmd_EditBranchDescription,
		eCmd_ViewLogRange,
		eCmd_ViewLogRangeReachableFromOnlyOne,
		eCmd_UnifiedDiff,
		eCmd_UpstreamDrop,
		eCmd_UpstreamSet,
		eCmd_DiffWC,
		eCmd_Copy,
		eCmd_Select,
	};

	enum eCol
	{
		eCol_Name,
		eCol_Upstream,
		eCol_AuthorDate,
		eCol_Msg,
		eCol_LastAuthor,
		eCol_CommitterDate,
		eCol_LastCommitter,
		eCol_Hash,
		eCol_Description,
	};

// Dialog Data
	enum { IDD = IDD_BROWSE_REFS };

protected:
	void DoDataExchange(CDataExchange* pDX) override;    // DDX/DDV support

	DECLARE_MESSAGE_MAP()

	afx_msg void OnBnClickedOk();
	BOOL OnInitDialog() override;

	CString			GetSelectedRef(bool onlyIfLeaf, bool pickFirstSelIfMultiSel = false);

	void			Refresh(CString selectRef = CString());

	CShadowTree&	GetTreeNode(CString refName, CShadowTree* pTreePos = nullptr, bool bCreateIfNotExist = false);

	void			FillListCtrlForTreeNode(HTREEITEM treeNode);

	void			FillListCtrlForShadowTree(CShadowTree* pTree, CString refNamePrefix, bool isFirstLevel, const CBrowseRefsDlgFilter& filter);

	bool			SelectRef(CString refName, bool bExactMatch);

	bool			ConfirmDeleteRef(VectorPShadowTree& leafs);
	bool			DoDeleteRefs(VectorPShadowTree& leafs);
	bool			DoDeleteRef(CString completeRefName);

	CString			GetFullRefName(CString partialRefName);

	CShadowTree*	GetListEntry(int index);
	CShadowTree*	GetTreeEntry(HTREEITEM treeItem);

private:
	bool			m_bHasWC = true;

	CString			m_cmdPath;

	STRING_VECTOR	remotes;

	CShadowTree		m_TreeRoot;
	CShadowTree*	m_pListCtrlRoot = nullptr;
	CGestureEnabledControlTmpl<CTreeCtrl>	m_RefTreeCtrl;
	CGestureEnabledControlTmpl<CResizableColumnsListCtrl<CListCtrl>>	m_ListRefLeafs;

	CFilterEdit		m_ctrlFilter;
	afx_msg void OnEnChangeEditFilter();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	DWORD			m_SelectedFilters;
	void			SetFilterCueText();
	afx_msg LRESULT OnClickedInfoIcon(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnClickedCancelFilter(WPARAM wParam, LPARAM lParam);
	CComboBox		m_cBranchFilter;

	int				m_currSortCol = 0;
	bool			m_currSortDesc = false;
	CRegDWORD		m_regCurrSortCol;
	CRegDWORD		m_regCurrSortDesc;
	afx_msg void OnTvnSelchangedTreeRef(NMHDR *pNMHDR, LRESULT *pResult);

	afx_msg void OnContextMenu(CWnd* pWndFrom, CPoint point);

	void		GetSelectedLeaves(VectorPShadowTree& selectedLeafs);
	void		OnContextMenu_ListRefLeafs(CPoint point);
	void		OnContextMenu_RefTreeCtrl(CPoint point);
	static CString GetTwoSelectedRefs(VectorPShadowTree& selectedLeafs, const CString &lastSelected, const CString &separator);

	bool		AreAllFrom(VectorPShadowTree& leafs, const wchar_t* from);
	void		ShowContextMenu(CPoint point, HTREEITEM hTreePos, VectorPShadowTree& selectedLeafs);
	BOOL		PreTranslateMessage(MSG* pMsg) override;
	afx_msg void OnLvnColumnclickListRefLeafs(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDestroy();
	afx_msg void OnNMDblclkListRefLeafs(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnItemChangedListRefLeafs(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnEndlabeleditListRefLeafs(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnBeginlabeleditListRefLeafs(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedCurrentbranch();
	afx_msg void OnBnClickedIncludeNestedRefs();
	afx_msg void OnCbnSelchangeBrowseRefsBranchfilter();
	BOOL		m_bIncludeNestedRefs;
	CRegDWORD	m_regIncludeNestedRefs;
	void		UpdateInfoLabel();

	CString	m_sLastSelected;
	CString m_initialRef;
	int		m_pickRef_Kind = gPickRef_All;
	CString m_pickedRef;
	bool	m_bWantPick = false;
	bool	m_bPickOne = false;
	bool	m_bShowRangeOptionWithTwoRefs = false;
	bool	m_bPickedRefSet = false;

	DWORD	m_DateFormat = DATE_SHORTDATE;		// DATE_SHORTDATE or DATE_LONGDATE
	bool	m_bRelativeTimes = false;	// Show relative times

public:
	static CString	PickRef(bool returnAsHash = false, CString initialRef = CString(), int pickRef_Kind = gPickRef_All, bool pickMultipleRefs = false, bool showRangeOptionWithTwoRefs = true);
	static bool		PickRefForCombo(CHistoryCombo& refComboBox, int pickRef_Kind = gPickRef_All, int useShortName = gPickRef_Head);
};
