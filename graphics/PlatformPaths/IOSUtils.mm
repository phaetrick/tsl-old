#include "tools/PlatformPaths.h"
#import <UIKit/UIKit.h>

void tsl::app::openURL(const std::string& url) {
    NSString* nsUrl = [NSString stringWithUTF8String:url.c_str()];
    NSURL* nsURL = [NSURL URLWithString:nsUrl];
    dispatch_async(dispatch_get_main_queue(), ^{
        [[UIApplication sharedApplication] openURL:nsURL
                                           options:@{}
                                 completionHandler:nil];
    });
}
