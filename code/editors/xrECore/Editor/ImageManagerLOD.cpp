#include "stdafx.h"
#include <xmmintrin.h>


#include "ImageManager.h"
#include "EditObject.h"
#include "EditMesh.h"
#include "ui_main.h"
#include "xrEngine/xrHemisphere.h"

using Fvector4Vec = xr_vector<Fvector4>;
using Fvector4It = Fvector4Vec::iterator;

// The texel the ray hit on a surface that renders: false when its pixels are missing
static bool GetPointColorOfSurface(CSurface* surf, SPickQuery::SResult* R, u32& alpha, u32& color)
{
    const Fvector2*			cuv[3];
    R->e_mesh->GetFaceTC	(R->tag,cuv);

    // barycentric coords
    // note: W,U,V order
    Fvector B;
    B.set	(1.0f - R->u - R->v,R->u,R->v);

    // calc UV
    Fvector2	uv;
    uv.x = cuv[0]->x*B.x + cuv[1]->x*B.y + cuv[2]->x*B.z;
    uv.y = cuv[0]->y*B.x + cuv[1]->y*B.y + cuv[2]->y*B.z;

    int U = iFloor(uv.x*float(surf->m_ImageData->w) + .5f);
    int V = iFloor(uv.y*float(surf->m_ImageData->h)+ .5f);

    if (surf->m_ImageData->w != 0)
    {
        U %= surf->m_ImageData->w;
        if (U < 0)
            U += surf->m_ImageData->w;
    }

    if (surf->m_ImageData->h != 0)
    {
        V %= surf->m_ImageData->h;
        if (V < 0)
            V += surf->m_ImageData->h;
    }


    auto& Layer = surf->m_ImageData->layers;

    if (!Layer.empty() && !Layer.back().empty())
    {
        color = Layer.back()[V * surf->m_ImageData->w + U];
        alpha = color_get_A(color);
    }
    else
    {
        Msg("! Error creating lod. Try again!");
        return false;
    }

    return true;
}

struct SBuildLight{
    Flight					light;
    float					energy;
};
using BLVec = xr_vector<SBuildLight>;
using BLIt = BLVec::iterator;

ICF static void  simple_hemi_callback(float x, float y, float z, float E, LPVOID P)
{
    BLVec* dst 					= (BLVec*)P;
    SBuildLight 				T;
    T.energy     				= E;
    T.light.direction.set		(x,y,z);
    dst->push_back  			(T);
}

const u32 lod_ss_quality  = 8;

void CreateLODSamples(const Fbox& bbox, U32Vec& tgt_data, u32 tgt_w, u32 tgt_h)
{
	U32Vec 	s_pixels,d_pixels;
    Fmatrix save_projection		= EDevice->mProject;
    Fmatrix save_view			= EDevice->mView;
    Fmatrix save_full			= EDevice->mFullTransform; // Infernis: the captures set it (and the frustum)

    // save render params
    Flags32 old_flag			= psDeviceFlags;
	u32 old_dwFillMode			= EDevice->dwFillMode;
    u32 old_dwShadeMode			= EDevice->dwShadeMode;
    // set render params

    u32 cc						= 	EPrefs->scene_clear_color;
    EPrefs->scene_clear_color 	= 	0x0000000;
    psDeviceFlags.zero			();
	psDeviceFlags.set			(rsFilterLinear,true);
	EDevice->dwFillMode			= D3DFILL_SOLID;
    EDevice->dwShadeMode			= D3DSHADE_GOURAUD;

    Fvector vP,vD,vN,vR;
    Fmatrix mV,mP;
    
    Fvector S;
    bbox.getradius				(S);
    float R 					= 2.f* std::max(S.x,S.z);

    u32 pitch 					= tgt_w*LOD_SAMPLE_COUNT;
    tgt_data.resize				(pitch*tgt_h);
    for (int frame=0; frame<LOD_SAMPLE_COUNT; frame++){
        float angle 			= frame*(PI_MUL_2/LOD_SAMPLE_COUNT);

		Fbox bb 				= bbox;
        // build camera matrix
        bb.getcenter			( vP );
        vN.set					( 0.f,1.f,0.f );
        vD.setHP				( angle, 0 );
        vR.crossproduct			( vN,vD );
        mV.build_camera_dir		(vP,vD,vN);
        bb.xform				(mV);
        // build project matrix
        mP.build_projection_ortho(R,bb.max.y-bb.min.y,bb.min.z,bb.max.z);
	    RCache.set_xform_project(mP);
	    RCache.set_xform_view	(mV);
        EDevice->mFullTransform.mul(mP,mV);
        EDevice->MakeScreenshot	(s_pixels,tgt_w*lod_ss_quality,tgt_h*lod_ss_quality);
        d_pixels.resize 		(tgt_w*tgt_h);
        DXTUtils::Filter::Process				(d_pixels.data(),tgt_w,tgt_h,s_pixels.data(),tgt_w*lod_ss_quality,tgt_h*lod_ss_quality, DXTUtils::Filter::imf_box);
        // copy LOD to final
		for (u32 y=0; y<tgt_h; y++)
    		CopyMemory			(tgt_data.data()+y*pitch+frame*tgt_w,d_pixels.data()+y*tgt_w,tgt_w*sizeof(u32));
	}

    // flip data
	for (u32 y=0; y<tgt_h/2; y++){
		u32 y2 = tgt_h-y-1;
		for (u32 x=0; x<pitch; x++){
        	std::swap	(tgt_data[y*pitch+x],tgt_data[y2*pitch+x]);	
		}
	}

    // restore render params
	EDevice->dwFillMode			= old_dwFillMode;
    EDevice->dwShadeMode			= old_dwShadeMode;
    psDeviceFlags 				= old_flag;
    EPrefs->scene_clear_color 	= cc;

    RCache.set_xform_view		(save_view);
    RCache.set_xform_project	(save_projection);
    EDevice->mFullTransform		= save_full;
    ::Render->ViewBase.CreateFromMatrix(save_full, FRUSTUM_P_ALL);
}

