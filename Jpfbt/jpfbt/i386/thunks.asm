;----------------------------------------------------------------------
; Purpose:
;		Function entry/exit thunks.
;
; Copyright:
;		Johannes Passing (johannes.passing@googlemail.com)
;

 .586                                   ; create 32 bit code
.model flat, stdcall                    ; 32 bit memory model
option casemap :none                    ; case sensitive

extrn JpfbtpGetCurrentThunkStack@0 : proc
extrn JpfbtpProcedureEntry@8 : proc
extrn JpfbtpProcedureExit@8 : proc

extrn RaiseException@16 : proc

extrn JpfbtpThreadDataTlsOffset : DWORD

.data
.code

ASSUME FS:NOTHING

;++
;	Routine Description:
;		Called instead of hooked function. 
;
;		This function has an unknown calling convention (may
;		be either cdecl, stdcall or fastcall) and never returns by
;		itself. All registers are left unchanged.
;
;	Stack Parameters:
;		Funcptr
;
;	Return Value:
;		Never returns, jumps to hooked function
;--
JpfbtpFunctionEntryThunk proc
	push ebp
	mov ebp, esp
	
	push eax				; scratch register
	push ecx				; scratch register
	push edx				; scratch register
	
	;
	; Swap return addresses s.t. the hooked procedure returns to 
	; the call thunk rather than to the caller.
	;
	; N.B. Modifying the return address interferes with the CPU's return 
	; address predictor - the CPU will not like it and there will be
	; a performance penalty, but this is the price to pay...
	;
	; Stack Before:         Stack After:
	;  saved edx	<-SP	 saved edx	<-SP
	;  saved ecx			 saved ecx
	;  saved eax			 saved eax
	;  saved ebp	<-BP	 saved ebp	<-BP
	;  Thunk RA				 Real RA
	;  Funcptr				 Funcptr
	;  Real RA				 Thunk RA
	;  Parameters			 Parameters	
	;
	mov eax, [ebp+4]		; Thunk RA
	mov edx, [ebp+12]		; Real RA
	mov [ebp+12], eax
	mov [ebp+4], edx
	
	;
	; Register state:
	;  eax: free
	;  ecx: free
	;  edx: free
	;
	
	;
	; Get thunkstack.
	;
	; N.B. All volatiles are free.
	;
	call JpfbtpGetCurrentThunkStack@0
	
	test eax, eax			; Check that we have got a stack
	jz NoStack
	
	mov ecx, [eax]			; ecx = thunkstack->StackPointer

	;
	; Register state:
	;  eax: thunkstack
	;  ecx: thunkstack->StackPointer
	;  edx: free
	;	
	
	;
	; Check if stack has enough free space.
	; The stack overflows if thunkstack->StackPointer would be
	; overwritten.
	;
	lea edx, [ecx-0ch]
	cmp edx, eax
	jle StackOverflow
	
	;
	; Save ESP value as it was at procedure entry.
	;
	lea edx, [ebp+0ch]
	mov [ecx-0ch], edx
	
	;
	; stash away funcptr on thunkstack.
	;
	mov edx, [ebp+8]
	mov [ecx-8], edx
	
	;
	; stash away Real RA on thunkstack.
	;
	mov edx, [ebp+4]
	mov [ecx-4], edx
	
	;
	; Adjust stack pointer:
	;   thunkstack->StackPointer--
	;
	lea edx, [ecx-0ch]
	mov [eax], edx			
	
	;
	; Register state:
	;  eax: free
	;  ecx: free
	;  edx: free
	;
	
	;
	; Prepare call to JpfbtpProcedureEntry.
	;	
	; Initialize JPFBT_CONTEXT.
	;
	sub esp, 028h			; sizeof( JPFBT_CONTEXT )
	
	mov [esp+00h], edi
	mov [esp+04h], esi
	mov [esp+08h], ebx
	push [ebp-00Ch]			; edx was preserved
	pop [esp+00Ch]			; 
	push [ebp-08h]			; ecx was preserved
	pop [esp+010h]			;
	push [ebp-04h]			; eax was preserved
	pop [esp+014h]			;
	
	push [ebp]				; ebp was preserved
	pop [esp+018h]
	push [ebp+8]			; use funcptr as EIP
	pop [esp+01Ch]			;
	pushfd
	pop [esp+020h]
	lea eax, [ebp+0Ch]		; Calculate original esp
	mov [esp+024h], eax

	;
	; Push parameters for JpfbtpProcedureEntry.
	;
	push [ebp+8]			; funcptr
	lea eax, [esp+4]		
	push eax				; &Context 

	call JpfbtpProcedureEntry@8

	;
	; Tear down CONTEXT.
	;
	add esp, 028h	; sizeof( JPFBT_CONTEXT ) 
	
