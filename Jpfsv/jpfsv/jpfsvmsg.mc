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
MessageId		= 0x9000
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_COMMAND_FAILED
Language		= English
The command failed.
.

MessageId		= 0x9001
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_UNSUP_ON_WOW64
Language		= English
This operation is not supported on WOW64.
.

MessageId		= 0x9002
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_TRACEPOINT_EXISTS
Language		= English
This tracepoint already exists.
.

MessageId		= 0x9003
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_TRACEPOINT_NOT_FOUND
Language		= English
Tracepoint does not exist.
.

MessageId		= 0x9004
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_PEER_DIED
Language		= English
The traced process has been terminated.
.

MessageId		= 0x9005
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_TRACES_ACTIVE
Language		= English
There are still traces active.
.

MessageId		= 0x9006
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_NO_TRACESESSION
Language		= English
Tracing has not been started yet.
.

MessageId		= 0x9007
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_ARCH_MISMATCH
Language		= English
Mismatch between CPU architectures (32 vs 64 bit).
.

MessageId		= 0x9008
Severity		= Warning
Facility		= Interface
SymbolicName	= JPFSV_E_UNSUPPORTED_TRACING_TYPE
Language		= English
Unsupported tracing type.
.

