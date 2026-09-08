//
// Created by pr on 07.10.25.
//
#include "InsetsHandler.h"
#include "app.h"
#include <android_native_app_glue.h>
using namespace tsl::android;

InsetsHandler::InsetsHandler(tsl::AppState *appState) : _appState(_appState) {
        memset(&currentInsets, 0, sizeof(WindowInsets));

}

InsetsHandler::~InsetsHandler() {
}

void InsetsHandler::setupJNI(android_app* app) {
    nativeActivity = app->activity->clazz;
}

void InsetsHandler::queryInsets() {
    ATTACH
    if (!env || !nativeActivity) return;
    memset(&currentInsets, 0, sizeof(WindowInsets));

    jclass activityClass = env->GetObjectClass(nativeActivity);
    if (!activityClass) return;

    // Get Android API level
    jclass versionClass = env->FindClass("android/os/Build$VERSION");
    if (!versionClass) { env->DeleteLocalRef(activityClass); return; }
    jfieldID sdkIntField = env->GetStaticFieldID(versionClass, "SDK_INT", "I");
    if (!sdkIntField) { env->DeleteLocalRef(versionClass); env->DeleteLocalRef(activityClass); env->ExceptionClear(); return; }
    int apiLevel = env->GetStaticIntField(versionClass, sdkIntField);
    env->DeleteLocalRef(versionClass);

    // Get Window
    jmethodID getWinMid = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
    if (!getWinMid) { env->DeleteLocalRef(activityClass); env->ExceptionClear(); return; }
    jobject window = env->CallObjectMethod(nativeActivity, getWinMid);
    if (!window) { env->DeleteLocalRef(activityClass); return; }

    jclass windowClass = env->GetObjectClass(window);
    if (!windowClass) { env->DeleteLocalRef(window); env->DeleteLocalRef(activityClass); return; }

    // Check window flags for fullscreen
    jmethodID getAttrMid = env->GetMethodID(windowClass, "getAttributes", "()Landroid/view/WindowManager$LayoutParams;");
    if (getAttrMid) {
        jobject attributes = env->CallObjectMethod(window, getAttrMid);
        if (attributes) {
            jclass paramsClass = env->GetObjectClass(attributes);
            jfieldID flagsField = env->GetFieldID(paramsClass, "flags", "I");
            if (flagsField) {
                int windowFlags = env->GetIntField(attributes, flagsField);
                bool isFullscreen = (windowFlags & 0x00000400) != 0 || (windowFlags & 0x00000200) != 0;

                // Get display cutout information (API 28+)
                if (apiLevel >= 28) {
                    jmethodID getDecorMid = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
                    if (getDecorMid) {
                        jobject decorView = env->CallObjectMethod(window, getDecorMid);
                        if (decorView) {
                            jclass viewClass = env->GetObjectClass(decorView);
                            jmethodID getRootInsMid = env->GetMethodID(viewClass, "getRootWindowInsets", "()Landroid/view/WindowInsets;");
                            if (getRootInsMid) {
                                jobject windowInsets = env->CallObjectMethod(decorView, getRootInsMid);
                                if (windowInsets) {
                                    jclass insetsClass = env->GetObjectClass(windowInsets);
                                    jmethodID getCutoutMid = env->GetMethodID(insetsClass, "getDisplayCutout", "()Landroid/view/DisplayCutout;");
                                    if (getCutoutMid) {
                                        jobject displayCutout = env->CallObjectMethod(windowInsets, getCutoutMid);
                                        if (displayCutout) {
                                            jclass cutoutClass = env->GetObjectClass(displayCutout);
                                            jmethodID lMid = env->GetMethodID(cutoutClass, "getSafeInsetLeft", "()I");
                                            jmethodID tMid = env->GetMethodID(cutoutClass, "getSafeInsetTop", "()I");
                                            jmethodID rMid = env->GetMethodID(cutoutClass, "getSafeInsetRight", "()I");
                                            jmethodID bMid = env->GetMethodID(cutoutClass, "getSafeInsetBottom", "()I");

                                            if (lMid) currentInsets.displayCutoutLeft = env->CallIntMethod(displayCutout, lMid);
                                            if (tMid) currentInsets.displayCutoutTop = env->CallIntMethod(displayCutout, tMid);
                                            if (rMid) currentInsets.displayCutoutRight = env->CallIntMethod(displayCutout, rMid);
                                            if (bMid) currentInsets.displayCutoutBottom = env->CallIntMethod(displayCutout, bMid);

                                            env->DeleteLocalRef(cutoutClass);
                                            env->DeleteLocalRef(displayCutout);
                                        }
                                    } else env->ExceptionClear();
                                    env->DeleteLocalRef(insetsClass);
                                    env->DeleteLocalRef(windowInsets);
                                }
                            } else env->ExceptionClear();
                            env->DeleteLocalRef(viewClass);
                            env->DeleteLocalRef(decorView);
                        }
                    } else env->ExceptionClear();
                }
            } else env->ExceptionClear();
            env->DeleteLocalRef(paramsClass);
            env->DeleteLocalRef(attributes);
        }
    } else env->ExceptionClear();

    // Get resources for system bar heights (works on all APIs)
    jmethodID getResMid = env->GetMethodID(activityClass, "getResources", "()Landroid/content/res/Resources;");
    if (getResMid) {
        jobject resources = env->CallObjectMethod(nativeActivity, getResMid);
        if (resources) {
            jclass resourcesClass = env->GetObjectClass(resources);
            jmethodID getIdentMid = env->GetMethodID(resourcesClass, "getIdentifier", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I");
            if (getIdentMid) {
                jstring dimenType = env->NewStringUTF("dimen");
                jstring androidPkg = env->NewStringUTF("android");
                jstring statusBarName = env->NewStringUTF("status_bar_height");
                int statusBarResId = env->CallIntMethod(resources, getIdentMid, statusBarName, dimenType, androidPkg);

                if (statusBarResId > 0) {
                    jmethodID getDimMid = env->GetMethodID(resourcesClass, "getDimensionPixelSize", "(I)I");
                    if (getDimMid) currentInsets.top = env->CallIntMethod(resources, getDimMid, statusBarResId);
                    else env->ExceptionClear();
                }
                env->DeleteLocalRef(statusBarName);
                env->DeleteLocalRef(androidPkg);
                env->DeleteLocalRef(dimenType);
            } else env->ExceptionClear();
            env->DeleteLocalRef(resourcesClass);
            env->DeleteLocalRef(resources);
        }
    } else env->ExceptionClear();

    env->DeleteLocalRef(windowClass);
    env->DeleteLocalRef(window);
    env->DeleteLocalRef(activityClass);

    if (env->ExceptionCheck()) env->ExceptionClear();
    DETACH
}

void InsetsHandler::enableEdgeToEdge() {
    ATTACH

    // Get Window object
    jclass activityClass = env->GetObjectClass(nativeActivity);
    jmethodID getWindow = env->GetMethodID(activityClass, "getWindow",
                                              "()Landroid/view/Window;");
    jobject window = env->CallObjectMethod(nativeActivity, getWindow);

    // Set window flags for edge-to-edge
    jclass windowClass = env->GetObjectClass(window);
    jmethodID setDecorFitsSystemWindows = env->GetMethodID(windowClass,
                                                              "setDecorFitsSystemWindows", "(Z)V");
    env->CallVoidMethod(window, setDecorFitsSystemWindows, JNI_FALSE);

    env->DeleteLocalRef(window);
    env->DeleteLocalRef(activityClass);
    DETACH
}

const WindowInsets& InsetsHandler::getInsets() const {
    return currentInsets;
}

// Get safe rendering area (excluding system UI and cutouts)
void InsetsHandler::getSafeArea(int32_t windowWidth, int32_t windowHeight,
                 int32_t& outX, int32_t& outY,
                 int32_t& outWidth, int32_t& outHeight) const {
    int32_t maxLeft = std::max(currentInsets.left, currentInsets.displayCutoutLeft);
    int32_t maxTop = std::max(currentInsets.top, currentInsets.displayCutoutTop);
    int32_t maxRight = std::max(currentInsets.right, currentInsets.displayCutoutRight);
    int32_t maxBottom = std::max(currentInsets.bottom, currentInsets.displayCutoutBottom);

    outX = maxLeft;
    outY = maxTop;
    outWidth = windowWidth - maxLeft - maxRight;
    outHeight = windowHeight - maxTop - maxBottom;
}