// Infernis: the surface's shader renders (CreateLODCapture looked it up: ShaderXRLC.Get scans the library)
static bool SurfaceRenders(const SLODBuild& build, const CSurface* surf)
{
    for (const auto& Entry : build.rendering)
        if (Entry.first == surf)
            return Entry.second;
    return false;
}

static bool GetPointColor(const SLODBuild& build, SPickQuery::SResult* R, u32& alpha, u32& color)
{
    CSurface* surf = R->e_mesh->GetSurfaceByFaceID(R->tag); VERIFY(surf);
    if (!SurfaceRenders(build, surf))
        return false;
    return GetPointColorOfSurface(surf, R, alpha, color);
}

void CImageManager::CreateLODCapture(SLODBuild& build)
{
    CEditableObject* OBJECT = build.object;
    build.lod_pixels.assign(LOD_IMAGE_SIZE * LOD_IMAGE_SIZE * LOD_SAMPLE_COUNT, 0);
    build.nm_pixels.assign(LOD_IMAGE_SIZE * LOD_IMAGE_SIZE * LOD_SAMPLE_COUNT, 0);
    build.rows_total = LOD_SAMPLE_COUNT * LOD_IMAGE_SIZE;
    build.rows_done = 0;
    build.fp_modes = _mm_getcsr();

    // the views (the GPU: this thread, between frames)
    CreateLODSamples(OBJECT->GetBox(), build.lod_pixels, LOD_IMAGE_SIZE, LOD_IMAGE_SIZE);

    // what the ray tracing reads, made here on one thread: the textures' pixels, the meshes' collision models, and
    // whether each surface renders
    build.rendering.clear();
    for (CSurface* surf : OBJECT->Surfaces())
    {
        Shader_xrLC* c_sh = EDevice->ShaderXRLC.Get(surf->_ShaderXRLCName());
        const bool renders = c_sh && c_sh->flags.bRendering;
        build.rendering.emplace_back(surf, renders);
        if (renders && nullptr == surf->m_ImageData)
            surf->CreateImageData();
    }
    OBJECT->PrepareCFModels();
}

