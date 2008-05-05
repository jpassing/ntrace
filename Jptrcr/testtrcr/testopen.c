/*----------------------------------------------------------------------
 * Purpose:
 *		Opening tests.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jptrcr.h>
#include <cfix.h>

#define TEST_HR( Hr, Expr ) CFIX_ASSERT_EQUALS_DWORD( ( ULONG ) Hr, ( Expr ) )
#define TEST_OK( Expr ) TEST_HR( S_OK, Expr )

#define E_FNF HRESULT_FROM_WIN32( ERROR_FILE_NOT_FOUND )

static void TestOpenInvalidFile()
{
	JPTRCRHANDLE Handle;

	TEST_HR( JPTRCR_E_INVALID_SIGNATURE, JptrcrOpenFile(
		L"testtrcr.lib" , &Handle ) );
	TEST_HR( E_FNF, JptrcrOpenFile( 
		L"idonotexist", &Handle ) );
}

// EOF, closing

CFIX_BEGIN_FIXTURE( OpenClose )
	CFIX_FIXTURE_ENTRY( TestOpenInvalidFile )
CFIX_END_FIXTURE()
