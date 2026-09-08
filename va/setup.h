#ifndef SETUP_H
#define SETUP_H
namespace tsl {
    struct AppState;
}
void handler();
void crashHandler();
void initFailed();
void cleanUp(tsl::AppState*);
void sampleRateFromApp(tsl::AppState*, double sr);
#endif