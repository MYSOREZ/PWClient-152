# Texture stage state audit (P1.5, part 1)

Status: **complete (static analysis)**. Companion to `d3d9-to-gles.md` §5 open question 1.
Method: mechanical extraction of every `SetTexture*OP/Args`, `D3DTSS_*`, `SetTextureCoordIndex`,
`SetTextureMatrix`, `SetTextureTransformFlags`, `SetTextureBump*` call site over
`Engine\Angelica3D`, `ElementCore`, `Common`, then per-file reading of the top renderers.
All findings below are callers in live code paths unless explicitly marked legacy.

## 1. The `m_bHWIDevice` trap — resolved before it could mislead us

Every A3D texture-stage wrapper in `A3DDevice.cpp` begins with
`if( m_bHWIDevice ) return true;`. That looked like "stage states are dead on modern configs".
They are not: `m_bHWIDevice = true` happens **only** in `InitAsHWI()`, which `Init()` switches to
only when `g_pA3DConfig->GetRunEnv() == A3DRUNENV_PURESERVER` (headless pure-server mode).
The normal client goes through `Init()` and runs with `m_bHWIDevice == false`, so every stage API
forwards to `raw_SetTextureStageState` / `raw_SetTransform` on the real D3D device.
(The same guards also no-op `Clear`, `Present`-adjacent query logic and sampler setters — all
headless-server semantics, irrelevant for the port.)

**Conclusion: the fixed-function stage machine is the client's real render path. No shortcut.**

## 2. Fixed-function vs shaders

| Evidence | Value |
|---|---|
| Direct `SetVertexShader` / `SetPixelShader` call sites (engine + client) | ~23 |
| `A3DHLSL` shader instances | serialized blobs loaded from data packs (`A3DHLSLDataSrc`), profiles `vs_2_0`/`vs_3_0`/`ps_2_0`/`ps_3_0`, plus macro permutations — actual count needs the game data, not the repo |
| Where shaders are used | special cases only (e.g. bloom `shaders\2.2\HLSL\Common\bloom_mask.hlsl` via `A3DSkinRenderWithEffect`) |

The bulk of rendering (terrain, water, grass, UI, fonts, lit models, particles collectors) goes
through FVF + texture-stage state. The GLES layer must implement the stage machine for real.

## 3. Liveness of the COLOR/ALPHA op+arg space

### Ops actually passed (mechanical scan, both trees)

| Op | Count | Notes |
|---|---|---|
| `DISABLE` | 80 | stage off / chain terminator |
| `MODULATE` | 76 | default everywhere |
| `SELECTARG1` | 27 | untextured color pass, UI |
| `MODULATE2X` | 20 | terrain outline, IBL |
| `SELECTARG2` | 14 | |
| `ADD` | 3 | |
| `ADDSIGNED` | 3 | |
| `BLENDFACTORALPHA` | 2 | terrain outline edge fade |
| `BLENDTEXTUREALPHA` | 1 | |
| `MODULATEALPHA_ADDCOLOR` | 1 | |
| via variable (`m_ColorOP`, `textureOP`, `opTex1`, `prevOp`) | 33 | resolve to `MODULATE`/`MODULATE2X`/`MODULATE4X`; `MODULATE4X` exists in this build only via `A3DIBLScene` |

Missing entirely: DOTPRODUCT3, LERP, blend-spec ops (BLENDCURRENT, DIFFUSEALPHA...),
PREMODULATE, MULTIPLYADD — the GLSL formula set is tiny.

### Args actually passed

`TEXTURE`, `CURRENT`, `DIFFUSE`, `TFACTOR`, `SPECULAR` (all 7 combos above; CONSTANT/ARG0 appear
only in RSCache defaults and the `A3DShader` stage struct machinery).

### Chain shape

- `MAX_TEX_LAYERS = 8` (`A3DDevice.h:38`), every stage state mirrors D3D9 exactly in the RS cache.
- Typical chains: 0–2 stages (UI, models), up to 6 explicitly configured stages in
  `A3DTerrain2Render` (rest set `DISABLE`), layer loop bounded by `pMask->GetLayerNum()` with ≤3
  active layers per pass.
- `A3DShader.cpp` (stage-list save/restore machinery, `SSMF_COLOROP` masks) already proves the
  design treats stage configs as serializable state — a vector of 8 stage descriptors.

## 4. Texture-coordinate pipeline — LIVE, must be implemented

