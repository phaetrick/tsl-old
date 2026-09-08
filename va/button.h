#ifndef Button_H
#define Button_H

#include <types.h>
#include <cstring>
#include <ButtonBase.h>
#include <app.h>
#include <IconsMaterialDesignReduced.h>

namespace tsl {
    namespace graphics {

        class ButtonViewButton : public Button {
        public:
            ButtonViewButton(tsl::AppState* appState, const char *text) : Button(appState, WRAP, 0, CENTER_ALIGN, text, text) {};

            virtual void render(void *);

            virtual void callback(const InputEvent &) {};

            void computeWidth();
        };


        class NormalButton : public Button {
        public:
            NormalButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment, const char *_normal,
                         const char *_pressed, int32_t _id, long offset = 0, int multi = 1);

            NormalButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                         const char8_t _normal[4],
                         const char8_t _pressed[4], int32_t _id, long offset = 0, int multi = 1)
                    : NormalButton(appState, _scalefactor,
                                   _aspect_ratio,
                                   _alignment, reinterpret_cast<const char *>(_normal),
                                   reinterpret_cast<const char *>(_pressed), _id, offset, multi) {};

            virtual void render(void *ctx) override;

            virtual void callback(const InputEvent &e) override;
        };


        class BypassOffButton : public NormalButton {
        public:
            BypassOffButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                            const char *_normal,
                            const char *_pressed, int32_t _id) : NormalButton(appState, _scalefactor,
                                                                          _aspect_ratio,
                                                                          _alignment, _normal,
                                                                          _pressed,
                                                                          _id) {};

            BypassOffButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                            const char8_t _normal[4],
                            const char8_t _pressed[4], int32_t _id) : NormalButton(appState, _scalefactor,
                                                                               _aspect_ratio,
                                                                               _alignment, _normal,
                                                                               _pressed,
                                                                               _id) {};

            virtual void render(void *ctx) override;

            void callback(const InputEvent &e) override {
                if (state == DISABLED)
                    return;
                else
                    NormalButton::callback(e);
            }
        };

        class PermButton : public TextToggle {
        public:
            PermButton(tsl::AppState* appState, const char *text, int32_t id = 0) : TextToggle(appState, text, id){};
            void render(void *) override;
        };


        class RecordButton : public NormalButton {
        public:
            RecordButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment) : NormalButton(
                    appState,
                    _scalefactor,
                    _aspect_ratio,
                    _alignment,
                    ICON_MD_FIBER_MANUAL_RECORD,
                    ICON_MD_FIBER_MANUAL_RECORD,
                    RECORDButton) {};

            void render(void *ctx) override;
        };
    }
}

#endif
