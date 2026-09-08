#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "app.h"

@interface TSLFilePicker : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, assign) tsl::AppState* appState;
@end

@implementation TSLFilePicker
- (void)documentPicker:(UIDocumentPickerViewController*)controller
didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    if (urls.count == 0) return;
    NSURL* url = urls[0];
    [url startAccessingSecurityScopedResource];
    if (self.appState->fileBrowserCallback)
        self.appState->fileBrowserCallback(url.path.UTF8String);
    [url stopAccessingSecurityScopedResource];
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
}
@end

void tsl::AppState::openFileBrowser() {
    dispatch_async(dispatch_get_main_queue(), ^{
        TSLFilePicker* picker = (__bridge TSLFilePicker*)this->iosFilePicker;
        if (!picker) {
            picker = [[TSLFilePicker alloc] init];
            this->iosFilePicker = (__bridge_retained void*)picker;
        }
        picker.appState = this;

        UIViewController* rootVC = [UIApplication sharedApplication].keyWindow.rootViewController;
        NSArray* types = @[UTTypeAudio.identifier, UTTypeMP3.identifier, 
                           UTTypeWAV.identifier, UTTypeAIFF.identifier];
        UIDocumentPickerViewController* docPicker =
            [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:
                @[UTTypeAudio] asCopy:YES];
        docPicker.delegate = picker;
        [rootVC presentViewController:docPicker animated:YES completion:nil];
    });
}