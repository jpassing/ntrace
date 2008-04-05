#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Auxiliary header file. Do not include directly.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

typedef enum _JPKFBT_KERNEL_TYPE
{
	JpkfbtKernelRetail	= 0,
	JpkfbtKernelWmk		= 1,
	JpkfbtKernelMax		= 1
} JPKFBT_KERNEL_TYPE;

typedef enum _JPKFBT_TRACING_TYPE
{
	JpkfbtTracingTypeWmk = 0,
	JpkfbtTracingTypeMax = 0
} JPKFBT_TRACING_TYPE;