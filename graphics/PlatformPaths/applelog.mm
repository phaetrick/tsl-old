#import <Foundation/Foundation.h>
extern "C" void tsl_nslog(const char* msg) {
    NSLog(@"%s", msg);
}