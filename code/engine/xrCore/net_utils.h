#pragma once

#include "client_id.h"

static constexpr const size_t NET_PacketSizeLimit = 16 * 1024;

// GSC's ini stream (Infernis: compiled with XR_NET_PACKET_INISTREAM, i.e. in builds with the editors). With it set, a
// packet's reads and writes also go to, or come from, a text form: the Level Editor keeps server entities in its
// scene files (.level, .part) that way, as GSC's SDK did. Builds without the editors keep the plain packet.
#ifdef XR_NET_PACKET_INISTREAM
struct XRCORE_API IIniFileStream {
    virtual void __stdcall move_begin() = 0;

    virtual void __stdcall w_float(float a) = 0;
    virtual void __stdcall w_vec3(const Fvector& a) = 0;
    virtual void __stdcall w_vec4(const Fvector4& a) = 0;
    virtual void __stdcall w_u64(u64 a) = 0;
    virtual void __stdcall w_s64(s64 a) = 0;
    virtual void __stdcall w_u32(u32 a) = 0;
    virtual void __stdcall w_s32(s32 a) = 0;
    virtual void __stdcall w_u16(u16 a) = 0;
    virtual void __stdcall w_s16(s16 a) = 0;
    virtual void __stdcall w_u8(u8 a) = 0;
    virtual void __stdcall w_s8(s8 a) = 0;
    virtual void __stdcall w_stringZ(const char* S) = 0;

    virtual void __stdcall r_vec3(Fvector&) = 0;
    virtual void __stdcall r_vec4(Fvector4&) = 0;
    virtual void __stdcall r_float(float&) = 0;
    virtual void __stdcall r_u8(u8&) = 0;
    virtual void __stdcall r_u16(u16&) = 0;
    virtual void __stdcall r_u32(u32&) = 0;
    virtual void __stdcall r_u64(u64&) = 0;
    virtual void __stdcall r_s8(s8&) = 0;
    virtual void __stdcall r_s16(s16&) = 0;
    virtual void __stdcall r_s32(s32&) = 0;
    virtual void __stdcall r_s64(s64&) = 0;

    virtual void __stdcall r_string(LPSTR dest, u32 dest_size) = 0;
    virtual void __stdcall skip_stringZ() = 0;
};
#define NET_INI_W(what_to_do)                                                                       \
    if (inistream)                                                                                 \
        inistream->what_to_do;
#define NET_INI_R(binary_read, ini_read)                                                            \
    if (inistream)                                                                                 \
        inistream->ini_read;                                                                       \
    else                                                                                           \
        binary_read;
#else
#define NET_INI_W(what_to_do)
#define NET_INI_R(binary_read, ini_read) binary_read;
#endif

struct NET_Buffer {
    BYTE data[NET_PacketSizeLimit];
    u32 count;
};

class XRCORE_API NET_Packet {
public:
#ifdef XR_NET_PACKET_INISTREAM
    IIniFileStream* inistream = nullptr;
#endif
    // The editors' scene files (.part, LTX) carry spawn data as ini keys: raw blocks (r/w) and the state size do not
    // go through the ini stream. Always false outside the editors' build.
    bool has_inistream() const {
#ifdef XR_NET_PACKET_INISTREAM
        return inistream != nullptr;
#else
        return false;
#endif
    }
    NET_Buffer B;
    u32 r_pos;
    u32 timeReceive;

    NET_Packet() 
        : B(),
          r_pos(0),
          timeReceive(0)
    {}

    NET_Packet(const void* data, const size_t size) : NET_Packet() {
        set(data, size);
    }

    void set(const void* data, const size_t size) {
        memcpy(B.data, data, size);
        B.count = size;
    }

    // writing - main
    void write_start() {
        B.count = 0;
        NET_INI_W(move_begin());
    }

    void w_begin(u16 type) {
        B.count = 0;
        w_u16(type);
    }

    struct W_guard {
        bool* guarded;
        W_guard(bool* b) : guarded(b) { *b = true; }
        ~W_guard() { *guarded = false; }
    };

