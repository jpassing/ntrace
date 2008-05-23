#include "test.h"

#ifdef JPFBT_TARGET_USERMODE
static BOOLEAN IsVistaOrNewer()
{
	OSVERSIONINFO OsVersion;
	OsVersion.dwOSVersionInfoSize = sizeof( OSVERSIONINFO );
	TEST( GetVersionEx( &OsVersion ) );
	return OsVersion.dwMajorVersion >= 6 ? TRUE : FALSE;
}
#else
static BOOLEAN IsVistaOrNewer()
{
	RTL_OSVERSIONINFOW OsVersion;
	OsVersion.dwOSVersionInfoSize = sizeof( RTL_OSVERSIONINFOW );
	TEST_SUCCESS( RtlGetVersion( &OsVersion ) );
	return OsVersion.dwMajorVersion >= 6 ? TRUE : FALSE;
}
#endif

void GetSymbolPointers( 
	__out PJPFBT_SYMBOL_POINTERS Pointers 
	)
{
	//
	// CAUTION: VAs are from Svr03 SP2 and Vista - these may change at any time!
	//
#ifdef JPFBT_TARGET_USERMODE
	Pointers->Unused = 0;
#elif JPFBT_WRK
	Pointers->ExceptionHandling.RtlDispatchException		= ( PVOID ) ( ULONG_PTR ) 0x808646da;
	Pointers->ExceptionHandling.RtlUnwind					= ( PVOID ) ( ULONG_PTR ) 0x80864858;
	Pointers->ExceptionHandling.RtlpGetStackLimits			= ( PVOID ) ( ULONG_PTR ) 0x8088541c;
#else
	if ( IsVistaOrNewer() )
	{
		Pointers->Ethread.SameThreadPassiveFlagsOffset		= 0x264;
		Pointers->Ethread.SameThreadApcFlagsOffset			= 0x268;

		Pointers->ExceptionHandling.RtlDispatchException	= ( PVOID ) ( ULONG_PTR ) 0x8188c7d7;
		Pointers->ExceptionHandling.RtlUnwind				= ( PVOID ) ( ULONG_PTR ) 0x8188ca0b;
		Pointers->ExceptionHandling.RtlpGetStackLimits		= ( PVOID ) ( ULONG_PTR ) 0x81881cdd;
	}
	else
	{
		Pointers->Ethread.SameThreadPassiveFlagsOffset		= 0x244;
		Pointers->Ethread.SameThreadApcFlagsOffset			= 0x248;

		Pointers->ExceptionHandling.RtlDispatchException	= ( PVOID ) ( ULONG_PTR ) 0x80838f96;
		Pointers->ExceptionHandling.RtlUnwind				= ( PVOID ) ( ULONG_PTR ) 0x80838e89;
		Pointers->ExceptionHandling.RtlpGetStackLimits		= ( PVOID ) ( ULONG_PTR ) 0x8081f912;
	}
#endif
}