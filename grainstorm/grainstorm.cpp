
#include "grainstorm.h"
#include "filter.h"
#include <params.h>
DATA::~DATA() {
#ifdef IS_MULTITHREADED
	for (auto& thr : channelThreads)thr.stop();
#endif
	snapShot.shutdown();
}

static MYFLOAT *sinewave = nullptr, *cosinewave = nullptr, *fulltri = nullptr;

MYFLOAT *getsinewave() { return sinewave; }

void setsinewave(MYFLOAT *s) { sinewave = s; }

MYFLOAT *getcosinewave() { return cosinewave; }

void setcosinewave(MYFLOAT *s) { cosinewave = s; }

MYFLOAT *getfulltri() { return fulltri; }

void setfulltri(MYFLOAT *s) { fulltri = s; }



