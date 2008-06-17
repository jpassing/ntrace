#include "test.h"

static LONG CustomPrologCallCount = 0;
static LONG ProcNoArgsCallCount = 0;
static LONG ProcArgsCallCount = 0;
static LONG FastcallProcSmallArgsLargeRetCallCount = 0;
static LONG StdcallRecursiveCallCount = 0;

__declspec(naked)
ULONG CustomProlog()
{
	_asm
	{
		push esi;
		push edi;
		xor eax, eax;
		mov eax, 1;
		
		lock inc [CustomPrologCallCount];

		pop edi;
		pop esi;
		ret;
	}
}


VOID CallCustomProlog()
{
	CustomProlog();
}

__declspec(naked)
ULONG ProcNoArgs()
{
	_asm
	{
		mov edi, edi;
		push ebp;
		mov ebp, esp;

		// return
		mov eax, 0BABEFACEh;

		lock inc [ProcNoArgsCallCount];

		pop ebp;
		ret;
	}
}

VOID CallProcNoArgs()
{
	ULONG Eax_, Ebx_, Ecx_, Edx_, Esi_, Edi_;
	_asm
	{
		// prep nonvolatiles
		push ebx;
		push esi;
		push edi;

		mov ebx, 0DEAD0001h;
		mov esi, 0DEAD0002h;
		mov edi, 0DEAD0003h;

		mov eax, 0BABE0001h;
		mov ecx, 0BABE0002h;
		mov edx, 0BABE0003h;

		call ProcNoArgs;

		mov [Eax_], eax;
		mov [Ecx_], ecx;
		mov [Edx_], edx;
		
		// check nonvolatiles
		mov [Ebx_], ebx;
		mov [Esi_], esi;
		mov [Edi_], edi;
	
		pop edi;
		pop esi;
		pop ebx;
	}

	TEST( Eax_ == 0xBABEFACE );

	TEST( Ecx_ == 0xBABE0002 );
	TEST( Edx_ == 0xBABE0003 );

	TEST( Ebx_ == 0xDEAD0001 );
	TEST( Esi_ == 0xDEAD0002 );
	TEST( Edi_ == 0xDEAD0003 );
}

__declspec(naked)
VOID ProcArgs(
	__inout PULONG Arg1,
	__inout PULONG Arg2,
	__inout PULONG Arg3
	)
{
	_asm
	{
		mov edi, edi;
		push ebp;
		mov ebp, esp;
		
		// reuse all args
		mov [Arg1], 0BADCAFE1h;
		mov [Arg2], 0BADCAFE2h;
		mov [Arg3], 0BADCAFE3h;

		lock inc [ProcArgsCallCount];

		pop ebp;
		ret 12;
	}
}

VOID __stdcall CallProcArgs()
{
	ULONG  Eax_, Ebx_, Ecx_, Edx_, Esi_, Edi_, Arg1, Arg2, Arg3;
	_asm
	{
		// prep nonvolatiles
		push ebx;
		push esi;
		push edi;

		mov ebx, 0DEAD0001h;
		mov esi, 0DEAD0002h;
		mov edi, 0DEAD0003h;

		mov eax, 0BABE0001h;
		mov ecx, 0BABE0002h;
		mov edx, 0BABE0003h;

		// push args
		push 0F00F003h;
		push 0F00F002h;
		push 0F00F001h;

		call ProcArgs;

		mov [Eax_], eax;
		mov [Ecx_], ecx;
		mov [Edx_], edx;

		mov eax, [esp-4];
		mov [Arg3], eax;
		
		mov eax, [esp-8];
		mov [Arg2], eax;

		mov eax, [esp-0ch];
		mov [Arg1], eax;

		// check nonvolatiles
		mov [Ebx_], ebx;
		mov [Esi_], esi;
		mov [Edi_], edi;
	
		pop edi;
		pop esi;
		pop ebx;
	}
	//TEST( Arg1 == 0xBADCAFE1 );
	//TEST( Arg2 == 0xBADCAFE2 );
	//TEST( Arg3 == 0xBADCAFE3 );

	TEST( Eax_ == 0xBABE0001 );

	TEST( Ecx_ == 0xBABE0002 );
	TEST( Edx_ == 0xBABE0003 );

	TEST( Ebx_ == 0xDEAD0001 );
	TEST( Esi_ == 0xDEAD0002 );
	TEST( Edi_ == 0xDEAD0003 );
}

__declspec(naked)
ULONG __fastcall FastcallProcSmallArgsLargeRet(
	__in ULONG Hi,
	__in ULONG Lo 
	)
{
	UNREFERENCED_PARAMETER( Hi );
	UNREFERENCED_PARAMETER( Lo );
	_asm
	{
		mov edi, edi;
		push ebp;
		mov ebp, esp;
		
		// return
		mov eax, edx;
		mov edx, ecx;

		lock inc [FastcallProcSmallArgsLargeRetCallCount];

		pop ebp;
		ret;
	}
}

