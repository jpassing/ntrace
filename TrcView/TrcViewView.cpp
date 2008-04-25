#include "stdafx.h"
#include "TrcView.h"
#include "TrcViewDoc.h"
#include "TrcViewView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CTrcViewView, CColumnTreeView)

BEGIN_MESSAGE_MAP(CTrcViewView, CColumnTreeView)
	ON_NOTIFY(TVN_ITEMEXPANDING, TreeID, OnGetDispInfo)
END_MESSAGE_MAP()


CTrcViewView::CTrcViewView()
{
}

CTrcViewView::~CTrcViewView()
{
}

BOOL CTrcViewView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CColumnTreeView::PreCreateWindow(cs);
}

void CTrcViewView::OnDraw(CDC* /*pDC*/)
{
	CTrcViewDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
	{
		return;
	}
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
	
	hditem.cxy = 100;
	hditem.pszText = L"Return code";
	header.InsertItem(3, &hditem);
	
	UpdateColumns();

	HTREEITEM hRoot = tree.InsertItem(L"All Employees", 0, 0, TVI_ROOT);
	
	TVINSERTSTRUCT Node;
	Node.hParent = hRoot;
	Node.hInsertAfter = TVI_LAST;
	Node.item.mask = TVIF_CHILDREN | TVIF_TEXT;
	Node.item.pszText = L"expandable";
	Node.item.cChildren = I_CHILDRENCALLBACK;

	tree.InsertItem( &Node );

	//HTREEITEM hCat = tree.InsertItem(L"New York", 0, 0, hRoot);
	//tree.InsertItem(L"John Smith\tA00012\t\t40,000 USD", 1, 1, hCat);
	//tree.InsertItem(L"Julia Brown\tA00235\t36,000 USD", 1, 1, hCat);
	//tree.InsertItem(L"Kevin Jones\tA00720\t28,000 USD", 1, 1, hCat);
	//tree.Expand(hCat, TVE_EXPAND);
	//hCat = tree.InsertItem(L"Warsaw", 0, 0, hRoot);
	//tree.InsertItem(L"Jan Kowalski\tB00241\t50,000 PLN", 1, 1, hCat);
	//tree.InsertItem(L"Maria Nowak\tB00532\t34,000 PLN", 1, 1, hCat);
	//tree.InsertItem(L"Adam Jaworek\tB00855\t31,500 PLN", 1, 1, hCat);
	//tree.Expand(hCat, TVE_EXPAND);
	//tree.Expand(hRoot, TVE_EXPAND);
}

void CTrcViewView::OnGetDispInfo(NMHDR *Hdr, LRESULT *Lresult)
{
}