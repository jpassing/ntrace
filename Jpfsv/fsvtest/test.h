#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <crtdbg.h>

#ifdef DBG
#define TEST( expr ) ( ( !! ( expr ) ) || ( \
	OutputDebugString( \
		L"Test failed: " _CRT_WIDE( __FILE__ ) L" - " \
		_CRT_WIDE( __FUNCTION__ ) L": " _CRT_WIDE( #expr ) L"\n" ), DebugBreak(), 0 ) )
#else
#define TEST( expr ) ( ( !! ( expr ) ) || ( \
	OutputDebugString( \
		L"Test failed: " _CRT_WIDE( __FILE__ ) L" - " \
		_CRT_WIDE( __FUNCTION__ ) L": " _CRT_WIDE( #expr ) L"\n" ), DebugBreak(), 0 ) )
#endif

#define TEST_OK( expr ) TEST( S_OK == ( expr ) )

void TestPsInfoEnums();