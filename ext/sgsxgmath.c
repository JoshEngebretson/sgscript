

#include <stdio.h>

#include "sgsxgmath.h"


#define XGM_VT XGM_VECTOR_TYPE
#define XGM_WARNING( err ) sgs_Printf( C, SGS_WARNING, err );
#define XGM_OHDR XGM_VT* hdr = (XGM_VT*) data->data;


/*  2 D   V E C T O R  */

static int xgm_v2_convert( SGS_CTX, sgs_VarObj* data, int type )
{
	XGM_OHDR;
	if( type == SGS_CONVOP_CLONE )
	{
		sgs_PushVec2( C, hdr[0], hdr[1] );
		return SGS_SUCCESS;
	}
	else if( type == SGS_CONVOP_TOTYPE )
	{
		sgs_PushString( C, "vec2" );
		return SGS_SUCCESS;
	}
	else if( type == SGS_VT_STRING )
	{
		char buf[ 128 ];
		sprintf( buf, "vec2(%g;%g)", hdr[0], hdr[1] );
		sgs_PushString( C, buf );
		return SGS_SUCCESS;
	}
	return SGS_ENOTSUP;
}

static int xgm_v2_getindex( SGS_CTX, sgs_VarObj* data, int prop )
{
	char* str;
	sgs_SizeVal size;
	XGM_OHDR;
	
	if( !sgs_ParseString( C, 0, &str, &size ) )
		return SGS_EINVAL;
	if( !strcmp( str, "x" ) ){ sgs_PushReal( C, hdr[ 0 ] ); return SGS_SUCCESS; }
	if( !strcmp( str, "y" ) ){ sgs_PushReal( C, hdr[ 1 ] ); return SGS_SUCCESS; }
	if( !strcmp( str, "length" ) ){ sgs_PushReal( C, sqrt( hdr[0] * hdr[0] + hdr[1] * hdr[1] ) ); return SGS_SUCCESS; }
	if( !strcmp( str, "length_squared" ) ){ sgs_PushReal( C, hdr[0] * hdr[0] + hdr[1] * hdr[1] ); return SGS_SUCCESS; }
	if( !strcmp( str, "normalized" ) )
	{
		XGM_VT lensq = hdr[0] * hdr[0] + hdr[1] * hdr[1];
		if( lensq )
		{
			lensq = 1.0 / sqrt( lensq );
			sgs_PushVec2( C, hdr[0] * lensq, hdr[1] * lensq );
		}
		else
			sgs_PushVec2( C, 0, 0 );
		return SGS_SUCCESS;
	}
	if( !strcmp( str, "angle" ) ){ sgs_PushReal( C, atan2( hdr[0], hdr[1] ) ); return SGS_SUCCESS; }
	if( !strcmp( str, "perp" ) ){ sgs_PushVec2( C, -hdr[1], hdr[0] ); return SGS_SUCCESS; }
	if( !strcmp( str, "perp2" ) ){ sgs_PushVec2( C, hdr[1], -hdr[0] ); return SGS_SUCCESS; }
	return SGS_ENOTFND;
}

static int xgm_v2_setindex( SGS_CTX, sgs_VarObj* data, int prop )
{
	char* str;
	sgs_SizeVal size;
	sgs_Real val;
	XGM_OHDR;
	
	if( !sgs_ParseString( C, 0, &str, &size ) )
		return SGS_EINVAL;
	if( !sgs_ParseReal( C, 1, &val ) )
		return SGS_EINVAL;
	if( !strcmp( str, "x" ) ){ hdr[ 0 ] = val; return SGS_SUCCESS; }
	if( !strcmp( str, "y" ) ){ hdr[ 1 ] = val; return SGS_SUCCESS; }
	return SGS_ENOTFND;
}