Three coupled states appear together on visible paths:

1. `TEXCOORDINDEX` — plain UV source index, or `TCI_CAMERASPACEPOSITION` ×6 /
   `TCI_CAMERASPACEREFLECTIONVECTOR` ×1.
2. `TEXTURETRANSFORMFLAGS` — `DISABLE` / `COUNT2` / `COUNT3` / `COUNT4|PROJECTED`.
3. Per-stage 4×4 texture matrix via `SetTextureMatrix` (`D3DTS_TEXTUREn`), applied by the
   RS cache as well.

Users (visible paths, all need GLES support):

| Renderer | Gimmick |
|---|---|
| `A3DTerrainWaterRender` | stage2 = camera-space pos, COUNT4\|PROJECTED; stage3 = reflection vector, COUNT3 |
| `A3DTerrainWaterSimpleAlphaRender` | same projected trick on stage2 |
| `A3DTerrain2Render` | shadow pass + scrolling/scale matrices |
| `EL_GrassType` | TCI camera-space pos COUNT3 (world-move grass) |
| `EC_FullGlowRender` | TCI COUNT4\|PROJECTED (glow projection) |
| `A3DGFXSurfaceRender` | TCI COUNT4\|PROJECTED |
| `EC_SunMoon`, `DlgMiniMap`, `A3DFTFont`, `AUIProgress`, `A3DSkinMesh`, `A3DScene`, `A3DMesh` (detail TM), `A3DPerlinNormalMap`, `A3DSkyGradient` (cloud TM), `ELTerrainOutline2` (COUNT3 silhouette) | plain matrices / COUNT2-3 |

## 5. Exotic states — verdicts

| State | Verdict |
|---|---|
| `BUMPENVMAT00..11`, `BUMPENVLSCALE`, `BUMPENVLOFFSET` | **dead by construction**: only writers are `A3DShader` script-token parsing with zero defaults; no direct callers of `SetTextureBumpMat/Scale/Offset`. Implement as log-and-ignore; revisit only if a data pack actually contains a bumpenv token (verifiable at runtime logging). |
| `RESULTARG` | set inside `A3DShader` machinery (defaults `CURRENT`); trivial as a temp-register assignment in the stage compiler — support it, cost ~0. |
| `COLORARG0` / `ALPHAARG0` | only RSCache defaults ever written; no reader may use third modifier in the op list above. Accept state, ignore. |
| `CONSTANT` | only default in RSCache; no live writer with data. Accept, ignore. |
| `BORDERCOLOR` | `A3DFTFont` sets stage0 border → ES 3.0 `GL_CLAMP_TO_BORDER` + border color. Implement. |
| `MIPMAPLODBIAS` | sampler bias — settable in ES via `TEXTURE_LOD_BIAS` (ES3 sampler objects). Implement. |

## 6. Decision: runtime stage-chain → GLSL compiler in `d3d9gles`

Prebaking whole variants was rejected: the live op×arg×transform surface is small (~11 ops × 5
args × coord modes), but renderer-specific chains are configured at runtime and the `A3DShader`
save/restore machinery can compose arbitrary 8-stage chains — enumeration would go stale.

Plan:
- `d3d9gles` accumulates the full 8-stage state vector (ops, args, coord index, TTFF, texture
  matrix) — it already mirrors D3D9's caching model.
- On draw, hash the vector → cache key; miss → generate a GLSL fragment program as a linear
  chain over `vec4 r = diffuse * tfactor-wrapped args` implementing the op formulas, plus a
  vertex shader parameterized by FVF decode + per-stage coordinate mode (UV / camera pos /
  reflection vector, matrix + PROJECTED divide in clip→tex space).
- Estimated ~300–500 LOC for the generator + program cache. If profiling later shows shader
  churn on hot paths (terrain, UI), add the obvious prebaked variants for those two.

## 7. What this audit did NOT cover (next)

- Actual HLSL instance count (needs game data packs; `A3DHLSLDataSrc` format already mapped:
  `FILEHEADER` + per-entry `SHADERINSTHEADER` `dwEntryID`/`dwProfile`/`dwShaderSize` + macros).
- ElementClient clear-state callers at `EC_*` level were spot-checked, not exhaustively read;
  the mechanical scans above cover every `SetTexture*` site in all three trees, so the op/arg
  table is exhaustive for static callers.
- `GetD3DDevice()` 169-callsite replacement list (separate deliverable).
