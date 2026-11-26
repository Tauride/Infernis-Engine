// DetailManager.h: interface for the CDetailManager class.
#ifndef DetailManagerH
#define DetailManagerH
#pragma once

#include "xrCore/xrpool.h"
#include "detailformat.h"
#include "detailmodel.h"
#include <atomic>
#include <mutex>

#ifdef _EDITOR
constexpr int dm_max_decompress = 14;
class CCustomObject;
typedef u32 ObjClassID;

typedef xr_list<CCustomObject*> ObjectList;
typedef ObjectList::iterator ObjectIt;
typedef xr_map<ObjClassID, ObjectList> ObjectMap;
typedef ObjectMap::iterator ObjectPairIt;
#else
constexpr int dm_max_decompress = 7;
#endif

// Infernis (after IX-Ray): the grass radius is set at run time (r__detail_radius, metres); dm_size is in slots (2 m)
// around the camera, GSC's 24. The cache follows it on the next frame (CDetailManager::cache_Resize).
inline int dm_size = 24;
inline std::atomic<int> dm_requested_size{24}; // r__detail_radius (the console's thread; read by MT_CALC)
constexpr int dm_cache1_count = 4;
inline int dm_cache1_line = 24 * 2 / dm_cache1_count;
constexpr int dm_max_objects = 64;
constexpr int dm_obj_in_slot = 4;
inline int dm_cache_line = 24 + 1 + 24;
inline int dm_cache_size = (24 + 1 + 24) * (24 + 1 + 24);
inline float dm_fade = static_cast<float>(2 * 24) - 0.5f;
constexpr float dm_slot_size = DETAIL_SLOT_SIZE;
// The grass radius in metres for a size in slots and back (IX-Ray's CCC_DetailRadius: an even number of slots)
inline int dm_size_for_radius(int radius) { return (radius / 4) * 2; }

// (no export macro: the Level Editor compiles the detail manager itself, its editor part uses the editor's scene;
// ECORE_API is empty in the game's build)
class CDetailManager {
public:
    struct SlotItem { 
        float scale;
        float scale_calculated;
        Fmatrix mRotY;
        u32 vis_ID; 
        float c_hemi;
        float c_sun;
#if RENDER == R_R1
        Fvector c_rgb;
#endif
    };
    using SlotItemVec = xr_vector<SlotItem*>;
    
    struct SlotPart {           
        u32 id;                 
        SlotItemVec items;      
        SlotItemVec r_items[3]; 
    };
    
    enum SlotType {
        stReady = 0, 
        stPending,   

        stFORCEDWORD = 0xffffffff
    };
    
    struct Slot { 
        struct {
            u32 empty : 1;
            u32 type : 1;
            u32 frame : 30;
        };
        int sx, sz;                 
        vis_data vis;               
        SlotPart G[dm_obj_in_slot]; 

        Slot() {
            frame = 0;
            empty = 1;
            type = stReady;
            sx = sz = 0;
            vis.clear();
        }
    };
    
    struct CacheSlot1 {
        u32 empty;
        vis_data vis;
        Slot** slots[dm_cache1_count * dm_cache1_count];
        CacheSlot1() {
            empty = 1;
            vis.clear();
        }
    };

    typedef xr_vector<xr_vector<SlotItemVec*>> vis_list;
    typedef svector<CDetail*, dm_max_objects> DetailVec;
    typedef DetailVec::iterator DetailIt;
    typedef poolSS<SlotItem, 4096> PSS;

public:
    int dither[16][16];

public:
    struct SSwingValue {
        float rot1;
        float rot2;
        float amp1;
        float amp2;
        float speed;
        void lerp(const SSwingValue& v1, const SSwingValue& v2, float factor);
    };
    SSwingValue swing_desc[2];
    SSwingValue swing_current;
    float m_time_rot_1;
    float m_time_rot_2;
    float m_time_pos;
    float m_global_time_old;

public:
    IReader* dtFS;
    DetailHeader dtH;
    DetailSlot* dtSlots; 
    DetailSlot DS_empty;

public:
    DetailVec objects;
    vis_list m_visibles[3]; 

#ifndef _EDITOR
    xrXRC xrc;
#endif
    // Sized by dm_size (cache_Resize): dm_cache1_line^2 groups of 4x4 slots, dm_cache_line^2 slots (rows of
    // dm_cache_line, z first), and as many in the pool
    xr_vector<CacheSlot1> cache_level1;
    xr_vector<Slot*> cache;
    xr_vector<Slot*> cache_task;
    xr_vector<Slot> cache_pool;
    int cache_cx;
    int cache_cz;
    Slot*& cache_at(int z, int x) { return cache[z * dm_cache_line + x]; }
    CacheSlot1& cache_level1_at(int z, int x) { return cache_level1[z * dm_cache1_line + x]; }
    void cache_Resize(); // to dm_requested_size: drops every unpacked slot and the visibility lists

