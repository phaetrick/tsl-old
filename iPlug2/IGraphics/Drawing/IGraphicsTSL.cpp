#include "IGraphicsTSL.h"

#ifdef __APPLE__
#include <os/log.h>
#endif

#include "config.h"
#if defined ENABLE_PROT_PLUGIN
#else
  #if defined USE_IMGUI
    #include <imguitsl.h>
  #endif
  #include <Input.h>
  #include <View.h>
  #include <app.h>
  #include <logger.h>
  #include <skia.h>
  #include <window.h>
#endif

#if defined OS_MAC || defined OS_IOS
#if defined GRAINSTORM
#include <DecoderWindows.h>
#endif

#include "os/log.h"
  #include "SkCGUtils.h"
  #if defined IGRAPHICS_GL2
    #include <OpenGL/gl.h>
  #elif defined IGRAPHICS_GL3
    #include <OpenGL/gl3.h>
  #elif defined IGRAPHICS_METAL
  // even though this is a .cpp we are in an objc(pp) compilation unit
    #include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
    #import <Metal/Metal.h>
    #import <QuartzCore/CAMetalLayer.h>
  #elif !defined IGRAPHICS_CPU
    #error Define either IGRAPHICS_GL2, IGRAPHICS_GL3, IGRAPHICS_METAL, or IGRAPHICS_CPU for IGRAPHICS_SKIA with OS_MAC
  #endif
#elif defined OS_WIN
  #pragma comment(lib, "opengl32.lib")
  #if defined ENABLE_PROT_PLUGIN
  #else
    #include <DecoderWindows.h>
  #endif
#endif
BEGIN_IPLUG_NAMESPACE
BEGIN_IGRAPHICS_NAMESPACE

void IGraphicsTSL::DrawResize()
{
  auto w = static_cast<int>(std::ceil(static_cast<float>(WindowWidth()) * GetScreenScale()));
  auto h = static_cast<int>(std::ceil(static_cast<float>(WindowHeight()) * GetScreenScale()));
  ScopedGLContext scopedGLContext{this};
  UpdatePlatformMetrics();
  tsl::app::resize(GetDelegate()->_appState, w, h);
}

void IGraphicsTSL::DrawResizeExplicit(int w, int h) {
  ScopedGLContext scopedGLContext{this};
  UpdatePlatformMetrics();
  tsl::app::resize(GetDelegate()->_appState, w, h);
}

// Desktop equivalent of tsl::app::setupPlatformMetrics (which only runs on iOS).
// The tsl UI is rendered into a WindowWidth()*GetScreenScale() px surface, so its
// coordinate space has GetScreenScale() physical pixels per on-screen point. That
// is the same quantity UIScreen.scale encodes on iOS, so mPpi = screenScale*160
// keeps touchSlop() (8dp) at 8 on-screen points on macOS/Windows, matching mobile.
// Deliberately independent of GetDrawScale() (the corner-resizer zoom): the physical
// slop should not change when the user stretches the window.
void IGraphicsTSL::UpdatePlatformMetrics() {
#if !defined(OS_IOS)
  auto* appState = GetDelegate()->_appState;
  if (appState)
    appState->mPpi = GetScreenScale() * 160.f;
#endif
}
void IGraphicsTSL::OnViewInitialized(void* pContext)
{
  IGraphics::OnViewInitialized(pContext);

#if defined OS_IOS && defined IGRAPHICS_METAL
  GetDelegate()->_appState->graphics.setMTLLayer(pContext);
#endif

#if defined USE_IMGUI
  tsl::imgui::OnViewInitialized();
#endif
  GetDelegate()->_appState->graphics.init();
  DrawResize();
}
void IGraphicsTSL::OnViewDestroyed()
{
  RemoveAllControls();
#if defined USE_IMGUI
  tsl::imgui::OnViewDestroyed();
#endif
  if (GetDelegate()->_appState)
    GetDelegate()->_appState->graphics.onViewDestroyed();
  IGraphics::OnViewDestroyed();
}
void IGraphicsTSL::BeginFrame()
{
  int width = WindowWidth() * GetScreenScale();
  int height = WindowHeight() * GetScreenScale();
  if (!GetDelegate()->_appState) return;
  tsl::app::loop(GetDelegate()->_appState, width, height);
  IGraphics::BeginFrame();
}
void IGraphicsTSL::EndFrame()
{
  // BeginFrame() bails on a null _appState; this has to match it, or IGraphics::Draw()
  // (which is just BeginFrame(); EndFrame();) walks into EndFrame with nothing set up.
  if (!GetDelegate()->_appState) return;
  GetDelegate()->_appState->graphics.EndFrame();
}

