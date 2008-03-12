#include <cfix.h>
#include "test.h"
#include "..\jpfbt\jpfbtp.h"

static void Patch()
{
	JPFBT_CODE_PATCH Patch;
	PJPFBT_CODE_PATCH PatchArray[ 1 ];
	ULONG PatchMe = 0xAABBCCDD;

	Patch.Target	= &PatchMe;
	Patch.CodeSize	= sizeof( ULONG );
	Patch.NewCode[ 0 ] = 0xEF;
	Patch.NewCode[ 1 ] = 0xBE;
	Patch.NewCode[ 2 ] = 0xAD;
	Patch.NewCode[ 3 ] = 0xDE;

	PatchArray[ 0 ] = &Patch;

	TEST_SUCCESS( JpfbtpPatchCode(
		JpfbtPatch,
		1,
		PatchArray ) );

	TEST( PatchMe == 0xDEADBEEF );

	TEST_SUCCESS( JpfbtpPatchCode(
		JpfbtUnpatch,
		1,
		PatchArray ) );

	TEST( PatchMe == 0xAABBCCDD );
}

CFIX_BEGIN_FIXTURE( Patch )
	CFIX_FIXTURE_ENTRY( Patch )
CFIX_END_FIXTURE()