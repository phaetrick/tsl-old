//
// Created by pr on 07.10.25.
//

#ifndef GRAINSTORM_INSETSHANDLER_H
#define GRAINSTORM_INSETSHANDLER_H

#include <android_native_app_glue.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>


namespace tsl {
    class AppState;
    namespace android {
        struct WindowInsets {
            int32_t left;
            int32_t top;
            int32_t right;
            int32_t bottom;
            int32_t displayCutoutLeft;
            int32_t displayCutoutTop;
            int32_t displayCutoutRight;
            int32_t displayCutoutBottom;
        };

        class InsetsHandler {
        private:
            tsl::AppState *_appState;
            WindowInsets currentInsets;
            jobject nativeActivity;
        public:
            InsetsHandler(tsl::AppState *appState);

            void setupJNI(android_app* app);

            void queryInsets();

            void enableEdgeToEdge();

            const WindowInsets &getInsets() const;

            // Get safe rendering area (excluding system UI and cutouts)
            void getSafeArea(int32_t windowWidth, int32_t windowHeight,
                             int32_t &outX, int32_t &outY,
                             int32_t &outWidth, int32_t &outHeight) const;

            ~InsetsHandler();
        };

    }
}

#endif //GRAINSTORM_INSETSHANDLER_H
