// TrcViewDoc.h : interface of the CTrcViewDoc class
//


#pragma once


class CTrcViewDoc : public CDocument
{
protected: // create from serialization only
	CTrcViewDoc();
	DECLARE_DYNCREATE(CTrcViewDoc)

// Attributes
public:

// Operations
public:

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);

// Implementation
public:
	virtual ~CTrcViewDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};


