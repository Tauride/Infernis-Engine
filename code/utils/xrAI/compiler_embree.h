#pragma once

// Intel Embree for xrAI's cover rays (after IX-Ray's xrLevelBuilder, xrAI/Compiler/compiler_embree). With -embree,
// the rays of the cover calculation (compiler_cover.cpp) are traced by Embree instead of OPCODE; the result follows
// GSC's rayTrace / getLastRP_Scale there: faces seen from behind are missed (CDB::OPT_CULL), faces whose shader casts
// no shadow are ignored, an opaque face blocks the ray, a transparent texel lets 1 - alpha of it through. Built only
// when CMake finds Embree (XRAI_EMBREE).

// Embree is in this build and the command line asks for it (-embree)
bool ai_embree_enabled();

// The scene of the level's ray-cast model (Level, g_rc_faces): after xrLoad; released with ai_embree_release
void ai_embree_build();
void ai_embree_release();
bool ai_embree_ready();

// The share of light left after the ray [P, P + D * range]: 0 when blocked, 1 when clear
float ai_embree_trace(const Fvector& P, const Fvector& D, float range);
