;//--------------------------------------------------------------------
;// Definitions
;//--------------------------------------------------------------------

MessageIdTypedef=HRESULT

SeverityNames=(
  Success=0x0
  Informational=0x1
  Warning=0x2
  Error=0x3
)

FacilityNames=(
  Interface=4
)

LanguageNames=(English=0x409:MSG00409)


;//--------------------------------------------------------------------
;// Errors
;//--------------------------------------------------------------------
MessageId		= 0x9400
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_INVALID_SIGNATURE
Language		= English
Invalid file signature.
.

MessageId		= 0x9401
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_INVALID_VERSION
Language		= English
Invalid file version.
.

MessageId		= 0x9402
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_INVALID_CHARACTERISTICS
Language		= English
Invalid file characteristics.
.

MessageId		= 0x9403
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_BITNESS_NOT_SUPPRTED
Language		= English
Bitness not supported
.

MessageId		= 0x9404
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_FILE_EMPTY
Language		= English
File is empty.
.

MessageId		= 0x9405
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_EOF
Language		= English
End of file reached.
.

MessageId		= 0x9406
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_UNRECOGNIZED_CHUNK_TYPE
Language		= English
The file contains an unrecognized chunk type.
.

MessageId		= 0x9407
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_RESERVED_FIELDS_USED
Language		= English
The file uses reserved fields.
.

MessageId		= 0x9408
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_TRUNCATED_CHUNK
Language		= English
File contains a truncated chunk.
.

MessageId		= 0x9409
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_CHUNK_STRADDLES_SEGMENT
Language		= English
The file contains a chunk that is so large that it straddles a 
segment boundary.
.

MessageId		= 0x940a
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_INVALID_CALL_HANDLE
Language		= English
The call handle is invalid.
.

MessageId		= 0x940b
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_INVALID_TRANSITION
Language		= English
Encountered invalid transition type in file.
.

MessageId		= 0x940c
Severity		= Error
Facility		= Interface
SymbolicName	= JPTRCR_E_CORRUPT
Language		= English
File corrupt.
.
