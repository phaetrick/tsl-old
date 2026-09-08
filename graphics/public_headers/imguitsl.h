#pragma once

namespace tsl::imgui{

void OnViewInitialized();
void BeginFrame(int width, int height, float fps);
void EndFrame();
void OnViewDestroyed();

}
