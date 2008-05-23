/*----------------------------------------------------------------------
 * Purpose:
 *		Short of a TLS mechanism, we use some spare bytes in the
 *		thread's ETHREAD to store data.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include "jpfbtp.h"

//
// Offset of ETHREAD fields - vary among releases.
// We use the top JPFBTP_SPARE_BITS bits of each.
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

NTSTATUS JpfbtSetFbtDataThread(
	__in PETHREAD Thread,
	__in PJPFBT_THREAD_DATA Data 
	)
{
	PUCHAR ThreadPtr = ( PUCHAR ) Thread;
	ULONG DataVa = ( ULONG ) ( ULONG_PTR ) Data;
	ULONG DataVaMaskHi = DataVa & 0xFFFF0000;
	ULONG DataVaMaskLo = DataVa << 16;
	PULONG SameThreadPassiveFlags;
	PULONG SameThreadApcFlags;
	
#if DBG
	PJPFBT_THREAD_DATA ThreadData;

	ThreadData = JpfbtGetFbtDataThread( Thread );
	ASSERT( ThreadData == NULL || 
			ThreadData->Signature == JPFBT_THREAD_DATA_SIGNATURE );
#endif

	SameThreadPassiveFlags	= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadPassiveFlagsOffset );
	SameThreadApcFlags		= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadApcFlagsOffset );

	//
	// Save: overwrite high words while keeping low words intact.
	//
	*SameThreadPassiveFlags = DataVaMaskHi | ( *SameThreadPassiveFlags & 0xFFFF );
	*SameThreadApcFlags		= DataVaMaskLo | ( *SameThreadApcFlags     & 0xFFFF );

	return STATUS_SUCCESS;
}

PJPFBT_THREAD_DATA JpfbtGetFbtDataThread(
	__in PETHREAD Thread
	)
{
	PJPFBT_THREAD_DATA ThreadData;
	PUCHAR ThreadPtr = ( PUCHAR ) Thread;
	ULONG DataVa;
	ULONG DataVaLo;
	ULONG DataVaHi;
	PULONG SameThreadPassiveFlags;
	PULONG SameThreadApcFlags;
	
	SameThreadPassiveFlags	= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadPassiveFlagsOffset );
	SameThreadApcFlags		= ( PULONG ) ( ThreadPtr + JpfbtsSameThreadApcFlagsOffset );

	DataVaHi = *SameThreadPassiveFlags	& 0xFFFF0000;
	DataVaLo = *SameThreadApcFlags		& 0xFFFF0000;
		
	DataVa = DataVaHi | ( ( DataVaLo >> 16 ) & 0xFFFF );

	ThreadData = ( PJPFBT_THREAD_DATA ) ( PVOID ) ( ULONG_PTR ) DataVa;

	ASSERT( ThreadData == NULL ||
			ThreadData->Signature == JPFBT_THREAD_DATA_SIGNATURE );
	return ThreadData;
}

