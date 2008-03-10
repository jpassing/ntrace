/*----------------------------------------------------------------------
 * Purpose:
 *		Code patching.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <jpfbt.h>
#include "..\jpfbtp.h"


NTSTATUS JpfbtpPatchCode(
	__in JPFBT_PATCH_ACTION Action,
	__in ULONG PatchCount,
	__in_ecount(PatchCount) PJPFBT_CODE_PATCH *Patches 
	)
{
	UNREFERENCED_PARAMETER( Action );
	UNREFERENCED_PARAMETER( PatchCount );
	UNREFERENCED_PARAMETER( Patches );
	return STATUS_NOT_IMPLEMENTED;
}
