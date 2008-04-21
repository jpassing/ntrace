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
	WmkAllocateEventEx(
		WMK_E_FBT_IMAGE_INFO, 
		Path->Length > 0 
			? Path->Length - 1
			: 0 );

	WmkEvent->ImageLoadAddress	= ImageLoadAddress;
	WmkEvent->ImageSize			= ImageSize;
	WmkEvent->PathLength		= Path->Length;

	if ( Path->Length > 0 )
	{
		RtlCopyMemory( 
			 WmkGetDataSection( WmkEvent ), 
			 Path->Buffer, 
			 Path->Length );
	}

    WmkCommitEvent();
}

VOID JpkfbtWmkLogProcedureEntryEvent(
    __in HANDLE ProcessId,
    __in HANDLE ThreadId,
	__in PVOID Procedure
	)
{
	WmkAllocateEvent( WMK_E_FBT_PROCEDURE_ENTRY );
	WmkEvent->ProcessId	= ProcessId;
	WmkEvent->ThreadId	= ThreadId;
	WmkEvent->Procedure	= ( ULONG_PTR ) Procedure;
	WmkCommitEvent();
}

VOID JpkfbtWmkLogProcedureExitEvent(
    __in HANDLE ProcessId,
    __in HANDLE ThreadId,
	__in PVOID Procedure
	)
{
	WmkAllocateEvent( WMK_E_FBT_PROCEDURE_EXIT );
	WmkEvent->ProcessId	= ProcessId;
	WmkEvent->ThreadId	= ThreadId;
	WmkEvent->Procedure	= ( ULONG_PTR ) Procedure;
	WmkCommitEvent();
}