bool IGraphicsTSL::OnMouseDblClick(float x, float y, const IMouseMod& mod)
{
  IMouseInfo info;
  info.x = x;
  info.y = y;
  info.ms = mod;
  std::vector<IMouseInfo> list{info};
  OnMouseDown(list);
  return true;
}

void IGraphicsTSL::OnMouseDown(const std::vector<IMouseInfo>& points)
{

  for (auto& point : points)
  {
#if defined(OS_IOS)
    auto x = point.x * GetDrawScale() - GetDelegate()->_appState->graphics.xOffset;
    auto y = point.y * GetDrawScale() - GetDelegate()->_appState->graphics.yOffset;
#else
    auto x = point.x * WindowWidth() / PLUG_WIDTH;
    auto y = point.y * WindowHeight() / PLUG_HEIGHT;
#endif

    const IMouseMod& mod = point.ms;
    tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_DOWN, (int)mod.touchID, x, y);

#if defined ENABLE_PROT_PLUGIN
    GetDelegate()->CallbackInput(ev);
#else
    tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif


  }
  return;
  /*
  //  Trace("IGraphics::OnMouseDown", __LINE__, "x:%0.2f, y:%0.2f, mod:LRSCA: %i%i%i%i%i", x, y, mod.L, mod.R, mod.S, mod.C, mod.A);

  bool singlePoint = points.size() == 1;

  if (singlePoint)
  {
    mMouseDownX = points[0].x;
    mMouseDownY = points[0].y;
  }

  for (auto& point : points)
  {
    float x = point.x;
    float y = point.y;
    const IMouseMod& mod = point.ms;

    IControl* pCapturedControl = GetMouseControl(x, y, true, false, mod.touchID);

    if (pCapturedControl)
    {
      int nVals = pCapturedControl->NVals();
#if defined AAX_API || !defined IGRAPHICS_NO_CONTEXT_MENU
      int valIdx = pCapturedControl->GetValIdxForPos(x, y);
      int paramIdx = pCapturedControl->GetParamIdx((valIdx > kNoValIdx) ? valIdx : 0);
#endif

#ifdef AAX_API
      if (mAAXViewContainer && paramIdx > kNoParameter)
      {
        auto GetAAXModifiersFromIMouseMod = [](const IMouseMod& mod) {
          uint32_t modifiers = 0;

          if (mod.A)
            modifiers |= AAX_eModifiers_Option; // ALT Key on Windows, ALT/Option key on mac

  #ifdef OS_WIN
          if (mod.C)
            modifiers |= AAX_eModifiers_Command;
  #else
          if (mod.C)
            modifiers |= AAX_eModifiers_Control;
          if (mod.R)
            modifiers |= AAX_eModifiers_Command;
  #endif
          if (mod.S)
            modifiers |= AAX_eModifiers_Shift;
          if (mod.R)
            modifiers |= AAX_eModifiers_SecondaryButton;

          return modifiers;
        };

        uint32_t aaxModifiersForPT = GetAAXModifiersFromIMouseMod(mod);
  #ifdef OS_WIN
        // required to get start/windows and alt keys
        uint32_t aaxModifiersFromPT = 0;
        mAAXViewContainer->GetModifiers(&aaxModifiersFromPT);
        aaxModifiersForPT |= aaxModifiersFromPT;
  #endif
        WDL_String paramID;
        paramID.SetFormatted(32, "%i", paramIdx + 1);

        if (mAAXViewContainer->HandleParameterMouseDown(paramID.Get(), aaxModifiersForPT) == AAX_SUCCESS)
        {
          ReleaseMouseCapture();
          return; // event handled by PT
        }
      }
#endif

#ifndef IGRAPHICS_NO_CONTEXT_MENU
      if (mod.R && paramIdx > kNoParameter)
      {
        ReleaseMouseCapture();
        PopupHostContextMenuForParam(pCapturedControl, paramIdx, x, y);
        return;
      }
#endif

      for (int v = 0; v < nVals; v++)
      {
        if (pCapturedControl->GetParamIdx(v) > kNoParameter)
          GetDelegate()->BeginInformHostOfParamChangeFromUI(pCapturedControl->GetParamIdx(v));
      }

      pCapturedControl->OnMouseDown(x, y, mod);
    }
  }
  */
}

