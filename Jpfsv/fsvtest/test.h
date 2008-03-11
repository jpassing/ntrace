#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <cfix.h>
#include <cdiag.h>

#define TEST CFIX_ASSERT
#define TEST_OK( expr ) CFIX_ASSERT_EQUALS_DWORD( 0, ( expr ) )

void LaunchNotepad(
	__out PPROCESS_INFORMATION ppi
	);

CDIAG_SESSION_HANDLE CreateDiagSession();
