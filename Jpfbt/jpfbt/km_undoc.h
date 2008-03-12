#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Undocumented, but exported NT APIs.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

typedef struct _DEFERRED_REVERSE_BARRIER {
    ULONG Barrier;
    ULONG TotalProcessors;
} DEFERRED_REVERSE_BARRIER, *PDEFERRED_REVERSE_BARRIER;

VOID
KeGenericCallDpc (
    __in PKDEFERRED_ROUTINE Routine,
    __in_opt PVOID Context
    );

VOID
KeSignalCallDpcDone (
    __in PVOID SystemArgument1
    );

LOGICAL
KeSignalCallDpcSynchronize (
    __in PVOID SystemArgument2
    );