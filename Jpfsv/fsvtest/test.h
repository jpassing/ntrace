#include <stdlib.h>
#include <stdio.h>
#include <windows.h>
#include <cfix.h>

#define TEST_OK( expr )  TEST( S_OK == ( expr ) )

void LaunchNotepad(
	__out PPROCESS_INFORMATION ppi
	);