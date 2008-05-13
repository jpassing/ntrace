#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Main header file for the TrcView application.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <cdiag.h>
#include "resource.h"


class CTrcViewApp : public CWinApp
{
private:
	//
	// Message resolver for error messages.
	//
	PCDIAG_MESSAGE_RESOLVER Resolver;

public:
	CTrcViewApp();
	~CTrcViewApp();

	virtual BOOL InitInstance();

	int ShowErrorMessage( 
		__in HRESULT Hr 
		);

	afx_msg void OnAppAbout();
	DECLARE_MESSAGE_MAP()
};

extern CTrcViewApp theApp;