static int xgm_v2_expr( SGS_CTX, sgs_VarObj* data, int type )
{
	if( type == SGS_EOP_ADD ||
		type == SGS_EOP_SUB ||
		type == SGS_EOP_MUL ||
		type == SGS_EOP_DIV ||
		type == SGS_EOP_MOD )
	{
		XGM_VT r[ 2 ], v1[ 2 ], v2[ 2 ];
		if( !sgs_ParseVec2( C, 0, v1, 0 ) || !sgs_ParseVec2( C, 1, v2, 0 ) )
			return SGS_EINVAL;
		
		if( ( type == SGS_EOP_DIV || type == SGS_EOP_MOD ) &&
			( v2[0] == 0 || v2[1] == 0 ) )
		{
			const char* errstr = type == SGS_EOP_DIV ?
				"vec2 operator '/' - division by zero" :
				"vec2 operator '%' - modulo by zero";
			sgs_Printf( C, SGS_ERROR, errstr );
			return SGS_EINPROC;
		}
		
		if( type == SGS_EOP_ADD )
			{ r[0] = v1[0] + v2[0]; r[1] = v1[1] + v2[1]; }
		else if( type == SGS_EOP_SUB )
			{ r[0] = v1[0] - v2[0]; r[1] = v1[1] - v2[1]; }
		else if( type == SGS_EOP_MUL )
			{ r[0] = v1[0] * v2[0]; r[1] = v1[1] * v2[1]; }
		else if( type == SGS_EOP_DIV )
			{ r[0] = v1[0] / v2[0]; r[1] = v1[1] / v2[1]; }
		else
			{ r[0] = fmod( v1[0], v2[0] ); r[1] = fmod( v1[1], v2[1] ); }
		
		sgs_PushVec2( C, r[0], r[1] );
		return SGS_SUCCESS;
	}
	else if( type == SGS_EOP_COMPARE )
	{
		XGM_VT *v1, *v2;
		if( !sgs_IsObject( C, 0, xgm_vec2_iface ) ||
			!sgs_IsObject( C, 1, xgm_vec2_iface ) )
			return SGS_EINVAL;
		
		v1 = (XGM_VT*) sgs_GetObjectData( C, 0 );
		v2 = (XGM_VT*) sgs_GetObjectData( C, 1 );
		
		if( v1[0] != v2[0] )
			return v1[0] - v2[0];
		return v1[1] - v2[1];
	}
	else if( type == SGS_EOP_NEGATE )
	{
		XGM_OHDR;
		sgs_PushVec2( C, -hdr[0], -hdr[1] );
		return SGS_SUCCESS;
	}
	return SGS_ENOTSUP;
}

static int xgm_vec2( SGS_CTX )
{
	int argc = sgs_StackSize( C );
	XGM_VT v[ 2 ] = { 0, 0 };
	
	SGSFN( "vec2" );
	
	if( !sgs_LoadArgs( C, "f|f.", v, v + 1 ) )
		return 0;
	
	if( argc == 1 )
		v[1] = v[0];
	
	sgs_PushVec2( C, v[ 0 ], v[ 1 ] );
	return 1;
}

static int xgm_vec2_dot( SGS_CTX )
{
	XGM_VT v1[2], v2[2];
	
	SGSFN( "vec2_dot" );
	
	if( !sgs_LoadArgs( C, "!x!x", sgs_ArgCheck_Vec2, v1, sgs_ArgCheck_Vec2, v2 ) )
		return 0;
	
	sgs_PushReal( C, v1[0] * v2[0] + v1[1] * v2[1] );
	return 1;
}



/*  3 D   V E C T O R  */

static int xgm_vec3( SGS_CTX )
{
	int argc = sgs_StackSize( C );
	XGM_VT v[ 3 ] = { 0, 0, 0 };
	
	SGSFN( "vec3" );
	
	if( !sgs_LoadArgs( C, "f|ff.", v, v + 1, v + 2 ) )
		return 0;
	
	if( argc == 2 )
		return XGM_WARNING( "expected 1 or 3 real values" );
	
	if( argc == 1 )
		v[2] = v[1] = v[0];
	
	sgs_PushVec3( C, v[0], v[1], v[2] );
	return 1;
}

static int xgm_vec3_dot( SGS_CTX )
{
	XGM_VT v1[3], v2[3];
	
	SGSFN( "vec3_dot" );
	
	if( !sgs_LoadArgs( C, "!x!x", sgs_ArgCheck_Vec3, v1, sgs_ArgCheck_Vec3, v2 ) )
		return 0;
	
	sgs_PushReal( C, v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2] );
	return 1;
}



/*  4 D   V E C T O R  */

