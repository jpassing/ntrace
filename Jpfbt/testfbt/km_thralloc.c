#include <cfix.h>
#include "test.h"
#include "..\jpfbt\jpfbtp.h"

void CreateGlobalState()
{
	KIRQL OldIrql;
	PJPFBT_GLOBAL_DATA State;

	KeRaiseIrql( DISPATCH_LEVEL, &OldIrql );
	TEST_SUCCESS( JpfbtpCreateGlobalState(
		1,
		8,
		0,
		FALSE,
		&State ) );
	JpfbtpFreeGlobalState( State );
	TEST_SUCCESS( JpfbtpCreateGlobalState(
		1,
		8,
		32,
		FALSE,
		&State ) );
	JpfbtpFreeGlobalState( State );
	KeLowerIrql( OldIrql );
}

void AllocateThreadDataAtApcLevel()
{
	PJPFBT_THREAD_DATA ThreadData;

	CFIX_ASSERT( KeGetCurrentIrql() <= APC_LEVEL );
	TEST_SUCCESS( JpfbtpCreateGlobalState(
		1,
		8,
		1,		// 1 preallocated struct
		FALSE,
		&JpfbtpGlobalState ) );

	//
	// Alloc & Free at low IRQL.
	//
	ThreadData = JpfbtpAllocateThreadDataForCurrentThread();
	TEST( ThreadData );
	JpfbtpFreeThreadData( ThreadData );
	TEST( ExQueryDepthSList( &JpfbtpGlobalState->ThreadDataFreeList ) == 0 );

	JpfbtpFreeGlobalState( JpfbtpGlobalState );
	JpfbtpGlobalState = NULL;
}

void AllocateThreadDataAtDirql()
{
	KIRQL OldIrql;
	PJPFBT_THREAD_DATA ThreadData;

	CFIX_ASSERT( KeGetCurrentIrql() <= APC_LEVEL );
	TEST_SUCCESS( JpfbtpCreateGlobalState(
		1,
		8,
		1,		// 1 preallocated struct
		FALSE,
		&JpfbtpGlobalState ) );

	//
	// Alloc & Free at high IRQL. Shouuld grab the one prealloc'ed struct.
	//
	KeRaiseIrql( DISPATCH_LEVEL + 1, &OldIrql );
	ThreadData = JpfbtpAllocateThreadDataForCurrentThread();
	TEST( ThreadData );

	//
	// 2nd allocation must fail.
	//
	TEST( NULL == JpfbtpAllocateThreadDataForCurrentThread() );

	TEST( ExQueryDepthSList( &JpfbtpGlobalState->ThreadDataPreallocationList ) == 0 );
	JpfbtpFreeThreadData( ThreadData );
	TEST( ExQueryDepthSList( &JpfbtpGlobalState->ThreadDataPreallocationList ) == 1 );
	TEST( ExQueryDepthSList( &JpfbtpGlobalState->ThreadDataFreeList ) == 0 );
	KeLowerIrql( OldIrql );
	
	JpfbtpFreeGlobalState( JpfbtpGlobalState );
	JpfbtpGlobalState = NULL;
}

void AllocateThreadDataAtApcAndFreeAtDirql()
{
	KIRQL OldIrql;
	PJPFBT_THREAD_DATA ThreadData;

	CFIX_ASSERT( KeGetCurrentIrql() <= APC_LEVEL );
	TEST_SUCCESS( JpfbtpCreateGlobalState(
		1,
		8,
		1,		// 1 preallocated struct
		FALSE,
		&JpfbtpGlobalState ) );

	//
	// Alloc at low IRQL. Shouuld grab the one prealloc'ed struct.
	//
	ThreadData = JpfbtpAllocateThreadDataForCurrentThread();
	TEST( ThreadData );

	//
	// free to free list.
	//
	KeRaiseIrql( DISPATCH_LEVEL + 1, &OldIrql );
	JpfbtpFreeThreadData( ThreadData );
	KeLowerIrql( OldIrql );

	TEST( ExQueryDepthSList( &JpfbtpGlobalState->ThreadDataFreeList ) == 1 );

	//
	// Alloc at low irql - free should free the struct on free list.
	//
	ThreadData = JpfbtpAllocateThreadDataForCurrentThread();
	TEST( ThreadData );
	JpfbtpFreeThreadData( ThreadData );

	TEST( ExQueryDepthSList( &JpfbtpGlobalState->ThreadDataFreeList ) == 0 );
	
	JpfbtpFreeGlobalState( JpfbtpGlobalState );
	JpfbtpGlobalState = NULL;
}

CFIX_BEGIN_FIXTURE( ThreadDataAllocation )
	CFIX_FIXTURE_ENTRY( CreateGlobalState )
	CFIX_FIXTURE_ENTRY( AllocateThreadDataAtApcLevel )
	CFIX_FIXTURE_ENTRY( AllocateThreadDataAtDirql )
	CFIX_FIXTURE_ENTRY( AllocateThreadDataAtApcAndFreeAtDirql )
CFIX_END_FIXTURE()