    void w(const void* p, const size_t count) {
        VERIFY(p && count);
        VERIFY(B.count + count < NET_PacketSizeLimit);
        std::memcpy(&B.data[B.count], p, count);
        B.count += count;
        VERIFY(B.count < NET_PacketSizeLimit);
    }

    void w_seek(const u32 pos, const void* p, const u32 count);
    u32 w_tell() const { return B.count; }

    // writing - utilities
    void w_float(const float a) {
        w(&a, sizeof(float));
        NET_INI_W(w_float(a));
    } // float

    void w_vec3(const Fvector& a) {
        w(&a, 3 * sizeof(float));
        NET_INI_W(w_vec3(a));
    } // vec3

    void w_vec4(const Fvector4& a) {
        w(&a, 4 * sizeof(float));
        NET_INI_W(w_vec4(a));
    } // vec4

    void w_u64(const u64 a) {
        w(&a, sizeof(u64));
        NET_INI_W(w_u64(a));
    } // qword (8b)

    void w_s64(const s64 a) {
        w(&a, sizeof(s64));
        NET_INI_W(w_s64(a));
    } // qword (8b)

    void w_u32(const u32 a) {
        w(&a, sizeof(u32));
        NET_INI_W(w_u32(a));
    } // dword (4b)

    void w_s32(const s32 a) {
        w(&a, sizeof(s32));
        NET_INI_W(w_s32(a));
    } // dword (4b)

    void w_u16(const u16 a) {
        w(&a, sizeof(u16));
        NET_INI_W(w_u16(a));
    } // word (2b)

    void w_s16(const s16 a) {
        w(&a, sizeof(s16));
        NET_INI_W(w_s16(a));
    } // word (2b)

    void w_u8(const u8 a) {
        w(&a, sizeof(u8));
        NET_INI_W(w_u8(a));
    } // byte (1b)

    void w_s8(const s8 a) {
        w(&a, sizeof(s8));
        NET_INI_W(w_s8(a));
    } // byte (1b)

    void w_float_q16(const float a, const float min, const float max) {
        VERIFY(a >= min && a <= max);
        const float q = (a - min) / (max - min);
        w_u16(u16(iFloor(q * 65535.f + 0.5f)));
    }

    void w_float_q8(const float a, const float min, const float max) {
        VERIFY(a >= min && a <= max);
        const float q = (a - min) / (max - min);
        w_u8(u8(iFloor(q * 255.f + 0.5f)));
    }

    void w_angle16(const float a) { w_float_q16(angle_normalize(a), 0.f, PI_MUL_2); }
    void w_angle8(const float a) { w_float_q8(angle_normalize(a), 0.f, PI_MUL_2); }
    void w_dir(const Fvector& D) { w_u16(pvCompress(D)); }
    void w_sdir(const Fvector& D) {
        Fvector C;
        float mag = D.magnitude();
        if (mag > EPS_S) {
            C.div(D, mag);
        } else {
            C.set(0, 0, 1);
            mag = 0;
        }
        w_dir(C);
        w_float(mag);
    }

    void w_stringZ(const char* S) {
        w(S, xr_strlen(S) + 1);
        NET_INI_W(w_stringZ(S));
    }

    void w_stringZ(const std::string_view str) {
        w(str.data(), str.size());
#ifdef XR_NET_PACKET_INISTREAM
        // the terminating zero is part of the string: not a separate value in the text form
        const u8 zero = 0;
        w(&zero, 1);
        if (inistream) {
            const std::string text(str);
            inistream->w_stringZ(text.c_str());
        }
#else
        w_u8(0);
#endif
    }

    void w_stringZ(const shared_str& p) {
        if (*p)
            w(*p, p.size() + 1);
        else {
            const u8 zero = 0; // GSC: written without the ini stream, the string itself goes there below
            w(&zero, 1);
        }
        NET_INI_W(w_stringZ(p.c_str() ? p.c_str() : ""));
    }

