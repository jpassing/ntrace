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
	JPKFBT_SESSION Session;

	TEST( ! IsDriverLoaded( L"jpkfag" ) );
	TEST_STATUS( STATUS_KFBT_KERNEL_NOT_SUPPORTED,
		JpkfbtAttach( JpkfbtKernelRetail, &Session ) );
}

void TestAttachDetachWmk()
{
	BOOL KernelSupported;
	JPKFBT_SESSION Session;

	TEST( ! IsDriverLoaded( L"jpkfag" ) );
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

	TEST( IsDriverLoaded( L"jpkfag" ) );

	//
	// Again, but this time unload.
	//
	TEST_SUCCESS( JpkfbtAttach( JpkfbtKernelWmk, &Session ) );
	TEST( Session );
	TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );

	TEST( ! IsDriverLoaded( L"jpkfag" ) );
}

void TestInitShutdownTracing()
{
	BOOL KernelSupported;
	JPKFBT_SESSION Session;
	JPKFBT_KERNEL_TYPE Type;
	ULONG TypesTested = 0;

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
				JpkfbtTracingTypeWmk + 1,
				0,
				0 ) );

		//
		// Shutdown without being started.
		//
		TEST_STATUS( STATUS_FBT_NOT_INITIALIZED, 
			JpkfbtShutdownTracing( Session ) );

		//
		// Init & Shutdown using valid type.
		//
		for( TracingType = JpkfbtTracingTypeWmk;
			 TracingType <= JpkfbtTracingTypeMax;
			 TracingType++ )
		{
			TEST_SUCCESS( JpkfbtInitializeTracing(
				Session,
				TracingType,
				0,
				0 ) );
			TEST_SUCCESS( JpkfbtShutdownTracing( Session ) );
		}

		TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );

		TypesTested++;
	}

	TEST( TypesTested > 0 );
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
	ULONG TypesTested = 0;

	for ( Type = JpkfbtKernelRetail;
		  Type <= JpkfbtKernelMax;
		  Type++ )
	{
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
		
		TEST_SUCCESS( JpkfbtInitializeTracing(
			Session,
			JpkfbtTracingTypeWmk,
			0,
			0 ) );
		
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
		TEST_SUCCESS( JpkfbtDetach( Session, TRUE ) );

		TypesTested++;
	}

	TEST( TypesTested > 0 );
}

CFIX_BEGIN_FIXTURE( AttachDetach )
	CFIX_FIXTURE_ENTRY( TestAttachDetachRetail )
	CFIX_FIXTURE_ENTRY( TestAttachDetachWmk )
	CFIX_FIXTURE_ENTRY( TestInitShutdownTracing )
	CFIX_FIXTURE_ENTRY( TestInstrumentFailures )
CFIX_END_FIXTURE()