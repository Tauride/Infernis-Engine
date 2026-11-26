#pragma once

// Intel Embree for xrLC's light rays (after IX-Ray's xrLevelBuilder, Light/Embree). With xrLC's -embree, the shadow
// rays against the level's ray-cast model are traced by Embree instead of OPCODE (CDB::COLLIDER); the result follows
// GSC's getLastRP_Scale: the face being lit and faces that cast no shadow are ignored, an opaque face blocks the
// light, a transparent texel lets 1 - alpha^2 of it through. Built only when CMake finds Embree (XRLC_EMBREE).

namespace CDB {
class MODEL;
}
class base_Face;

// Embree is in this build and the command line asks for it (-embree)
XRLC_LIGHT_API bool lc_embree_enabled();

// The scene of the level's ray-cast model: built when the model is created, released with it
void lc_embree_build(const CDB::MODEL* model);
// xrDO_Light's scene: gl_data's ray-cast model (from build.cform) with its faces (g_rc_faces); released with
// lc_embree_release
void lc_embree_build_details();
void lc_embree_release();

// The scene answers for this model
bool lc_embree_traces(const CDB::MODEL* model);

// The light left after the ray [P, P + D * range]: 0 when blocked, 1 when clear, between for transparent texels.
// When an opaque face blocks it, its triangle goes to blocker (the light's cached occluder, as GSC's).
// skip: the face being lit (xrLC), or null. cull: as OPCODE's CDB::OPT_CULL, faces seen from behind are ignored
// (xrDO_Light's rays).
float lc_embree_occlusion(const Fvector& P, const Fvector& D, float range, const base_Face* skip, Fvector blocker[3],
                          bool cull = false);
