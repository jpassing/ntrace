#include <jpfsv.h>
#include "test.h"

static void TestThreadEnum( 
	__in DWORD ProcId 
	)
{
	JPFSV_ENUM_HANDLE Enum;
	JPFSV_THREAD_INFO Thr;
	UINT Count = 0;

	TEST_OK( JpfsvEnumThreads( 0, ProcId, &Enum ) );
	TEST( Enum );

	Thr.Size = 123;
	TEST( E_INVALIDARG == JpfsvGetNextItem( Enum, &Thr ) );

	for ( ;; )
	{
		HRESULT Hr;
		Thr.Size = sizeof( JPFSV_THREAD_INFO );
		Hr = JpfsvGetNextItem( Enum, &Thr );

		TEST( Count == 0 ? S_OK == Hr : SUCCEEDED( Hr ) );
		Count++;

		if ( S_FALSE == Hr )
		{
			break;
		}

		TEST( ProcId == 0 || Thr.ThreadId > 0 );
	}

	TEST( Count > 0 );

	TEST( E_INVALIDARG == JpfsvCloseEnum( NULL ) );
	TEST_OK( JpfsvCloseEnum( Enum ) );
}

static void EnumModules( 
	__in DWORD ProcId,
	__in JPFSV_ENUM_HANDLE Enum 
	)
{
	JPFSV_MODULE_INFO Mod;
	UINT Count = 0;
	HRESULT Hr;

	TEST( Enum );

	Mod.Size = 0;
	TEST( E_INVALIDARG == JpfsvGetNextItem( Enum, &Mod ) );

	Mod.Size = 123;
	TEST( ProcId == 0 || E_INVALIDARG == JpfsvGetNextItem( Enum, &Mod ) );

	for ( ;; )
	{
		Mod.Size = sizeof( JPFSV_MODULE_INFO );
		Hr = JpfsvGetNextItem( Enum, &Mod );

		if ( ProcId == 0 )
		{
			TEST( Hr == S_FALSE );
		}
		else
		{
			TEST( Count == 0 ? S_OK == Hr : SUCCEEDED( Hr ) );
		}
		Count++;

		if ( S_FALSE == Hr )
		{
			break;
		}

		TEST( Mod.LoadAddress );
		TEST( wcslen( Mod.ModuleName ) );
		//OutputDebugString( L"  " );
		//OutputDebugString( Mod.ModuleName );
		//OutputDebugString( L"\n" );
	}

	TEST( Count > 0 );

	TEST( E_INVALIDARG == JpfsvCloseEnum( NULL ) );
	TEST_OK( JpfsvCloseEnum( Enum ) );
}

static void TestUserModuleEnum( DWORD ProcId )
{
	JPFSV_ENUM_HANDLE Enum;
	HRESULT Hr;

	Hr = JpfsvEnumModules( 0, ProcId, &Enum );
	if ( E_ACCESSDENIED == Hr )
	{
		OutputDebugString( L"  Access denied\n" );
		return;
	}
	else if ( 0x8007012b == Hr )
	{
		OutputDebugString( L"  Access failed\n" );
		return;
	}
	
	EnumModules( ProcId, Enum );
}

static void TestProcessEnum()
{
	JPFSV_ENUM_HANDLE Enum;
	JPFSV_PROCESS_INFO Proc;
	UINT Count = 0;

	TEST_OK( JpfsvEnumProcesses( 0, &Enum ) );
	TEST( Enum );

	Proc.Size = 123;
	TEST( E_INVALIDARG == JpfsvGetNextItem( Enum, &Proc ) );

	for ( ;; )
	{
		HRESULT Hr;
		Proc.Size = sizeof( JPFSV_PROCESS_INFO );
		Hr = JpfsvGetNextItem( Enum, &Proc );

		TEST( Count == 0 ? S_OK == Hr : SUCCEEDED( Hr ) );
		Count++;

		if ( S_FALSE == Hr )
		{
			break;
		}

		TEST( wcslen( Proc.ExeName ) );

		//OutputDebugString( Proc.ExeName );
		//OutputDebugString( L"\n" );

		TestThreadEnum( Proc.ProcessId );
		TestUserModuleEnum( Proc.ProcessId );
	}

	TEST( E_INVALIDARG == JpfsvCloseEnum( NULL ) );
	TEST_OK( JpfsvCloseEnum( Enum ) );
}

static void TestDriverEnum()
{
	JPFSV_ENUM_HANDLE Enum;
	HRESULT Hr;

	Hr = JpfsvEnumModules( 0, JPFSV_KERNEL, &Enum );
	if ( E_ACCESSDENIED == Hr )
	{
		OutputDebugString( L"  Access denied\n" );
		return;
	}
	
	EnumModules( JPFSV_KERNEL, Enum );
}

static void TestSanitize()
{
	WCHAR Buffer[ MAX_PATH ];
	TEST( 
		E_INVALIDARG ==
		JpfsvSanitizeDeviceDriverPath(
			L"",
			_countof( Buffer ),
			Buffer ) );

	TEST( 
		JPFSV_E_UNRECOGNIZED_PATH_FORMAT ==
		JpfsvSanitizeDeviceDriverPath(
			L"\\",
			_countof( Buffer ),
			Buffer ) );

	TEST( 
		JPFSV_E_UNRECOGNIZED_PATH_FORMAT ==
		JpfsvSanitizeDeviceDriverPath(
			L"c:\\",
			_countof( Buffer ),
			Buffer ) );

	TEST( 
		JPFSV_E_UNRECOGNIZED_PATH_FORMAT ==
		JpfsvSanitizeDeviceDriverPath(
			L"\\SystemRoot\\",
			_countof( Buffer ),
			Buffer ) );

	TEST_OK( JpfsvSanitizeDeviceDriverPath(
			L"c:\\windows\\system32\\notepad.exe",
			_countof( Buffer ),
			Buffer ) );
	TEST( 0 == _wcsicmp( Buffer, L"c:\\windows\\system32\\notepad.exe" ) );

	TEST_OK( JpfsvSanitizeDeviceDriverPath(
			L"\\windows\\system32\\notepad.exe",
			_countof( Buffer ),
			Buffer ) );
	TEST( 0 == _wcsicmp( Buffer, L"c:\\windows\\system32\\notepad.exe" ) );

	TEST_OK( JpfsvSanitizeDeviceDriverPath(
			L"\\SystemRoot\\system32\\notepad.exe",
			_countof( Buffer ),
			Buffer ) );
	TEST( 0 == _wcsicmp( Buffer, L"c:\\windows\\system32\\notepad.exe" ) );
}

CFIX_BEGIN_FIXTURE( PsInfoEnum )
	CFIX_FIXTURE_ENTRY( TestSanitize )
	CFIX_FIXTURE_ENTRY( TestProcessEnum )
	CFIX_FIXTURE_ENTRY( TestDriverEnum )
CFIX_END_FIXTURE()