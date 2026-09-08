#include "view.h"
#include "logger.h"
#include "skia.h"
#include "colours.h"
#include "app.h"
#include <SkColorSpace.h>
#include "os/log.h"
#ifdef IGRAPHICS_METAL

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#if defined OS_IOS

#import <UIKit/UIKit.h>

#if defined OS_IOS
void tsl::app::setupPlatformMetrics(tsl::AppState* _appState) {
    UIScreen* screen = [UIScreen mainScreen];
    float scale = (float)screen.scale;
    _appState->mPpi = scale * 160.0f;
    // iOS touch coords are in logical points (already density-normalized),
    // so fling thresholds must be in points/s — no density scaling.
    _appState->mMinimumFlingVelocity = 50.0f;
    _appState->mMaximumFlingVelocity = 8000.0f;
}

bool tsl::app::isRunningAsAppExtension() {
    NSString* bundlePath = [[NSBundle mainBundle] bundleURL].path;
    return [bundlePath.pathExtension isEqualToString:@"appex"];
}

SafeInsets tsl::graphics::Graphics::getSafeInsets() {
    SafeInsets insets{};
    if (mMTLLayer) {
        CAMetalLayer* layer = (__bridge CAMetalLayer*)mMTLLayer;
        id delegate = layer.delegate;
        if (delegate && [delegate isKindOfClass:[UIView class]]) {
            UIView* view = (UIView*)delegate;
            UIEdgeInsets ui = view.safeAreaInsets;
            CGFloat scale = view.contentScaleFactor;
            insets.left   = (int)(ui.left * scale);
            insets.top    = (int)(ui.top * scale);
            insets.right  = (int)(ui.right * scale);
            insets.bottom = (int)(ui.bottom * scale);
            mInsetsQueried = true;  // NEW
           
        }
    }
    return insets;
}
#endif
#endif
void tsl::graphics::Graphics::init() {
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (device == nil) {
		return;
	}
	id<MTLCommandQueue> queue = [device newCommandQueue];
	mMTLDevice = (__bridge void*)device;
	mMTLCommandQueue = (__bridge void*)queue;

	GrMtlBackendContext backendContext = {};
	backendContext.fDevice.retain((__bridge GrMTLHandle)device);
	backendContext.fQueue.retain((__bridge GrMTLHandle)queue);

	grContext = GrDirectContexts::MakeMetal(backendContext);
}

void tsl::graphics::Graphics::BeginFrame() {
	if (grContext.get())
	{
		id<CAMetalDrawable> drawable = [(__bridge CAMetalLayer*)mMTLLayer nextDrawable];

		int width = (int)drawable.texture.width;
		int height = (int)drawable.texture.height;

		GrMtlTextureInfo fbInfo;
		fbInfo.fTexture.retain((__bridge const void*)(drawable.texture));
		auto backendRT = GrBackendRenderTargets::MakeMtl(width, height, fbInfo);
		backEndSurface = SkSurfaces::WrapBackendRenderTarget(grContext.get(), backendRT, kTopLeft_GrSurfaceOrigin, kBGRA_8888_SkColorType, nullptr, nullptr);
		backEndSurface->getCanvas()->clear(SK_ColorBLACK);

		mMTLDrawable = (__bridge void*)drawable;

	}
	else {
		NSLog(@"[TSL] BeginFrame: no grContext!");
	}
}

void tsl::graphics::Graphics::EndFrame() {
	flush();

	id<MTLCommandBuffer> commandBuffer = [(__bridge id<MTLCommandQueue>) mMTLCommandQueue commandBuffer];
	commandBuffer.label = @"Present";

	[commandBuffer presentDrawable : (__bridge id<CAMetalDrawable>) mMTLDrawable] ;
	[commandBuffer commit] ;

	backEndSurface = nullptr;
}

#endif // IGRAPHICS_METAL