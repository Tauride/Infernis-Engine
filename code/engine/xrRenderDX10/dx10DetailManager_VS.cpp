#include "stdafx.h"
#include "xrRenderCommon/DetailManager.h"

#include "xrEngine/igame_persistent.h"
#include "xrEngine/environment.h"

#include "../xrRenderDX10/dx10BufferUtils.h"

const int quant = 16384;
const int c_hdr = 10;
const int c_size = 4;

static D3DVERTEXELEMENT9 dwDecl[] = {
    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },  // pos
    { 0, 12, D3DDECLTYPE_SHORT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 }, // uv
    D3DDECL_END()
};

#pragma pack(push, 1)
struct vertHW {
    float x, y, z;
    short u, v, t, mid;
};
#pragma pack(pop)

short QC(float v);
//{
//	int t=iFloor(v*float(quant)); clamp(t,-32768,32767);
//	return short(t&0xffff);
//}

void CDetailManager::hw_Load_Shaders() {
    // Create shader to access constant storage
    ref_shader S;
    S.create("details\\set");
    R_constant_table& T0 = *(S->E[0]->passes[0]->constants);
    R_constant_table& T1 = *(S->E[1]->passes[0]->constants);
    hwc_consts = T0.get("consts");
    hwc_wave = T0.get("wave");
    hwc_wind = T0.get("dir2D");
    hwc_array = T0.get("array");
    hwc_s_consts = T1.get("consts");
    hwc_s_xform = T1.get("xform");
    hwc_s_array = T1.get("array");
}

void CDetailManager::hw_Render() {
    // Render-prepare
    //	Update timer
    //	Can't use Device.fTimeDelta since it is smoothed! Don't know why, but smoothed value looks
    //more choppy!
    float fDelta = Device.fTimeGlobal - m_global_time_old;
    if ((fDelta < 0) || (fDelta > 1))
        fDelta = 0.03;
    m_global_time_old = Device.fTimeGlobal;

    m_time_rot_1 += (PI_MUL_2 * fDelta / swing_current.rot1);
    m_time_rot_2 += (PI_MUL_2 * fDelta / swing_current.rot2);
    m_time_pos += fDelta * swing_current.speed;

    // float		tm_rot1		= (PI_MUL_2*Device.fTimeGlobal/swing_current.rot1);
    // float		tm_rot2		= (PI_MUL_2*Device.fTimeGlobal/swing_current.rot2);
    float tm_rot1 = m_time_rot_1;
    float tm_rot2 = m_time_rot_2;

    Fvector4 dir1, dir2;
    dir1.set(std::sin(tm_rot1), 0, std::cos(tm_rot1), 0).normalize().mul(swing_current.amp1);
    dir2.set(std::sin(tm_rot2), 0, std::cos(tm_rot2), 0).normalize().mul(swing_current.amp2);

    // Infernis (after IX-Ray's grass shadows): in the sun's shadow map (r2_sun_details) the waving grass is drawn with
    // its shadow element (depth only, alpha-tested); the still grass casts no shadow
    const bool shadow = RImplementation.phase == CRender::PHASE_SMAP;
    const u32 lod_wave = shadow ? SE_R2_SHADOW : 0;

#ifdef USE_DX11
    // Every visible item of this frame into the instance buffer, once for the three variants and every pass
    const bool instanced = RImplementation.o.detail_instancing;
    if (instanced) {
        Device.Statistic->RenderDUMP_DT_Count = hw_Instances_Fill();
        if (!Device.Statistic->RenderDUMP_DT_Count)
            return;
    }
    const auto dump = [&](const Fvector4& c, const Fvector4& w, const Fvector4& d, u32 var_id, u32 lod_id) {
        if (instanced)
            hw_Render_instanced(c, w, d, var_id, lod_id);
        else
            hw_Render_dump(c, w, d, var_id, lod_id);
    };
#else
    const auto dump = [&](const Fvector4& c, const Fvector4& w, const Fvector4& d, u32 var_id, u32 lod_id) {
        hw_Render_dump(c, w, d, var_id, lod_id);
    };
#endif

    // Setup geometry and DMA
    RCache.set_Geometry(hw_Geom);

    // Wave0
    float scale = 1.f / float(quant);
    Fvector4 wave;
    Fvector4 consts;
    consts.set(scale, scale, ps_r__Detail_l_aniso, ps_r__Detail_l_ambient);
    // wave.set				(1.f/5.f,		1.f/7.f,	1.f/3.f,
    // Device.fTimeGlobal*swing_current.speed);
    wave.set(1.f / 5.f, 1.f / 7.f, 1.f / 3.f, m_time_pos);
    // RCache.set_c			(&*hwc_consts,	scale,		scale,		ps_r__Detail_l_aniso,	ps_r__Detail_l_ambient);
    // // consts  RCache.set_c			(&*hwc_wave,	wave.div(PI_MUL_2));	// wave
    // RCache.set_c			(&*hwc_wind,	dir1);																					//
    // wind-dir  hw_Render_dump			(&*hwc_array,	1, 0, c_hdr );
    dump(consts, wave.div(PI_MUL_2), dir1, 1, lod_wave);

    // Wave1
    // wave.set				(1.f/3.f,		1.f/7.f,	1.f/5.f,
    // Device.fTimeGlobal*swing_current.speed);
    wave.set(1.f / 3.f, 1.f / 7.f, 1.f / 5.f, m_time_pos);
    // RCache.set_c			(&*hwc_wave,	wave.div(PI_MUL_2));	// wave
    // RCache.set_c			(&*hwc_wind,	dir2);																					//
    // wind-dir  hw_Render_dump			(&*hwc_array,	2, 0, c_hdr );
    dump(consts, wave.div(PI_MUL_2), dir2, 2, lod_wave);

    // Still
    consts.set(scale, scale, scale, 1.f);
    // RCache.set_c			(&*hwc_s_consts,scale,		scale,		scale,
    // 1.f);  RCache.set_c			(&*hwc_s_xform,	Device.mFullTransform);
    // hw_Render_dump			(&*hwc_s_array,	0, 1, c_hdr );
    if (!shadow)
        dump(consts, wave.div(PI_MUL_2), dir2, 0, 1);

#ifdef USE_DX11
    if (instanced)
        SRVSManager.SetVSResource(0, nullptr); // the next shaders' slot, and the buffer may be recreated
#endif
}

