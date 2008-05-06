/*----------------------------------------------------------------------
 * Purpose:
 *		Opening tests.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <jptrcr.h>
#include <cfix.h>

#define DATA_DIR L"..\\..\\..\\Jptrcr\\testtrcr\\data\\"

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

static void TestOpenTruncatedFile()
{
	JPTRCRHANDLE Handle;
	TEST_HR( JPTRCR_E_INVALID_VERSION, JptrcrOpenFile(
		DATA_DIR L"truncated.jtrc" , &Handle ) );
}

static void TestOpenSimpleFile()
{
	JPTRCRHANDLE Handle;
	TEST_OK( JptrcrOpenFile( DATA_DIR L"simple.jtrc" , &Handle ) );
	TEST_OK( JptrcrCloseFile( Handle ) );
}

CFIX_BEGIN_FIXTURE( OpenClose )
	CFIX_FIXTURE_ENTRY( TestOpenInvalidFile )
	CFIX_FIXTURE_ENTRY( TestOpenTruncatedFile )
	CFIX_FIXTURE_ENTRY( TestOpenSimpleFile )
CFIX_END_FIXTURE()
