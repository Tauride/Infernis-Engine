#include "common.h"

uniform float4 		consts; // {1/quant,1/quant,diffusescale,ambient}
#ifdef DETAIL_INSTANCING
// Infernis (after IX-Ray's instanced grass): R4 (r4_detail_instancing) draws each grass model with one instanced call; the items are in a
// buffer of two float4 each (CDetailManager::HWInstance): position and scale, then the scaled rotation about Y
// (_11 and _31 of the item's matrix), hemisphere and sun. instance_base.x: this call's first item.
Buffer<float4>		detail_instances : register(t0);
uniform float4 		instance_base;

v2p_flat 	main (v_detail v, uint instance : SV_InstanceID)
{
	v2p_flat 		O;
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

v2p_flat 	main (v_detail v)
{
	v2p_flat 		O;
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

	// Normal in world coords
	float3 	norm;	
		norm.x 	= pos.x - m0.w	;
		norm.y 	= pos.y - m1.w	+ .75f;	// avoid zero
		norm.z	= pos.z - m2.w	;

	// Final out
	float4	Pp 	= mul		(m_WVP,	pos				);
	O.hpos 		= Pp;
	O.N 		= mul		(m_WV,  normalize(norm)	);
	float3	Pe	= mul		(m_WV,  pos				);
	O.tcdh 		= float4	((v.misc * consts).xyyy	);

# if defined(USE_R2_STATIC_SUN)
	O.tcdh.w	= c0.x;								// (,,,dir-occlusion)
# endif

	O.position	= float4	(Pe, 		c0.w		);

	return O;
}
FXVS;
