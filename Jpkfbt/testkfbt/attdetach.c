/*----------------------------------------------------------------------
 * Purpose:
 *		Attach/Detach test.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <cfix.h>
#include <jpkfbt.h>
#include <jpfbtdef.h>
#include "util.h"

void TestAttachDetachRetail()
{
	BOOL KernelSupported;
	JPKFBT_SESSION Session;

	TEST( ! IsDriverLoaded( L"jpkfar" ) );
	TEST_SUCCESS( JpkfbtIsKernelTypeSupported( 
		JpkfbtKernelRetail, &KernelSupported ) );
	if ( ! KernelSupported )
	{
		CFIX_INCONCLUSIVE( L"Kernel not Retail-compatible." );
	}

	TEST_SUCCESS( JpkfbtAttach( JpkfbtKernelRetail, &Session ) );
	TEST( Session );
	TEST_SUCCESS( JpkfbtDetach( Session, FALSE ) );

	Session = NULL;

	TEST( IsDriverLoaded( L"jpkfar" ) );

	//
	// Again, but this time unload.
	//
	TEST_SUCCESS( JpkfbtAttach( JpkfbtKernelRetail, &Session ) );
	TEST( Session );
	TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );

	TEST( ! IsDriverLoaded( L"jpkfar" ) );
}

void TestQueryStats()
{
	BOOL KernelSupported;
	JPKFBT_SESSION Session;
	JPKFBT_STATISTICS Stat;

	TEST( ! IsDriverLoaded( L"jpkfar" ) );
	TEST_SUCCESS( JpkfbtIsKernelTypeSupported( 
		JpkfbtKernelRetail, &KernelSupported ) );
	if ( ! KernelSupported )
	{
		CFIX_INCONCLUSIVE( L"Kernel not Retail-compatible." );
	}

	TEST_SUCCESS( JpkfbtAttach( JpkfbtKernelRetail, &Session ) );
	TEST( Session );
	
	DeleteFile( L"__testkfbt.log" );
	TEST_SUCCESS( JpkfbtInitializeTracing(
		Session,
		JpkfbtTracingTypeDefault,
		0x10,
		0x1000, 
		L"__testkfbt.log" ) );

	TEST_SUCCESS( JpkfbtQueryStatistics( Session, &Stat ) );
	TEST( Stat.Buffers.Free == 0x10 );
	TEST( Stat.Buffers.Dirty == 0 );

	TEST_SUCCESS( JpkfbtShutdownTracing( Session ) );
	TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );

	TEST( ! IsDriverLoaded( L"jpkfar" ) );
}

void TestAttachDetachWmk()
{
	BOOL KernelSupported;
	JPKFBT_SESSION Session;

	TEST( ! IsDriverLoaded( L"jpkfaw" ) );
	TEST_SUCCESS( JpkfbtIsKernelTypeSupported( 
		JpkfbtKernelWmk, &KernelSupported ) );
	if ( ! KernelSupported )
	{
		CFIX_INCONCLUSIVE( L"Kernel not WMK-compatible." );
	}

	TEST_SUCCESS( JpkfbtAttach( JpkfbtKernelWmk, &Session ) );
	TEST( Session );
	TEST_SUCCESS( JpkfbtDetach( Session, FALSE ) );

	Session = NULL;

	TEST( IsDriverLoaded( L"jpkfaw" ) );

	//
	// Again, but this time unload.
	//
	TEST_SUCCESS( JpkfbtAttach( JpkfbtKernelWmk, &Session ) );
	TEST( Session );
	TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );

	TEST( ! IsDriverLoaded( L"jpkfaw" ) );
}

void TestInitShutdownTracing()
{
	BOOL KernelSupported;
	JPKFBT_SESSION Session;
	JPKFBT_KERNEL_TYPE Type;
	ULONG KernelsTested = 0;
	ULONG TrcTypesTested;

	for ( Type = JpkfbtKernelRetail;
		  Type <= JpkfbtKernelMax;
		  Type++ )
	{
		JPKFBT_TRACING_TYPE TracingType;

		TEST_SUCCESS( JpkfbtIsKernelTypeSupported( 
			Type, &KernelSupported ) );
		if ( ! KernelSupported )
		{
			continue;
		}

		TEST_SUCCESS( JpkfbtAttach( Type, &Session ) );
		TEST( Session );
		
		//
		// Invalid type.
		//
		TEST_STATUS( STATUS_INVALID_PARAMETER,
			JpkfbtInitializeTracing(
				Session,
				JpkfbtTracingTypeMax + 1,
				0,
				0,
				NULL ) );

		//
		// Shutdown without being started.
		//
		TEST_STATUS( STATUS_FBT_NOT_INITIALIZED, 
			JpkfbtShutdownTracing( Session ) );

		//
		// Init & Shutdown using valid type.
		//
		TrcTypesTested = 0;
		for( TracingType = JpkfbtTracingTypeDefault;
			 TracingType <= JpkfbtTracingTypeMax;
			 TracingType++ )
		{
			NTSTATUS Status;

			ULONG BufCount;
			ULONG BufSize;
			PWSTR LogFile;
			
			if ( TracingType == JpkfbtTracingTypeDefault )
			{
				BufCount = 64;
				BufSize = 64;
				LogFile = L"__testkfbt.log";

				DeleteFile( LogFile );
			}
			else
			{
				BufCount = 0;
				BufSize = 0;
				LogFile = NULL;
			}
	
			TEST_STATUS( STATUS_INVALID_PARAMETER, JpkfbtInitializeTracing(
				Session,
				TracingType,
				BufCount,
				BufSize,
				TracingType == JpkfbtTracingTypeDefault
					? NULL
					: L"foo" ) );
			
			Status = JpkfbtInitializeTracing(
				Session,
				TracingType,
				BufCount,
				BufSize,
				LogFile );
			CFIX_LOG( L"Status=%x", Status );
			TEST( NT_SUCCESS( Status ) ||
				  STATUS_KFBT_TRCTYPE_NOT_SUPPORTED == Status );

			if ( NT_SUCCESS( Status ) )
			{
				TrcTypesTested++;
				TEST_SUCCESS( JpkfbtShutdownTracing( Session ) );
			}
		}
		TEST( TrcTypesTested > 0 );

		TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );

		KernelsTested++;
	}

	TEST( KernelsTested  > 0 );
}

//
// Test procedurecount == 0, invalid actions etc --> KM validation!
//
void TestInstrumentFailures()
{
	JPFBT_PROCEDURE FailedProcedure;
	BOOL KernelSupported;
	JPFBT_PROCEDURE PatchProcedureKm;
	JPFBT_PROCEDURE PatchProcedureUm;
	JPKFBT_SESSION Session;
	JPKFBT_KERNEL_TYPE Type;
	ULONG KernelsTested = 0;
	ULONG TrcTypesTested = 0;

	for ( Type = JpkfbtKernelRetail;
		  Type <= JpkfbtKernelMax;
		  Type++ )
	{
		JPKFBT_TRACING_TYPE TracingType;

		TEST_SUCCESS( JpkfbtIsKernelTypeSupported( 
			Type, &KernelSupported ) );
		if ( ! KernelSupported )
		{
			continue;
		}

		TEST_SUCCESS( JpkfbtAttach( Type, &Session ) );
		TEST( Session );

		////
		//// Tracing not initialized yet.
		////
		//TEST_STATUS( STATUS_FBT_NOT_INITIALIZED, 
		//	JpkfbtInstrumentProcedure(
		//		Session,
		//		JpfbtAddInstrumentation,
		//		?,
		//		?,
		//		&FailedProcedure ) );

		//
		// Init tracing.
		//
		PatchProcedureUm.u.ProcedureVa = 0x7F000F00;
		PatchProcedureKm.u.ProcedureVa = 0x8F000F00;
		
		TrcTypesTested = 0;
		for( TracingType = JpkfbtTracingTypeDefault;
			 TracingType <= JpkfbtTracingTypeMax;
			 TracingType++ )
		{
			NTSTATUS Status;

			ULONG BufCount;
			ULONG BufSize;
			PWSTR LogFile;
			
			if ( TracingType == JpkfbtTracingTypeDefault )
			{
				BufCount = 64;
				BufSize = 64;
				LogFile = L"__testkfbt.log";

				DeleteFile( LogFile );
			}
			else
			{
				BufCount = 0;
				BufSize = 0;
				LogFile = NULL;
			}

			Status = JpkfbtInitializeTracing(
				Session,
				TracingType,
				BufCount,
				BufSize, 
				LogFile );
			
			if ( STATUS_KFBT_TRCTYPE_NOT_SUPPORTED == Status )
			{
				continue;
			}

			TEST_SUCCESS( Status );

			//
			// Mem.
			//
			TEST_STATUS( STATUS_NO_MEMORY, 
				JpkfbtInstrumentProcedure(
					Session,
					JpfbtAddInstrumentation,
					0xF0000000,
					&PatchProcedureUm,
					&FailedProcedure ) );
			TEST( FailedProcedure.u.Procedure == NULL );

			//
			// Procs.
			//
			TEST_STATUS( STATUS_INVALID_PARAMETER,
				JpkfbtInstrumentProcedure(
					Session,
					JpfbtAddInstrumentation,
					0,
					NULL,
					&FailedProcedure ) );
			TEST( FailedProcedure.u.Procedure == NULL );

			TEST_STATUS( STATUS_KFBT_PROC_OUTSIDE_SYSTEM_RANGE, 
				JpkfbtInstrumentProcedure(
					Session,
					JpfbtAddInstrumentation,
					1,
					&PatchProcedureUm,
					&FailedProcedure ) );
			TEST( FailedProcedure.u.Procedure == PatchProcedureUm.u.Procedure );

			TEST_STATUS( STATUS_KFBT_PROC_OUTSIDE_MODULE, 
				JpkfbtInstrumentProcedure(
					Session,
					JpfbtAddInstrumentation,
					1,
					&PatchProcedureKm,
					&FailedProcedure ) );
			TEST( FailedProcedure.u.Procedure == PatchProcedureKm.u.Procedure );

			TEST_SUCCESS( JpkfbtShutdownTracing( Session ) );
			TrcTypesTested++;
		}

		TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );
		TEST( TrcTypesTested > 0 );

		KernelsTested++;
	}

	TEST( KernelsTested > 0 );
}

CFIX_BEGIN_FIXTURE( AttachDetach )
	CFIX_FIXTURE_ENTRY( TestAttachDetachRetail )
	CFIX_FIXTURE_ENTRY( TestAttachDetachWmk )
	CFIX_FIXTURE_ENTRY( TestInitShutdownTracing )
	CFIX_FIXTURE_ENTRY( TestInstrumentFailures )
	CFIX_FIXTURE_ENTRY( TestQueryStats )
CFIX_END_FIXTURE()