void IGraphicsTSL::OnMouseUp(const std::vector<IMouseInfo>& points)
{

  //  Trace("IGraphics::OnMouseUp", __LINE__, "x:%0.2f, y:%0.2f, mod:LRSCA: %i%i%i%i%i", x, y, mod.L, mod.R, mod.S, mod.C, mod.A);
  for (auto& point : points)
  {
#if defined(OS_IOS)
    auto x = point.x * GetDrawScale() - GetDelegate()->_appState->graphics.xOffset;
    auto y = point.y * GetDrawScale() - GetDelegate()->_appState->graphics.yOffset;
#else
    auto x = point.x * WindowWidth() / PLUG_WIDTH;
    auto y = point.y * WindowHeight() / PLUG_HEIGHT;
#endif
    const IMouseMod& mod = point.ms;
    tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_UP, (int)mod.touchID, x, y);
#if defined ENABLE_PROT_PLUGIN
    GetDelegate()->CallbackInput(ev);
#else
    tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif
  }
  return;
  /*
  if (ControlIsCaptured())
  {
    for (auto& point : points)
    {
      float x = point.x;
      float y = point.y;
      const IMouseMod& mod = point.ms;
      auto itr = mCapturedMap.find(mod.touchID);

      if (itr != mCapturedMap.end())
      {
        IControl* pCapturedControl = itr->second;

        pCapturedControl->OnMouseUp(x, y, mod);

        int nVals = pCapturedControl->NVals();

        for (int v = 0; v < nVals; v++)
        {
          if (pCapturedControl->GetParamIdx(v) > kNoParameter)
            GetDelegate()->EndInformHostOfParamChangeFromUI(pCapturedControl->GetParamIdx(v));
        }

        mCapturedMap.erase(mod.touchID);
      }
    }
  }

  if (mResizingInProcess)
  {
    EndDragResize();
  }

  if (points.size() == 1 && !points[0].ms.IsTouch())
    OnMouseOver(points[0].x, points[0].y, points[0].ms);
    */
}

#ifdef GRAINSTORM
void IGraphicsTSL::OnDrop(const char* str, float x, float y) {

#if defined ENABLE_PROT_PLUGIN
  GetDelegate()->OnDrop(str);
#else
  filebrowsercallback(GetDelegate()->_appState, str);
#endif

}
#endif

void IGraphicsTSL::OnMouseDrag(const std::vector<IMouseInfo>& points)
{
  for (auto& point : points)
  {
#if defined(OS_IOS)
    auto x = point.x * GetDrawScale() - GetDelegate()->_appState->graphics.xOffset;
    auto y = point.y * GetDrawScale() - GetDelegate()->_appState->graphics.yOffset;
#else
    auto x = point.x * WindowWidth() / PLUG_WIDTH;
    auto y = point.y * WindowHeight() / PLUG_HEIGHT;
#endif
    const IMouseMod& mod = point.ms;
    tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_MOVE, (int)mod.touchID, x, y);
#if defined ENABLE_PROT_PLUGIN
    GetDelegate()->CallbackInput(ev);
#else
    tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif
  }
  return;
  /*
Trace("IGraphics::OnMouseDrag:", __LINE__, "x:%0.2f, y:%0.2f, dX:%0.2f, dY:%0.2f, mod:LRSCA: %i%i%i%i%i", points[0].x, points[0].y, points[0].dX, points[0].dY, points[0].ms.L, points[0].ms.R,
      points[0].ms.S, points[0].ms.C, points[0].ms.A);

if (mResizingInProcess && points.size() == 1)
  OnDragResize(points[0].x, points[0].y);
else if (ControlIsCaptured() && !IsInPlatformTextEntry())
{
  IControl* textEntry = nullptr;

  if (GetControlInTextEntry())
    textEntry = mTextEntryControl.get();

  for (auto& point : points)
  {
    float x = point.x;
    float y = point.y;
    float dX = point.dX;
    float dY = point.dY;
    IMouseMod mod = point.ms;

    auto itr = mCapturedMap.find(mod.touchID);

    if (itr != mCapturedMap.end())
    {
      IControl* pCapturedControl = itr->second;

      if (textEntry && pCapturedControl != textEntry)
        pCapturedControl = nullptr;

      if (pCapturedControl && (dX != 0 || dY != 0))
      {
        pCapturedControl->OnMouseDrag(x, y, dX, dY, mod);
      }
    }
  }
}
*/
}