VOID CallFastcallProcSmallArgsLargeRet()
{
	ULONG Eax_, Edx_, Ebx_, Esi_, Edi_;
	_asm
	{
		// prep nonvolatiles
		push ebx;
		push esi;
		push edi;

		mov ebx, 0DEAD0001h;
		mov esi, 0DEAD0002h;
		mov edi, 0DEAD0003h;

		mov eax, 0DEADBEEFh;
		mov edx, 0DEADBEEFh;

		// args
		mov ecx, 055667788h;
		mov edx, 011223344h;

		call FastcallProcSmallArgsLargeRet;

		mov [Eax_], eax;
		mov [Edx_], edx;
		
		// check nonvolatiles
		mov [Ebx_], ebx;
		mov [Esi_], esi;
		mov [Edi_], edi;
	
		pop edi;
		pop esi;
		pop ebx;
	}

	TEST( Eax_ == 0x11223344 );
	TEST( Edx_ == 0x55667788 );

	TEST( Ebx_ == 0xDEAD0001 );
	TEST( Esi_ == 0xDEAD0002 );
	TEST( Edi_ == 0xDEAD0003 );
}

__declspec(naked)
ULONG StdcallRecursive(
	__in ULONG N
	)
{
	_asm
	{
		mov edi, edi;
		push ebp;
		mov ebp, esp;
		
		mov eax, [N];
		test eax, eax;
		jnz Check1

		; N == 0
		mov eax, 0;
		jmp Return;
Check1:
		cmp eax, 1;
		jnz Recurse;

		; N == 1
		mov eax, 1;
		jmp Return;
Recurse:
		; N > 1
		push ebx;
		push edi;

		mov ebx, eax;
		xor edi, edi;		; sum

		dec eax;
		push eax;
		call StdcallRecursive;
		add edi, eax;
		
		mov eax, ebx;
		sub eax, 2;
		push eax;
		call StdcallRecursive;

		add eax, edi;

		pop edi;
		pop ebx;
Return:
		// scrap volatiles
		mov ecx, 0DEADBEEFh;
		mov edx, 0DEADBEEFh;

		lock inc [StdcallRecursiveCallCount];

		pop ebp;
		ret 4;
	}
}

VOID CallStdcallRecursive()
{
	ULONG Eax_, Ebx_, Esi_, Edi_;
	_asm
	{
		// prep nonvolatiles
		push ebx;
		push esi;
		push edi;

		mov ebx, 0DEAD0001h;
		mov esi, 0DEAD0002h;
		mov edi, 0DEAD0003h;

		mov eax, 0DEADBEEFh;
		mov ecx, 0DEADBEEFh;
		mov edx, 0DEADBEEFh;

		// push args
		push 3;

		call StdcallRecursive;

		mov [Eax_], eax;
		
		// check nonvolatiles
		mov [Ebx_], ebx;
		mov [Esi_], esi;
		mov [Edi_], edi;
	
		pop edi;
		pop esi;
		pop ebx;
	}

	TEST( Eax_ == 2 );

	TEST( Ebx_ == 0xDEAD0001 );
	TEST( Esi_ == 0xDEAD0002 );
	TEST( Edi_ == 0xDEAD0003 );
}

__declspec(naked)
VOID Raise()
{
	_asm
	{
		mov edi, edi;
		push ebp;
		mov ebp, esp;
	}

#ifdef JPFBT_TARGET_USERMODE
	RaiseException( 'excp', 0, 0, NULL );
#else
	ExRaiseStatus( 'excp' );
#endif
	_asm 
	{
		mov esp, ebp;
		pop ebp;
		ret;
	}
}

//
// N.B. For profiling, exclude these procs from being instrumented.
//
static SAMPLE_PROC SampleProcs[] = 
{
	{ ( PVOID ) CustomProlog,					CallCustomProlog,					&CustomPrologCallCount,					 0, 0, 1, FALSE },
	{ ( PVOID ) ProcNoArgs,						CallProcNoArgs, 					&ProcNoArgsCallCount,					 0, 0, 1, TRUE },
	{ ( PVOID ) ProcArgs,						CallProcArgs,						&ProcArgsCallCount,						 0, 0, 1, TRUE },
	{ ( PVOID ) FastcallProcSmallArgsLargeRet,	CallFastcallProcSmallArgsLargeRet,	&FastcallProcSmallArgsLargeRetCallCount, 0, 0, 1, TRUE },
	{ ( PVOID ) StdcallRecursive,				CallStdcallRecursive,				&StdcallRecursiveCallCount,				 0, 0, 5, TRUE },
};

static SAMPLE_PROC_SET SampleProcSet = { _countof( SampleProcs ), SampleProcs };

PSAMPLE_PROC_SET GetSampleProcs()
{
	return &SampleProcSet;
}