
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

#include "sgs_cfg.h"
#include "sgs_tok.h"
#include "sgs_ctx.h"


static SGS_INLINE int detectline( const char* code, int32_t at )
{
	return code[ at ] == '\r' || ( code[ at ] == '\n' && ( at == 0 || code[ at - 1 ] != '\r' ) );
}

static void skipcomment( SGS_CTX, MemBuf* out, LineNum* line, const char* code, int32_t* at, int32_t length )
{
	int32_t i = *at + 1;
	UNUSED( out );
	if( code[ i ] == '/' )
	{
		i++;
		while( i < length && code[ i ] != '\n' && code[ i ] != '\r' )
			i++;
		if( code[ i ] == '\r' && code[ i + 1 ] == '\n' )
			i++;
		(*line)++;
		*at = i;
	}
	else
	{
		LineNum init = *line;
		i++;
		while( i < length )
		{
			if( detectline( code, i ) )
				(*line)++;
			if( code[ i ] == '/' && i > 0 && code[ i - 1 ] == '*' )
				break;
			else
				i++;
		}
		if( i == length )
		{
			sgs_Printf( C, SGS_WARNING, init, "Comment has no end" );
			*at = i - 1;
		}
		else
			*at = i;
	}
}

static int32_t string_inplace_fix( char* str, int32_t len )
{
	char *ipos = str, *opos = str, *iend = str + len;
	while( ipos < iend )
	{
		if( *ipos == '\\' )
		{
			ipos++;
			if( *ipos >= '0' && *ipos <= '7' )
			{
				int oct = *ipos++ - '0';
				if( *ipos >= '0' && *ipos <= '7' ){ oct *= 8; oct += *ipos++ - '0'; }
				if( *ipos >= '0' && *ipos <= '7' ){ oct *= 8; oct += *ipos++ - '0'; }
				ipos--;
				if( oct > 0xffff ) *opos++ = oct >> 8;
				if( oct > 0xff ) *opos++ = oct >> 4;
				*opos = oct;
			}
			else
			{
				switch( *ipos )
				{
				case 'a': *opos = '\a'; break;
				case 'b': *opos = '\b'; break;
				case 'f': *opos = '\f'; break;
				case 'n': *opos = '\n'; break;
				case 'r': *opos = '\r'; break;
				case 't': *opos = '\t'; break;
				case 'v': *opos = '\v'; break;
				case 'x':
					if( hexchar( ipos[1] ) && hexchar( ipos[2] ) )
					{
						*opos = ( gethex( ipos[1] ) << 4 ) | gethex( ipos[2] );
						ipos += 2;
						if( hexchar( ipos[1] ) && hexchar( ipos[2] ) )
						{
							opos++;
							*opos = ( gethex( ipos[1] ) << 4 ) | gethex( ipos[2] );
							ipos += 2;
						}
						break;
					}
				/* ', ", \ too: */
				default: *opos = *ipos; break;
				}
			}
		}
		else
			*opos = *ipos;
		ipos++;
		opos++;
	}
	return opos - str;
}


static int ident_equal( const char* ptr, int size, const char* what )
{
	int wlen = strlen( what );
	return size == wlen && memcmp( ptr, what, size ) == 0;
}
static void readident( SGS_CTX, MemBuf* out, const char* code, int32_t* at, int32_t length )
{
	int32_t sz = 0;
	int32_t i = *at;
	int32_t pos_rev = out->size;
	UNUSED( C );
	membuf_appchr( out, ST_IDENT );
	membuf_appchr( out, 0 );
	while( i < length && ( isalnum( code[ i ] ) || code[ i ] == '_' ) )
	{
		if( sz++ < 255 )
			membuf_appchr( out, code[ i ] );
		else
			sgs_BreakIf( sz >= 255 );
		i++;
	}
	out->ptr[ pos_rev + 1 ] = sz;
	if( ident_equal( out->ptr + pos_rev + 2, sz, "var" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "global" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "null" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "true" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "false" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "if" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "else" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "do" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "while" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "for" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "foreach" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "break" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "continue" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "function" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "return" ) ||
		ident_equal( out->ptr + pos_rev + 2, sz, "mutate" ) )
	{
		out->ptr[ pos_rev ] = ST_KEYWORD;
	}
	*at = --i;
}

