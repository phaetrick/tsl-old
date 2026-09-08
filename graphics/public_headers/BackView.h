//
// Created by pr on 06.10.25.
//

#ifndef GRAINSTORM_BACKVIEW_H
#define GRAINSTORM_BACKVIEW_H
#include "view.h"
#include <functional>
namespace tsl {
    class AppState;
}
namespace tsl::graphics {

    class BackView : public View {
    public:
        BackView(tsl::AppState* appState, std::function<void()> cb);
        ~BackView();
        void callback(const InputEvent& e) override;
    protected:
        void addRecursiveCB() override;
        void delRecursiveCB() override;
    private:
        int32_t _state{}, pointerid{ -1 };
        float xpos{ -1 }, ypos{ -1 };
        std::function<void()> cb_{nullptr};
    };
}

#endif //GRAINSTORM_BACKVIEW_H
