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
	} Data;
} JPFSV_ENUM, *PJPFSV_ENUM;

/*----------------------------------------------------------------------
 *
 * Privates.
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
 * Module Enum.
 *
 */

static HRESULT JpfsvsNextUserProcessModule(
	__in PJPFSV_ENUM Enum,
	__out PVOID Item
	)
{
	PJPFSV_MODULE_INFO Module = ( PJPFSV_MODULE_INFO ) Item;
	MODULEENTRY32 Entry;
	BOOL Res;

	if ( ! Enum || 
		 ! Item ||
		 Module->Size != sizeof( JPFSV_MODULE_INFO ) )
	{
		return E_INVALIDARG;
	}

	if ( Enum->Data.ToolhelpEnum.ProcessId == 0 )
	{
		//
		// Toolhelp returns modules of own process for id 0. 
		// Enumerating modules for process 0 is futile anyway,
		// so end the enumeration.
		//
		return S_FALSE;
	}

	Entry.dwSize = sizeof( MODULEENTRY32 );

	if ( Enum->Data.ToolhelpEnum.FirstFetched )
	{
		Res = Module32Next(
			Enum->Data.ToolhelpEnum.Snapshot,
			&Entry );
	}
	else
	{
		Res = Module32First(
			Enum->Data.ToolhelpEnum.Snapshot,
			&Entry );

		Enum->Data.ToolhelpEnum.FirstFetched = TRUE;
	}

	if ( Res )
	{
		Module->BaseAddress = ( DWORD_PTR ) Entry.modBaseAddr;
		Module->ModuleSize = Entry.modBaseSize;
		return StringCchCopy(
			Module->ModuleName,
			MAX_PATH,
			Entry.szModule );
	}
	else
	{
		return S_FALSE;	
	}
}

static HRESULT JpfsvEnumUserProcessModules(
	__in DWORD ProcessId,
	__out JPFSV_ENUM_HANDLE *EnumHandle
	)
{
	HANDLE Snapshot;
	
	ASSERT( EnumHandle );

	Snapshot = CreateToolhelp32Snapshot(
		TH32CS_SNAPMODULE,
		ProcessId );

	if ( Snapshot == INVALID_HANDLE_VALUE )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	return JpfsvsCreateToolhelpEnum(
		Snapshot,
		ProcessId,
		JpfsvsNextUserProcessModule,
		EnumHandle );
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