    PSS poolSI; 

    void UpdateVisibleM();
    void UpdateVisibleS();

public:
#ifdef _EDITOR
    virtual ObjectList* GetSnapList() = 0;
#endif

#ifdef _EDITOR
    // The editors compile details\set as a fixed-function pass (CBlender_Detail_Still, bEditor): it has no constants
    // for the vertex shader path (hw_Load_Shaders), so they draw details with the software path
    [[nodiscard]] IC bool UseVS() const { return false; }
#else
    [[nodiscard]] IC bool UseVS() const { return HW.Caps.geometry_major >= 1; }
#endif

    ref_geom soft_Geom;
    void soft_Load();
    void soft_Unload();
    void soft_Render();

    ref_geom hw_Geom;
    u32 hw_BatchSize;
    ID3DVertexBuffer* hw_VB;
    ID3DIndexBuffer* hw_IB;
    ref_constant hwc_consts;
    ref_constant hwc_wave;
    ref_constant hwc_wind;
    ref_constant hwc_array;
    ref_constant hwc_s_consts;
    ref_constant hwc_s_xform;
    ref_constant hwc_s_array;
    
    void hw_Load();
    void hw_Load_Geom();
    void hw_Load_Shaders();
    void hw_Unload();
    void hw_Render();
#if defined(USE_DX11)
    // Instanced grass (after IX-Ray's D3D11 detail renderer; r4_detail_instancing): every visible item of a call
    // packed into one buffer, drawn with one instanced call per model and variant; the vertex shaders read the item
    // by SV_InstanceID (deffer_detail_s_flat.vs, deffer_detail_w_flat.vs, DETAIL_INSTANCING)
    struct HWInstance {
        float x, y, z, scale; // position, scale (the matrix's second row)
        float rot_x, rot_z;   // the scaled rotation about Y: _11 and _31 of the item's matrix, times its scale
        float hemi, sun;
    };
    struct HWRange {
        u32 first, count;
    };
    ID3D11Buffer* hw_Instances = nullptr;
    ID3D11ShaderResourceView* hw_InstancesView = nullptr;
    u32 hw_InstancesSize = 0; // in instances
    u32 hw_InstancesFrame = u32(-1), hw_InstancesCount = 0; // the frame filled, and its items
    xr_vector<HWRange> hw_Ranges[3];
    void hw_Instances_Reserve(u32 count);
    void hw_Instances_Release();
    u32 hw_Instances_Fill(); // packs m_visibles into the buffer; the number of instances
    void hw_Render_instanced(const Fvector4& consts, const Fvector4& wave, const Fvector4& wind, u32 var_id,
                             u32 lod_id);
    void hw_Render_dump(const Fvector4& consts, const Fvector4& wave, const Fvector4& wind, u32 var_id, u32 lod_id);
#elif defined(USE_DX10)
    void hw_Render_dump(const Fvector4& consts, const Fvector4& wave, const Fvector4& wind, u32 var_id, u32 lod_id);
#else 
    void hw_Render_dump(ref_constant array, u32 var_id, u32 lod_id, u32 c_base);
#endif 

public:
    DetailSlot& QueryDB(int sx, int sz);

    void cache_Initialize();
    void cache_Update(int sx, int sz, Fvector& view, int limit);
    void cache_Task(int gx, int gz, Slot* D);
    Slot* cache_Query(int sx, int sz);
    void cache_Decompress(Slot* D);
    BOOL cache_Validate();
    
    int cg2w_X(int x) { return cache_cx - dm_size + x; }
    int cg2w_Z(int z) { return cache_cz - dm_size + (dm_cache_line - 1 - z); }
    int w2cg_X(int x) { return x - cache_cx + dm_size; }
    int w2cg_Z(int z) { return cache_cz - dm_size + (dm_cache_line - 1 - z); }

    void Load();
    void Unload();
    void Render();

    // MT stuff
    std::recursive_mutex MT;
    std::atomic<u32> m_frame_calc{0};
    std::atomic<u32> m_frame_rendered{0};

    void __stdcall MT_CALC();
    ICF void MT_SYNC() {
        if (m_frame_calc.load(std::memory_order_acquire) == RDEVICE.dwFrame)
            return;
        MT_CALC();
    }

    CDetailManager();
    virtual ~CDetailManager();
};

#endif // DetailManagerH