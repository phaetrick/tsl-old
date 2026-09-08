#pragma once
#include "IGraphics.h"
#include "IPlugPlatform.h"
#include <chrono>
BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE
class IGraphicsTSL : public IGraphics
{
public:
  IGraphicsTSL(IGEditorDelegate& dlg, int w, int h, int fps, float scale)
    : IGraphics(dlg, w, h, fps, scale) {};
  void DrawResize() override;
  void DrawResizeExplicit(int w, int h) override;
  void BeginFrame() override;
  void EndFrame() override;
  void OnMouseDown(const std::vector<IMouseInfo>& points) override;
  void OnMouseUp(const std::vector<IMouseInfo>& points) override;
  void OnMouseDrag(const std::vector<IMouseInfo>& points) override;
  void OnTouchCancelled(const std::vector<IMouseInfo>& points) override;
#ifdef GRAINSTORM
  void OnDrop(const char* str, float x, float y) override;
#endif
  bool OnKeyDown(float x, float y, const IKeyPress& key) override;
  bool OnKeyUp(float x, float y, const IKeyPress& key) override;
  bool OnMouseOver(float x, float y, const IMouseMod& mod) override;
  bool OnMouseDblClick(float x, float y, const IMouseMod& mod) override;
  bool OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override;
  void OnMouseOut() override;

  void OnViewInitialized(void* pContext) override;
  void OnViewDestroyed() override;

  // Keeps _appState->mPpi in sync with the display's backing scale on desktop
  // (no-op on iOS, which sets it via tsl::app::setupPlatformMetrics).
  void UpdatePlatformMetrics();

  const char* GetDrawingAPIStr() override { return "TSL"; };
  void DrawBitmap(const IBitmap& bitmap, const IRECT& dest, int srcX, int srcY, const IBlend* pBlend) override {}

  void PathClear() override {}
  void PathClose() override {}
  void PathArc(float cx, float cy, float r, float a1, float a2, EWinding winding) {};

  void PathMoveTo(float x, float y) override {}
  void PathLineTo(float x, float y) override {}

  void PathCubicBezierTo(float x1, float y1, float x2, float y2, float x3, float y3) override {}

  void PathQuadraticBezierTo(float cx, float cy, float x2, float y2) override {}

  void PathStroke(const IPattern& pattern, float thickness, const IStrokeOptions& options, const IBlend* pBlend) override {};
  void PathFill(const IPattern& pattern, const IFillOptions& options, const IBlend* pBlend) override {};
  void DrawFastDropShadow(const IRECT& innerBounds, const IRECT& outerBounds, float xyDrop, float roundness, float blur, IBlend* pBlend) override {};

  IColor GetPoint(int x, int y) override { return IColor::FromRGBAf(nullptr); }
  void* GetDrawContext() override { return (void*)nullptr; }

  bool BitmapExtSupported(const char* ext) override { return false; }
  int AlphaChannel() const override { return 3; }
  bool FlippedBitmap() const override { return false; }

  APIBitmap* CreateAPIBitmap(int width, int height, float scale, double drawScale, bool cacheable = false) override { return nullptr; }

  void GetLayerBitmapData(const ILayerPtr& layer, RawBitmapData& data) override {};
  void ApplyShadowMask(ILayerPtr& layer, RawBitmapData& mask, const IShadow& shadow) override {};

  void UpdateLayer() override {};

protected:
  float DoMeasureText(const IText& text, const char* str, IRECT& bounds) const override { return 0.f; };
  void DoDrawText(const IText& text, const char* str, const IRECT& bounds, const IBlend* pBlend) override {};

  bool LoadAPIFont(const char* fontID, const PlatformFontPtr& font) override { return false; };

  APIBitmap* LoadAPIBitmap(const char* fileNameOrResID, int scale, EResourceLocation location, const char* ext) override { return nullptr; };
  APIBitmap* LoadAPIBitmap(const char* name, const void* pData, int dataSize, int scale) override { return nullptr; }

private:
  void PathTransformSetMatrix(const IMatrix& m) override {};
  void SetClipRegion(const IRECT& r) override {};
  std::chrono::steady_clock::time_point _lastFrameTime;
  const double _frameIntervalMs = 1000.0 / 60.0; // 60 Hz cap
};
}
}