#include <jpqlpc.h>
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

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#define STATUS_OBJECT_NAME_COLLISION	 ((NTSTATUS)0xC0000035L)

#define TEST_SUCCESS( expr ) TEST( 0 == ( expr ) )


void QlpcTestPort();
void QlpcTestTransfer();
void UfagTestServer();
void UfbtTest();