/*----------------------------------------------------------------------
 * Purpose:
 *		PS API Wrappers.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfsv.h>
#include <stdlib.h>
#include <tlhelp32.h>
#include <psapi.h>
#include "internal.h"

#pragma warning( push )
#pragma warning( disable: 6011; disable: 6387 )
#include <strsafe.h>
#pragma warning( pop )


struct _JPFSV_ENUM;

#define JPFSV_ENUM_SIGNATURE 'EvsF'

typedef HRESULT ( *JPFSV_NEXT_ITEM_ROUTINE )(
		__in struct _JPFSV_ENUM *Enum,
		__out PVOID Item
		);

typedef HRESULT ( *JPFSV_CLOSE_ROUTINE )(
	__in struct _JPFSV_ENUM *Enum
	);

typedef struct _JPFSV_ENUM
{
	DWORD Signature;
	
	struct
	{
		JPFSV_NEXT_ITEM_ROUTINE NextItem;
		JPFSV_CLOSE_ROUTINE Close;
	} Routines;

	union
	{
		struct
		{
			HANDLE Snapshot;
			BOOL FirstFetched;
			DWORD ProcessId;
		} ToolhelpEnum;

		struct
		{
			HANDLE Process;
			DWORD ModuleCount;
			DWORD NextIndex;
			HMODULE Modules[ ANYSIZE_ARRAY ];
		} PsapiEnum;
	} Data;
} JPFSV_ENUM, *PJPFSV_ENUM;

/*----------------------------------------------------------------------
 *
 * Toolhelp Helper functions.
 *
 */
static HRESULT JpfsvsCloseToolhelpEnum(
	__in PJPFSV_ENUM Enum
	)
{
	if ( ! Enum )
	{
		return E_INVALIDARG;
	}

	if ( Enum->Data.ToolhelpEnum.Snapshot )
	{
		VERIFY( CloseHandle( Enum->Data.ToolhelpEnum.Snapshot ) );
	}

	free( Enum );
	
	return S_OK;
}

static HRESULT JpfsvsCreateToolhelpEnum(
	__in HANDLE Snapshot,
	__in DWORD ProcessId,
	__in JPFSV_NEXT_ITEM_ROUTINE NextItemRoutine,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	)
{
	PJPFSV_ENUM Enum;
	HRESULT Hr = E_UNEXPECTED;

	*EnumHandle = NULL;

	//
	// Allocate enum.
	//
	Enum = ( PJPFSV_ENUM ) malloc( sizeof( JPFSV_ENUM ) );
	if ( ! Enum )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Initialize.
	//
	Enum->Signature = JPFSV_ENUM_SIGNATURE;
	Enum->Routines.Close = JpfsvsCloseToolhelpEnum;
	Enum->Routines.NextItem = NextItemRoutine;

	Enum->Data.ToolhelpEnum.FirstFetched = FALSE;
	Enum->Data.ToolhelpEnum.Snapshot = Snapshot;
	Enum->Data.ToolhelpEnum.ProcessId = ProcessId;

	if ( Enum->Data.ToolhelpEnum.Snapshot == INVALID_HANDLE_VALUE )
	{
		DWORD Err = GetLastError();
		Hr = HRESULT_FROM_WIN32( Err );
		goto Cleanup;
	}

	Hr = S_OK;
	*EnumHandle = Enum;

Cleanup:
	if ( FAILED( Hr ) )
	{
		if ( Enum )
		{	
			ASSERT( Enum->Routines.Close );
			Enum->Routines.Close( Enum );
		}
	}

	return Hr;
}


/*----------------------------------------------------------------------
 *
 * Process Enum.
 *
 */

static HRESULT JpfsvsNextProcess(
	__in PJPFSV_ENUM Enum,
	__out PVOID Item
	)
{
	PJPFSV_PROCESS_INFO Process = ( PJPFSV_PROCESS_INFO ) Item;
	PROCESSENTRY32 Entry;
	BOOL Res;

	if ( ! Enum || 
		 ! Item ||
		 Process->Size != sizeof( JPFSV_PROCESS_INFO ) )
	{
		return E_INVALIDARG;
	}

	Entry.dwSize = sizeof( PROCESSENTRY32 );

	if ( Enum->Data.ToolhelpEnum.FirstFetched )
	{
		Res = Process32Next(
			Enum->Data.ToolhelpEnum.Snapshot,
			&Entry );
	}
	else
	{
		Res = Process32First(
			Enum->Data.ToolhelpEnum.Snapshot,
			&Entry );

		Enum->Data.ToolhelpEnum.FirstFetched = TRUE;
	}

	if ( Res )
	{
		Process->ProcessId = Entry.th32ProcessID;
		return StringCchCopy(
			Process->ExeName,
			MAX_PATH,
			Entry.szExeFile );
	}
	else
	{
		return S_FALSE;	
	}
}

