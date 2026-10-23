#pragma once

// A minimal D3D9 vocabulary: enums and constants only, no COM, no interfaces.
// Angelica3D code keeps compiling against these names while the implementation lives in GLES.
// Values match the Direct3D 9 headers, which matters for cached state and saved data.

#include <cstdint>

namespace pwx::d3d {

// --- formats ---------------------------------------------------------------
using D3DFORMAT = uint32_t;
constexpr D3DFORMAT D3DFMT_UNKNOWN = 0;
constexpr D3DFORMAT D3DFMT_A8R8G8B8 = 21;
constexpr D3DFORMAT D3DFMT_X8R8G8B8 = 22;
constexpr D3DFORMAT D3DFMT_R5G6B5 = 23;
constexpr D3DFORMAT D3DFMT_A4R4G4B4 = 26;
constexpr D3DFORMAT D3DFMT_A1R5G5B5 = 25;
constexpr D3DFORMAT D3DFMT_DXT1 = 0x31545844;
constexpr D3DFORMAT D3DFMT_DXT3 = 0x33545844;
constexpr D3DFORMAT D3DFMT_DXT5 = 0x35545844;
constexpr D3DFORMAT D3DFMT_D16 = 80;
constexpr D3DFORMAT D3DFMT_D24S8 = 75;

// --- primitive types -------------------------------------------------------
enum D3DPRIMITIVETYPE : uint32_t {
    D3DPT_POINTLIST = 1,
    D3DPT_LINELIST = 2,
    D3DPT_LINESTRIP = 3,
    D3DPT_TRIANGLELIST = 4,
    D3DPT_TRIANGLESTRIP = 5,
    D3DPT_TRIANGLEFAN = 6,
};

// --- render states (subset actually used by the device) --------------------
enum D3DRENDERSTATETYPE : uint32_t {
    D3DRS_ZENABLE = 7,
    D3DRS_FILLMODE = 8,
    D3DRS_SHADEMODE = 9,
    D3DRS_ZWRITEENABLE = 14,
    D3DRS_ALPHATESTENABLE = 15,
    D3DRS_SRCBLEND = 19,
    D3DRS_DESTBLEND = 20,
    D3DRS_CULLMODE = 22,
    D3DRS_ZFUNC = 23,
    D3DRS_ALPHAREF = 24,
    D3DRS_ALPHAFUNC = 25,
    D3DRS_DITHERENABLE = 26,
    D3DRS_ALPHABLENDENABLE = 27,
    D3DRS_FOGENABLE = 28,
    D3DRS_SPECULARENABLE = 29,
    D3DRS_STENCILENABLE = 52,
    D3DRS_STENCILFAIL = 53,
    D3DRS_STENCILZFAIL = 54,
    D3DRS_STENCILPASS = 55,
    D3DRS_STENCILFUNC = 56,
    D3DRS_STENCILREF = 57,
    D3DRS_STENCILMASK = 58,
    D3DRS_STENCILWRITEMASK = 59,
    D3DRS_TEXTUREFACTOR = 60,
    D3DRS_LIGHTING = 137,
    D3DRS_AMBIENT = 139,
    D3DRS_COLORWRITEENABLE = 190,
    D3DRS_BLENDOP = 171,
    D3DRS_SCISSORTESTENABLE = 174,
    D3DRS_SLOPESCALEDEPTHBIAS = 175,
    D3DRS_DEPTHBIAS = 195,
};

enum D3DCULL : uint32_t {
    D3DCULL_NONE = 1,
    D3DCULL_CW = 2,
    D3DCULL_CCW = 3,
};

enum D3DBLEND : uint32_t {
    D3DBLEND_ZERO = 1,
    D3DBLEND_ONE = 2,
    D3DBLEND_SRCCOLOR = 3,
    D3DBLEND_INVSRCCOLOR = 4,
    D3DBLEND_SRCALPHA = 5,
    D3DBLEND_INVSRCALPHA = 6,
    D3DBLEND_DESTALPHA = 7,
    D3DBLEND_INVDESTALPHA = 8,
    D3DBLEND_DESTCOLOR = 9,
    D3DBLEND_INVDESTCOLOR = 10,
    D3DBLEND_SRCALPHASAT = 11,
};

enum D3DBLENDOP : uint32_t {
    D3DBLENDOP_ADD = 1,
    D3DBLENDOP_SUBTRACT = 2,
    D3DBLENDOP_REVSUBTRACT = 3,
    D3DBLENDOP_MIN = 4,
    D3DBLENDOP_MAX = 5,
};

enum D3DCMPFUNC : uint32_t {
    D3DCMP_NEVER = 1,
    D3DCMP_LESS = 2,
    D3DCMP_EQUAL = 3,
    D3DCMP_LESSEQUAL = 4,
    D3DCMP_GREATER = 5,
    D3DCMP_NOTEQUAL = 6,
    D3DCMP_GREATEREQUAL = 7,
    D3DCMP_ALWAYS = 8,
};

enum D3DTRANSFORMSTATETYPE : uint32_t {
    D3DTS_VIEW = 2,
    D3DTS_PROJECTION = 3,
    D3DTS_TEXTURE0 = 16,
    D3DTS_WORLD = 256,
};

enum D3DSAMPLERSTATETYPE : uint32_t {
    D3DSAMP_ADDRESSU = 1,
    D3DSAMP_ADDRESSV = 2,
    D3DSAMP_ADDRESSW = 3,
    D3DSAMP_BORDERCOLOR = 4,
    D3DSAMP_MAGFILTER = 5,
    D3DSAMP_MINFILTER = 6,
    D3DSAMP_MIPFILTER = 7,
    D3DSAMP_MIPMAPLODBIAS = 8,
    D3DSAMP_MAXMIPLEVEL = 9,
    D3DSAMP_MAXANISOTROPY = 10,
};

enum D3DTEXTUREADDRESS : uint32_t {
    D3DTADDRESS_WRAP = 1,
    D3DTADDRESS_MIRROR = 2,
    D3DTADDRESS_CLAMP = 3,
    D3DTADDRESS_BORDER = 4,
    D3DTADDRESS_MIRRORONCE = 5,
};

enum D3DTEXTUREFILTERTYPE : uint32_t {
    D3DTEXF_NONE = 0,
    D3DTEXF_POINT = 1,
    D3DTEXF_LINEAR = 2,
    D3DTEXF_ANISOTROPIC = 3,
    D3DTEXF_PYRAMIDALQUAD = 6,
    D3DTEXF_GAUSSIANQUAD = 7,
};

// --- flexible vertex format bits ------------------------------------------
constexpr uint32_t D3DFVF_POSITION_MASK = 0x000E;
constexpr uint32_t D3DFVF_XYZ = 0x0002;
constexpr uint32_t D3DFVF_XYZRHW = 0x0004;
constexpr uint32_t D3DFVF_NORMAL = 0x0010;
constexpr uint32_t D3DFVF_PSIZE = 0x0020;
constexpr uint32_t D3DFVF_DIFFUSE = 0x0040;
constexpr uint32_t D3DFVF_SPECULAR = 0x0080;
constexpr uint32_t D3DFVF_TEXCOUNT_MASK = 0x0F00;
constexpr uint32_t D3DFVF_TEX1 = 0x0100;
constexpr uint32_t D3DFVF_TEX2 = 0x0200;

}  // namespace pwx::d3d
