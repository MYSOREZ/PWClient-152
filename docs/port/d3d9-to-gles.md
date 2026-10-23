# D3D9 → OpenGL ES 3.0 port spec (v0)

Status: planning. The Android shell (SDL3 + GLES 3.0 triangle) already runs; the engine is not
integrated yet. This document fixes the seam, the mapping and the traps *before* code moves, so
the port does not drift into "fix 1,559 call sites one by one".

## 1. Facts about the existing client

Measured by grep over `PWClient152/`:

| Fact | Value |
|---|---|
| Direct3D 9 call sites | 1,559 (Source 1,225 / Header 334 / CElementEditor 141 / ElementClient 78) |
| Device seam | `Engine/Angelica3D/Header/A3DDevice.h` — 641 lines, ~260 methods |
| Shaders | HLSL strings in `Source/A3DHLSLDataSrc.cpp` (44 KB) and `A3DHLSL.cpp` (57 KB), profiles `vs_*` / `ps_*` |
| Effects framework | not used — only `IDirect3DVertexShader9` / `IDirect3DPixelShader9` |
| Render targets / surfaces | 105 `IDirect3DSurface9`, 18 `IDirect3DSwapChain9` |
| Texture format set | DXT1/3/5, A8R8G8B8, A16B16G16R16F, R32F |

## 2. Strategy: keep the A3D surface, replace the D3D one

Two options were considered.

1. **Fake COM (`d3d9gles` shim)**: implement `IDirect3DDevice9` and friends so the untouched engine
   calls a look-alike. Cheap in call-site edits, expensive in COM bookkeeping (refcounts, 260-method
   vtables, `IUnknown`, device-lost semantics) and it keeps D3D9 mental model in the codebase.
2. **Reimplement the A3D layer (chosen)**: keep `A3DDevice`, `A3DStream`, `A3DTexture`,
   `A3DRenderTarget`, `A3DVertexShader`, `A3DPixelShader`, `A3DViewport`, and reimplement their
   bodies on GLES 3.0. Replace `<d3d9.h>` with our own `pwx_d3dtypes.h` that keeps the D3D *enums
   and structs* (`D3DFORMAT`, `D3DRENDERSTATETYPE`, `D3DMATRIX`, ...) so game-side code and data
   files keep compiling, but no COM object is ever created.

Why option 2: the A3D layer is ~30 files, while the game code (`EC_*`) consumes A3D, not D3D. The
two hot leaks of raw D3D into game code are `ElementClient` (78 calls) and `GfxCommon2`/`CCommon`
(88) — small enough to patch individually.

The D3D header is also the reason `A3DDevice::GetD3DDevice()` (line 263) exists. It must be replaced
by an A3D-native accessor; every caller of `GetD3DDevice()` is a porting task of its own — measured
at 169 call sites, 91 inside `Angelica3D` and **78 outside the seam** (the `ElementClient` group).

## 3. Mapping table