#ifdef USE_DX11
void CDetailManager::hw_Instances_Reserve(u32 count) {
    if (count <= hw_InstancesSize)
        return;
    hw_Instances_Release();
    u32 size = 4096;
    while (size < count)
        size *= 2;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = size * sizeof(HWInstance);
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    R_CHK(HW.pDevice->CreateBuffer(&desc, nullptr, &hw_Instances));

    // A typed buffer of float4 (two per instance): vertex shaders read it on any D3D11 feature level
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    view.Buffer.FirstElement = 0;
    view.Buffer.NumElements = size * (sizeof(HWInstance) / sizeof(Fvector4));
    R_CHK(HW.pDevice->CreateShaderResourceView(hw_Instances, &view, &hw_InstancesView));
    hw_InstancesSize = size;
}

void CDetailManager::hw_Instances_Release() {
    SRVSManager.SetVSResource(0, nullptr);
    _RELEASE(hw_InstancesView);
    _RELEASE(hw_Instances);
    hw_InstancesSize = 0;
}

u32 CDetailManager::hw_Instances_Fill() {
    static_assert(sizeof(HWInstance) == 2 * sizeof(Fvector4));
    // The main view, the scope lens and the sun's cascades draw the same items in a frame
    if (hw_InstancesFrame == Device.dwFrame)
        return hw_InstancesCount;
    hw_InstancesFrame = Device.dwFrame;
    hw_InstancesCount = 0;
    u32 total = 0;
    for (u32 var_id = 0; var_id < 3; var_id++) {
        hw_Ranges[var_id].assign(objects.size(), HWRange{0, 0});
        for (const xr_vector<SlotItemVec*>& vis : m_visibles[var_id])
            for (const SlotItemVec* items : vis)
                total += u32(items->size());
    }
    if (!total)
        return 0;

    hw_Instances_Reserve(total);
    D3D11_MAPPED_SUBRESOURCE map;
    R_CHK(HW.pContext->Map(hw_Instances, 0, D3D11_MAP_WRITE_DISCARD, 0, &map));
    HWInstance* dest = static_cast<HWInstance*>(map.pData);
    u32 count = 0;
    for (u32 var_id = 0; var_id < 3; var_id++) {
        vis_list& list = m_visibles[var_id];
        for (u32 O = 0; O < objects.size() && O < list.size(); O++) {
            HWRange& range = hw_Ranges[var_id][O];
            range.first = count;
            for (const SlotItemVec* items : list[O]) {
                for (const SlotItem* Instance : *items) {
                    // The item's matrix is a rotation about Y and a position (cache_Decompress)
                    const Fmatrix& M = Instance->mRotY;
                    const float scale = Instance->scale_calculated;
                    dest[count++] = {M._41, M._42, M._43, scale, M._11 * scale, M._31 * scale, Instance->c_hemi,
                                     Instance->c_sun};
                }
            }
            range.count = count - range.first;
        }
    }
    HW.pContext->Unmap(hw_Instances, 0);
    hw_InstancesCount = count;
    return count;
}

