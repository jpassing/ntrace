//--------------------------------------------------------------------
// Definitions
//--------------------------------------------------------------------
//--------------------------------------------------------------------
// Errors
//--------------------------------------------------------------------
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: JPTRCR_E_INVALID_SIGNATURE
//
// MessageText:
//
// Invalid file signature.
//
#define JPTRCR_E_INVALID_SIGNATURE       ((HRESULT)0xC0049400L)

//
// MessageId: JPTRCR_E_INVALID_VERSION
//
// MessageText:
//
// Invalid file version.
//
#define JPTRCR_E_INVALID_VERSION         ((HRESULT)0xC0049401L)

//
// MessageId: JPTRCR_E_INVALID_CHARACTERISTICS
//
// MessageText:
//
// Invalid file characteristics.
//
#define JPTRCR_E_INVALID_CHARACTERISTICS ((HRESULT)0xC0049402L)

//
// MessageId: JPTRCR_E_BITNESS_NOT_SUPPRTED
//
// MessageText:
//
// Bitness not supported
//
#define JPTRCR_E_BITNESS_NOT_SUPPRTED    ((HRESULT)0xC0049403L)

//
// MessageId: JPTRCR_E_FILE_EMPTY
//
// MessageText:
//
// File is empty.
//
#define JPTRCR_E_FILE_EMPTY              ((HRESULT)0xC0049404L)

//
// MessageId: JPTRCR_E_EOF
//
// MessageText:
//
// End of file reached.
//
#define JPTRCR_E_EOF                     ((HRESULT)0xC0049405L)

//
// MessageId: JPTRCR_E_UNRECOGNIZED_CHUNK_TYPE
//
// MessageText:
//
// The file contains an unrecognized chunk type.
//
#define JPTRCR_E_UNRECOGNIZED_CHUNK_TYPE ((HRESULT)0xC0049406L)

//
// MessageId: JPTRCR_E_RESERVED_FIELDS_USED
//
// MessageText:
//
// The file uses reserved fields.
//
#define JPTRCR_E_RESERVED_FIELDS_USED    ((HRESULT)0xC0049407L)

//
// MessageId: JPTRCR_E_TRUNCATED_CHUNK
//
// MessageText:
//
// File contains a truncated chunk.
//
#define JPTRCR_E_TRUNCATED_CHUNK         ((HRESULT)0xC0049408L)

//
// MessageId: JPTRCR_E_CHUNK_STRADDLES_SEGMENT
//
// MessageText:
//
// The file contains a chunk that is so large that it straddles a 
// segment boundary.
//
#define JPTRCR_E_CHUNK_STRADDLES_SEGMENT ((HRESULT)0xC0049409L)

//
// MessageId: JPTRCR_E_INVALID_CALL_HANDLE
//
// MessageText:
//
// The call handle is invalid.
//
#define JPTRCR_E_INVALID_CALL_HANDLE     ((HRESULT)0xC004940AL)

//
// MessageId: JPTRCR_E_INVALID_TRANSITION
//
// MessageText:
//
// Encountered invalid transition type in file.
//
#define JPTRCR_E_INVALID_TRANSITION      ((HRESULT)0xC004940BL)

//
// MessageId: JPTRCR_E_MODULE_UNKNOWN
//
// MessageText:
//
// Unknown module.
//
#define JPTRCR_E_MODULE_UNKNOWN          ((HRESULT)0xC004940CL)

