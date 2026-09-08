
#include "tools/aligned_memalloc.h"
#include "grainstorm.h"
#include "envelope.h"

DATA::DATA(tsl::AppState* s) : _appState(s) {
    activeSeg.store(-1);
}