HRESULT JpfsvEnumProcesses(
	__reserved PVOID Reserved,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	)
{
	HANDLE Snapshot;
	if ( Reserved != NULL ||
		 ! EnumHandle )
	{
		return E_INVALIDARG;
	}

	Snapshot = CreateToolhelp32Snapshot(
		TH32CS_SNAPPROCESS,
		0 );

	if ( Snapshot == INVALID_HANDLE_VALUE )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	return JpfsvsCreateToolhelpEnum(
		Snapshot,
		0,
		JpfsvsNextProcess,
		EnumHandle );
}

/*----------------------------------------------------------------------
 *
 * Thread Enum.
 *
 */

static HRESULT JpfsvsNextThread(
	__in PJPFSV_ENUM Enum,
	__out PVOID Item
	)
{
	PJPFSV_THREAD_INFO Thread = ( PJPFSV_THREAD_INFO ) Item;
	THREADENTRY32 Entry;
	BOOL Res;

	if ( ! Enum || 
		 ! Item ||
		 Thread->Size != sizeof( JPFSV_THREAD_INFO ) )
	{
		return E_INVALIDARG;
	}

	Entry.dwSize = sizeof( THREADENTRY32 );

	do
	{
		if ( Enum->Data.ToolhelpEnum.FirstFetched )
		{
			Res = Thread32Next(
				Enum->Data.ToolhelpEnum.Snapshot,
				&Entry );
		}
		else
		{
			Res = Thread32First(
				Enum->Data.ToolhelpEnum.Snapshot,
				&Entry );

			Enum->Data.ToolhelpEnum.FirstFetched = TRUE;
		}
	}
	while ( Res && 
		    Entry.th32OwnerProcessID != Enum->Data.ToolhelpEnum.ProcessId );

	if ( Res )
	{
		Thread->ThreadId = Entry.th32ThreadID;
		return S_OK;
	}
	else
	{
		return S_FALSE;	
	}
}

HRESULT JpfsvEnumThreads(
	__reserved PVOID Reserved,
	__in DWORD ProcessId,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	)
{
	HANDLE Snapshot;

	if ( Reserved != NULL ||
		 ! EnumHandle )
	{
		return E_INVALIDARG;
	}

	Snapshot = CreateToolhelp32Snapshot(
		TH32CS_SNAPTHREAD,
		ProcessId );

	if ( Snapshot == INVALID_HANDLE_VALUE )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	return JpfsvsCreateToolhelpEnum(
		Snapshot,
		ProcessId,
		JpfsvsNextThread,
		EnumHandle );
}

/*----------------------------------------------------------------------
 *
 * Empty Enum.
 *
 */

static HRESULT JpfsvsCloseEmptyEnum(
	__in PJPFSV_ENUM Enum
	)
{
	if ( ! Enum )
	{
		return E_INVALIDARG;
	}

	free( Enum );
	
	return S_OK;
}

static HRESULT JpfsvsNextEmptyEnum(
	__in PJPFSV_ENUM Enum,
	__out PVOID Item
	)
{
	UNREFERENCED_PARAMETER( Item );
	if ( ! Enum || 
		 ! Item ||
		 *( ( PDWORD ) Item ) == 0 )
	{
		return E_INVALIDARG;
	}

	//
	// This enum is empty.
	//
	return S_FALSE;
}

static HRESULT JpfsvsCreateEmptyEnum(
	__out JPFSV_ENUM_HANDLE *EnumHandle
	)
{
	PJPFSV_ENUM Enum;

	*EnumHandle = NULL;

	//
	// Allocate enum.
	//
	Enum = ( PJPFSV_ENUM ) malloc( sizeof( JPFSV_ENUM ) );
	if ( ! Enum )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Initialize.
	//
	Enum->Signature = JPFSV_ENUM_SIGNATURE;
	Enum->Routines.Close = JpfsvsCloseEmptyEnum;
	Enum->Routines.NextItem = JpfsvsNextEmptyEnum;

	*EnumHandle = Enum;

	return S_OK;
}
/*----------------------------------------------------------------------
 *
 * Module Enum.
 *
 */
static HRESULT JpfsvsClosePsapiEnum(
	__in PJPFSV_ENUM Enum
	)
{
	if ( ! Enum )
	{
		return E_INVALIDARG;
	}

	if ( Enum->Data.PsapiEnum.Process )
	{
		VERIFY( CloseHandle( Enum->Data.PsapiEnum.Process ) );
	}

	free( Enum );
	
	return S_OK;
}

