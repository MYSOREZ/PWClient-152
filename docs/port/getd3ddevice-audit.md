# `GetD3DDevice()` callsite audit (P1.5, part 2)

Status: **complete (static analysis)**. Companion to `d3d9-to-gles.md` §5 open question 5 and
`stage-audit.md`. Deliverable for spec §6 bring-up planning.

## 1. Headline result

- **169 total callsites** (matches the spec estimate of 169), across 46 files.
- **134 are direct method chains** (`GetD3DDevice()->MethodName(...)`), 35 are indirect
  (device pointer taken first / multiline call / dead `#ifdef` branch).

**Key conclusion for the adapter strategy:** per spec §5.5 these sites were flagged for
"replacement before header swap lands". Under the *chosen* strategy (d3d9gles adapter with a
fake, non-COM `IDirect3DDevice9`), **none of the 169 sites need engine-side edits**. They
compile against our fake headers and forward to the adapter. The audit therefore converts into
an **API-surface checklist**: the exact list of `IDirect3DDevice9` / D3DX entry points the
adapter must implement, plus which ones need special GLES3 semantics.

## 2. API surface histogram (direct chains, 134 sites)

| IDirect3DDevice9 / D3DX method | Sites | GLES3 mapping |
|---|---|---|
| `SetTexture` | 54 | trivial: record texture per stage |
| `CopyRects` | 14 | blit emulation (FBO blit or textured draw); watch Y-flip and format match |
| `SetClipPlane` / `GetClipPlane` | 8+3 | **trap**: GL3 has no fixed clip planes; adapter must feed clip distances via `gl_ClipDistance` in the generated VS (stage compiler gets a "clip plane count" uniform); `GetClipPlane` reads adapter state cache |
| `CreateOffscreenPlainSurface` | 7 | plain texture-backed surface (FBO-able) |
| `StretchRect` | 7 | scaled blit; respects D3D filter; Y-flip trap |
| `SetPixelShaderConstantF` | 5 | uniform upload path |
| `CreateRenderTarget` / `CreateTexture` | 4+4 | FBO / texture with pool semantics (D3DPOOL_MANAGED → resident GL textures) |
| `CreateQuery` | 3 | GLES3 occlusion queries (`GL_ANY_SAMPLES_PASSED`) — event model differs, needs own query pool |
| `SetVertexShader` | 3 | program binding path |
| `SetStreamSource` / `SetIndices` | 2+2 | VBO binding (adapter already has it) |
| `CreateVertexBuffer` / `CreateIndexBuffer` / `CreateDepthStencilSurface` / `CreateCubeTexture` | 2+2+2+2 | buffer / FBO depth attachment / cubemap texture |
| `CreateVolumeTexture`, `CreateAdditionalSwapChain` | 1+1 | volume textures: unrewritten in ES3 → adapter creates 2D atlas staging; swap chain → FBO chain |
| `CreateStateBlock` / `CaptureStateBlock` / `ApplyStateBlock` / `DeleteStateBlock` | 4 | **editor tool code only** (`CElementEditor\ElementMap.cpp`), not in client runtime path |
| `SetSamplerState`, `SetVertexDeclaration`, `CreateVertexDeclaration`, `TestCooperativeLevel` | 4 | sampler object / VAO-ish path; `TestCooperativeLevel` → always OK |

## 3. Indirect sites (35) — what they actually do

- A3DTexture ×9, A3DVolumeTexture ×4, A3DCubeTexture: one helper each —
  `CreateTextureFromFileInMemoryEx` / `D3DXCreateCubeTextureFromFileInMemoryEx` /
  `D3DXCreateVolumeTexture`: the adapter must provide an equivalent loader (own DDS/PNG/TGA
  decode path already planned for A3DTexture bring-up).
