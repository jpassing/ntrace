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
