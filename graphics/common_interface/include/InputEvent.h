#pragma once
#include <cstdint>

namespace tsl {

    struct AppState;
    namespace graphics {
        class View;

        enum KeyboardType : uint32_t {
            TYPE_CLASS_TEXT = 0x00000001,
            TYPE_CLASS_NUMBER = 0x00000002,
            TYPE_NUMBER_FLAG_SIGNED = 0x00001000,
            TYPE_NUMBER_FLAG_DECIMAL = 0x00002000
        };

        enum inputtype_t {
            ACTION_DOWN = 0,
            ACTION_UP = 1,
            ACTION_MOVE = 2,
            ACTION_ENTER = 3,
            ACTION_LEAVE = 4,
            ACTION_KEY_DOWN = 5,
            ACTION_KEY_UP = 6,
            ACTION_MOUSE_OVER = 7,
            ACTION_MOUSE_OUT = 8,
            ACTION_MOUSE_WHEEL = 9
        };


        struct InputEvent {
            View* v{};
            int action{};
            int pointer_id{};
            int64_t time{};
            float x{}, y{};
            InputEvent() = default;

            InputEvent(View* _v, int _action, int _pointer_id, float _x, float _y) {
                v = _v;
                action = _action;
                pointer_id = _pointer_id;
                x = _x;
                y = _y;
            }
        };
	} // namespace graphics
} // namespace tsl