bool IGraphicsTSL::OnKeyDown(float x, float y, const IKeyPress& key)
{
  if (key.VK != kVK_SHIFT && key.VK != kVK_CAPITAL)
    GetDelegate()->_appState->shiftPressed = key.S;

  tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_KEY_DOWN, key.VK, x, y);
#if defined ENABLE_PROT_PLUGIN
  return GetDelegate()->CallbackInput(ev);
#else
  return tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif
}

bool IGraphicsTSL::OnKeyUp(float x, float y, const IKeyPress& key)
{
  if (key.VK != kVK_SHIFT && key.VK != kVK_CAPITAL)
    GetDelegate()->_appState->shiftPressed = key.S;

  tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_KEY_UP, key.VK, x, y);
#if defined ENABLE_PROT_PLUGIN
  return GetDelegate()->CallbackInput(ev);
#else
  return tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif
}

bool IGraphicsTSL::OnMouseOver(float x, float y, const IMouseMod& mod)
{
#if defined(OS_IOS)
  x = x * GetScreenScale() - GetDelegate()->_appState->graphics.xOffset;
  y = y * GetScreenScale() - GetDelegate()->_appState->graphics.yOffset;
#else
  x = x * WindowWidth() / PLUG_WIDTH;
  y = y * WindowHeight() / PLUG_HEIGHT;
#endif
  tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_MOUSE_OVER, (int)mod.touchID, x, y);
#if defined ENABLE_PROT_PLUGIN
  return GetDelegate()->CallbackInput(ev);
#else
  return tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif

  /*

  // N.B. GetMouseControl handles which controls can receive mouseovers
  IControl* pControl = GetMouseControl(x, y, false, true);

  if (pControl != mMouseOver)
  {
    if (mMouseOver)
      mMouseOver->OnMouseOut();

    mMouseOver = pControl;
  }

  if (mMouseOver)
    mMouseOver->OnMouseOver(x, y, mod);

  return pControl;
  */
}

bool IGraphicsTSL::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
  d = d > 0 ? 1 : -1;
#if defined(OS_IOS)
  x = x * GetScreenScale() - GetDelegate()->_appState->graphics.xOffset;
  y = y * GetScreenScale() - GetDelegate()->_appState->graphics.yOffset;
#else
  x = x * WindowWidth() / PLUG_WIDTH;
  y = y * WindowHeight() / PLUG_HEIGHT;
#endif
  tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_MOUSE_WHEEL, d, x, y);
#if defined ENABLE_PROT_PLUGIN
  return GetDelegate()->CallbackInput(ev);
#else
  return tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif

  /*
  IControl* pControl = GetMouseControl(x, y, false);
  if (pControl)
    pControl->OnMouseWheel(x, y, mod, d);
    */
}

void IGraphicsTSL::OnMouseOut()
{
  IGraphics::OnMouseOut();
  tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_MOUSE_OUT, 0, 0, 0);
#if defined ENABLE_PROT_PLUGIN
  GetDelegate()->CallbackInput(ev);
#else
  tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif
}

void IGraphicsTSL::OnTouchCancelled(const std::vector<IMouseInfo>& points)
{
  for (auto& point : points)
  {
#if defined(OS_IOS)
    auto x = point.x * GetDrawScale() - GetDelegate()->_appState->graphics.xOffset;
    auto y = point.y * GetDrawScale() - GetDelegate()->_appState->graphics.yOffset;
#else
    auto x = point.x * WindowWidth() / PLUG_WIDTH;
    auto y = point.y * WindowHeight() / PLUG_HEIGHT;
#endif
    const IMouseMod& mod = point.ms;
    tsl::graphics::InputEvent ev(nullptr, tsl::graphics::ACTION_UP, (int)mod.touchID, x, y);
#if defined ENABLE_PROT_PLUGIN
    GetDelegate()->CallbackInput(ev);
#else
    tsl::graphics::callback_input(GetDelegate()->_appState, ev);
#endif
  }
}