static int xgm_vec4( SGS_CTX )
{
	int argc = sgs_StackSize( C );
	XGM_VT v[ 4 ] = { 0, 0, 0, 0 };
	
	SGSFN( "vec4" );
	
	if( !sgs_LoadArgs( C, "f|fff.", v, v + 1, v + 2, v + 3 ) )
		return 0;
	
	if( argc == 1 )
		v[3] = v[2] = v[1] = v[0];
	else if( argc == 2 )
	{
		v[3] = v[1];
		v[2] = v[1] = v[0];
	}
	
	sgs_PushVec4( C, v[0], v[1], v[2], v[3] );
	return 1;
}

static int xgm_vec4_dot( SGS_CTX )
{
	XGM_VT v1[4], v2[4];
	
	SGSFN( "vec4_dot" );
	
	if( !sgs_LoadArgs( C, "!x!x", sgs_ArgCheck_Vec4, v1, sgs_ArgCheck_Vec4, v2 ) )
		return 0;
	
	sgs_PushReal( C, v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2] + v1[3] * v2[3] );
	return 1;
}



sgs_ObjCallback xgm_vec2_iface[] =
{
	SGS_OP_GETINDEX, xgm_v2_getindex,
	SGS_OP_SETINDEX, xgm_v2_setindex,
	SGS_OP_EXPR, xgm_v2_expr,
	SGS_OP_CONVERT, xgm_v2_convert,
	SGS_OP_END
};

sgs_ObjCallback xgm_vec3_iface[] =
{
	SGS_OP_END
};

sgs_ObjCallback xgm_vec4_iface[] =
{
	SGS_OP_END
};


void sgs_PushVec2( SGS_CTX, XGM_VT x, XGM_VT y )
{
	XGM_VT* nv = (XGM_VT*) sgs_PushObjectIPA( C, sizeof(XGM_VT) * 2, xgm_vec2_iface );
	nv[ 0 ] = x;
	nv[ 1 ] = y;
}

void sgs_PushVec3( SGS_CTX, XGM_VT x, XGM_VT y, XGM_VT z )
{
	XGM_VT* nv = (XGM_VT*) sgs_PushObjectIPA( C, sizeof(XGM_VT) * 3, xgm_vec2_iface );
	nv[ 0 ] = x;
	nv[ 1 ] = y;
	nv[ 2 ] = z;
}

void sgs_PushVec4( SGS_CTX, XGM_VT x, XGM_VT y, XGM_VT z, XGM_VT w )
{
	XGM_VT* nv = (XGM_VT*) sgs_PushObjectIPA( C, sizeof(XGM_VT) * 4, xgm_vec2_iface );
	nv[ 0 ] = x;
	nv[ 1 ] = y;
	nv[ 2 ] = z;
	nv[ 3 ] = w;
}


int sgs_ParseVec2( SGS_CTX, int pos, XGM_VT* v2f, int strict )
{
	int ty = sgs_ItemType( C, pos );
	if( !strict && ( ty == SGS_VT_INT || ty == SGS_VT_REAL ) )
	{
		v2f[0] = v2f[1] = sgs_GetReal( C, pos );
		return 1;
	}
	if( sgs_ItemType( C, pos ) != SGS_VT_OBJECT )
		return 0;
	
	if( sgs_IsObject( C, pos, xgm_vec2_iface ) )
	{
		XGM_VT* hdr = (XGM_VT*) sgs_GetObjectData( C, pos );
		v2f[0] = hdr[0];
		v2f[1] = hdr[1];
		return 1;
	}
	return 0;
}

int sgs_ParseVec3( SGS_CTX, int pos, XGM_VT* v3f, int strict )
{
	int ty = sgs_ItemType( C, pos );
	if( !strict && ( ty == SGS_VT_INT || ty == SGS_VT_REAL ) )
	{
		v3f[0] = v3f[1] = v3f[2] = sgs_GetReal( C, pos );
		return 1;
	}
	if( sgs_ItemType( C, pos ) != SGS_VT_OBJECT )
		return 0;
	
	if( sgs_IsObject( C, pos, xgm_vec3_iface ) )
	{
		XGM_VT* hdr = (XGM_VT*) sgs_GetObjectData( C, pos );
		v3f[0] = hdr[0];
		v3f[1] = hdr[1];
		v3f[2] = hdr[2];
		return 1;
	}
	return 0;
}