void CDetailManager::hw_Render_instanced(const Fvector4& consts, const Fvector4& wave, const Fvector4& wind,
                                         u32 var_id, u32 lod_id) {
    static shared_str strConsts("consts");
    static shared_str strWave("wave");
    static shared_str strDir2D("dir2D");
    static shared_str strXForm("xform");
    static shared_str strInstanceBase("instance_base");

    // Each model's first copy in the geometry (vertices and indices of one item)
    u32 vOffset = 0;
    u32 iOffset = 0;
    for (u32 O = 0; O < objects.size(); O++) {
        CDetail& Object = *objects[O];
        const HWRange& range = hw_Ranges[var_id][O];
        if (range.count && Object.shader->E[lod_id]) {
            for (u32 iPass = 0; iPass < Object.shader->E[lod_id]->passes.size(); ++iPass) {
                RCache.set_Element(Object.shader->E[lod_id], iPass);
                if (RImplementation.phase != CRender::PHASE_SMAP)
                    RImplementation.apply_lmaterial();

                RCache.set_c(strConsts, consts);
                RCache.set_c(strWave, wave);
                RCache.set_c(strDir2D, wind);
                RCache.set_c(strXForm, Device.mFullTransform);
                RCache.set_c(strInstanceBase, Fvector4().set(float(range.first), 0.f, 0.f, 0.f));
                SRVSManager.SetVSResource(0, hw_InstancesView);

                RCache.RenderInstanced(vOffset, Object.number_vertices, iOffset, Object.number_indices / 3,
                                       range.count);
                RCache.stat.r.s_details.add(range.count * Object.number_vertices);
            }
        }
        vOffset += hw_BatchSize * Object.number_vertices;
        iOffset += hw_BatchSize * Object.number_indices;
    }
}
#endif // USE_DX11

// GSC's batches (R3, and R4 with r4_detail_instancing off): up to hw_BatchSize items per draw, in the constant array

