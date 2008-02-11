#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Common definitions shared among all subprojects.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

#define STATUS_SUCCESS                   ((NTSTATUS)0x00000000L)
#define STATUS_UNSUCCESSFUL              ((NTSTATUS)0xC0000001L)
#define STATUS_NOT_IMPLEMENTED           ((NTSTATUS)0xC0000002L)
#define STATUS_INVALID_PARAMETER         ((NTSTATUS)0xC000000DL)
#define STATUS_OBJECT_NAME_COLLISION	 ((NTSTATUS)0xC0000035L)

#define ASSERT _ASSERTE
#ifndef VERIFY
#if defined(DBG) || defined( DBG )
#define VERIFY ASSERT
#else
#define VERIFY( x ) ( x )
#endif
#endif