int sgs_ParseVec4( SGS_CTX, int pos, XGM_VT* v4f, int strict )
{
	int ty = sgs_ItemType( C, pos );
	if( !strict && ( ty == SGS_VT_INT || ty == SGS_VT_REAL ) )
	{
		v4f[0] = v4f[1] = v4f[2] = v4f[3] = sgs_GetReal( C, pos );
		return 1;
	}
	if( sgs_ItemType( C, pos ) != SGS_VT_OBJECT )
		return 0;
	
	if( sgs_IsObject( C, pos, xgm_vec4_iface ) )
	{
		XGM_VT* hdr = (XGM_VT*) sgs_GetObjectData( C, pos );
		v4f[0] = hdr[0];
		v4f[1] = hdr[1];
		v4f[2] = hdr[2];
		v4f[3] = hdr[3];
		return 1;
	}
	return 0;
}


int sgs_ArgCheck_Vec2( SGS_CTX, int argid, va_list args, int flags )
{
	XGM_VT* out = NULL;
	XGM_VT v[2];
	if( !( flags & SGS_LOADARG_NOWRITE ) )
		out = va_arg( args, XGM_VT* );
	
	if( sgs_ParseVec2( C, argid, v, flags & SGS_LOADARG_STRICT ? 1 : 0 ) )
	{
		if( out )
		{
			out[ 0 ] = v[ 0 ];
			out[ 1 ] = v[ 1 ];
		}
		return 1;
	}
	if( flags & SGS_LOADARG_OPTIONAL )
		return 1;
	return sgs_ArgErrorExt( C, argid, 0, "vec2", "" );
}

int sgs_ArgCheck_Vec3( SGS_CTX, int argid, va_list args, int flags )
{
	XGM_VT* out = NULL;
	XGM_VT v[3];
	if( !( flags & SGS_LOADARG_NOWRITE ) )
		out = va_arg( args, XGM_VT* );
	
	if( sgs_ParseVec3( C, argid, v, flags & SGS_LOADARG_STRICT ? 1 : 0 ) )
	{
		if( out )
		{
			out[ 0 ] = v[ 0 ];
			out[ 1 ] = v[ 1 ];
			out[ 2 ] = v[ 2 ];
		}
		return 1;
	}
	if( flags & SGS_LOADARG_OPTIONAL )
		return 1;
	return sgs_ArgErrorExt( C, argid, 0, "vec3", "" );
}

int sgs_ArgCheck_Vec4( SGS_CTX, int argid, va_list args, int flags )
{
	XGM_VT* out = NULL;
	XGM_VT v[4];
	if( !( flags & SGS_LOADARG_NOWRITE ) )
		out = va_arg( args, XGM_VT* );
	
	if( sgs_ParseVec4( C, argid, v, flags & SGS_LOADARG_STRICT ? 1 : 0 ) )
	{
		if( out )
		{
			out[ 0 ] = v[ 0 ];
			out[ 1 ] = v[ 1 ];
			out[ 2 ] = v[ 2 ];
			out[ 3 ] = v[ 3 ];
		}
		return 1;
	}
	if( flags & SGS_LOADARG_OPTIONAL )
		return 1;
	return sgs_ArgErrorExt( C, argid, 0, "vec4", "" );
}


static sgs_RegFuncConst xgm_fconsts[] =
{
	{ "vec2", xgm_vec2 },
	{ "vec2_dot", xgm_vec2_dot },
	
	{ "vec3", xgm_vec3 },
	{ "vec3_dot", xgm_vec3_dot },
	
	{ "vec4", xgm_vec4 },
	{ "vec4_dot", xgm_vec4_dot },
};


#ifdef SGS_COMPILE_MODULE
#  define xgm_module_entry_point sgscript_main
#endif


#ifdef WIN32
__declspec(dllexport)
#endif
int xgm_module_entry_point( SGS_CTX )
{
	sgs_RegFuncConsts( C, xgm_fconsts, sizeof(xgm_fconsts) / sizeof(xgm_fconsts[0]) );
	return SGS_SUCCESS;
}
