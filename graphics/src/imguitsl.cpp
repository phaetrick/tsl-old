/*
 ==============================================================================

 This file is part of the iPlug 2 library. Copyright (C) the iPlug 2 developers.

 See LICENSE.txt for  more info.

 ==============================================================================
*/

#include <imgui.h>
#include "imguitsl.h"

#define IGRAPHICS_GL3
#define IMGUI_IMPL_OPENGL_LOADER_GLAD
#if defined IGRAPHICS_GL2
  #include <../../sources/imgui/backends/imgui_impl_opengl2.cpp>
#elif defined IGRAPHICS_GL3 || defined IGRAPHICS_GLES2 || defined IGRAPHICS_GLES3
  #include <backends/imgui_impl_opengl3.cpp>
#else
  #if defined OS_MAC || defined OS_IOS
    #import <Metal/Metal.h>
    #import <QuartzCore/QuartzCore.h>
    #include <imgui_impl_metal.cpp>
  #else
    #error "ImGui is only supported on this platform using the OpenGL based backends"
  #endif
#endif

#ifdef __ANDROID__
#include <EGL/egl.h>
#include <GLES3/gl3.h>
;//#include <backends/imgui_impl_android.cpp>
#endif

#include <imgui.h>
#include <iostream>

void tsl::imgui::OnViewInitialized(){
   IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
#ifdef __ANDROID__
#define IMGUI_IMPL_OPENGL_ES3
#define IGRAPHICS_GLES3
//    const char* glsl_version = "#version 300 es";
//    io.IniFilename = nullptr;
//    io.BackendPlatformName = "imgui_impl_android";
#else
//    const char* glsl_version = "#version 150";
#endif
#if defined IGRAPHICS_GL2
    ImGui_ImplOpenGL2_Init();
#elif defined IGRAPHICS_GL3 || defined IGRAPHICS_GLES2 || defined IGRAPHICS_GLES3
    ImGui_ImplOpenGL3_Init(nullptr);
#endif

};

void tsl::imgui::BeginFrame(int width, int height, float fps){
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize.x = (float) width;
  io.DisplaySize.y = (float) height;
  io.Framerate = fps;
  float scale = 1.f;//(float) mGraphics->GetScreenScale();
  io.DisplayFramebufferScale = ImVec2(scale, scale);
#if defined IGRAPHICS_GL2
  ImGui_ImplOpenGL2_NewFrame();
#elif defined IGRAPHICS_GL3 || defined IGRAPHICS_GLES2 || defined IGRAPHICS_GLES3
  ImGui_ImplOpenGL3_NewFrame();
#endif
  ImGui::NewFrame();
}

void tsl::imgui::EndFrame(){
  ImGui::Render();
#if defined IGRAPHICS_GL2
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#elif defined IGRAPHICS_GL3 || defined IGRAPHICS_GLES2 || defined IGRAPHICS_GLES3
  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

void tsl::imgui::OnViewDestroyed(){
#if defined IGRAPHICS_GL2
    ImGui_ImplOpenGL2_Shutdown();
#elif defined IGRAPHICS_GL3 || defined IGRAPHICS_GLES2 || defined IGRAPHICS_GLES3
    ImGui_ImplOpenGL3_Shutdown();
#endif
    ImGui::DestroyContext();
};