/*
void IGraphics::OnTouchCancelled(const std::vector<IMouseInfo>& points)
{
º
  if (ControlIsCaptured())
  {
    // work out which of mCapturedMap controls the cancel relates to
    for (auto& point : points)
    {
      float x = point.x;
      float y = point.y;
      const IMouseMod& mod = point.ms;

      auto itr = mCapturedMap.find(mod.touchID);

      if (itr != mCapturedMap.end())
      {
        IControl* pCapturedControl = itr->second;
        pCapturedControl->OnTouchCancelled(x, y, mod);
        mCapturedMap.erase(mod.touchID); // remove from captured list

        //        DBGMSG("DEL - NCONTROLS captured = %lu\n", mCapturedMap.size());
      }
    }
  }
}

bool IGraphics::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  Trace("IGraphics::OnMouseOver", __LINE__, "x:%0.2f, y:%0.2f, mod:LRSCA: %i%i%i%i%i", x, y, mod.L, mod.R, mod.S, mod.C, mod.A);

  // N.B. GetMouseControl handles which controls can receive mouseovers
  IControl* pControl = GetMouseControl(x, y, false, true);

  if (pControl != mMouseOver)
  {
    if (mMouseOver)
      mMouseOver->OnMouseOut();

    mMouseOver = pControl;
  }

  if (mMouseOver)
    mMouseOver->OnMouseOver(x, y, mod);

  return pControl;
}

void IGraphics::OnMouseOut()
{
  Trace("IGraphics::OnMouseOut", __LINE__, "");

  // Store the old cursor type so this gets restored when the mouse enters again
  mCursorType = SetMouseCursor(ECursor::ARROW);
  ForAllControls(&IControl::OnMouseOut);
  ClearMouseOver();
}
*/

/*
bool IGraphics::OnMouseDblClick(float x, float y, const IMouseMod& mod)
{
  Trace("IGraphics::OnMouseDblClick", __LINE__, "x:%0.2f, y:%0.2f, mod:LRSCA: %i%i%i%i%i", x, y, mod.L, mod.R, mod.S, mod.C, mod.A);

  IControl* pControl = GetMouseControl(x, y, true);

  if (pControl)
  {
    if (pControl->GetMouseDblAsSingleClick())
    {
      IMouseInfo info;
      info.x = x;
      info.y = y;
      info.ms = mod;
      std::vector<IMouseInfo> list{info};
      OnMouseDown(list);
    }
    else
    {
      pControl->OnMouseDblClick(x, y, mod);
      ReleaseMouseCapture();
    }
  }

  return pControl;
}

void IGraphics::OnMouseWheel(float x, float y, const IMouseMod& mod, float d)
{
  IControl* pControl = GetMouseControl(x, y, false);
  if (pControl)
    pControl->OnMouseWheel(x, y, mod, d);
}

bool IGraphics::OnKeyDown(float x, float y, const IKeyPress& key)
{
  Trace("IGraphics::OnKeyDown", __LINE__, "x:%0.2f, y:%0.2f, key:%s", x, y, key.utf8);

  bool handled = false;

  IControl* pControl = GetMouseControl(x, y, false);

  if (pControl && pControl != GetControl(0))
    handled = pControl->OnKeyDown(x, y, key);

  if (!handled)
    handled = mKeyHandlerFunc ? mKeyHandlerFunc(key, false) : false;

  return handled;
}

bool IGraphics::OnKeyUp(float x, float y, const IKeyPress& key)
{
  Trace("IGraphics::OnKeyUp", __LINE__, "x:%0.2f, y:%0.2f, key:%s", x, y, key.utf8);

  bool handled = false;

  IControl* pControl = GetMouseControl(x, y, false);

  if (pControl && pControl != GetControl(0))
    handled = pControl->OnKeyUp(x, y, key);

  if (!handled)
    handled = mKeyHandlerFunc ? mKeyHandlerFunc(key, true) : false;

  return handled;
}

void IGraphics::OnDrop(const char* str, float x, float y)
{
  IControl* pControl = GetMouseControl(x, y, false);
  if (pControl)
    pControl->OnDrop(str);
}

void IGraphics::OnDropMultiple(const std::vector<const char*>& paths, float x, float y)
{
  IControl* pControl = GetMouseControl(x, y, false);
  if (pControl)
    pControl->OnDropMultiple(paths);
}
*/
END_IGRAPHICS_NAMESPACE
END_IPLUG_NAMESPACE
