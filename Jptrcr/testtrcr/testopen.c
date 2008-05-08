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

typedef struct _CALLBACK_CONTEXT
{
	JPTRCRHANDLE Handle;
	ULONG Counter;
} CALLBACK_CONTEXT, *PCALLBACK_CONTEXT;

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

static void ChildCallsCallback(
	__in PJPTRCR_CALL Call,
	__in_opt PVOID Context
	)
{
	PCALLBACK_CONTEXT Ctx = ( PCALLBACK_CONTEXT ) Context;
	TEST( Ctx );
	if ( ! Ctx ) return;
	Ctx->Counter++;

	TEST( ! ( ( Call->EntryType == JptrcrSyntheticEntry ) && 
		      ( Call->ExitType == JptrcrSyntheticExit ) ) );

	//
	// Recurse.
	//
	if ( Call->EntryType == JptrcrSyntheticEntry )
	{
		TEST( Call->ChildCalls == 0 );
	
		TEST_HR( E_INVALIDARG, 
			JptrcrEnumChildCalls( Ctx->Handle, &Call->CallHandle, ChildCallsCallback, Ctx ) );
	}
	else
	{
		TEST_OK( JptrcrEnumChildCalls( Ctx->Handle, &Call->CallHandle, ChildCallsCallback, Ctx ) );
	}
}

static void TopLevelCallsCallback(
	__in PJPTRCR_CALL Call,
	__in_opt PVOID Context
	)
{
	PCALLBACK_CONTEXT Ctx = ( PCALLBACK_CONTEXT ) Context;
	CALLBACK_CONTEXT ChildrenCtx;

	TEST( Ctx );
	if ( ! Ctx ) return;
	Ctx->Counter++;

	TEST( ! ( ( Call->EntryType == JptrcrSyntheticEntry ) && 
		      ( Call->ExitType == JptrcrSyntheticExit ) ) );

	if ( Call->EntryType == JptrcrSyntheticEntry )
	{
		TEST( Call->ChildCalls == 0 );

		TEST_HR( E_INVALIDARG, 
			JptrcrEnumChildCalls( Ctx->Handle, &Call->CallHandle, ChildCallsCallback, &ChildrenCtx ) );
	}
	else
	{
		//
		// Walk all children and see if their count is correct.
		//
		ChildrenCtx.Counter = 0;
		ChildrenCtx.Handle = Ctx->Handle;
		TEST_OK( JptrcrEnumChildCalls( Ctx->Handle, &Call->CallHandle, ChildCallsCallback, &ChildrenCtx ) );

		//
		// Allow some bias due to missing tranitions
		//
		TEST( abs( ( LONG ) ChildrenCtx.Counter - ( LONG ) Call->ChildCalls ) <= 
			  ( LONG ) ChildrenCtx.Counter / 20 + 5 );
	}
}

static void ExpectSomeClientsCallback(
	__in PJPTRCR_CLIENT Client,
	__in_opt PVOID Context
	)
{
	PCALLBACK_CONTEXT Ctx = ( PCALLBACK_CONTEXT ) Context;
	CALLBACK_CONTEXT SubCtx;

	TEST( Ctx );
	if ( ! Ctx ) return;
	Ctx->Counter++;

	TEST( ( Client->ThreadId % 4 ) == 0 );

	SubCtx.Handle = Ctx->Handle;
	SubCtx.Counter = 0;
	TEST_OK( JptrcrEnumCalls( Ctx->Handle, Client, TopLevelCallsCallback, &SubCtx ) );

	TEST( SubCtx.Counter > 0 );
}


static void TestOpenNtfsFile()
{
	JPTRCRHANDLE Handle;
	CALLBACK_CONTEXT Ctx;
	Ctx.Counter = 0;
	TEST_OK( JptrcrOpenFile( DATA_DIR L"ntfs.jtrc" , &Handle ) );
	TEST_OK( JptrcrEnumModules( Handle, ExpectNtfsModuleCallback, &Ctx.Counter  ) );
	TEST( Ctx.Counter > 0 );

	Ctx.Counter = 0;
	Ctx.Handle = Handle;
	TEST_OK( JptrcrEnumClients( Handle, ExpectSomeClientsCallback, &Ctx ) );
	TEST( Ctx.Counter > 0 );

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
