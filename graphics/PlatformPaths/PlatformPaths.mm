#include <string>
#import <Foundation/Foundation.h>
#include "app.h"
#include "tools/PlatFormPaths.h"
std::string tsl::app::getAppSupportDir() {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(
        NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString *base = [paths firstObject];

    NSString *fullId = [[NSString stringWithUTF8String:tsl::app::bundleName]
                         stringByAppendingFormat:@".%@",
                         [[NSString stringWithUTF8String:tsl::app::appName] lowercaseString]];

    NSString *appDir = [base stringByAppendingPathComponent:fullId];

    NSFileManager *fm = [NSFileManager defaultManager];
    if (![fm fileExistsAtPath:appDir]) {
        [fm createDirectoryAtPath:appDir
      withIntermediateDirectories:YES
                       attributes:nil
                            error:nil];
    }
    return std::string([appDir UTF8String]);
}