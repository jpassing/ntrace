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
	//
	// TODO: distinguish tracing types.
	//
	JpkfbtTracingTypeDefault = 0,
	JpkfbtTracingTypeWmk = 1,
	JpkfbtTracingTypeMax = 1
} JPKFBT_TRACING_TYPE;

typedef struct _JPKFBT_STATISTICS
{
	ULONG InstrumentedRoutinesCount;

	struct
	{
		ULONG Free;
		ULONG Dirty;
		ULONG Collected;
	} Buffers;

	struct
	{
		ULONG FreePreallocationPoolSize;
		ULONG FailedPreallocationPoolAllocations;
	} ThreadData;

	ULONG ReentrantThunkExecutionsDetected;

	struct
	{

		ULONG EntryEventsDropped;
		ULONG ExitEventsDropped;
		ULONG UnwindEventsDropped;
		ULONG ImageInfoEventsDropped;
		ULONG FailedChunkFlushes;
	} Tracing;

	ULONG EventsCaptured;
	ULONG ExceptionsUnwindings;
} JPKFBT_STATISTICS, *PJPKFBT_STATISTICS;