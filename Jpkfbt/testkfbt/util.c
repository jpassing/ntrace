/*----------------------------------------------------------------------
 * Purpose:
 *		Utility routines.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */
#include <cfix.h>
#include "util.h"

BOOL IsDriverLoaded(
	__in PCWSTR Name
	)
{
	BOOL Loaded;
	SC_HANDLE ScMgr, Service;
	SERVICE_STATUS Status;

	ScMgr = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS );
	TEST( ScMgr );

	Service = OpenService( ScMgr, Name, SERVICE_ALL_ACCESS );
	TEST( Service != NULL || GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST );
	if ( Service != NULL )
	{
		TEST( QueryServiceStatus( Service, &Status ) );

		Loaded = ( Status.dwCurrentState == SERVICE_RUNNING );
		TEST( CloseServiceHandle( Service ) );
	}
	else
	{
		Loaded = FALSE;
	}
	TEST( CloseServiceHandle( ScMgr ) );

	return Loaded;
}