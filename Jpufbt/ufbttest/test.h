#include <jpqlpc.h>
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <cfix.h>

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#define STATUS_OBJECT_NAME_COLLISION	 ((NTSTATUS)0xC0000035L)

#define TEST_SUCCESS( expr ) TEST( 0 == ( expr ) )
