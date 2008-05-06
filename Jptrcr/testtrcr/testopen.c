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

#define TEST CFIX_ASSERT
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

static void ExpectNtfsModuleCallback(
	__in PJPTRCR_MODULE Module,
	__in_opt PVOID Context
	)
{
	PULONG Counter = ( PULONG ) Context;
	TEST( Counter );
	if ( ! Counter ) return;

	TEST( *Counter == 0 );
	( *Counter )++;

	TEST( 0 == _wcsicmp( Module->Name, L"ntfs.sys" ) );
}

static void ExpectSomeClientsCallback(
	__in PJPTRCR_CLIENT Client,
	__in_opt PVOID Context
	)
{
	PULONG Counter = ( PULONG ) Context;
	TEST( Counter );
	if ( ! Counter ) return;
	( *Counter )++;

	//TEST( Client->ThreadId != 0 );
	TEST( ( Client->ThreadId % 4 ) == 0 );
}


static void TestOpenNtfsFile()
{
	JPTRCRHANDLE Handle;
	ULONG Count = 0;
	TEST_OK( JptrcrOpenFile( DATA_DIR L"ntfs.jtrc" , &Handle ) );
	TEST_OK( JptrcrEnumModules( Handle, ExpectNtfsModuleCallback, &Count ) );
	
	Count = 0;
	TEST_OK( JptrcrEnumClients( Handle, ExpectSomeClientsCallback, &Count ) );
	TEST( Count > 0 );

	TEST_OK( JptrcrCloseFile( Handle ) );
}

//static void TestOpenNtfsAfdFile()
//{
//	JPTRCRHANDLE Handle;
//	TEST_OK( JptrcrOpenFile( DATA_DIR L"ntfsafd.jtrc" , &Handle ) );
//	TEST_OK( JptrcrCloseFile( Handle ) );
//}

CFIX_BEGIN_FIXTURE( OpenClose )
	CFIX_FIXTURE_ENTRY( TestOpenInvalidFile )
	CFIX_FIXTURE_ENTRY( TestOpenTruncatedFile )
	CFIX_FIXTURE_ENTRY( TestOpenSimpleFile )
	CFIX_FIXTURE_ENTRY( TestOpenNtfsFile )
CFIX_END_FIXTURE()
