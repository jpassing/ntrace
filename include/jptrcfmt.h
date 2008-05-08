#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		Trace file format.
 *
 *		A trace file consists of a file header, defined by 
 *		JPTRC_FILE_HEADER, follows by an unlimited sequence of chunks, 
 *		each starting with a header defined by the JPTRC_CHUNK_HEADER.
 *
 *		Further constraints:
 *		 1)	A chunk MUST NOT straddle a segment boundary. To avoid such 
 *			situations, an application may chose to pad the file
 *			with a JPTRC_PAD_CHUNK to the next 256 KB boundary, followed
 *			by the actual chunk.
 *			
 *			This constraint is motivated by the aim to make reading
 *			the file using a memory mapped file with a sliding window
 *			easier.
 *
 *		 2) All chunks MUST be 16 byte aligned within the file.
 *			
 *			This constraint is motivated by the aim to allow using
 *			the structures as-is when the file is used as a memory 
 *			mapped file without suffering from unaligned reads.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#pragma pack( push, 4 )
#pragma warning( push )
#pragma warning( disable: 4214 ) // ULONGLONG Bitfields

#define JPTRC_SEGMENT_SIZE							( 256 * 1024 )
#define JPTRC_CHUNK_ALIGNMENT						8

#define JPTRC_HEADER_SIGNATURE						'CRTJ'
#define JPTRC_HEADER_VERSION						0x1000

//
// Defines the format of Timestamp fields.
//
#define JPTRC_CHARACTERISTIC_TIMESTAMP_TSC			1
#define JPTRC_CHARACTERISTIC_TIMESTAMP_PERFCOUNTER	2

//
// 32/64 bit - defines whether *32 or *64 structs are to be used.
// 64 bit is currently not implemented.
//
#define JPTRC_CHARACTERISTIC_32BIT					4
#define JPTRC_CHARACTERISTIC_64BIT					8

/*++
	Structure Description:
		Header of the file. Implementations must verify the values
		of this header.
--*/
typedef struct _JPTRC_FILE_HEADER
{
	ULONG Signature;
	USHORT Version;
	USHORT Characteristics;
	ULONG __Reserved[ 2 ];
} JPTRC_FILE_HEADER, *PJPTRC_FILE_HEADER;

C_ASSERT( sizeof( JPTRC_FILE_HEADER ) == 16 );

/*++
	Structure Description:
		Header of a chunk. 
--*/
typedef struct _JPTRC_CHUNK_HEADER
{
	USHORT Type;

	//
	// Unused, must be 0.
	//
	USHORT Reserved;

	//
	// Overall size in bytes, including header.
	//
	ULONG Size;
} JPTRC_CHUNK_HEADER, *PJPTRC_CHUNK_HEADER;

#define JPTRC_CHUNK_TYPE_PAD			0
#define JPTRC_CHUNK_TYPE_IMAGE_INFO		1
#define JPTRC_CHUNK_TYPE_TRACE_BUFFER	2

#define JPTRC_PROCEDURE_TRANSITION_ENTRY				0
#define JPTRC_PROCEDURE_TRANSITION_EXIT					1
#define JPTRC_PROCEDURE_TRANSITION_EXCEPTION			2

/*++
	Structure Description:
		Describes a procedure transition, i.e. an entry or exit
		event. To save space, this structure is optimized for 
		32-bit only.
--*/
typedef struct _JPTRC_PROCEDURE_TRANSITION32
{
	//
	// ENTRY/EXIT discriminator.
	//
	ULONGLONG Type : 2;
	ULONGLONG __Unused : 2;

	//
	// Timestamp, either TSC or Performance Counter value.
	//
	ULONGLONG Timestamp : 60;

	//
	// Procedure VA (not the RVA).
	//
	ULONG Procedure;

	union
	{
		//
		// For ENTRY transitions.
		//
		ULONG CallerIp;
		
		//
		// For EXIT transitions.
		//
		ULONG ReturnValue;

		//
		// For EXCEPTION transitions.
		//
		struct
		{
			ULONG Code;
		} Exception;
	} Info;
} JPTRC_PROCEDURE_TRANSITION32, *PJPTRC_PROCEDURE_TRANSITION32;

C_ASSERT( sizeof( JPTRC_PROCEDURE_TRANSITION32 ) == 4 * sizeof( ULONG ) );

typedef struct _JPTRC_TRACE_BUFFER_CHUNK32
{
	JPTRC_CHUNK_HEADER Header;

	struct
	{
		ULONG ProcessId;
		ULONG ThreadId;
	} Client;
	
	JPTRC_PROCEDURE_TRANSITION32 Transitions[ ANYSIZE_ARRAY ];
} JPTRC_TRACE_BUFFER_CHUNK32, *PJPTRC_TRACE_BUFFER_CHUNK32;

/*++
	Structure Description:
		Information about a module that has been involved in
		tracing activity. The structure is of variable length and
		structured as follows:

		+-----------------------------------------+
		| JPTRC_IMAGE_INFO_CHUNK members          |
		+-----------------------------------------+
		| Path                                    |
		+-[qword aligned]-------------------------+
		| Sequence of IMAGE_DEBUG_DIRECTORY       |
		| (at offset DebugDirectoryOffset)        |
		+-----------------------------------------+
		| Debug data referred to by debug dirs    |
		| (offset: see below)                     |
		+-[JPTRC_CHUNK_ALIGNMENT aligned]---------+

		IMAGE_DEBUG_DIRECTORY::AddressOfRawData is 0.
		IMAGE_DEBUG_DIRECTORY::PointerToRawData contains
			offset of debug data, relative to base pointer of
			own structure.
--*/
typedef struct _JPTRC_IMAGE_INFO_CHUNK
{
	JPTRC_CHUNK_HEADER Header;
	
	//
	// Image information.
	//
	ULONGLONG LoadAddress;
	ULONG Size;

	//
	// Debug information.
	//
	USHORT DebugDirectoryOffset;
	USHORT DebugDirectorySize;

	//
	// Total size of IMAGE_DEBUG_DIRECTORY and debug data.
	//
	USHORT DebugSize;

	//
	// Module path - in NT format.
	//
	USHORT PathSize;
	CHAR Path[ ANYSIZE_ARRAY ];
} JPTRC_IMAGE_INFO_CHUNK, *PJPTRC_IMAGE_INFO_CHUNK;

typedef struct _JPTRC_PAD_CHUNK
{
	JPTRC_CHUNK_HEADER Header;
} JPTRC_PAD_CHUNK, *PJPTRC_PAD_CHUNK;

#pragma pack( pop )
#pragma warning( pop )