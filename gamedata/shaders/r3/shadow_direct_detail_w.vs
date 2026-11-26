// Infernis (after IX-Ray's grass shadows): the waving grass in the sun's shadow map (r2_sun_details), element
// SE_R2_SHADOW of CBlender_Detail_Still; made from deffer_detail_w_flat.vs
#include "common.h"

uniform float4 		consts; // {1/quant,1/quant,diffusescale,ambient}
uniform float4 		wave; 	// cx,cy,cz,tm
uniform float4 		dir2D; 
#ifdef DETAIL_INSTANCING
// Infernis (after IX-Ray's instanced grass): R4 (r4_detail_instancing) draws each grass model with one instanced call; the items are in a
// buffer of two float4 each (CDetailManager::HWInstance): position and scale, then the scaled rotation about Y
// (_11 and _31 of the item's matrix), hemisphere and sun. instance_base.x: this call's first item.
Buffer<float4>		detail_instances : register(t0);
uniform float4 		instance_base;

v2p_shadow_direct_aref 	main (v_detail v, uint instance : SV_InstanceID)
{
	v2p_shadow_direct_aref 		O;
	uint 	i 	= (uint(instance_base.x) + instance) * 2;
	float4 	a 	= detail_instances[i+0];
	float4 	b 	= detail_instances[i+1];
	float4  m0 	= float4(b.x, 0, b.y, a.x);
	float4  m1 	= float4(0, a.w, 0, a.y);
	float4  m2 	= float4(-b.y, 0, b.x, a.z);
	float4  c0 	= float4(b.w, b.w, b.w, b.z);
#else
//uniform float4 		array	[200] : register(c12);
//tbuffer DetailsData
//{
	uniform float4 		array[61*4];
//}

v2p_shadow_direct_aref 	main (v_detail v)
{
	v2p_shadow_direct_aref 		O;
	// index
	int 	i 	= v.misc.w;
	float4  m0 	= array[i+0];
	float4  m1 	= array[i+1];
	float4  m2 	= array[i+2];
	float4  c0 	= array[i+3];
#endif

	// Transform pos to world coords
	float4 	pos;
 	pos.x 		= dot	(m0, v.pos);
 	pos.y 		= dot	(m1, v.pos);
 	pos.z 		= dot	(m2, v.pos);
	pos.w 		= 1;

	// 
	float 	base 	= m1.w;
	float 	dp	= calc_cyclic   (dot(pos,wave));
	float 	H 	= pos.y - base;			// height of vertex (scaled)
	float 	frac 	= v.misc.z*consts.x;		// fractional
	float 	inten 	= H * dp;
	float2 	result	= calc_xz_wave	(dir2D.xz*inten,frac);
	pos		= float4(pos.x+result.x, pos.y, pos.z+result.y, 1);

	// Final out: the shadow map's position and the base texture's coordinates (alpha test)
	O.hpos 		= mul		(m_WVP,	pos				);
	O.tc0 		= (v.misc * consts).xy;

	return O;
}
FXVS;
