;//--------------------------------------------------------------------
;// Definitions
;//--------------------------------------------------------------------

MessageIdTypedef=NTSTATUS

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
MessageId		= 0x9200
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_NO_THUNKSTACK
Language		= English
No thunkstack available.
.

MessageId		= 0x9201
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_PROC_NOT_PATCHABLE
Language		= English
Procedure is not patchable.
.

MessageId		= 0x9202
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_PROC_ALREADY_PATCHED
Language		= English
Procedure already patched.
.

MessageId		= 0x9203
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_PROC_TOO_FAR
Language		= English
The procedure address is too far away from the trampoline.
.

MessageId		= 0x9204
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_INIT_FAILURE
Language		= English
FBT initialization failed.
.

MessageId		= 0x9205
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_THR_SUSPEND_FAILURE
Language		= English
An error occured while trying to suspend all threads.
.

MessageId		= 0x9206
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_THR_CTXUPD_FAILURE
Language		= English
A required instruction pointer update could not be performed.
.

MessageId		= 0x9207
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_STILL_PATCHED
Language		= English
The library cannot be uninitialized as there are still patched procedures.
.

MessageId		= 0x9208
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_NOT_INITIALIZED
Language		= English
Library not initialized.
.

MessageId		= 0x9209
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_ALREADY_INITIALIZED
Language		= English
Library has already been initialized.
.

MessageId		= 0x920a
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_UNUSABLE_TLS_SLOT
Language		= English
The TLS slot allocated cannot be used as it is an extension slot.
.

MessageId		= 0x920b
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_AUPTR_IN_USE
Language		= English
AuxiliaryUserPointer is in use.
.

MessageId		= 0x920c
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_NOT_PATCHED
Language		= English
Procedure not patched.
.

MessageId		= 0x920d
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_PATCHES_ACTIVE
Language		= English
At least one patch is currently in use and cannot be removed.
.

MessageId		= 0x920e
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_INVALID_BUFFER_SIZE
Language		= English
Invalid buffer size - must be a multple of 8.
.

MessageId		= 0x920f
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_CANNOT_LOCATE_EH_ROUTINES
Language		= English
The RTL exception handling routines cannot be located.
.

MessageId		= 0x9210
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_REENTRANT_ALLOCATION
Language		= English
Allocation reentrance detected.
.

MessageId		= 0x9211
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_UNSUPPORTED_KERNEL_BUILD
Language		= English
Unsupported kernel build.
.

MessageId		= 0x9212
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_CV_GUID_LOOKUP_FAILED
Language		= English
Looking up the GUID in the kernel module's CodeView information failed 
as the corresponding record could not be found.
.

MessageId		= 0x9213
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_UNRECOGNIZED_CV_HEADER
Language		= English
The kernel image contains an unrecognized CodeView record
.

MessageId		= 0x9214
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_REENTRANT_EXIT
Language		= English
Reentrant exit detected.
.

MessageId		= 0x9215
Severity		= Error
Facility		= Interface
SymbolicName	= STATUS_FBT_THUNKSTACK_UNDERFLOW
Language		= English
Thunkstack underflow.
.
