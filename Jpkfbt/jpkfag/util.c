/*----------------------------------------------------------------------
 * Purpose:
 *		Utility routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#include <wdm.h>

#include "jpkfagp.h"

NTSTATUS JpkfagpCompleteRequest(
	__in PIRP Irp,
	__in NTSTATUS Status,
	__in ULONG_PTR Information,
	__in CCHAR PriorityBoost
	)
{
	ASSERT( NULL == Irp->CancelRoutine );
	
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Information;
	
	IoCompleteRequest( Irp, PriorityBoost );

	return Status;
}