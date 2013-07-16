

#include <ctype.h>

#define SGS_INTERNAL
#include <sgs_int.h>

#include "sgs_idbg.h"


/*
	To successfully debug recursion issues,
	call stack limit must be slightly raised
	to allow execution of debug code.
*/
#ifndef SGS_IDBG_STACK_EXTENSION
#define SGS_IDBG_STACK_EXTENSION 128
#endif


#define STREQ( a, b ) ( strcmp( a, b ) == 0 )


#define BFR_SIZE 1024

static void idbg_readStdin( SGS_IDBG )
{
	/* read from STDIN */
	int wsz = 31, i = 0;
	char bfr[ BFR_SIZE ];
	membuf_resize( &D->input, D->C, 0 );
	while( fgets( bfr, BFR_SIZE, stdin ) )
	{
		int len = strlen( bfr );
		membuf_appbuf( &D->input, D->C, bfr, len );
		if( len && bfr[ len - 1 ] == '\n' )
			break;
	}
	membuf_appchr( &D->input, D->C, 0 );

	/* parse first word */
	while( i < wsz && i < D->input.size && isalpha( D->input.ptr[ i ] ) )
	{
		D->iword[ i ] = D->input.ptr[ i ];
		i++;
	}
	D->iword[ i ] = 0;
}


static void idbgPrintFunc( void* data, SGS_CTX, int type, const char* message )
{
	SGS_IDBG = (sgs_IDbg*) data;

	D->stkoff = C->stack_off - C->stack_base;
	D->stksize = C->stack_top - C->stack_base;
	D->pfn( D->pctx, C, type, message );
	if( D->inside || type < D->minlev )
		return;
	D->inside = 1;
	C->sf_count -= SGS_IDBG_STACK_EXTENSION;
	printf( "----- Interactive SGScript Debug Inspector -----" );

	for(;;)
	{
		printf( "\n> " );
		idbg_readStdin( D );
		if( !*D->iword )
			continue;

		if( STREQ( D->iword, "continue" ) || STREQ( D->iword, "cont" ) ) break;
		if( STREQ( D->iword, "quit" ) ) exit( 0 );
		if( STREQ( D->iword, "reprint" ) ) D->pfn( D->pctx, C, type, message );
		if( STREQ( D->iword, "stack" ) ) sgs_Stat( C, SGS_STAT_DUMP_STACK );
		if( STREQ( D->iword, "globals" ) ) sgs_Stat( C, SGS_STAT_DUMP_GLOBALS );
		if( STREQ( D->iword, "objects" ) ) sgs_Stat( C, SGS_STAT_DUMP_OBJECTS );
		if( STREQ( D->iword, "callstack" ) || STREQ( D->iword, "frames" ) )
			sgs_Stat( C, SGS_STAT_DUMP_FRAMES );
		if( STREQ( D->iword, "exec" ) )
			sgs_ExecBuffer( C, D->input.ptr + 5, D->input.size - 5 );
		if( STREQ( D->iword, "eval" ) )
		{
			int rvc = 0;
			sgs_EvalBuffer( C, D->input.ptr + 5, D->input.size - 5, &rvc );
			sgs_PushGlobal( C, "printvars" );
			sgs_Call( C, rvc, 0 );
		}
		if( STREQ( D->iword, "print" ) )
		{
			int rvc = 0;
			MemBuf prepstr = membuf_create();
			membuf_appbuf( &prepstr, C, "return (", 8 );
			membuf_appbuf( &prepstr, C, D->input.ptr + 5, D->input.size - 5 );
			membuf_appbuf( &prepstr, C, ");", 2 );
			sgs_EvalBuffer( C, prepstr.ptr, prepstr.size, &rvc );
			membuf_destroy( &prepstr, C );
			sgs_PushGlobal( C, "printvars" );
			sgs_Call( C, rvc, 0 );
		}
	}

	C->sf_count += SGS_IDBG_STACK_EXTENSION;
	D->inside = 0;
}


static int idbg_stackitem( SGS_CTX )
{
	int full = 0, argc = sgs_StackSize( C );
	sgs_Int off, cnt;
	sgs_Variable* a, *b;
	SGS_IDBG = (sgs_IDbg*) C->print_ctx;

	if( argc < 1 || argc > 2 ||
		!sgs_ParseInt( C, 0, &off ) ||
		( argc >= 2 && !sgs_ParseBool( C, 1, &full ) ) )
	{
		sgs_Printf( C, SGS_WARNING, "dbg_stackitem(): "
			"unexpected arguments; function expects int[, bool]" );
		return 0;
	}

	a = full ? C->stack_base : C->stack_base + D->stkoff;
	b = C->stack_base + D->stksize;
	cnt = b - a;
	if( off >= cnt || -off > cnt )
	{
		sgs_Printf( C, SGS_WARNING, "dbg_stackitem(): "
			"index %d out of bounds, count = %d\n", (int) off, (int) cnt );
		return 0;
	}

	sgs_PushVariable( C, off >= 0 ? a + off : b + off );
	return 1;
}

static int idbg_setstackitem( SGS_CTX )
{
	int full = 0, argc = sgs_StackSize( C );
	sgs_Int off, cnt;
	sgs_Variable* a, *b, *x;
	SGS_IDBG = (sgs_IDbg*) C->print_ctx;

	if( argc < 2 || argc > 3 ||
		!sgs_ParseInt( C, 0, &off ) ||
		( argc >= 3 && !sgs_ParseBool( C, 2, &full ) ) )
	{
		sgs_Printf( C, SGS_WARNING, "dbg_setstackitem(): "
			"unexpected arguments; function expects int, any[, bool]" );
		return 0;
	}

	a = full ? C->stack_base : C->stack_base + D->stkoff;
	b = C->stack_base + D->stksize;
	cnt = b - a;
	if( off >= cnt || -off > cnt )
	{
		sgs_Printf( C, SGS_WARNING, "dbg_setstackitem(): "
			"index %d out of bounds, count = %d\n", (int) off, (int) cnt );
		return 0;
	}

	x = off >= 0 ? a + off : b + off;
	sgs_Release( C, x );
	sgs_GetStackItem( C, 1, x );
	sgs_Acquire( C, x );
	return 0;
}


int sgs_InitIDbg( SGS_CTX, SGS_IDBG )
{
	D->C = C;
	D->pfn = C->print_fn;
	D->pctx = C->print_ctx;
	C->print_fn = idbgPrintFunc;
	C->print_ctx = D;

	D->input = membuf_create();
	D->iword[0] = 0;
	D->inside = 0;
	D->minlev = SGS_WARNING;

	sgs_PushCFunction( C, idbg_stackitem );
	sgs_StoreGlobal( C, "dbg_stackitem" );
	sgs_PushCFunction( C, idbg_setstackitem );
	sgs_StoreGlobal( C, "dbg_setstackitem" );

	return SGS_SUCCESS;
}

int sgs_CloseIDbg( SGS_CTX, SGS_IDBG )
{
	C->print_fn = D->pfn;
	C->print_ctx = D->pctx;
	membuf_destroy( &D->input, C );
	return SGS_SUCCESS;
}

