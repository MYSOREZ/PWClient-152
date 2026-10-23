# HLSL → GLSL translation plan (P1.5, part 3 — static)

Status: **plan complete; instance count pending game data packs**.
Companion to `d3d9-to-gles.md` §5 open question 2 and `stage-audit.md` §2.

## 1. How shaders work in the original client (verified in code)

- HLSL **source text ships inside the data packs** (not compiled bytecode): `A3DHLSLDataSrc`
  receives `#line 1 "..."`-wrapped C-like HLSL text and feeds it to
  `D3DXCreateEffectCompiler` (effect-style) or `D3DXCompileShader` (direct instances,
  with macro array + include handler), profiles `vs_2_0`/`vs_3_0`/`ps_2_0`/`ps_3_0`.
- `A3DHLSLMan::LoadShader(file, vsEntry, psEntry, macroString, ...)` keys a cache entry by
  (logical shader path, entry names, macro permutation). `A3DHLSLDataSrc::MACROPROP` holds
  macro groups; the runtime picks permutations (`wGroup` bit mask).
- Coverage in client code: ~23 direct `SetVertexShader`/`SetPixelShader` sites; the visible
  bulk is fixed-function (see `stage-audit.md`). HLSL is used for effects/bloom/glow-type
  passes via `A3DEffect` + `A3DSkinRenderWithEffect` (e.g. `shaders\2.2\HLSL\Common\bloom_mask.hlsl`).
- Constants flow through **constant tables** (`ID3DXConstantTable` + engine
  `CommonConstTable` submitted by `A3DSkinRender::SubmitCommTable`), register/name-based.

## 2. Consequence for the port

Runtime HLSL compilation under GLES3 is impossible (no D3DX), so:

1. **Offline translation**: HLSL texts are extracted from packs → translated to
   GLSL ES 3.00 → checked into `android/app/src/main/cpp/shaders/glsl/`.
   Runtime: the d3d9gles adapter serves cached GLSL by the same key
   (path/entries/macros) that `A3DHLSLMan` would use. The A3DHLSL class surface stays,
   its "compile" step becomes "load GLSL by key".
2. **Constant-table compatibility**: the GLSL translator must preserve constant *names*
   (registers become uniform locations bound at link time), so `ID3DXConstantTable`
   semantics keep working through a thin adapter shim (`GetConstantByName`,
   `SetFloat{,Array}/SetVector{,Array}/SetMatrix{,Array}` on the adapter with
   `glGetUniformLocation`). Common-const-table wiring unchanged.
3. **Macro permutations**: each actually-used permutation = one offline GLSL variant.
   The registry manifest (json/csv) maps key → glsl asset. Unknown key at runtime =
   one-time logged error + error output shader (never silent, spec rule §6).

## 3. Toolchain plan (python, sources in repo)

| Step | Tool | Input → output |
|---|---|---|
| Pack extraction | `tools/pwdata/pwpack.py` (to write; format from `AFilePackage.cpp`) | `.pck` → binary file list |
| Shader collection | `tools/pwdata/extract_shaders.py` | packs → `data/hlsl/*.hlsl` + index json (path/entries/macro list/profiles) |
| Translation | `tools/hlsl2glsl/` (new, iterative) | one `*.hlsl`+entry → GLSL ES 3.00; subset-oriented: vertex/fragment, samplers, `tex2D`/`tex2Dlod`, simple flow, `#define` macros |
| Validation | SPIRV cross-check (optional, later): glslangValidator compile-check per variant | fail list |
| Packing for device | existing gradle assets | `shaders/glsl/**` + manifest into APK assets |

The translator is built bottom-up on the *actual* HLSL corpus once packs are available
(translate on demand, unit-check per file, expand as corpus requires). If a construct proves
hostile (rare in these old engines: no branches-on-texlookup in this code), hand-port that
file with the standard annotation `// PWX @feature author DD/MM/yyyy` in the git-committed GLSL.

## 4. What remains unknown until data packs arrive

- **Instance count** and macro-permutation width (affects total GLSL variants).
- Actual construction (how many unique `file+entry` pairs).
- Whether any shader uses HLSL features outside the planned subset (determines tool effort).

None of these change the architecture above; they size it. The build-up order remains per
spec §6/§7: fixed-function core first (stage compiler), HLSL effects second (they gate on
post-effect passes like bloom, i.e. after the first engine frame).

## 5. Decision record

- Offline translation + runtime key-cache: **confirmed** (mirrors the lean already recorded
  in `d3d9-to-gles.md` §5).
- GLSL files committed to repo, HLSL stays documentation-only (GitHub-rule English docs —
  code comments stay as-is since they are original engine artifacts).
- Runtime fallback for a missing key: hard fail with log (correctness rule, no silent
  degradation).
