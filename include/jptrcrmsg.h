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

