/*----------------------------------------------------------------------
 * Purpose:
 *		Symbol Resolver. Wraps all functionality relying on 
 *		dbghelp!Sym* routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "internal.h"

#define DBGHELP_TRANSLATE_TCHAR
#include <dbghelp.h>

#include <stdlib.h>

#define JPFSV_SYM_RESOLVER_SIGNATURE 'RmyS'

typedef struct _JPFSV_SYM_RESOLVER
{
	DWORD Signature;

	volatile LONG ReferenceCount;

	//
	// Required by dbghelp.
	//
	HANDLE ProcessHandle;
} JPFSV_SYM_RESOLVER, *PJPFSV_SYM_RESOLVER;

/*----------------------------------------------------------------------
 * 
 * Exports.
 *
 */

HRESULT JpfsvCreateSymbolResolver(
	__in HANDLE Process,
	__in_opt PWSTR UserSearchPath,
	__out JPFSV_HANDLE *ResolverHandle
	)
{
	PJPFSV_SYM_RESOLVER Resolver;
	HRESULT Hr = E_UNEXPECTED;

	if ( ! Process || ! ResolverHandle )
	{
		return E_INVALIDARG;
	}

	//
	// Lazily create resolver if neccessary.
	//
	EnterCriticalSection( &JpfsvpDbghelpLock );
	
	if ( ! SymInitialize( 
		Process,
		UserSearchPath,
		FALSE ) )
	{
		DWORD Err = GetLastError();
		Hr = HRESULT_FROM_WIN32( Err );
	}

	LeaveCriticalSection( &JpfsvpDbghelpLock );

	//
	// Create and initialize resolver object,
	//
	Resolver = malloc( sizeof( JPFSV_SYM_RESOLVER ) );

	if ( Resolver )
	{
		Resolver->Signature = JPFSV_SYM_RESOLVER_SIGNATURE;
		Resolver->ProcessHandle = Process;
		Resolver->ReferenceCount = 0;

		*ResolverHandle = Resolver;
		return S_OK;
	}
	else
	{
		return E_OUTOFMEMORY;
	}
}

HRESULT JpfsvCloseSymbolResolver(
	__in JPFSV_HANDLE ResolverHandle
	)
{
	PJPFSV_SYM_RESOLVER Resolver = ( PJPFSV_SYM_RESOLVER ) ResolverHandle;

	if ( ! Resolver ||
		 Resolver->Signature != JPFSV_SYM_RESOLVER_SIGNATURE )
	{
		return E_INVALIDARG;
	}

	VERIFY( SymCleanup( Resolver->ProcessHandle ) );
	free( Resolver );

	return S_OK;
}

HRESULT JpfsvLoadModule(
	__in JPFSV_HANDLE ResolverHandle,
	__in PWSTR ModulePath,
	__in DWORD_PTR LoadAddress,
	__in_opt DWORD SizeOfDll
	)
{
	PJPFSV_SYM_RESOLVER Resolver = ( PJPFSV_SYM_RESOLVER ) ResolverHandle;
	CHAR ModulePathAnsi[ MAX_PATH ];
	DWORD64 ImgLoadAddress;

	if ( ! Resolver ||
		 Resolver->Signature != JPFSV_SYM_RESOLVER_SIGNATURE ||
		 ! ModulePath ||
		 ! LoadAddress )
	{
		return E_INVALIDARG;
	}

	//
	// Dbghelp wants ANSI :(
	//
	if ( 0 == WideCharToMultiByte(
		CP_ACP,
		0,
		ModulePath,
		-1,
		ModulePathAnsi,
		sizeof( ModulePathAnsi ),
		NULL,
		NULL ) )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	EnterCriticalSection( &JpfsvpDbghelpLock );

	ImgLoadAddress = SymLoadModule64(
		Resolver->ProcessHandle,
		NULL,
		ModulePathAnsi,
		NULL,
		LoadAddress,
		SizeOfDll );

	LeaveCriticalSection( &JpfsvpDbghelpLock );

	if ( 0 == ImgLoadAddress )
	{
		DWORD Err = GetLastError();
		return HRESULT_FROM_WIN32( Err );
	}

	return S_OK;
}