| D3D9 | GLES 3.0 | Notes |
|---|---|---|
| `SetTransform(WORLD/VIEW/PROJ)` | CPU matrices → shader uniforms | Angelica already tracks the three matrices; keep them CPU-side and push a single MVP |
| `SetRenderState(ZENABLE/ZWRITEENABLE/ZFUNC)` | `glEnable(GL_DEPTH_TEST)`, `glDepthMask`, `glDepthFunc` | D3D default is `ZENABLE=TRUE`, `ZWRITEENABLE=TRUE`, `ZFUNC=LESSEQUAL` |
| `SetRenderState(CULLMODE)` | `glEnable(GL_CULL_FACE)` + `glCullFace` + `glFrontFace` | D3D default `CULLMODE=CCW`; verify winding, it flips with Y |
| `SetRenderState(SRCBLEND/DESTBLEND/ALPHABLENDENABLE)` | `glBlendFunc` / `glEnable(GL_BLEND)` | D3D defaults `ONE`/`ZERO`; `ALPHABLENDENABLE` defaults to FALSE |
| `SetRenderState(ALPHATESTENABLE/ALPHAFUNC/ALPHAREF)` | `discard` in fragment shader | fixed-function alpha test is gone in ES 3.0 |
| `SetRenderState(COLORWRITEENABLE)` | `glColorMask` | 4-bit mask, not a boolean |
| `SetRenderState(TEXTUREFACTOR)` | uniform `uTextureFactor` | D3D packs BGRA |
| `SetTexture(stage)` | `glActiveTexture` + `glBindTexture` | 8 stages (`MAX_TEX_LAYERS`) |
| `SetSamplerState(MINFILTER/MAGFILTER/MIPFILTER)` | `glTexParameteri` MIN/MAG filter | D3D has three flags, GL one enum; derive both from the pair |
| `SetSamplerState(ADDRESSU/V/W)` | `glTexParameteri` WRAP_S/T | D3D `WRAP` == GL `REPEAT` (not `CLAMP`) |
| `SetTextureStageState(COLOROP/ARGS)` | generated shader or prebaked shader variant | **biggest unknown** — see §5 |
| `SetFVF` / `SetVertexDeclaration` | VAO + `glVertexAttribPointer` | FVF must be decoded into an attribute layout; cache VAOs per (FVF, stream) |
| `SetStreamSource` / `SetIndices` | VBO/EBO bindings | `D3DUSAGE_DYNAMIC` → `GL_STREAM_DRAW` + orphan or `GL_MAP_UNSYNCHRONIZED_BIT` |
| `DrawIndexedPrimitive` | `glDrawElements` | `BaseVertexIndex` needs `glDrawElementsBaseVertex` (ES 3.2) or a rebased index buffer |
| `DrawPrimitiveUP` | transient VBO write + draw | avoid `glBufferData` per call: ring buffer |
| `CreateRenderTarget` + `SetRenderTarget` | FBO + texture/renderbuffer | 105 surface call sites; depth as `DEPTH24_STENCIL8` |
| `StretchRect` / `UpdateSurface` | `glBlitFramebuffer` | format restrictions apply (no depth→color, no MSAA resolve mismatch) |
| `CopyToBack` | `glBlitFramebuffer` to default FBO | Y origin differs: FBO (0,0) is bottom-left, D3D surface top-left |
| `Lock/UnlockVertexBuffer` | `glMapBufferRange` / `glFlushMappedBufferRange` | lock flags carry usage intent; see §5 |
| `Clear` | `glClearColor` + `glClear` (+ scissor for rect clears) | |
| `SetViewport` | `glViewport` with **Y flipped** | `glViewport(x, height - y - h, w, h)` |
| `SetScissorRect` | `glScissor` with Y flipped | same flip as viewport |
| Occlusion query | `GL_ANY_SAMPLES_PASSED` query | `A3DOccQueryImpl` |
| Clip planes | `gl_ClipDistance[]` in vertex shader | ES 3.0 has no `glClipPlane` |
| Point sprites | `gl_PointSize` + `gl_PointCoord` | D3D point size is screen-space pixels |
| Indexed vertex blending | skinning in vertex shader | `SMRD_INDEXEDVERTMATRIX` path |
| `SetGammaLevel` | not available | gamma via `EGL_EXT_...`/shader, or drop and log once |
| Device lost / reset | EGL context loss / surface recreation | `Reset()` becomes "recreate GL objects"; all `A3DDevObject`s re-create |

## 4. Rasterisation conventions (the bug factory)

Taken from the GeneralsZH GLES lessons and re-checked against this engine:

1. **Half-pixel bias.** D3D9 maps a vertex at `+0.5` to a pixel centre; GL maps `0.0` to the pixel
   edge. 2D code in this client contains deliberate `-0.5` offsets. Port them as-is first, then
   remove per draw path with a visible A/B — never in bulk.
2. **Viewport Y.** D3D viewport origin is top-left, GL bottom-left. Flip in `SetViewport`/
   `SetScissor`, not per draw.
3. **Render-target origin.** `glCopyTexImage`/`glBlitFramebuffer` read bottom-up. UI composed into
   an FBO and then blitted will look vertically mirrored if this is missed.