    void w_matrix(const Fmatrix& M) {
        w_vec3(M.i);
        w_vec3(M.j);
        w_vec3(M.k);
        w_vec3(M.c);
    }

    void w_clientID(ClientID C) { w_u32(C.value()); }

    void w_chunk_open8(u32& position) {
        position = w_tell();
        w_u8(0);
    }

    void w_chunk_close8(const u32 position) {
        const u32 size = u32(w_tell() - position) - sizeof(u8);
        VERIFY(size < 256);
        const u8 _size = static_cast<u8>(size);
        w_seek(position, &_size, sizeof(_size));
    }

    void w_chunk_open16(u32& position) {
        position = w_tell();
        w_u16(0);
    }

    void w_chunk_close16(const u32 position) {
        const u32 size = u32(w_tell() - position) - sizeof(u16);
        VERIFY(size < 65536);
        const u16 _size = static_cast<u16>(size);
        w_seek(position, &_size, sizeof(_size));
    }

    // reading
    void read_start() {
        r_pos = 0;
        NET_INI_W(move_begin());
    }

    u32 r_begin(u16& type);
    void r_seek(const u32 pos);
    u32 r_tell() const {
        return r_pos;
    }

    void r(void* p, const size_t count) {
        VERIFY(p && count);
        std::memcpy(p, &B.data[r_pos], count);
        r_pos += count;
        VERIFY(r_pos <= B.count);
    }

    bool r_eof() const {
        return r_pos >= B.count;
    }

    u32 r_elapsed() const {
        return B.count - r_pos;
    }

    void r_advance(const u32 size);

    // reading - utilities
    void r_vec3(Fvector& A) {
        NET_INI_R(r(&A, sizeof(Fvector)), r_vec3(A))
    }

    void r_vec4(Fvector4& A) {
        NET_INI_R(r(&A, sizeof(Fvector4)), r_vec4(A))
    }

    void r_float(float& A) {
        NET_INI_R(r(&A, sizeof(float)), r_float(A))
    }

    void r_u64(u64& A) {
        NET_INI_R(r(&A, sizeof(u64)), r_u64(A))
    }

    void r_s64(s64& A) {
        NET_INI_R(r(&A, sizeof(s64)), r_s64(A))
    }

    void r_u32(u32& A) {
        NET_INI_R(r(&A, sizeof(u32)), r_u32(A))
    }

    void r_s32(s32& A) {
        NET_INI_R(r(&A, sizeof(s32)), r_s32(A))
    }

    void r_u16(u16& A) {
        NET_INI_R(r(&A, sizeof(u16)), r_u16(A))
    }

    void r_s16(s16& A) {
        NET_INI_R(r(&A, sizeof(s16)), r_s16(A))
    }

    void r_u8(u8& A) {
        NET_INI_R(r(&A, sizeof(u8)), r_u8(A))
    }

    void r_s8(s8& A) {
        NET_INI_R(r(&A, sizeof(s8)), r_s8(A))
    }

    // IReader compatibility
    Fvector r_vec3();
    Fvector4 r_vec4();
    float r_float_q8(const float min, const float max);
    float r_float_q16(const float min, const float max);
    float r_float();
    u64 r_u64();
    s64 r_s64();
    u32 r_u32();
    s32 r_s32();
    u16 r_u16();
    s16 r_s16();
    u8 r_u8();
    s8 r_s8();

    void r_float_q16(float& A, const float min, const float max);
    void r_float_q8(float& A, const float min, const float max);
    void r_angle16(float& A);
    void r_angle8(float& A);
    void r_dir(Fvector& A);

    void r_sdir(Fvector& A);
    void r_stringZ(char* S);
    void r_stringZ(xr_string& dest);
    void r_stringZ(shared_str& dest);

    void skip_stringZ();

    void r_stringZ_s(char* string, const size_t size);

    template <size_t size>
    void r_stringZ_s(char (&string)[size]) {
        r_stringZ_s(string, size);
    }

    void r_matrix(Fmatrix& M);
    void r_clientID(ClientID& C);
};