static void readstring( SGS_CTX, MemBuf* out, LineNum* line, const char* code, int32_t* at, int32_t length )
{
	int32_t i = *at + 1;
	char endchr = code[ *at ];
	int escaped = FALSE;

	while( i < length )
	{
		char c = code[ i ];
		if( detectline( code, *at ) )
			(*line)++;
		if( c == '\\' )
			escaped = !escaped;
		else if( c == endchr && !escaped )
		{
			int32_t size = i - *at - 1, newsize;
			int32_t numpos = out->size + 1;
			membuf_appchr( out, ST_STRING );
			membuf_appbuf( out, (char*) &size, 4 );
			membuf_appbuf( out, code + *at + 1, size );
			*at += size + 1;
			newsize = string_inplace_fix( out->ptr + numpos + 4, size );
			*((int32_t*)( out->ptr + numpos )) = newsize;
			out->size -= size - newsize;
			return;
		}
		else
			escaped = FALSE;
		i++;
	}

	C->state |= SGS_MUST_STOP;
	sgs_Printf( C, SGS_ERROR, *at, "end of string not found" );
}

static const char* sgs_opchars = "=<>+-*/%!~&|^.$";
static const char* sgs_operators = "===;!==;==;!=;<=;>=;+=;-=;*=;/=;%=;&=;|=;^=;<<=;>>=;$=;<<;>>;&&=;||=;&&;||;<;>;=&;=;++;--;+;-;*;/;%;&;|;^;.;$;!;~";
static const TokenType sgs_optable[] =
{
	ST_OP_SEQ, ST_OP_SNEQ, ST_OP_EQ, ST_OP_NEQ, ST_OP_LEQ, ST_OP_GEQ,
	ST_OP_ADDEQ, ST_OP_SUBEQ, ST_OP_MULEQ, ST_OP_DIVEQ, ST_OP_MODEQ,
	ST_OP_ANDEQ, ST_OP_OREQ, ST_OP_XOREQ, ST_OP_LSHEQ, ST_OP_RSHEQ, ST_OP_CATEQ,
	ST_OP_LSH, ST_OP_RSH, ST_OP_BLAEQ, ST_OP_BLOEQ, ST_OP_BLAND, ST_OP_BLOR,
	ST_OP_LESS, ST_OP_GRTR, ST_OP_COPY, ST_OP_SET, ST_OP_INC, ST_OP_DEC,
	ST_OP_ADD, ST_OP_SUB, ST_OP_MUL, ST_OP_DIV, ST_OP_MOD, ST_OP_AND,
	ST_OP_OR, ST_OP_XOR, ST_OP_MMBR, ST_OP_CAT, ST_OP_NOT, ST_OP_INV
};
static const char sgs_opsep = ';';

static int op_oneof( const char* str, const char* test, char sep, int* outlen )
{
	const char* pstr = str;
	int passed = 0, equal = 0, which = 0, len = 0;

	do
	{
		if( *test == sep || *test == 0 )
		{
			if( passed == equal )
			{
				*outlen = len;
				return which;
			}
			if( *test == 0 )
				return -1;
			passed = 0;
			equal = 0;
			len = 0;
			pstr = str;
			which++;
		}
		else
		{
			len++;
			passed++;
			if( *pstr == *test )
				equal++;
			pstr += !!*pstr;
		}
	}
	while( *test++ );

	return -1;
}

static void readop( SGS_CTX, MemBuf* out, LineNum line, const char* code, int32_t* at, int32_t length )
{
	char opstr[ 4 ];
	int32_t opsize = 0, ropsize = 0, i = *at, whichop, len = -1;

	memset( opstr, 0, 4 );

	/* read in the operator */
	while( i < length && isoneof( code[ i ], sgs_opchars ) )
	{
		if( opsize < 3 )
			opstr[ opsize++ ] = code[ i ];
		ropsize++;
		i++;
	}

	/* test for various errors */
	if( ropsize > 3 ){ *at += ropsize; goto op_read_error; }
	whichop = op_oneof( opstr, sgs_operators, sgs_opsep, &len );
	*at += len - 1;
	if( whichop < 0 ) goto op_read_error;

	membuf_appchr( out, sgs_optable[ whichop ] );
	return;

op_read_error:
	C->state |= SGS_HAS_ERRORS;
	sgs_Printf( C, SGS_ERROR, line, "invalid operator found: \"%s%s\", size=%d", opstr, ropsize > 3 ? "..." : "", ropsize );
}

