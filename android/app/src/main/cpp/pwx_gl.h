#pragma once

namespace pwx {

// Demo scene driven through the d3d9gles device: it calls the same D3D-shaped API that
// Angelica3D will call once the engine is wired in.
void GlInit();
void GlRender(float timeSeconds, int width, int height);
void GlShutdown();

}  // namespace pwx