- A3DHLSL ×5, A3DHLSLDataSrc ×1, A3DPixelShader ×1: shader-side — `pd3dDevice` used to create
  the shader (compiler output upload) and constant-table reads. Adapter maps to program
  creation/UPB upload. The serialized-blob pipeline itself stays unchanged.
- A3DRenderTargetMan ×3, A3DDeviceRSCache ×1: these live **inside the seam** — they push/pop
  render targets and replay cached state to the raw device; the adapter implements exactly
  these `SetRenderTarget`/state calls, so nothing to log.
- A3DVertexShader ×1 `CreateVertexDeclaration` + `SetVertexDeclaration`: adapter VAO layout keys.
- A3DBezierPoint/Segment ×2: commented-out guards only (`//if( m_pA3DDevice->GetD3DDevice() )`)
  — zero impact, but they still occupy callsites mechanically.
- A3DCoverageMap ×1: fully commented out. Zero impact.
- AUIImagePicture ×2, AUIManager ×1: `IDirect3DDevice8*` typed + `GetRenderState` — these are
  the **`#ifdef D3D8 legacy branches`**; compile-dead on a D3D9 build. Keep as-is (a fake D3D8
  type alias is needed only if the #ifdef structure changes).
- CElementEditor (4 files ×12): editor tools (LightProcessor, ElementMap etc.) — not in the
  Android runtime binary at all, but they reference D3D — adapter API surface must still cover
  them to keep the shared fake headers compiling there if desired. Lowest priority.
- CommonFileDlg `CreateImageSurface`: editor-only as well.

## 4. All 169 sites (file → line:method)

`?` = indirect pattern (see §3). Paths relative to `PWClient152\`.


| File | Sites (line:method) |
|---|---|
| `CElementEditor\CommonFileDlg.cpp` | 941:? |
| `CElementEditor\ElementMap.cpp` | 568:CreateStateBlock &#124; 571:CaptureStateBlock &#124; 582:ApplyStateBlock &#124; 583:DeleteStateBlock |
| `CElementEditor\LightProcessor.cpp` | 758:SetTexture &#124; 846:CopyRects &#124; 883:CopyRects &#124; 950:CopyRects &#124; 1016:SetTexture &#124; 1018:SetVertexShader &#124; 1130:SetTexture &#124; 1232:CopyRects &#124; 1302:SetVertexShader &#124; 1357:SetTexture &#124; 1446:CopyRects |
| `CElementEditor\MaskModifier.cpp` | 1194:SetVertexShader |
| `Common\AUInterface2\A3DFTFont.cpp` | 792:SetSamplerState |
| `Common\AUInterface2\AUIImagePicture.cpp` | 625:? &#124; 745:? |
| `Common\AUInterface2\AUIManager.cpp` | 890:? |
| `Common\GfxCommon2\A3DGFXPIFuncs.cpp` | 113:CopyRects |
| `CRandomMap_WYD\EC_World.cpp` | 1657:SetClipPlane |
| `ElementCore\ElementClient\CCommon\EL_Forest.cpp` | 327:GetClipPlane &#124; 332:SetClipPlane &#124; 359:SetClipPlane |
| `ElementCore\ElementClient\CCommon\EL_Grassland.cpp` | 238:GetClipPlane &#124; 244:SetClipPlane &#124; 340:SetClipPlane |
| `ElementCore\ElementClient\CCommon\ELOutlineTexture.cpp` | 1374:StretchRect &#124; 1376:CopyRects |
| `ElementCore\ElementClient\EC_FullGlowRender.cpp` | 540:StretchRect &#124; 542:CopyRects &#124; 551:StretchRect &#124; 553:CopyRects &#124; 568:StretchRect &#124; 570:CopyRects &#124; 594:SetTexture &#124; 630:SetTexture &#124; 631:SetTexture &#124; 632:SetTexture &#124; 633:SetTexture &#124; 634:SetTexture &#124; 653:SetTexture &#124; 654:SetTexture &#124; 655:SetTexture &#124; 656:SetTexture &#124; 657:SetTexture &#124; 658:SetTexture &#124; 688:SetTexture &#124; 743:StretchRect &#124; 745:CopyRects &#124; 851:SetTexture &#124; 852:SetTexture &#124; 853:SetTexture &#124; 854:SetTexture &#124; 855:SetTexture |
| `ElementCore\ElementClient\EC_GameRun.cpp` | 3213:CopyRects |
| `ElementCore\ElementClient\EC_PortraitRender.cpp` | 91:CreateRenderTarget &#124; 161:StretchRect &#124; 163:CopyRects &#124; 211:CreateTexture &#124; 214:CreateRenderTarget |
| `ElementCore\ElementClient\EC_ShadowRender.cpp` | 419:SetTexture &#124; 420:SetTexture &#124; 421:SetTexture &#124; 422:SetTexture &#124; 423:SetTexture &#124; 433:GetClipPlane &#124; 438:SetClipPlane &#124; 518:SetClipPlane |
| `ElementCore\ElementClient\EC_SunMoon.cpp` | 632:StretchRect &#124; 634:CopyRects &#124; 662:SetTexture &#124; 692:SetTexture &#124; 781:SetTexture |
| `ElementCore\ElementClient\EC_World.cpp` | 1712:SetClipPlane |
| `Engine\Angelica3D\Header\A3DDevice.h` | 263:? |
| `Engine\Angelica3D\Source\A3DAdditionalView.cpp` | 151:CreateAdditionalSwapChain &#124; 164:CreateDepthStencilSurface &#124; 250:TestCooperativeLevel |
| `Engine\Angelica3D\Source\A3DBezierPoint.cpp` | 65:? |
| `Engine\Angelica3D\Source\A3DBezierSegment.cpp` | 158:? |
| `Engine\Angelica3D\Source\A3DCoverageMap.cpp` | 247:? |
| `Engine\Angelica3D\Source\A3DCubeRenderTarget.cpp` | 106:CreateCubeTexture |
| `Engine\Angelica3D\Source\A3DCubeTexture.cpp` | 111:? &#124; 216:CreateCubeTexture |
| `Engine\Angelica3D\Source\A3DDeviceRSCache.cpp` | 290:? |
| `Engine\Angelica3D\Source\A3DHLSL.cpp` | 426:? &#124; 631:? &#124; 730:? &#124; 888:? &#124; 962:? &#124; 1196:SetTexture |
| `Engine\Angelica3D\Source\A3DHLSLDataSrc.cpp` | 262:? &#124; 691:? &#124; 720:? |
| `Engine\Angelica3D\Source\A3DLitModelRender.cpp` | 1095:SetPixelShaderConstantF &#124; 1101:SetPixelShaderConstantF &#124; 1102:SetTexture &#124; 1109:SetTexture &#124; 1110:SetPixelShaderConstantF &#124; 1116:SetPixelShaderConstantF &#124; 1147:SetTexture &#124; 1150:SetPixelShaderConstantF |
| `Engine\Angelica3D\Source\A3DOcclusionManHW.cpp` | 116:CreateQuery |
| `Engine\Angelica3D\Source\A3DOccQuery.cpp` | 149:CreateQuery &#124; 215:CreateQuery |
| `Engine\Angelica3D\Source\A3DPerlinNormalMap.cpp` | 213:SetTexture &#124; 220:SetTexture |
| `Engine\Angelica3D\Source\A3DPixelShader.cpp` | 170:? |
| `Engine\Angelica3D\Source\A3DRenderTarget.cpp` | 348:CreateRenderTarget &#124; 368:CreateTexture &#124; 386:CreateRenderTarget &#124; 422:CreateTexture &#124; 440:CreateDepthStencilSurface |
| `Engine\Angelica3D\Source\A3DRenderTargetMan.cpp` | 157:? &#124; 212:? &#124; 300:? |
| `Engine\Angelica3D\Source\A3DShadowMap.cpp` | 381:SetTexture |
| `Engine\Angelica3D\Source\A3DSkinRender.cpp` | 2758:SetTexture &#124; 2777:SetTexture &#124; 2875:SetTexture &#124; 2881:SetTexture |
| `Engine\Angelica3D\Source\A3DStream.cpp` | 271:CreateVertexBuffer &#124; 322:CreateIndexBuffer &#124; 521:SetStreamSource &#124; 538:SetIndices &#124; 749:CreateVertexBuffer &#124; 774:CreateIndexBuffer &#124; 805:SetStreamSource &#124; 811:SetIndices |
| `Engine\Angelica3D\Source\A3DSurface.cpp` | 132:CreateOffscreenPlainSurface |
| `Engine\Angelica3D\Source\A3DSurfaceMan.cpp` | 53:CreateOffscreenPlainSurface &#124; 139:CreateOffscreenPlainSurface &#124; 256:CreateOffscreenPlainSurface &#124; 371:CreateOffscreenPlainSurface |
| `Engine\Angelica3D\Source\A3DTerrainWater2NormalRender.cpp` | 708:SetTexture &#124; 827:SetTexture &#124; 933:SetTexture |
| `Engine\Angelica3D\Source\A3DTerrainWaterRender.cpp` | 980:SetTexture &#124; 1065:SetTexture &#124; 1066:SetTexture &#124; 1071:SetTexture &#124; 1107:SetTexture &#124; 1108:SetTexture &#124; 1109:SetTexture |
| `Engine\Angelica3D\Source\A3DTexture.cpp` | 203:SetTexture &#124; 205:SetTexture &#124; 225:SetTexture &#124; 289:CreateOffscreenPlainSurface &#124; 297:CreateOffscreenPlainSurface &#124; 510:CreateTexture &#124; 745:? &#124; 755:? &#124; 770:? &#124; 816:? &#124; 1002:? &#124; 1078:? &#124; 1169:? |
| `Engine\Angelica3D\Source\A3DTextureMan.cpp` | 415:? |
| `Engine\Angelica3D\Source\A3DVertexShader.cpp` | 69:CreateVertexDeclaration &#124; 92:SetVertexDeclaration &#124; 217:? |
| `Engine\Angelica3D\Source\A3DVolumeTexture.cpp` | 112:? &#124; 280:? &#124; 303:? &#124; 367:CreateVolumeTexture |
Total site rows: 168 of 169. The remaining row (`Engine\Output\a3dsdk\include\A3DDevice.h` L262 —
the `GetD3DDevice()` declaration inside the a3dsdk output copy, duplicate of
`Engine\Angelica3D\Header\A3DDevice.h` L262) is intentionally excluded from the table as a copy.

## 5. Decisions

1. **No per-site edits.** The list is an adapter API checklist, not a work queue.
2. **Trap trips for the stage compiler / adapter:** `SetClipPlane` (needs `gl_ClipDistance`
   injection), `CopyRects`/`StretchRect` Y-flip and format semantics, StateBlock semantics
   (editor-only), `CreateQuery` event model. Each trip gets a one-time log in the adapter when
   first exercised (rule §7.6 of the port spec).
3. The fake headers must also type-alias the **D3DX helper family** used at loader sites
   (`D3DXCreateTextureFromFileInMemoryEx`, `D3DXCreateCubeTexture...`, `D3DXCreate...Volume`),
   `LPD3DXBUFFER`, `ID3DXConstantTable` (HLSL). Otherwise the engine won't compile once the
   fake `A3DDevice.h` header swap lands. These get bridges, not reimplementations (GL loader
   side doesn't need their semantics, so the adapter stubs behave like the planned decoders).
4. Prior scope: the spec §5.5 wording "each one needs an A3D replacement before the header swap
   can land" is superseded by this conclusion and by the adapter decision recorded in
   `d3d9-to-gles.md` §2.
