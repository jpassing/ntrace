/*----------------------------------------------------------------------
 * Purpose:
 *		WMK-specific routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#pragma warning(disable:4214)   // bit field types other than int
#pragma warning(disable:4201)   // nameless struct/union
#pragma warning(disable:4115)   // named type definition in parentheses
#pragma warning(disable:4127)   // condition expression is constant
#pragma warning(disable:4242)   
#pragma warning(disable:4244)   
#pragma warning(disable:4311)   
#pragma warning(disable:4312)   
#pragma warning(disable:4273)	// inconsistent linkage

#pragma warning(disable:6320)   
#pragma warning(disable:6385)   

#include <wmk.h>


VOID JpkfbtWmkLogImageInfoEvent(
	__in ULONGLONG ImageLoadAddress,
	__in ULONG ImageSize,
	__in PANSI_STRING Path 
	)
{
	PWMK_E_FBT_IMAGE_INFO Event;

	Event = _WmkAllocateEvent(
		sizeof( WMK_E_FBT_IMAGE_INFO ), 
		WMK_E_FBT_IMAGE_INFO_ID,
		Path->Length > 0 
			? Path->Length - 1
			: 0 );

	Event->ImageLoadAddress	= ImageLoadAddress;
	Event->ImageSize			= ImageSize;
	Event->PathLength		= Path->Length;

	if ( Path->Length > 0 )
	{
		RtlCopyMemory( 
			 WmkGetDataSection( Event ), 
			 Path->Buffer, 
			 Path->Length );
	}

	_WmkCommitEvent( Event );
}

VOID JpkfbtWmkLogProcedureEntryEvent(
    __in HANDLE ProcessId,
    __in HANDLE ThreadId,
	__in PVOID Procedure
	)
{
	PWMK_E_FBT_PROCEDURE_ENTRY Event;

	Event = _WmkAllocateEvent( 
		sizeof( WMK_E_FBT_PROCEDURE_ENTRY ), 
		WMK_E_FBT_PROCEDURE_ENTRY_ID,
		0 );

	Event->ProcessId	= ProcessId;
	Event->ThreadId		= ThreadId;
	Event->Procedure	= ( ULONG_PTR ) Procedure;

	_WmkCommitEvent( Event ); ( ULONG_PTR ) Procedure;
}

VOID JpkfbtWmkLogProcedureExitEvent(
    __in HANDLE ProcessId,
    __in HANDLE ThreadId,
	__in PVOID Procedure
	)
{
	PWMK_E_FBT_PROCEDURE_EXIT Event;

	Event = _WmkAllocateEvent( 
		sizeof( WMK_E_FBT_PROCEDURE_EXIT ), 
		WMK_E_FBT_PROCEDURE_EXIT_ID,
		0 );

	Event->ProcessId	= ProcessId;
	Event->ThreadId		= ThreadId;
	Event->Procedure	= ( ULONG_PTR ) Procedure;

	_WmkCommitEvent( Event );
}
