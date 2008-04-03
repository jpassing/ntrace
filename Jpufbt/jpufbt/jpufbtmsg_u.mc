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
MessageId		= 0x9100
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_STILL_ACTIVE
Language		= English
Tracing has not been stopped yet.
.

MessageId		= 0x9101
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_AGENT_NOT_FOUND
Language		= English
The agent DLL could not be found.
.

MessageId		= 0x9102
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_INJECTION_FAILED
Language		= English
Injecting the agent DLL into the target process failed.
.

MessageId		= 0x9103
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_PEER_FAILED
Language		= English
The target process behaved unexpectedly and did not properly open the QLPC port.
.

MessageId		= 0x9104
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_INVALID_PEER_MSG
Language		= English
The target process sent an invalid message.
.

MessageId		= 0x9105
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_UNEXPECTED_PEER_MSG
Language		= English
The target process sent an unexpected message.
.

MessageId		= 0x9106
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_INVALID_PEER_MSG_FMT
Language		= English
The target process sent a message with an invalid format.
.

MessageId		= 0x9107
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_TRACING_NOT_INITIALIZED
Language		= English
Tracing has not been initialized yet.
.

MessageId		= 0x9108
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_INVALID_HANDLE
Language		= English
Invalid process handle.
.

MessageId		= 0x9109
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_TIMED_OUT
Language		= English
The timeout elapsed.
.

MessageId		= 0x910a
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_UFBT_PEER_DIED
Language		= English
The target process terminated unexpectedly.
.

