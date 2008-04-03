#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Utility routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define TEST CFIX_ASSERT
#define TEST_SUCCESS( expr ) CFIX_ASSERT_EQUALS_DWORD( 0, ( expr ) )
#define TEST_STATUS( status, expr ) CFIX_ASSERT_EQUALS_DWORD( ( ULONG ) ( status ), ( expr ) )

BOOL IsDriverLoaded(
	__in PCWSTR Name
	);