#include "stdafx.h"
#include "TrcView.h"
#include "TrcViewDoc.h"
#include "TrcViewView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CTrcViewView, CColumnTreeView)

BEGIN_MESSAGE_MAP(CTrcViewView, CColumnTreeView)
	ON_NOTIFY(TVN_ITEMEXPANDING, TreeID, OnExpanding)
	ON_NOTIFY(TVN_GETDISPINFO, TreeID, OnGetDispInfo)
END_MESSAGE_MAP()

/*----------------------------------------------------------------------
 * 
 * Node classes. Instances of these classes are atteched to the tree
 * nodes to carry context information.
 *
 */
class INode
{
public:
	virtual HRESULT Enumerate(
		__in JPTRCRHANDLE FileHandle,
		__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
		__in_opt PVOID Context
		) PURE;
};

class ClientNode : public INode
{
private:
	JPTRCR_CLIENT Client;

public:
	ClientNode( PJPTRCR_CLIENT Client )
	{
		this->Client = *Client;
	}

	virtual HRESULT Enumerate(
		__in JPTRCRHANDLE FileHandle,
		__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
		__in_opt PVOID Context
		)
	{
		return JptrcrEnumCalls(
			FileHandle,
			&this->Client,
			Callback,
			Context );
	}
};

class ChildNode : public INode
{
private:
	JPTRCR_CALL_HANDLE CallerHandle;

public:
	ChildNode( PJPTRCR_CALL_HANDLE CallerHandle )
	{
		this->CallerHandle = *CallerHandle;
	}

	virtual HRESULT Enumerate(
		__in JPTRCRHANDLE FileHandle,
		__in JPTRCR_ENUM_CALLS_ROUTINE Callback,
		__in_opt PVOID Context
		)
	{
		return JptrcrEnumChildCalls(
			FileHandle,
			&this->CallerHandle,
			Callback,
			Context );
	}
};

struct CHILD_CALLBACK_CONTEXT
{
	HTREEITEM ParentItem;
	CTrcViewView *View;
};

/*----------------------------------------------------------------------
 * 
 * Privates.
 *
 */

void __stdcall CTrcViewView::EnumClientsCallback(
	__in PJPTRCR_CLIENT Client,
	__in_opt PVOID Context
	)
{
	CTrcViewView* View = static_cast< CTrcViewView* >( Context );
	CTreeCtrl& tree = View->GetTreeCtrl();
	
	CString Caption;
	Caption.Format( 
		IDS_CLIENT_CAPTION, 
		Client->ProcessId,
		Client->ProcessId,
		Client->ThreadId,
		Client->ThreadId );
	
	TVINSERTSTRUCT Node;
	Node.hParent = TVI_ROOT ;
	Node.hInsertAfter = TVI_LAST;
	Node.item.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM;
	Node.item.pszText = Caption.GetBuffer();
	Node.item.lParam = ( LONG_PTR ) ( PVOID ) new ClientNode( Client );
	Node.item.cChildren = 1;

	tree.InsertItem( &Node );
}

void __stdcall CTrcViewView::EnumCallsCallback(
	__in PJPTRCR_CALL Call,
	__in_opt PVOID PvContext
	)
{
	CHILD_CALLBACK_CONTEXT* Context = 
		static_cast< CHILD_CALLBACK_CONTEXT* >( PvContext );

	CTreeCtrl& tree = Context->View->GetTreeCtrl();

	CString ModuleName;
	if ( Call->Module != NULL )
	{
		ModuleName = Call->Module->Name;
	}
	else
	{
		ModuleName = L"[unknown]";
	}

	CString SymName;
	if ( Call->Symbol != NULL )
	{
		SymName = Call->Symbol->Name;
	}
	else
	{
		SymName.Format( L"%x", Call->Procedure );
	}

	CString Caption;
	Caption.Format( 
		L"%s!%s\t%I64u\t%I64u\t%I64u\t%d\t%x",
		ModuleName,
		SymName,
		Call->EntryTimestamp,
		Call->ExitTimestamp,
		Call->ExitTimestamp - Call->EntryTimestamp,
		Call->ChildCalls,
		Call->Result.ReturnValue );

	TVINSERTSTRUCT Node;
	Node.hParent = Context->ParentItem;
	Node.hInsertAfter = TVI_LAST;
	Node.item.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM;
	Node.item.pszText = Caption.GetBuffer();
	Node.item.cChildren = Call->ChildCalls > 0 ? 1 : 0;
	Node.item.lParam = ( LONG_PTR ) ( PVOID ) new ChildNode( &Call->CallHandle );

	tree.InsertItem( &Node );
}