void CImageManager::CreateLODCompute(SLODBuild& build)
{
    CEditableObject* OBJECT = build.object;
    CTimer Timer;
    Timer.Start();

    // build hemi light
    BLVec simple_hemi;
    xrHemisphereBuild(1, 2.f, simple_hemi_callback, &simple_hemi);

    Fbox bb = OBJECT->GetBox();
    U32Vec& lod_pixels = build.lod_pixels;
    U32Vec& nm_pixels = build.nm_pixels;
    U32Vec hemi_tmp(LOD_IMAGE_SIZE * LOD_IMAGE_SIZE * LOD_SAMPLE_COUNT, 0);
    Fvector o_center, o_size;
    bb.getradius(o_size);
    bb.getcenter(o_center);
    const float dW = std::max(o_size.x, o_size.z) / (LOD_IMAGE_SIZE / 2);
    const float dH = o_size.y / (LOD_IMAGE_SIZE / 2);
    const float dR = bb.getradius();
    const float d2R = dR * 2.f;

    const float LOD_CALC_SAMPLES = float(build.quality);
    const s32 LOD_CALC_SAMPLES_LIM = build.quality / 2;

    // Infernis: the rows of the eight views in parallel (each pixel writes only its own texels), each worker with its
    // own collider. IX-Ray and GSC traced them on one thread, with the editor waiting.
    // (every row with the main thread's floating point modes, taken by the capture: the pool's threads have their
    // own, and the rows are traced the same whichever thread takes them)
    const unsigned int CallerCSR = build.fp_modes;
    auto TraceRow = [&](int row)
    {
        if (build.cancel)
            return;
        _mm_setcsr(CallerCSR);
        thread_local CDB::COLLIDER Collider;
        Collider.ray_options(CDB::OPT_CULL);

        const u32 sample_idx = u32(row) / LOD_IMAGE_SIZE;
        const s32 iH = row % LOD_IMAGE_SIZE;
        Fmatrix M, Mi;
        const float angle = sample_idx * (PI_MUL_2 / LOD_SAMPLE_COUNT);
        M.setXYZ(0, angle, 0);
        M.translate_over(o_center);
        Mi.invert(M);

        const float Y = (iH - (LOD_IMAGE_SIZE - 1) / 2) * dH;
        FvectorVec n_vec;
        Fvector4Vec sample_pt_vec;
        SPickQuery PQ;
        for (s32 iW = 0; iW < LOD_IMAGE_SIZE; iW++)
        {
            const float X = (iW - (LOD_IMAGE_SIZE) / 2) * dW;

            const u32 pixel = (LOD_IMAGE_SIZE - iH - 1) * LOD_SAMPLE_COUNT * LOD_IMAGE_SIZE + LOD_IMAGE_SIZE * sample_idx + iW;
            u32& tgt_c = lod_pixels[pixel];
            u32& tgt_n = nm_pixels[pixel];
            u32& tgt_h = hemi_tmp[pixel];

            n_vec.clear();
            sample_pt_vec.clear();
            Fvector start;
            for (s32 iiH = -LOD_CALC_SAMPLES_LIM; iiH <= LOD_CALC_SAMPLES_LIM; iiH++)
            {
                const float dY = iiH * (dH / LOD_CALC_SAMPLES);
                for (s32 iiW = -LOD_CALC_SAMPLES_LIM; iiW <= LOD_CALC_SAMPLES_LIM; iiW++)
                {
                    const float dX = iiW * (dW / LOD_CALC_SAMPLES);
                    start.set(X + dX, Y + dY, 0);
                    M.transform_tiny(start);
                    start.mad(M.k, -dR);
                    PQ.prepare_rq(start, M.k, d2R, CDB::OPT_CULL);
                    OBJECT->RayQuery(PQ, Collider);

                    if (PQ.r_count())
                    {
                        PQ.r_sort();
                        Fvector N = { 0,0,0 };

                        for (s32 k = PQ.r_count() - 1; k >= 0; k--)
                        {
                            SPickQuery::SResult* R = PQ.r_begin() + k;
                            u32 uA, uC;
                            if (!GetPointColor(build, R, uA, uC)) continue;
                            const float fA = float(uA) / 255.f;
                            if (uA)
                            {
                                Fvector pt; pt.mad(PQ.m_Start, PQ.m_Direction, R->range - EPS_L);
                                Fvector4 ptt; ptt.set(pt.x, pt.y, pt.z, fA);
                                sample_pt_vec.push_back(ptt);
                            }
                            // normal
                            Fvector Nn;
                            Nn.mknormal(R->verts[0], R->verts[1], R->verts[2]);
                            Nn.mul(fA);

                            N.mul(1.f - fA);
                            N.add(Nn);
                        }

                        const float n_mag = N.magnitude();
                        if (!fis_zero(n_mag, EPS))
                            n_vec.push_back(N.div(n_mag));
                    }
                }
            }

            Fvector N = { 0,0,0 };
            if (!n_vec.empty())
            {
                for (const Fvector& n : n_vec) N.add(n);
                N.div(float(n_vec.size()));
                N.normalize_safe();
                Mi.transform_dir(N);
            }

            N.mul(0.5f);
            N.add(0.5f);
            N.mul(255.f);
            tgt_n = color_rgba(iFloor(N.x), iFloor(N.y), iFloor(N.z), color_get_A(tgt_c));

            if (0 == color_get_A(tgt_c))
                continue;

            // light points (far to near). Infernis: from the nearest opaque one: res_transp * (1 - 1) drops all the
            // points behind it, so their hemisphere rays changed nothing (the same result, without them)
            float res_transp = 0.f;
            size_t first_pt = 0;
            for (size_t i = sample_pt_vec.size(); i-- > 0;)
                if (sample_pt_vec[i].w >= 1.f)
                {
                    first_pt = i;
                    break;
                }
            for (size_t pt_idx = first_pt; pt_idx < sample_pt_vec.size(); pt_idx++)
            {
                const Fvector4& pt = sample_pt_vec[pt_idx];
                float avg_transp = 0.f;
                for (const SBuildLight& light : simple_hemi)
                {
                    Fvector hemi_start;
                    hemi_start.mad(Fvector().set(pt.x, pt.y, pt.z), light.light.direction, -dR);
                    PQ.prepare_rq(hemi_start, light.light.direction, dR, CDB::OPT_CULL);
                    OBJECT->RayQuery(PQ, Collider);
                    float ray_transp = 1.f;

                    for (s32 k = 0; k < PQ.r_count(); k++)
                    {
                        u32 a, uC;
                        if (!GetPointColor(build, PQ.r_begin() + k, a, uC)) continue;
                        ray_transp *= (1.f - float(a) / 255.f);
                        if (fis_zero(ray_transp, EPS_L)) break;
                    }
                    avg_transp += ray_transp;
                }
                avg_transp /= simple_hemi.size();
                res_transp = res_transp * (1.f - pt.w) + avg_transp * pt.w;
            }
            const u8 h = (u8)iFloor(res_transp * 255.f);
            tgt_h = color_rgba(h, h, h, color_get_A(tgt_c));
        }
        build.rows_done++;
    };
    concurrency::parallel_for(0, int(build.rows_total), TraceRow);
    if (build.cancel)
        return;

    ImageLib.ApplyBorders(lod_pixels, LOD_IMAGE_SIZE * LOD_SAMPLE_COUNT, LOD_IMAGE_SIZE);
    ImageLib.ApplyBorders(nm_pixels, LOD_IMAGE_SIZE * LOD_SAMPLE_COUNT, LOD_IMAGE_SIZE);
    ImageLib.ApplyBorders(hemi_tmp, LOD_IMAGE_SIZE * LOD_SAMPLE_COUNT, LOD_IMAGE_SIZE);

    // fill alpha to N-channel (HEMI)
    for (size_t px_idx = 0; px_idx < nm_pixels.size(); px_idx++)
        nm_pixels[px_idx] = subst_alpha(nm_pixels[px_idx], color_get_R(hemi_tmp[px_idx]));

    build.seconds = Timer.GetElapsed_sec();
}

