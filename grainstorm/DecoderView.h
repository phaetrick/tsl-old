#pragma once
#include "types.h"
#include "view.h"
#include "audio/Recording.h"

namespace tsl {
    struct Decdata {
        Decdata() = delete;
        explicit Decdata(tsl::AppState* appState) : _appState{ appState } {
        }
        Decdata(tsl::AppState* appState, std::string fileName_, int sr_, int channels_)
            : _appState{ appState }, fileName{ std::move(fileName_) }, sr{ sr_ }, channels{ channels_ } {
        }
        std::string fileName{};
        size_t off{};
        std::atomic<size_t> offset{};
        std::vector<short> buffer[MAX_CHANNELS];
        int sr{}, channels{};
        std::string statusMessage{};
        tsl::AppState* _appState{};
		std::mutex mtx;
    };
    namespace graphics {
        class DecoderView : public View {
        public:
            DecoderView(tsl::AppState* appState) : View(appState), decData(appState) {
                prio = 0;
                perm = true;
            }
            void init() override;

            void render(void* ctx) override;

            void callback(const InputEvent& e) override;

            Decdata decData;

        protected:
            void addRecursiveCB() override {
                hasFocus.store(true);
                View::addRecursiveCB();
            };

            void delRecursiveCB() override {
                View::delRecursiveCB();
                hasFocus.store(false);
            };

            float lastx{}, lasty{};
            int32_t dragid{ -1 };
#ifndef USE_IMGUI
            int windex{ -1 };
#endif
            void delRecursiveDraw();
        };

    }
}