void CTrcViewView::ReloadClients()
{
	HRESULT Hr = JptrcrEnumClients(
		GetDocument()->GetTraceHandle(),
		EnumClientsCallback,
		this );
	if ( FAILED( Hr ) )
	{
		theApp.ShowErrorMessage( Hr );
	}
}

/*----------------------------------------------------------------------
 * 
 * Public/Protected.
 *
 */

CTrcViewView::CTrcViewView() : ColumnsCreated( FALSE )
{
}

CTrcViewView::~CTrcViewView()
{
}

BOOL CTrcViewView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CColumnTreeView::PreCreateWindow(cs);
}


#ifdef _DEBUG
void CTrcViewView::AssertValid() const
{
	CColumnTreeView::AssertValid();
}

void CTrcViewView::Dump(CDumpContext& dc) const
{
	CColumnTreeView::Dump(dc);
}

CTrcViewDoc* CTrcViewView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CTrcViewDoc)));
	return (CTrcViewDoc*)m_pDocument;
}
#endif //_DEBUG


void CTrcViewView::OnInitialUpdate()
{
	CColumnTreeView::OnInitialUpdate();

	if ( ! this->ColumnsCreated )
	{
		CTreeCtrl& tree = GetTreeCtrl();
		CHeaderCtrl& header = GetHeaderCtrl();

		DWORD dwStyle = GetWindowLong(tree, GWL_STYLE);
		dwStyle |= TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT;
		SetWindowLong(tree, GWL_STYLE, dwStyle);

		//m_ImgList.Create(IDB_IMAGES, 16, 1, RGB(255,0,255));
		//tree.SetImageList(&m_ImgList, TVSIL_NORMAL);

		HDITEM hditem;
		hditem.mask = HDI_TEXT | HDI_WIDTH | HDI_FORMAT;
		hditem.fmt = HDF_LEFT | HDF_STRING;
		hditem.cxy = 400;
		hditem.pszText = L"Routine";
		header.InsertItem(0, &hditem);
		
		hditem.cxy = 150;
		hditem.pszText = L"Entry time";
		header.InsertItem(1, &hditem);
		
		hditem.cxy = 150;
		hditem.pszText = L"Exit time";
		header.InsertItem(2, &hditem);
		
		hditem.cxy = 150;
		hditem.pszText = L"Duration";
		header.InsertItem(3, &hditem);
		
		hditem.cxy = 50;
		hditem.pszText = L"Child Calls";
		header.InsertItem(4, &hditem);
		
		hditem.cxy = 100;
		hditem.pszText = L"Return code";
		header.InsertItem(5, &hditem);

		UpdateColumns();

		this->ColumnsCreated = TRUE;
	}
}

void CTrcViewView::OnUpdate(
	CView*,
	LPARAM,
	CObject* 
	)
{
	CTrcViewDoc* Doc = GetDocument();

	if ( ! Doc->IsLoaded() )
	{
		//
		// Not loaded yet -> nothing to do.
		//
	}
	else
	{
		ReloadClients();
	}
}

void CTrcViewView::OnExpanding( NMHDR *Hdr, LRESULT *Lresult )
{
	LPNMTREEVIEW Notif = ( LPNMTREEVIEW ) Hdr;
	if ( Notif->action == TVE_EXPAND )
	{
		INode* Node = ( INode* ) ( PVOID ) Notif->itemNew.lParam;
		ASSERT( Node );

		CHILD_CALLBACK_CONTEXT Context;
		Context.View = this;
		Context.ParentItem = Notif->itemNew.hItem;

		Node->Enumerate( 
			GetDocument()->GetTraceHandle(),
			EnumCallsCallback,
			&Context );

		*Lresult = FALSE;
	}
	else if ( Notif->action == TVE_COLLAPSE )
	{
		*Lresult = FALSE;
	}
}

void CTrcViewView::OnGetDispInfo( NMHDR *Hdr, LRESULT * )
{
	LPNMTVDISPINFO Notif = ( LPNMTVDISPINFO ) Hdr;
	if ( Notif->item.mask & TVIF_CHILDREN )
	{
		Notif->item.cChildren = 0;
	}
}