void CImageManager::CreateLODTexture(CEditableObject* OBJECT, U32Vec& lod_pixels, U32Vec& nm_pixels, u32 tgt_w, u32 tgt_h, int _samples, int quality)
{
    SLODBuild build;
    build.object = OBJECT;
    build.quality = quality;
    CreateLODCapture(build);
    CreateLODCompute(build);
    Msg("LOD '%s': %.1f s", OBJECT->GetName(), build.seconds);
    lod_pixels.swap(build.lod_pixels);
    nm_pixels.swap(build.nm_pixels);
}

//------------------------------------------------------------------------------
//
//------------------------------------------------------------------------------
void CImageManager::CreateLODTexture(CEditableObject* OBJECT, const char* tex_name, u32 tgt_w, u32 tgt_h, int samples, time_t age, int quality)
{
    SLODBuild build;
    build.object = OBJECT;
    build.quality = quality;
    CreateLODCapture(build);
    CreateLODCompute(build);
    Msg("LOD '%s': %.1f s", OBJECT->GetName(), build.seconds);
    SaveLODTexture(build, tex_name, age);
}

void CImageManager::SaveLODTexture(const SLODBuild& build, const char* tex_name, time_t age)
{
    const u32 tgt_w = LOD_IMAGE_SIZE, tgt_h = LOD_IMAGE_SIZE, samples = LOD_SAMPLE_COUNT;
    U32Vec& lod_pixels = const_cast<U32Vec&>(build.lod_pixels); // (MakeTGA takes them by reference; it only reads)
    U32Vec& nm_pixels = const_cast<U32Vec&>(build.nm_pixels);

    string_path out_name, src_name;
    // save lod
    strcpy(src_name, tex_name);

    strcpy(src_name, EFS.ChangeFileExt(src_name, ".thm").c_str());
    FS.file_delete(_textures_, src_name);

    strcpy(src_name, EFS.ChangeFileExt(src_name, ".tga").c_str());
    FS.update_path(out_name, _textures_, src_name);
    DXTUtils::Converter::MakeTGA(tgt_w * samples, tgt_h, lod_pixels, out_name);
    FS.set_file_age(out_name, age);
    SynchronizeTexture(src_name, age);

    // save normal map
    xr_strconcat(src_name, tex_name, "_nm");
    strcpy(src_name, EFS.ChangeFileExt(src_name, ".thm").c_str());
    FS.file_delete(_textures_, src_name);

    strcpy(src_name, EFS.ChangeFileExt(src_name, ".tga").c_str());
    FS.update_path(out_name, _textures_, src_name);
    DXTUtils::Converter::MakeTGA(tgt_w * samples, tgt_h, nm_pixels, out_name);
    FS.set_file_age(out_name, age);
    SynchronizeTexture(src_name, age);
}