4. **Texture addressing.** `D3DTADDRESS_WRAP` is `GL_REPEAT`; `D3DTADDRESS_CLAMP` is `GL_CLAMP_TO_EDGE`.
   A default of `CLAMP_TO_EDGE` everywhere hides tiling bugs until the terrain.
5. **State defaults.** Zero is a value, not "unset". Initialise every cached state to the
   documented D3D default (see table above) and flush the full state at frame start once, then
   only deltas.
6. **Unsigned state values.** `DWORD 0x80808080` cast to `GLint` becomes negative and silently
   clamps. Cast through `uint32_t` and mask per state width.
7. **FVF / vertex layout.** D3D `D3DFVF_XYZ | D3DFVF_DIFFUSE` has an implicit stride and channel
   order; decode it, do not assume `vec3 + ubyte4`.
8. **Index base.** `D3DINDEXEDPRIMITIVE` with non-zero `BaseVertexIndex` is common in model
   rendering; ES 3.0 has no `glDrawElementsBaseVertex` (3.2 does). Rebase indices on the CPU or
   request a 3.2 context (the device reports 3.2 anyway).

## 5. Open questions to resolve before writing the renderer

1. **Texture stage states.** Are `SetTextureColorOP/ARGS` still meaningful in this client, or does
   every visible path use HLSL pixel shaders? If they are live, we need either generated GLSL from
   the stage machine or a small set of prebaked variants. Audit: instrument `A3DDevice.cpp` on
   Windows and log which paths run during a normal login.

   Call-site counts (grep, engine / outside `Angelica3D`):

   | Method | Total | Engine | Outside |
   |---|---|---|---|
   | `SetTextureColorOP` | 204 | 132 | 72 |
   | `SetTextureAlphaOP` | 89 | 58 | 31 |
   | `SetTextureColorArgs` | 59 | 40 | 19 |
   | `SetTextureStageState` | 29 | 17 | 12 |
   | `SetSamplerState` | 92 | 74 | 18 |
   | `SetFVF` | 62 | 46 | 16 |

   The calls exist and are numerous, so they cannot simply be dropped — but a call existing does not
   prove the state reaches a draw. The audit still has to run.
2. **Shader translation.** How many distinct HLSL programs actually get compiled at runtime
   (`CreateVertexShader` is called 48 times in source, but the DB may hold more)? Decide between
   (a) a one-time offline HLSL→GLSL translation committed as GLSL, or (b) a runtime translator.
   Recommendation: offline, checked into `shaders/glsl/`, with the HLSL kept for reference.
3. **Buffer usage.** Which streams are written every frame? Everything dynamic goes to a ring
   buffer; anything else is static. `glBufferData` per lock is a stall generator on Adreno.
4. **Device lost.** Android destroys the EGL context on backgrounding more aggressively than D3D9
   ever did. `A3DDevice::Reset()` and every `A3DDevObject` must survive it — this is a correctness
   requirement, not an optimisation.
5. **`GetD3DDevice()` callers.** Each one is raw D3D outside the seam and needs an A3D replacement
   before the header swap can land.

## 6. Bring-up order

1. `pwx_d3dtypes.h`: D3D enums/structs, no COM. (compile-only)
2. `A3DDevice` on SDL/GLES: init, clear, present, state cache, viewport, matrices.
3. `A3DStream` (vertex/index buffers) + FVF decoding → first triangle from engine data.
4. `A3DTexture` (DDS/DXT decode → GL) → textured quad.
5. `A3DRenderTarget` (FBO) → UI composited to target, then blit to back buffer.
6. `A3DVertexShader`/`A3DPixelShader` with the offline GLSL set → first lit model.
7. Only then: terrain, skinning, particles, water, effects.

## 7. Logging and verification rules

- Every unimplemented or approximated call logs **once** with its D3D name and arguments:
  `W/GL: D3DRS_SEPARATEALPHABLENDENABLE unsupported, approximating`.
- Each APK carries `BUILD_STAMP`; the first log line repeats it. A symptom report without a stamp
  is treated as "unknown build".
- One A/B per build. A failure with an exact measurement ("exactly 70%") is data, not noise.
