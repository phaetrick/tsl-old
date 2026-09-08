#include "keydefines.h"
#include <cctype>
using namespace tsl::graphics;

#ifdef  ANDROID
#ifdef USE_IMGUI
#include <imgui.h>
#include <android/input.h>

int32_t ImGui_ImplAndroid_HandleInputEvent(AInputEvent* input_event) {
    ImGuiIO& io = ImGui::GetIO();
    int32_t event_type = AInputEvent_getType(input_event);
    switch (event_type) {
    case AINPUT_EVENT_TYPE_KEY: {
        int32_t event_key_code = AKeyEvent_getKeyCode(input_event);
        int32_t event_scan_code = AKeyEvent_getScanCode(input_event);
        int32_t event_action = AKeyEvent_getAction(input_event);
        int32_t event_meta_state = AKeyEvent_getMetaState(input_event);

        io.AddKeyEvent(ImGuiKey_ModCtrl, (event_meta_state & AMETA_CTRL_ON) != 0);
        io.AddKeyEvent(ImGuiKey_ModShift, (event_meta_state & AMETA_SHIFT_ON) != 0);
        io.AddKeyEvent(ImGuiKey_ModAlt, (event_meta_state & AMETA_ALT_ON) != 0);
        io.AddKeyEvent(ImGuiKey_ModSuper, (event_meta_state & AMETA_META_ON) != 0);

        switch (event_action) {
            // FIXME: AKEY_EVENT_ACTION_DOWN and AKEY_EVENT_ACTION_UP occur at once as soon as a touch pointer
            // goes up from a key. We use a simple key event queue/ and process one event per key per frame in
            // ImGui_ImplAndroid_NewFrame()...or consider using IO queue, if suitable: https://github.com/ocornut/imgui/issues/2787
        case AKEY_EVENT_ACTION_DOWN:
        case AKEY_EVENT_ACTION_UP: {
            ImGuiKey key = static_cast<ImGuiKey>(0);//ImGui_ImplAndroid_KeyCodeToImGuiKey(event_key_code);
            if (key != ImGuiKey_None && (event_action == AKEY_EVENT_ACTION_DOWN ||
                event_action == AKEY_EVENT_ACTION_UP)) {
                io.AddKeyEvent(key, event_action == AKEY_EVENT_ACTION_DOWN);
                io.SetKeyEventNativeData(key, event_key_code, event_scan_code);
            }

            break;
        }
        default:
            break;
        }
        break;
    }
    case AINPUT_EVENT_TYPE_MOTION: {
        int32_t event_action = AMotionEvent_getAction(input_event);
        int32_t event_pointer_index = (event_action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        event_action &= AMOTION_EVENT_ACTION_MASK;
        switch (event_action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_UP:
            // Physical mouse buttons (and probably other physical devices) also invoke the actions AMOTION_EVENT_ACTION_DOWN/_UP,
            // but we have to process them separately to identify the actual button pressed. This is done below via
            // AMOTION_EVENT_ACTION_BUTTON_PRESS/_RELEASE. Here, we only process "FINGER" input (and "UNKNOWN", as a fallback).
            if ((AMotionEvent_getToolType(input_event, event_pointer_index) ==
                AMOTION_EVENT_TOOL_TYPE_FINGER)
                || (AMotionEvent_getToolType(input_event, event_pointer_index) ==
                    AMOTION_EVENT_TOOL_TYPE_UNKNOWN)) {
                io.AddMousePosEvent(AMotionEvent_getX(input_event, event_pointer_index),
                    AMotionEvent_getY(input_event, event_pointer_index));
                io.AddMouseButtonEvent(0, event_action == AMOTION_EVENT_ACTION_DOWN);
            }
            break;
        case AMOTION_EVENT_ACTION_BUTTON_PRESS:
        case AMOTION_EVENT_ACTION_BUTTON_RELEASE: {
            int32_t button_state = AMotionEvent_getButtonState(input_event);
            io.AddMouseButtonEvent(0, (button_state & AMOTION_EVENT_BUTTON_PRIMARY) != 0);
            io.AddMouseButtonEvent(1, (button_state & AMOTION_EVENT_BUTTON_SECONDARY) != 0);
            io.AddMouseButtonEvent(2, (button_state & AMOTION_EVENT_BUTTON_TERTIARY) != 0);
        }
                                                break;
        case AMOTION_EVENT_ACTION_HOVER_MOVE: // Hovering: Tool moves while NOT pressed (such as a physical mouse)
        case AMOTION_EVENT_ACTION_MOVE:       // Touch pointer moves while DOWN
            io.AddMousePosEvent(AMotionEvent_getX(input_event, event_pointer_index),
                AMotionEvent_getY(input_event, event_pointer_index));
            break;
        case AMOTION_EVENT_ACTION_SCROLL:
            io.AddMouseWheelEvent(
                AMotionEvent_getAxisValue(input_event, AMOTION_EVENT_AXIS_HSCROLL,
                    event_pointer_index),
                AMotionEvent_getAxisValue(input_event, AMOTION_EVENT_AXIS_VSCROLL,
                    event_pointer_index));
            break;
        default:
            break;
        }
    }
                                 return 1;
    default:
        break;
    }

    return 0;
}



#endif

#include <android/keycodes.h>
#include <android/log.h>


tsl::graphics::KeyboardCode tsl::graphics::KeyCodeToVkey(int32_t key_code) {
    switch (key_code) {
    case AKEYCODE_TAB:
        return VKEY_TAB;
    case AKEYCODE_DPAD_LEFT:
        return VKEY_LEFT;
    case AKEYCODE_DPAD_RIGHT:
        return VKEY_RIGHT;
    case AKEYCODE_DPAD_UP:
        return VKEY_UP;
    case AKEYCODE_DPAD_DOWN:
        return VKEY_DOWN;
    case AKEYCODE_PAGE_UP:
        return VKEY_PRIOR;
    case AKEYCODE_PAGE_DOWN:
        return VKEY_NEXT;
    case AKEYCODE_MOVE_HOME:
        return VKEY_HOME;
    case AKEYCODE_MOVE_END:
        return VKEY_END;
    case AKEYCODE_INSERT:
        return VKEY_INSERT;
    case AKEYCODE_FORWARD_DEL:
        return VKEY_DELETE;
    case AKEYCODE_DEL:
        return VKEY_BACK;
    case AKEYCODE_SPACE:
        return VKEY_SPACE;
    case AKEYCODE_NUMPAD_ENTER:
    case AKEYCODE_ENTER:
        return VKEY_RETURN;
    case AKEYCODE_ESCAPE:
    case AKEYCODE_BACK:
        return VKEY_ESCAPE;
    case AKEYCODE_APOSTROPHE:
        return VKEY_OEM_7;
    case AKEYCODE_COMMA:
        return VKEY_OEM_COMMA;
    case AKEYCODE_MINUS:
        return VKEY_OEM_MINUS;
    case AKEYCODE_PERIOD:
        return VKEY_OEM_PERIOD;
    case AKEYCODE_SLASH:
        return VKEY_OEM_2;
    case AKEYCODE_SEMICOLON:
        return VKEY_OEM_1;
    case AKEYCODE_EQUALS:
        return VKEY_OEM_PLUS;
    case AKEYCODE_LEFT_BRACKET:
        return VKEY_OEM_4;
    case AKEYCODE_BACKSLASH:
        return VKEY_OEM_5;
    case AKEYCODE_RIGHT_BRACKET:
        return VKEY_OEM_6;
    case AKEYCODE_GRAVE:
        return VKEY_OEM_102;
    case AKEYCODE_CAPS_LOCK:
        return VKEY_CAPITAL;
    case AKEYCODE_SCROLL_LOCK:
        return VKEY_SCROLL;
    case AKEYCODE_NUM_LOCK:
        return VKEY_NUMLOCK;
    case AKEYCODE_SYSRQ:
        return VKEY_PRINT;
    case AKEYCODE_BREAK:
        return VKEY_PAUSE;
    case AKEYCODE_NUMPAD_0:
    case AKEYCODE_NUMPAD_1:
    case AKEYCODE_NUMPAD_2:
    case AKEYCODE_NUMPAD_3:
    case AKEYCODE_NUMPAD_4:
    case AKEYCODE_NUMPAD_5:
    case AKEYCODE_NUMPAD_6:
    case AKEYCODE_NUMPAD_7:
    case AKEYCODE_NUMPAD_8:
    case AKEYCODE_NUMPAD_9:
        return static_cast<KeyboardCode>(VKEY_NUMPAD0 + (key_code - AKEYCODE_NUMPAD_0));
    case AKEYCODE_NUMPAD_DOT:
        return VKEY_DECIMAL;
    case AKEYCODE_NUMPAD_DIVIDE:
        return VKEY_DIVIDE;
    case AKEYCODE_NUMPAD_MULTIPLY:
        return VKEY_MULTIPLY;
    case AKEYCODE_NUMPAD_SUBTRACT:
        return VKEY_SUBTRACT;
    case AKEYCODE_NUMPAD_ADD:
        return VKEY_ADD;
    case AKEYCODE_NUMPAD_EQUALS:
        return VKEY_OEM_PLUS;
    case AKEYCODE_CTRL_LEFT:
        return VKEY_CONTROL;
    case AKEYCODE_SHIFT_LEFT:
        return VKEY_SHIFT;
    case AKEYCODE_CTRL_RIGHT:
        return VKEY_CONTROL;
    case AKEYCODE_SHIFT_RIGHT:
        return VKEY_SHIFT;
    case AKEYCODE_MENU:
    case AKEYCODE_ALT_LEFT:
    case AKEYCODE_ALT_RIGHT:
    case AKEYCODE_META_LEFT:
    case AKEYCODE_META_RIGHT:
        return VKEY_MENU;
    case AKEYCODE_0:
    case AKEYCODE_1:
    case AKEYCODE_2:
    case AKEYCODE_3:
    case AKEYCODE_4:
    case AKEYCODE_5:
    case AKEYCODE_6:
    case AKEYCODE_7:
    case AKEYCODE_8:
    case AKEYCODE_9:
        return static_cast<KeyboardCode>(VKEY_0 + key_code - AKEYCODE_0);
    case AKEYCODE_A:
    case AKEYCODE_B:
    case AKEYCODE_C:
    case AKEYCODE_D:
    case AKEYCODE_E:
    case AKEYCODE_F:
    case AKEYCODE_G:
    case AKEYCODE_H:
    case AKEYCODE_I:
    case AKEYCODE_J:
    case AKEYCODE_K:
    case AKEYCODE_L:
    case AKEYCODE_M:
    case AKEYCODE_N:
    case AKEYCODE_O:
    case AKEYCODE_P:
    case AKEYCODE_Q:
    case AKEYCODE_R:
    case AKEYCODE_S:
    case AKEYCODE_T:
    case AKEYCODE_U:
    case AKEYCODE_V:
    case AKEYCODE_W:
    case AKEYCODE_X:
    case AKEYCODE_Y:
    case AKEYCODE_Z:
        return static_cast<KeyboardCode>(VKEY_A + key_code - AKEYCODE_A);
    case AKEYCODE_F1:
    case AKEYCODE_F2:
    case AKEYCODE_F3:
    case AKEYCODE_F4:
    case AKEYCODE_F5:
    case AKEYCODE_F6:
    case AKEYCODE_F7:
    case AKEYCODE_F8:
    case AKEYCODE_F9:
    case AKEYCODE_F10:
    case AKEYCODE_F11:
    case AKEYCODE_F12:
        return static_cast<KeyboardCode>(VKEY_F1 + (key_code - AKEYCODE_F1));
    default:
        return VKEY_UNKNOWN;
    }
}



#endif


char tsl::graphics::VKtoChar(int vk, bool shiftPressed) {
    if (vk >= VKEY_0 && vk <= VKEY_9)
        return '0' + vk - VKEY_0;
    else if ((vk >= VKEY_A && vk <= VKEY_Z))
        return shiftPressed ? 'A' + vk - VKEY_A : std::tolower('A' + vk - VKEY_A);
    switch (vk) {
    case VKEY_OEM_7:
        return '\'';
    case VKEY_OEM_COMMA:
        return ',';            // ,
    case VKEY_OEM_MINUS:
        return '-';
    case VKEY_OEM_PERIOD:
        return '.';// .
    case VKEY_OEM_2:
        return '/';
    case VKEY_OEM_1:
        return ';';
    case VKEY_OEM_PLUS:
        return shiftPressed ? '+' : '=';
    case VKEY_OEM_4:
        return '[';
    case VKEY_OEM_5:
        return '\\';
    case VKEY_OEM_6:
        return ']';
    case VKEY_OEM_102:
        return '`';
    default:
        return '\0';
    }
}
