#pragma once
#include <string>
#include <view.h>
#include <Input.h>
#include <atomic>
#include <memory>


namespace tsl {
    struct AppState;

    namespace graphics {
        class FloatingView : public View {
        public:
            explicit FloatingView(tsl::AppState* appState);
            FloatingView(tsl::AppState* appState, const char *title, bool isScroller = false);

            void callback(const InputEvent& e)override;
            void render(void*) override;
            void init()override;
			void setTitle(const char* t);
        protected:
            virtual void computeContent(int maxWidth, int maxHeight) = 0;
            virtual int cb(float xpos, float ypos, int action, int pid = 0) = 0;
            virtual void renderContent(void *) = 0;
            virtual void addRecursiveDraw() override;
            virtual void delRecursiveDraw() override;
            virtual void delRecursiveCB() override;
            int contentWidth{}, contentHeight{}, borderSize{}, rowHeight{};
        private:
            int _oldWindowWidth{ 0 }, _oldWindowHeight{ 0 }, windex{ -1 };
            std::atomic<float> x{}, y{};
            InputEvent olde{};
            std::atomic<int> active{ -1 };
            int contentStartY{};
			const char* title{};
			int tx{}, ty{};
			bool isScroller{};
        };
    }
}
