#pragma once
//
// Created by pr on 10.10.23.
//

#ifndef POCKET_ANALOG_WINDOW_H
#define POCKET_ANALOG_WINDOW_H
#include "types.h"
#ifdef __ANDROID__

#include <android_native_app_glue.h>
#include <include/core/SkImage.h>

#endif
 namespace tsl::app {
        void loop(tsl::AppState* _appState, int w, int h);
        void setupthr(tsl::AppState* _appState);
        void resize(tsl::AppState* _appState, int w, int h);
#if defined NEW_UI
        void loop2(tsl::AppState* _appState, int w, int h);
        void setupthr2(tsl::AppState* _appState);
        void resize2(tsl::AppState* _appState, int w, int h);
#endif

#ifdef __ANDROID__


        extern void drawThreadGL(struct android_app *app);
#endif
    }



#endif //POCKET_ANALOG_WINDOW_H
