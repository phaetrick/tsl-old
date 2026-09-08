#pragma once
#include "DecoderView.h"
#include <functional>
#include <string>

namespace tsl {
    namespace decoding {
        int decodeWithMediaFoundation(tsl::Decdata&, std::function<int(tsl::Decdata&, void*, uint64_t)> func);
    }
}