static HRESULT JpfsvsNextUserProcessModule(
	__in PJPFSV_ENUM Enum,
	__out PVOID Item
	)
{
	PJPFSV_MODULE_INFO Module = ( PJPFSV_MODULE_INFO ) Item;
	if ( ! Enum || 
		 ! Item ||
		 Module->Size != sizeof( JPFSV_MODULE_INFO ) )
	{
		return E_INVALIDARG;
	}

	if ( Enum->Data.PsapiEnum.NextIndex < Enum->Data.PsapiEnum.ModuleCount )
	{
		MODULEINFO ModuleInfo;
		HMODULE ModuleHandle = 
			Enum->Data.PsapiEnum.Modules[ Enum->Data.PsapiEnum.NextIndex ];
		
		if ( ! GetModuleInformation(
			Enum->Data.PsapiEnum.Process,
			ModuleHandle,
			&ModuleInfo,
			sizeof( MODULEINFO ) ) )
		{
			DWORD Err = GetLastError();
			return HRESULT_FROM_WIN32( Err );
		}

		Module->LoadAddress = ( DWORD_PTR ) ModuleInfo.lpBaseOfDll;
		Module->ModuleSize = ModuleInfo.SizeOfImage;
		
		if ( ! GetModuleBaseName(
			Enum->Data.PsapiEnum.Process,
			ModuleHandle,
			Module->ModuleName,
			MAX_PATH ) )
		{
			DWORD Err = GetLastError();
			return HRESULT_FROM_WIN32( Err );
		}

		if ( ! GetModuleFileNameEx(
			Enum->Data.PsapiEnum.Process,
			ModuleHandle,
			Module->ModulePath,
			MAX_PATH ) )
		{
			DWORD Err = GetLastError();
			return HRESULT_FROM_WIN32( Err );
		}

		//
		// Advance enum.
		//
		Enum->Data.PsapiEnum.NextIndex++;

		return S_OK;
	}
	else
	{
		//
		// End of enum.
		//
		return S_FALSE;
	}
}

static HRESULT JpfsvEnumUserProcessModules(
	__in DWORD ProcessId,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	)
{
	DWORD SizeRequired = 0;
	HANDLE Process;
	HMODULE Dummy;
	PJPFSV_ENUM Enum;
	if ( ! EnumHandle )
	{
		return E_INVALIDARG;
	}

	if ( ProcessId == 0 )
	{
		//
		// Process 0 cannot be enumerated, so return empty enum.
		// 
		return JpfsvsCreateEmptyEnum( EnumHandle );
	}

	Process = OpenProcess( 
		PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
		FALSE, 
		ProcessId );
	if ( ! Process )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	if ( ! EnumProcessModules(
		Process,
		&Dummy,
		sizeof( Dummy ),
		&SizeRequired ) )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	ASSERT( SizeRequired > 0 );
	ASSERT( ( SizeRequired % sizeof( HMODULE ) ) == 0 );

	//
	// Allocate enum.
	//
	Enum = ( PJPFSV_ENUM ) malloc( 
		RTL_SIZEOF_THROUGH_FIELD( 
			JPFSV_ENUM,
			Data.PsapiEnum.Modules[ ( SizeRequired / sizeof( HMODULE ) ) - 1 ] ) );
	if ( ! Enum )
	{
		return E_OUTOFMEMORY;
	}

	//
	// Initialize.
	//
	Enum->Signature = JPFSV_ENUM_SIGNATURE;
	Enum->Routines.Close = JpfsvsClosePsapiEnum;
	Enum->Routines.NextItem = JpfsvsNextUserProcessModule;

	Enum->Data.PsapiEnum.ModuleCount = SizeRequired / sizeof( HMODULE );
	Enum->Data.PsapiEnum.NextIndex = 0;
	Enum->Data.PsapiEnum.Process = Process;

	if ( ! EnumProcessModules(
		Process,
		Enum->Data.PsapiEnum.Modules,
		SizeRequired,
		&SizeRequired ) )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	*EnumHandle = Enum;

	return S_OK;
}

HRESULT JpfsvEnumModules(
	__reserved PVOID Reserved,
	__in DWORD ProcessId,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	)
{
	if ( Reserved != NULL ||
		 ! EnumHandle )
	{
		return E_INVALIDARG;
	}

	if ( ProcessId == JPFSV_KERNEL )
	{
		ASSERT(! "NIY" );
		return E_NOTIMPL;
	}
	else
	{
		return JpfsvEnumUserProcessModules( ProcessId, EnumHandle );
	}
}

/*----------------------------------------------------------------------
 *
 * Wrappers.
 *
 */
HRESULT JpfsvGetNextItem(
	__in JPFSV_ENUM_HANDLE EnumHandle,
	__out PVOID Item
	)
{
	PJPFSV_ENUM Enum = ( PJPFSV_ENUM ) EnumHandle;

	if ( ! Enum ||
		 Enum->Signature != JPFSV_ENUM_SIGNATURE ||
		 ! Item )
	{
		return E_INVALIDARG;
	}

	return Enum->Routines.NextItem( Enum, Item );
}

HRESULT JpfsvCloseEnum(
	__in JPFSV_ENUM_HANDLE EnumHandle
	)
{
	PJPFSV_ENUM Enum = ( PJPFSV_ENUM ) EnumHandle;

	if ( ! Enum ||
		 Enum->Signature != JPFSV_ENUM_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	return Enum->Routines.Close( Enum );
}