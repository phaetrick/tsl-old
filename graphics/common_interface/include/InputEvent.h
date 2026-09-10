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

        // Modifier snapshot for ACTION_KEY_* events, captured by the platform
        // at event time and carried WITH the event. This is the only modifier
        // channel for key routing — there is deliberately no global "shift is
        // down" state, so a key's meaning can never depend on the release
        // order of a chord or on a latch that missed an edge.
        // winuser.h defines MOD_SHIFT (0x0004) and MOD_ALT (0x0001) for
        // RegisterHotKey, so on Windows those names are macros and the
        // enumerators below expand to "0x0004 = 1" -- a syntax error. Nothing
        // here uses the hotkey API, so drop them. windows.h is include-guarded,
        // so once removed they cannot come back in this translation unit.
        // (MOD_CTRL is safe: Windows spells that one MOD_CONTROL.)
#ifdef _WIN32
#undef MOD_SHIFT
#undef MOD_ALT
#endif
        enum keymod_t : uint8_t {
            MOD_SHIFT = 1,
            MOD_ALT = 2,   // folds into MOD_SHIFT at the mapping site: no Alt layer exists
            MOD_CTRL = 4
        };

        // Effective "shifted" for a hardware key event, collapsed to the single
        // MOD_SHIFT bit the text input consumes: the event's own snapshot (Alt
        // folds into Shift) plus the on-screen keyboard's caps latch. The
        // transient shift latch is deliberately NOT an input here — it only
        // previews labels, so a latch that missed an edge can mislabel keys
        // but never mistype.
        inline uint8_t FoldHwShift(uint8_t mods, bool oskCaps) {
            return ((mods & (MOD_SHIFT | MOD_ALT)) != 0 || oskCaps)
                ? (uint8_t)MOD_SHIFT : (uint8_t)0;
        }

        struct InputEvent {
            View* v{};
            int action{};
            int pointer_id{};
            int64_t time{};
            float x{}, y{};
            uint8_t mods{};   // keymod_t bits; meaningful on ACTION_KEY_* only
            char keychar{};   // OS-translated printable ASCII (layout-aware) for
                              // hardware key events; 0 = derive from the vkey table
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