void CDetailManager::hw_Render_dump(const Fvector4& consts, const Fvector4& wave,
                                    const Fvector4& wind, u32 var_id, u32 lod_id) {
    static shared_str strConsts("consts");
    static shared_str strWave("wave");
    static shared_str strDir2D("dir2D");
    static shared_str strArray("array");
    static shared_str strXForm("xform");

    Device.Statistic->RenderDUMP_DT_Count = 0;

    // Matrices and offsets
    u32 vOffset = 0;
    u32 iOffset = 0;

    vis_list& list = m_visibles[var_id];

    CEnvDescriptor& desc = *g_pGamePersistent->Environment().CurrentEnv;
    Fvector c_sun, c_ambient, c_hemi;
    c_sun.set(desc.sun_color.x, desc.sun_color.y, desc.sun_color.z);
    c_sun.mul(.5f);
    c_ambient.set(desc.ambient.x, desc.ambient.y, desc.ambient.z);
    c_hemi.set(desc.hemi_color.x, desc.hemi_color.y, desc.hemi_color.z);

    // Iterate
    for (u32 O = 0; O < objects.size(); O++) {
        CDetail& Object = *objects[O];
        xr_vector<SlotItemVec*>& vis = list[O];
        if (!vis.empty() && Object.shader->E[lod_id]) {
            for (u32 iPass = 0; iPass < Object.shader->E[lod_id]->passes.size(); ++iPass) {
                // Setup matrices + colors (and flush it as necessary)
                // RCache.set_Element				(Object.shader->E[lod_id]);
                RCache.set_Element(Object.shader->E[lod_id], iPass);
                if (RImplementation.phase != CRender::PHASE_SMAP)
                    RImplementation.apply_lmaterial();

                //	This could be cached in the corresponding consatant buffer
                //	as it is done for DX9
                RCache.set_c(strConsts, consts);
                RCache.set_c(strWave, wave);
                RCache.set_c(strDir2D, wind);
                RCache.set_c(strXForm, Device.mFullTransform);

                // ref_constant constArray = RCache.get_c(strArray);
                // VERIFY(constArray);

                // u32			c_base				= x_array->vs.index;
                // Fvector4*	c_storage			=
                // RCache.get_ConstantCache_Vertex().get_array_f().access(c_base);
                Fvector4* c_storage = 0;
                //	Map constants to memory directly
                {
                    void* pVData = nullptr;
                    RCache.get_ConstantDirect(strArray, hw_BatchSize * sizeof(Fvector4) * 4,
                                              &pVData, 0, 0);
                    c_storage = (Fvector4*)pVData;
                }
                // Infernis: a grass shader without the constant array (one built for instancing, or a stale copy)
                // leaves nothing to write the items to: no grass instead of a crash
                if (!c_storage) {
                    static bool reported = false;
                    if (!reported) {
                        Msg("! [DETAILS] the grass shader has no 'array' constant: grass is not drawn");
                        reported = true;
                    }
                    continue;
                }

                u32 dwBatch = 0;

                xr_vector<SlotItemVec*>::iterator _vI = vis.begin();
                xr_vector<SlotItemVec*>::iterator _vE = vis.end();
                for (; _vI != _vE; _vI++) {
                    SlotItemVec* items = *_vI;
                    auto _iI = items->begin();
                    auto _iE = items->end();
                    for (; _iI != _iE; _iI++) {
                        SlotItem& Instance = **_iI;
                        u32 base = dwBatch * 4;

                        // Build matrix ( 3x4 matrix, last row - color )
                        float scale = Instance.scale_calculated;
                        Fmatrix& M = Instance.mRotY;
                        c_storage[base + 0].set(M._11 * scale, M._21 * scale, M._31 * scale, M._41);
                        c_storage[base + 1].set(M._12 * scale, M._22 * scale, M._32 * scale, M._42);
                        c_storage[base + 2].set(M._13 * scale, M._23 * scale, M._33 * scale, M._43);
                        // RCache.set_ca(&*constArray, base+0, M._11*scale,	M._21*scale,
                        // M._31*scale,	M._41	);  RCache.set_ca(&*constArray, base+1, M._12*scale,
                        // M._22*scale,	M._32*scale,	M._42	);  RCache.set_ca(&*constArray,
                        // base+2, M._13*scale,	M._23*scale,	M._33*scale,	M._43	);

                        // Build color
                        // R2 only needs hemisphere
                        float h = Instance.c_hemi;
                        float s = Instance.c_sun;
                        c_storage[base + 3].set(s, s, s, h);
                        // RCache.set_ca(&*constArray, base+3, s,				s,
                        // s,				h		);
                        dwBatch++;
                        if (dwBatch == hw_BatchSize) {
                            // flush
                            Device.Statistic->RenderDUMP_DT_Count += dwBatch;
                            u32 dwCNT_verts = dwBatch * Object.number_vertices;
                            u32 dwCNT_prims = (dwBatch * Object.number_indices) / 3;
                            // RCache.get_ConstantCache_Vertex().b_dirty				=
                            // TRUE;  RCache.get_ConstantCache_Vertex().get_array_f().dirty
                            // (c_base,c_base+dwBatch*4);
                            RCache.Render(D3DPT_TRIANGLELIST, vOffset, 0, dwCNT_verts, iOffset,
                                          dwCNT_prims);
                            RCache.stat.r.s_details.add(dwCNT_verts);

                            // restart
                            dwBatch = 0;

                            //	Remap constants to memory directly (just in case anything goes
                            //wrong)
                            {
                                void* pVData;
                                RCache.get_ConstantDirect(
                                    strArray, hw_BatchSize * sizeof(Fvector4) * 4, &pVData, 0, 0);
                                c_storage = (Fvector4*)pVData;
                            }
                            VERIFY(c_storage);
                        }
                    }
                }
                // flush if nessecary
                if (dwBatch) {
                    Device.Statistic->RenderDUMP_DT_Count += dwBatch;
                    u32 dwCNT_verts = dwBatch * Object.number_vertices;
                    u32 dwCNT_prims = (dwBatch * Object.number_indices) / 3;
                    // RCache.get_ConstantCache_Vertex().b_dirty				=
                    // TRUE;  RCache.get_ConstantCache_Vertex().get_array_f().dirty
                    // (c_base,c_base+dwBatch*4);
                    RCache.Render(D3DPT_TRIANGLELIST, vOffset, 0, dwCNT_verts, iOffset,
                                  dwCNT_prims);
                    RCache.stat.r.s_details.add(dwCNT_verts);
                }
            }
            // (the lists stay for the scope lens and the sun's cascades; UpdateVisibleM empties them for the next frame)
        }
        vOffset += hw_BatchSize * Object.number_vertices;
        iOffset += hw_BatchSize * Object.number_indices;
    }
}