TokenList sgsT_Gen( SGS_CTX, const char* code, int32_t length )
{
	int32_t i;
	LineNum line = 1;
	MemBuf s = membuf_create();
	membuf_reserve( &s, SGS_TOKENLIST_PREALLOC );
	if( length < 0 )
		length = strlen( code );

	for( i = 0; i < length; ++i )
	{
		LineNum tokline = line;
		char fc = code[ i ];
		int isz = s.size;

		/* whitespace */
		if( detectline( code, i ) )
			line++;
		if( isoneof( fc, " \n\r\t" ) )
			continue;

		/* comment */
		if( fc == '/' && ( code[ i + 1 ] == '/'
					|| code[ i + 1 ] == '*' ) )	skipcomment	( C, &s, &line, code, &i, length );

		/* special symbol */
		else if( isoneof( fc, "()[]{},;" ) )	membuf_appchr( &s, fc );

		/* identifier */
		else if( fc == '_' || isalpha( fc ) )	readident	( C, &s, code, &i, length );

		/* number */
		else if( isdigit( fc ) )
		{
			sgs_Integer vi = 0;
			sgs_Real vr = 0;
			const char* pos = code + i;
			int res = util_strtonum( &pos, code + length, &vi, &vr );
			if( res == 0 )
			{
				C->state |= SGS_HAS_ERRORS;
				sgs_Printf( C, SGS_ERROR, i, "failed to parse numeric constant" );
			}
			else if( res == 1 )
			{
				membuf_appchr( &s, ST_NUMINT );
				membuf_appbuf( &s, &vi, sizeof( vi ) );
			}
			else if( res == 2 )
			{
				membuf_appchr( &s, ST_NUMREAL );
				membuf_appbuf( &s, &vr, sizeof( vr ) );
			}
			else{ sgs_BreakIf( "Invalid return value from util_strtonum." ); }
			i = pos - code;
			i--;
		}

		/* string */
		else if( fc == '\'' || fc == '\"' )		readstring	( C, &s, &line, code, &i, length );

		/* operator */
		else if( isoneof( fc, sgs_opchars ) )	readop		( C, &s, line, code, &i, length );

		/* - unexpected symbol */
		else
		{
			C->state |= SGS_HAS_ERRORS;
			sgs_Printf( C, SGS_ERROR, i, "unexpected symbol: %c", fc );
		}

		if( s.size != isz ) /* write a line only if successfully wrote something (a token) */
			membuf_appbuf( &s, &tokline, sizeof( tokline ) );

		if( C->state & SGS_MUST_STOP )
			break;
	}

	membuf_appchr( &s, ST_NULL );
	return (TokenList) s.ptr;
}

void sgsT_Free( TokenList tlist )
{
	StrBuf s = strbuf_partial( (char*) tlist, 0 );
	strbuf_destroy( &s );
}

TokenList sgsT_Next( TokenList tok )
{
	sgs_BreakIf( !tok );
	sgs_BreakIf( !*tok );

	switch( *tok )
	{
	case ST_IDENT:
	case ST_KEYWORD:
		return tok + tok[ 1 ] + 2 + sizeof( LineNum );
	case ST_NUMREAL:
	case ST_NUMINT:
		return tok + 9 + sizeof( LineNum );
	case ST_STRING:
		return tok + 5 + sizeof( LineNum ) + ST_READINT( tok + 1 );
	default:
		return tok + 1 + sizeof( LineNum );
	}
}

LineNum sgsT_LineNum( TokenList tok )
{
	tok = sgsT_Next( tok );
	return ST_READLN( tok - 2 );
}


int32_t sgsT_ListSize( TokenList tlist )
{
	int32_t i = 0;
	while( *tlist )
	{
		tlist = sgsT_Next( tlist );
		i++;
	}
	return i;
}

int32_t sgsT_ListMemSize( TokenList tlist )
{
	TokenList last = tlist;
	while( *last )
		last = sgsT_Next( last );
	return last - tlist + 1;
}


