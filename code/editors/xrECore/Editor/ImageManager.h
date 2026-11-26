//---------------------------------------------------------------------------
#ifndef ImageManagerH
#define ImageManagerH

class CEditableObject;
#include "EThumbnail.h"
struct SSimpleImage
{
	shared_str	name;
    using DATAVec = xr_vector<U32Vec>;
    using DATAIt = DATAVec::iterator;

	DATAVec		layers;
    u32			w,h,a;
    u32			tag;
    int 		LongestEdge()const 	{return (w>h)?w:h;}
    int			Area()const			{return w*h;}
};
IC bool operator == (const SSimpleImage& a, shared_str nm){return a.name==nm;}
IC bool operator < (const SSimpleImage& a, const SSimpleImage& b){return a.name<b.name;}

// Infernis: one object's LOD texture, made in steps so that the editor can keep drawing frames meanwhile: the views
// rendered on the main thread between frames (CreateLODCapture), the normals and the hemisphere ray-traced on any
// thread, in parallel inside (CreateLODCompute), then the textures saved on the main thread (SaveLODTexture).
struct ECORE_API SLODBuild
{
	CEditableObject*	object = nullptr;
	int					quality = 1;
	U32Vec				lod_pixels;
	U32Vec				nm_pixels;
	xr_vector<std::pair<const void*, bool>> rendering; // each surface's shader renders (looked up once)
	std::atomic<u32>	rows_done{0};	// progress of CreateLODCompute, out of rows_total
	u32					rows_total = 0;
	std::atomic<bool>	cancel{false};	// set by anyone: CreateLODCompute stops early (the result is then unusable)
	float				seconds = 0.f;
	unsigned int		fp_modes = 0;	// the main thread's SSE modes (MXCSR), for the tracing threads
};

using SSimpleImageVec = xr_vector<SSimpleImage>;
using SSimpleImageVecIt = SSimpleImageVec::iterator;

class ECORE_API CImageManager
{
  
public:
    bool		MakeGameTexture(ETextureThumbnail* THM, const char* game_name, u32* data);
    static void		MakeThumbnailImage	(ETextureThumbnail* THM, u32* data, u32 w, u32 h, u32 a);
public:
				CImageManager		(){;}
				~CImageManager		(){;}
	// texture routines
    void  	RemoveTexture	(UIItemListForm::Node& node);
    bool		CheckCompliance		(const char* fname, int& compl_);
    void		CheckCompliance		(FS_FileSet& files, FS_FileSet& compl_);
    int			GetTextures			(FS_FileSet& files, bool bFolder=false);
    int			GetTexturesRaw		(FS_FileSet& files, bool bFolder=false);
//	int			GetServerModifiedTextures(CLocatorAPI::files_query& files);
	int 		GetLocalNewTextures	(FS_FileSet& files);
	void		SafeCopyLocalToServer(FS_FileSet& files);

	void		SynchronizeTextures	(bool sync_thm, bool sync_game, bool bForceGame, FS_FileSet* source_map, AStringVec* sync_list_without_extention, FS_FileSet* modif_map=nullptr, bool bForceBaseAge=false);
    void 		SynchronizeTexture	(const char* tex_name, time_t age);
//	void		ChangeFileAgeTo		(FS_FileSet* source_map, int age);
	// make/update routines
    bool		MakeGameTexture		(const char* game_name, u32* data, const STextureParams& tp);
    void		CreateTextureThumbnail(ETextureThumbnail* THM, const xr_string& src_name, const char* path=nullptr, bool bSetDefParam=true);
    bool		CreateOBJThumbnail	(const char* tex_name, CEditableObject* obj, time_t age);
    void		CreateLODTexture	(CEditableObject* object, U32Vec& lod_pixels, U32Vec& nm_pixels, u32 tgt_w, u32 tgt_h, int samples, int quality);
    void		CreateLODTexture	(CEditableObject* object, const char* tex_name, 	u32 tgt_w, u32 tgt_h, int samples, time_t age, int quality);
    void		CreateLODCapture	(SLODBuild& build);	// main thread, outside a frame
    void		CreateLODCompute	(SLODBuild& build);	// any thread, after the capture
    void		SaveLODTexture		(const SLODBuild& build, const char* tex_name, time_t age); // main thread
    void		CreateGameTexture	(const char* src_name, ETextureThumbnail* thumb=nullptr);
    bool		LoadTextureData		(const char* src_name, U32Vec& data, u32& w, u32& h, time_t* age=nullptr);

    // result 0-can't fit images, 1-ok, -1 can't load image 
    void		MergedTextureRemapUV(float& dest_u, float& dest_v, float src_u, float src_v, const Fvector2& offs, const Fvector2& scale, bool bRotate);
    int			CreateMergedTexture	(const SharedStrVec& src_names, const char* dest_name, STextureParams::ETFormat fmt, int dest_width, int dest_height, Fvector2Vec& dest_offset, Fvector2Vec& dest_scale, boolVec& dest_rotate, U32Vec& remap);
    int			CreateMergedTexture	(const SharedStrVec& src_names, const char* dest_name, STextureParams::ETFormat fmt, int dest_width_min, int dest_width_max, int dest_height_min, int dest_height_max, Fvector2Vec& dest_offset, Fvector2Vec& dest_scale, boolVec& dest_rotate, U32Vec& remap);
    int			CreateMergedTexture	(u32 layer_cnt, SSimpleImageVec& src_images, SSimpleImage& dst_image, int dest_width, int dest_height, Fvector2Vec& dest_offset, Fvector2Vec& dest_scale, boolVec& dest_rotate, U32Vec& remap);
	int			CreateMergedTexture	(u32 layer_cnt, SSimpleImageVec& src_images, SSimpleImage& dst_image, int dest_width_min, int dest_width_max, int dest_height_min, int dest_height_max, Fvector2Vec& dest_offset, Fvector2Vec& dest_scale, boolVec& dest_rotate, U32Vec& remap);
	void 		ApplyBorders		(U32Vec& tgt_data, u32 w, u32 h);

    EImageThumbnail* CreateThumbnail(const char* src_name, ECustomThumbnail::THMType type, bool bLoad=true);

    void 		RefreshTextures		(AStringVec* modif);

    xr_string	UpdateFileName		(xr_string& fn);

    void		WriteAssociation	(CInifile* ltx_ini, const char* base_name, const STextureParams& fmt);

    bool		CreateSmallerCubeMap(const char* src_name, const char* dst_name);
};

extern ECORE_API CImageManager ImageLib;
//---------------------------------------------------------------------------
#endif
