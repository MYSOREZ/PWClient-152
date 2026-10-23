#pragma once

namespace pwx {

// Minimal GLES 3.0 pipeline: one shader program, one VBO, one triangle.
// It exists to prove the whole chain (context -> shader compile -> draw -> swap) on device
// before the D3D9 state machine is ported.
void GlInit();
void GlRender(float timeSeconds);

}  // namespace pwx
