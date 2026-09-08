#ifndef METER_H
#define METER_H
#include "view.h"

namespace tsl{
    namespace graphics{
class Meter : public View{
public:
    Meter(tsl::AppState* appState, float _scalefactor, int _aspect, int _align) : View(appState, _scalefactor, _aspect, _align, 10, true, "Meter"){};
    void render(void *ctx) override ;
    void rendervert(void *ctx);
};
    }
}
#endif
