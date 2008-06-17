/*----------------------------------------------------------------------
 * Purpose:
 *		Short of a TLS mechanism, we use some spare bytes in the
 *		thread's ETHREAD to store data.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "jpfbtp.h"

#define JPFBTP_BUSY_BIT 0x8000

//
// Offset of ETHREAD fields - vary among releases.
// We use the top JPFBTP_SPARE_BITS bits of each:
//  16+1 bits from SameThreadPassiveFlags
//  14 bits from SameThreadApcFlags
//
// Both SameThreadPassiveFlags and SameThreadApcFlags are
// for use by the same thread only and are thus safe to use w/o
// synchronization.
//
static JpfbtsSameThreadPassiveFlagsOffset	= 0;
static JpfbtsSameThreadApcFlagsOffset		= 0;

/*----------------------------------------------------------------------
 *
 * Internals.
 *
 */
NTSTATUS JpfbtpInitializeKernelTls(
	__in ULONG SameThreadPassiveFlagsOffset,
	__in ULONG SameThreadApcFlagsOffset
	)
{
	JpfbtsSameThreadPassiveFlagsOffset	= SameThreadPassiveFlagsOffset;
	JpfbtsSameThreadApcFlagsOffset		= SameThreadApcFlagsOffset;
	return STATUS_SUCCESS;
}


VOID JpfbtpDeleteKernelTls()
{
	ASSERT( JpfbtsSameThreadPassiveFlagsOffset != 0);
}

BOOLEAN JpfbtpAcquireCurrentThread()
{
	PUCHAR ThreadPtr = ( PUCHAR ) PsGetCurrentThread();
	volatile LONG* SameThreadPassiveFlags;
	ULONG OldValue;

	SameThreadPassiveFlags	= ( volatile LONG* ) 
		( ThreadPtr + JpfbtsSameThreadPassiveFlagsOffset );

	//
	// Try to set JPFBTP_BUSY_BIT.
	//
	OldValue = InterlockedOr( SameThreadPassiveFlags, JPFBTP_BUSY_BIT );
	if ( OldValue & JPFBTP_BUSY_BIT )
	{
		//
		// Already acquired by someone else, reentrance must have
		// occured.
		//
		InterlockedIncrement( 
			&JpfbtpGlobalState->Counters.ReentrantThunkExecutionsDetected );

		return FALSE;
	}
	else
	{
		//
		// Bit was 0 before, we own the thread now.
		//
		ASSERT( ! JpfbtpAcquireCurrentThread() );
		return TRUE;
	}
}

VOID JpfbtpReleaseCurrentThread()
{
	PUCHAR ThreadPtr = ( PUCHAR ) PsGetCurrentThread();
	volatile LONG* SameThreadPassiveFlags;
	ULONG OldValue;

	SameThreadPassiveFlags	= ( volatile LONG* ) 
		( ThreadPtr + JpfbtsSameThreadPassiveFlagsOffset );

	//
	// Clear JPFBTP_BUSY_BIT.
	//
	OldValue = InterlockedAnd( SameThreadPassiveFlags, ~JPFBTP_BUSY_BIT );
	if ( ! ( OldValue & JPFBTP_BUSY_BIT ) )
	{
		//
		// Huh? Not acquired!
		//
		ASSERT( !"Thread released that has not been acquired before" );
	}
}

NTSTATUS JpfbtpSetFbtDataThread(
	__in PETHREAD Thread,
	__in PJPFBT_THREAD_DATA Data 
	)
{
	PUCHAR ThreadPtr = ( PUCHAR ) Thread;
	ULONG DataVa = ( ULONG ) ( ULONG_PTR ) Data;
	ULONG DataVaMaskHi = DataVa & 0xFFFF0000;
	ULONG DataVaMaskLo = DataVa << 16;
	volatile PULONG SameThreadPassiveFlags;
	volatile PULONG SameThreadApcFlags;
	
#if DBG
	PJPFBT_THREAD_DATA ThreadData;

	ThreadData = JpfbtpGetFbtDataThread( Thread );
	ASSERT( ThreadData == NULL || 
			ThreadData->Signature == JPFBT_THREAD_DATA_SIGNATURE );
#endif

	//
	// N.B. We are protected by JpfbtpAcquireCurrentThread, so it is
	// ok to be non-atomic here.
	//

	SameThreadPassiveFlags	= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadPassiveFlagsOffset );
	SameThreadApcFlags		= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadApcFlagsOffset );

	//
	// Save: overwrite high words while keeping low words intact.
	//
	*SameThreadPassiveFlags = DataVaMaskHi | ( *SameThreadPassiveFlags & 0xFFFF );
	*SameThreadApcFlags		= DataVaMaskLo | ( *SameThreadApcFlags     & 0x3FFFF );

	return STATUS_SUCCESS;
}

PJPFBT_THREAD_DATA JpfbtpGetFbtDataThread(
	__in PETHREAD Thread
	)
{
	PJPFBT_THREAD_DATA ThreadData;
	PUCHAR ThreadPtr = ( PUCHAR ) Thread;
	ULONG DataVa;
	ULONG DataVaLo;
	ULONG DataVaHi;
	volatile PULONG SameThreadPassiveFlags;
	volatile PULONG SameThreadApcFlags;
	
	//
	// N.B. We are protected by JpfbtpAcquireCurrentThread, so it is
	// ok to be non-atomic here.
	//

	SameThreadPassiveFlags	= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadPassiveFlagsOffset );
	SameThreadApcFlags		= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadApcFlagsOffset );

	DataVaHi = *SameThreadPassiveFlags	& 0xFFFF0000;
	DataVaLo = *SameThreadApcFlags		& 0xFFFC0000;
		
	//ASSERT( ( DataVaHi == 0 ) == ( DataVaLo == 0 ) );

	DataVa = DataVaHi | ( ( DataVaLo >> 16 ) & 0xFFFF );

	ThreadData = ( PJPFBT_THREAD_DATA ) ( PVOID ) ( ULONG_PTR ) DataVa;

	ASSERT( ThreadData == NULL ||
			ThreadData->Signature == JPFBT_THREAD_DATA_SIGNATURE );
	return ThreadData;
}