void sgsT_DumpToken( TokenList tok )
{
	switch( *tok )
	{
	case ST_RBRKL:
	case ST_RBRKR:
	case ST_SBRKL:
	case ST_SBRKR:
	case ST_CBRKL:
	case ST_CBRKR:
	case ST_ARGSEP:
	case ST_STSEP:
		printf( "%c", *tok );
		break;
	case ST_IDENT:
		fwrite( "id(", 1, 3, stdout );
		fwrite( tok + 2, 1, tok[ 1 ], stdout );
		fwrite( ")", 1, 1, stdout );
		break;
	case ST_KEYWORD:
		fwrite( "[", 1, 1, stdout );
		fwrite( tok + 2, 1, tok[ 1 ], stdout );
		fwrite( "]", 1, 1, stdout );
		break;
	case ST_NUMREAL:
		printf( "real(%f)", *((double*)(tok + 1)) );
		break;
	case ST_NUMINT:
		printf( "int(%" PRId64 ")", *((int64_t*)(tok + 1)) );
		break;
	case ST_STRING:
		fwrite( "str(", 1, 4, stdout );
		print_safe( (const char*) tok + 5, ST_READINT( tok + 1 ) );
		fwrite( ")", 1, 1, stdout );
		break;
#define OPR( op ) printf( "%s", op );
	case ST_OP_SEQ: OPR( "===" ); break;
	case ST_OP_SNEQ: OPR( "!==" ); break;
	case ST_OP_EQ: OPR( "==" ); break;
	case ST_OP_NEQ: OPR( "!=" ); break;
	case ST_OP_LEQ: OPR( "<=" ); break;
	case ST_OP_GEQ: OPR( ">=" ); break;
	case ST_OP_ADDEQ: OPR( "+=" ); break;
	case ST_OP_SUBEQ: OPR( "-=" ); break;
	case ST_OP_MULEQ: OPR( "*=" ); break;
	case ST_OP_DIVEQ: OPR( "/=" ); break;
	case ST_OP_MODEQ: OPR( "%=" ); break;
	case ST_OP_ANDEQ: OPR( "&=" ); break;
	case ST_OP_OREQ: OPR( "|=" ); break;
	case ST_OP_XOREQ: OPR( "^=" ); break;
	case ST_OP_LSHEQ: OPR( "<<=" ); break;
	case ST_OP_RSHEQ: OPR( ">>=" ); break;
	case ST_OP_BLAEQ: OPR( "&&=" ); break;
	case ST_OP_BLOEQ: OPR( "||=" ); break;
	case ST_OP_CATEQ: OPR( "$=" ); break;
	case ST_OP_BLAND: OPR( "&&" ); break;
	case ST_OP_BLOR: OPR( "||" ); break;
	case ST_OP_LESS: OPR( "<" ); break;
	case ST_OP_GRTR: OPR( ">" ); break;
	case ST_OP_SET: OPR( "=" ); break;
	case ST_OP_COPY: OPR( "=&" ); break;
	case ST_OP_ADD: OPR( "+" ); break;
	case ST_OP_SUB: OPR( "-" ); break;
	case ST_OP_MUL: OPR( "*" ); break;
	case ST_OP_DIV: OPR( "/" ); break;
	case ST_OP_MOD: OPR( "%" ); break;
	case ST_OP_AND: OPR( "&" ); break;
	case ST_OP_OR: OPR( "|" ); break;
	case ST_OP_XOR: OPR( "^" ); break;
	case ST_OP_LSH: OPR( "<<" ); break;
	case ST_OP_RSH: OPR( ">>" ); break;
	case ST_OP_MMBR: OPR( "." ); break;
	case ST_OP_CAT: OPR( "$" ); break;
	case ST_OP_NOT: OPR( "!" ); break;
	case ST_OP_INV: OPR( "~" ); break;
	case ST_OP_INC: OPR( "++" ); break;
	case ST_OP_DEC: OPR( "--" ); break;
#undef OPR
	default:
		fwrite( "<invalid>", 1, 9, stdout );
		break;
	}
}
void sgsT_DumpList( TokenList tlist, TokenList tend )
{
	printf( "\n" );
	while( tlist != tend && *tlist != 0 )
	{
		printf( "   " );
		sgsT_DumpToken( tlist );
		tlist = sgsT_Next( tlist );
	}
	printf( "\n\n" );
}