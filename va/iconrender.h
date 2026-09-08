//
// Created by pr on 22.08.21.
//

#ifndef POCKET_ANALOG_ICONRENDER_H
#define POCKET_ANALOG_ICONRENDER_H


#include "view.h"
namespace tsl { namespace graphics{
class IconRender2 : public View{
public:
    IconRender2(float a, float b, float c):View(a,b,c){}
void render(void *ctx)override ;
private:

};
} }

#endif //POCKET_ANALOG_ICONRENDER_H
