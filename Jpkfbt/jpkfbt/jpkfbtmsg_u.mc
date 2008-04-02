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
MessageId		= 0x9300
Severity		= Warning
Facility		= Interface
SymbolicName	= STATUS_KFBT_KERNEL_NOT_SUPPORTED
Language		= English
The specified kernel is not supported by this library.
.

MessageId		= 0x9301
Severity		= Warning
Facility		= Interface
SymbolicName	= STATUS_KFBT_AGENT_NOT_FOUND
Language		= English
The FBT agent driver could not be found.
.

MessageId		= 0x9302
Severity		= Warning
Facility		= Interface
SymbolicName	= STATUS_KFBT_OPEN_SCM_FAILED
Language		= English
Contacting the SCM failed.
.

MessageId		= 0x9303
Severity		= Warning
Facility		= Interface
SymbolicName	= STATUS_KFBT_OPEN_DRIVER_FAILED
Language		= English
Opening handle to agent driver failed.
.

MessageId		= 0x9304
Severity		= Warning
Facility		= Interface
SymbolicName	= STATUS_KFBT_CREATE_DRIVER_FAILED
Language		= English
Creating handle to agent driver failed.
.

MessageId		= 0x9305
Severity		= Warning
Facility		= Interface
SymbolicName	= STATUS_KFBT_START_DRIVER_FAILED
Language		= English
Starting the agent driver failed.
.

MessageId		= 0x9306
Severity		= Warning
Facility		= Interface
SymbolicName	= STATUS_KFBT_STOP_DRIVER_FAILED
Language		= English
Stopping the agent driver failed.
.