JumpToTarget:
	
	mov eax, [ebp+8]		; Funcptr
	add eax, 2				; skip overwritten part of prolog
	mov [ebp+8], eax
		
	pop edx
	pop ecx
	pop eax
	pop ebp	
	
	add esp, 8
	
	;
	; Resume procedure - as we have restored the RA, the function will
	; return to the caller rather than to JpfbtProcedureCallThunk.
	;
	jmp [esp-4]

NoStack:
	;
	; We have not been able to retrieve a thunkstack, so intercepting
	; this procedure is not possible. We thus undo our changes made,
	; i.e. re-adjust esp, restore registers and - most importantly -
	; restore the original return address.
	;
	
	mov eax, [ebp+4]		; Real RA
	mov [ebp+12], eax		; restore real RA
	
	jmp JumpToTarget
	
StackOverflow:
	push 0					; lpArguments
	push 0					; nNumberOfArguments
	push 1					; EXCEPTION_NONCONTINUABLE
	push 0C00000FDh			; EXCEPTION_STACK_OVERFLOW
	call RaiseException@16
JpfbtpFunctionEntryThunk endp

;++
;	Routine Description:
;		Thunk called by patched prolog of hooked function.
;
;		This function is basically naked, i.e. it has no parameters and
;		does not clean up the stack. All registers are left unchanged.
;
;	Parameters
;		None
;
;	Return Value:
;		No own return value, preserved value returned by hooked
;		function.
;--
JpfbtpFunctionCallThunk proc
	call JpfbtpFunctionEntryThunk

	;
	; N.B. We do not know which calling convention the hooked 
	; procedure used, neither do we know whether any out-parameters
	; are used. As a result, we may not touch *any* registers
	; and may even not touch the stack!
	;
	; In order to get the first free register, we leverage an 
	; undocumented windows feature, the TEB ArbitraryUserPointer
	; available at fs:[014h].
	;
	; One register is a good start, but we need some memory 
	; so that we actually do something.
	; Our redemption is thus to get some thread-local meomory by
	; leveraging TLS. Alas, we cannot call JpfbtpGetCurrentThunkStack
	; as we would need registers and stack for the call, so we are in
	; a catch-22.
	;
	; In order to get out of this situation, we have to perform some
	; nasty maneuver - we we can neither call JpfbtpGetCurrentThunkStack 
	; nor TlsGetValue, we will directly access the TEB to achieve
	; the same result as JpfbtpGetCurrentThunkStack, but without
	; the requirement for stack & registers.
	;
	;
	
	;
	; Store eax's value in the TIB's ArbitraryUserPointer.
	;
	mov fs:[014h], eax;
	
	;
	; Reach into TEB to get pointer to JPFBT_THREAD_DATA and fetch
	; Sp value.
	;
	; Sp was the stack pointer when the hooked procedure was called -
	; evth above Sp is now invalid (local variables of hooked 
	; procedure), so we now finally have space to use.
	;
	mov eax, [JpfbtpThreadDataTlsOffset]
	mov eax, fs:[eax]		

	test eax, eax			; Check that we have got a pointer
	jz NoStack
	
	mov eax, [eax+0ch]		; eax = threaddata.ThunkStack->StackPointer
	mov eax, [eax]			; eax = StackPointer->Sp

	sub eax, 4				; skip 1 stack location (RA)
		
	mov [eax], esp			; preserve esp
	mov esp, eax			; skip untouchable stack space
	
	push fs:[014h]			; copy preserved eax onto stack
							; to free ArbitraryUserPointer again
	mov fs:[014h], 0
	
	;
	; Setup frame.
	;	
	push ebp
	mov ebp, esp
	
	push ecx
	push edx

	;
	; Now we have a stack frame.
	;
	; Stack:
	;   preserved edx		<- SP
	;   preserved ecx
	;   preserved ebp		<- BP
	;   preserved eax
	;   preserved esp         ---+
	;   ???                      |
	;   ...                      |
	;   ???                      |
	;   ??? (SP pointed here) <--+
	;   ???
	;   ...
	;   ???
	;

	;
	; Get thunkstack (now the regular way).
	;
	; N.B. All volatiles are free.
	;
	call JpfbtpGetCurrentThunkStack@0
	
	test eax, eax			; Check that we have got a stack
	jz NoStack
	
	mov ecx, [eax]			; ecx = thunkstack->StackPointer
	
	;
	; Register state:
	;  eax: thunkstack
	;  ecx: thunkstack->StackPointer
	;  edx: free
	;	
	
	
	
	;
	; Restore RA to original ESP location 
	; (i.e. as referred to by Sp field in current stack location).
	;
	mov edx, [ecx]			; Get RA address (original Sp)
	push [ecx+8]
	pop fs:[014h]
	
	;
	; Retrieve funcptr.
	;
	mov edx, [ecx+4]
	
	;
	; Adjust stack pointer:
	;   thunkstack->StackPointer++
	;
	lea ecx, [ecx+0ch]
	mov [eax], ecx
	
	;
	; Register state:
	;  eax: free
	;  ecx: free
	;  edx: funcptr
	;
	
	;
	; Prepare call to JpfbtpProcedureExit.
	;
	; Initialize JPFBT_CONTEXT.
	;
	sub esp, 028h			; sizeof( JPFBT_CONTEXT )
	
	mov [esp+000h], edi
	mov [esp+004h], esi
	mov [esp+008h], ebx
	push [ebp-08h]			; edx was preserved
	pop [esp+00Ch]			; 
	push [ebp-04h]			; ecx was preserved
	pop [esp+010h]			;
	push [ebp+04h]			; eax was preserved
	pop [esp+014h]			;
	
	push [ebp]				; ebp was preserved
	pop [esp+018h]
	mov [esp+01Ch], edx		; use funcptr as EIP
	
	pushfd
	pop [esp+020h]
	
	push [ebp+8]			; esp was preserved
	pop [esp+024h]
	
	;
	; Push parameters for JpfbtpProcedureExit
	;
	push edx				; funcptr
	lea eax, [esp+4h]
	push eax				; &Context

	call JpfbtpProcedureExit@8
	
	; Tear down CONTEXT.
	add esp, 028h	; sizeof( JPFBT_CONTEXT )
	
	;
	; Tear down stack frame. Note that there are 2 possible ways to
	; return to the caller:
	;  1. Retore ESP as it was at beginning of *this* procedure, 
	;     push real RA and ret.
	;     --> pushing RA would overwrite the last parameter of the 
	;         hooked procedure, which might be an out-parameter.
	;  2. Use the ESP as it was at beginning of the hooked procedure,
	;     and return. This will avoid the problem
	;     of overwriting patrameters - however, in case of a stdcall
	;     procedure, we must clean the right amount of stack space. 
	;     Thus, we calculate how many parameters the hooked procedure
	;     took.
	;  3. Jmp rather than ret. Does not touch the stack, but undermines
	;     return address predictor.
	;
	;  3. is the way to go for the moment.
	;
	
	
	
	;
	; Note that again, we have a kind of
	; catch-22, due to the requirement of having to restore all registers
	; without having any stack (we do have stack, but in order to address
	; it, we need a register...).
	; No matter in which order we try to restore registers,
	; we will leave at least one srapped.
	;
	; Again, we have to use the TEB again and use the Sp field of
	; the current thunk stack frame as scrap memory.
	;
	
	;
	; Fill in Sp field of current thunk stack - we will need it later.
	; Note that due to register shortage, we could not have done this
	; assignment when we first accessed the TEB.
	;
	; N.B. For some callconvs (cdecl) the esp is not the same as the 
	; esp initially saved to the thunk stack.
	;
	mov eax, [JpfbtpThreadDataTlsOffset]
	mov eax, fs:[eax]		
	mov eax, [eax+0ch]		; eax = threaddata.ThunkStack->StackPointer
	mov ecx, [ebp+8]		; preserved esp
	mov [eax], ecx			; StackPointer->Sp := preserved esp
	
	;
	; Restore volatiles.
	;
	pop edx
	pop ecx
	mov eax, [ebp+4]
	pop ebp
		
	;
	; Now the tricky part - esp... We have no register left, thus we
	; use esp as general purpose register.
	;
	mov esp, [JpfbtpThreadDataTlsOffset]
	mov esp, fs:[esp]		
	mov esp, [esp+0ch]		; eax = threaddata.ThunkStack->StackPointer
	mov esp, [esp]			; esp := StackPointer->Sp
	
	;
	; Return  - if the real procedure is stcall or fastcall, it already  
	; cleaned the stack. If it cdecl, the caller will. In any case,
	; we are not in charge.
	;
	jmp fs:[014h]

NoStack:
	;
	; Now we are in trouble - being in this procedure means
	; that JpfbtpFunctionEntryThunk was able to obtain a stack,
	; yet we did not get one. This must never happen, thus bail out.
	;
	push 0					; lpArguments
	push 0					; nNumberOfArguments
	push 1					; EXCEPTION_NONCONTINUABLE
	push 0EFB20000h			; EXCEPTION_FBT_NO_THUNKSTACK
	call RaiseException@16	
	
JpfbtpFunctionCallThunk endp


END