#pragma once
#ifndef Button_H
#define Button_H

#include "defines.h"
#include "types.h"
#include <cstring>
#include <ButtonBase.h>
#include <IconsMaterialDesignReduced.h>
#include "params.h"

namespace tsl {
    namespace graphics {

        class NormalButton : public Button {
        public:
            NormalButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment, const char* _normal,
                const char* _pressed, int32_t _id, long offset = 0, int multi = 1);
#ifndef USE_CHAR_FOR_CHAR8_T
            NormalButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                const char8_t _normal[4],
                const char8_t _pressed[4], int32_t _id, long offset = 0, int multi = 1)
                : NormalButton(appState, _scalefactor,
                    _aspect_ratio,
                    _alignment, reinterpret_cast<const char*>(_normal),
                    reinterpret_cast<const char*>(_pressed), _id, offset, multi) {
            };
#endif // !USE_CHAR_FOR_CHAR8_T

            virtual void render(void* ctx) override;

            virtual void callback(const InputEvent& e) override;

            void addRecursiveDraw() override;

#ifdef PLUGIN_MODE
            void delRecursiveCB() override;
#endif
        protected:
			tsl::parameters::Event e{};
        };

        class TextButton2 : public TextButtonFramed {
        public:
            TextButton2(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                int32_t _id, const std::function<void()>& f = nullptr);

            void render(void*) override;

            void callback(const InputEvent& e) override;

            std::function<void()> func{};

        };

        class JoinEndsButton : public Button {
        public:
            JoinEndsButton(tsl::AppState* appState, float scale, int32_t as, int al, int _id) : Button(appState, scale, as, al,
                ICON_MD_CHECK_BOX,
                ICON_MD_CHECK_BOX) {
                id = _id;
            }

            void render(void* ctx) override;

            void callback(const InputEvent& e) override;
        };

        class MidilearnButton : public Button {
        public:
            MidilearnButton(tsl::AppState* appState, float _scale, int32_t as, int al) : Button(appState, _scale, as, al) {

            }

            void render(void* ctx) override;

            void callback(const InputEvent& e) override;
        };


        class SyncMidiButton : public Button {
        public:
            SyncMidiButton(tsl::AppState* appState, float sc, int32_t as, int al) : Button(appState, sc, as, al) {};

            void render(void* ctx) override;

            void callback(const InputEvent& e) override;
        };


        class ConvolverButton : public Button {
        public:
            ConvolverButton(tsl::AppState* appState, int32_t _trackindex) : Button(appState, WRAP, 0, CENTER_ALIGN) {
                trackindex = _trackindex;
            };

            void callback(const InputEvent& e) override;

            void render(void*) override;

        private:
            int32_t trackindex;
        };


        class EnvelopeButton : public Button {
        public:
            EnvelopeButton(tsl::AppState* appState, const char* text) : Button(appState) {
                name_normal = text;
                name_pressed = text;
                
            };

            void render(void*) override;

            void computeWidth() override {
                width = 2 * height;
            };
		protected:
			tsl::parameters::Event e{};

        };


        class MicButton : public NormalButton {
        public:
            MicButton(tsl::AppState* appState) : NormalButton(appState, WRAP, 0, CENTER_ALIGN, ICON_MD_MIC,
                ICON_MD_MIC, MICROPHONEButton) {
                ;
            }

            void render(void*) override;
        };

        class TrackButton : public Button {
        public:
            TrackButton(tsl::AppState* appState, float _scalefactor, int32_t _aspectratio, int _alignment, int _trackindex);

            void render(void* ctx) override;

            void callback(const InputEvent& e) override;

            static void func(tsl::AppState* _appState, int target);

        private:
            int32_t trackindex;
            static constexpr char tnames[5]{ "1234" };
        };


        class BypassOffButton : public NormalButton {
        public:
            BypassOffButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                const char* _normal,
                const char* _pressed, int32_t _id, int offset = 0, int offsetmulti = 1) : NormalButton(appState, _scalefactor,
                    _aspect_ratio,
                    _alignment, _normal,
                    _pressed,
                    _id, offset, offsetmulti) {
            };
#ifndef USE_CHAR_FOR_CHAR8_T

            BypassOffButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment,
                const char8_t _normal[4],
                const char8_t _pressed[4], int32_t _id, int offset = 0, int offsetmulti = 1) : NormalButton(appState, _scalefactor,
                    _aspect_ratio,
                    _alignment, _normal,
                    _pressed,
                    _id, offset, offsetmulti) {
            };
#endif
            virtual void render(void* ctx) override;

            void callback(const InputEvent& e) override {
                if (state == DISABLED)
                    return;
                else
                    NormalButton::callback(e);
            }
        };

        class RecordButton : public NormalButton {
        public:
            RecordButton(tsl::AppState* appState, float _scalefactor, int32_t _aspect_ratio, int _alignment) : NormalButton(appState,
                _scalefactor,
                _aspect_ratio,
                _alignment,
                ICON_MD_FIBER_MANUAL_RECORD,
                ICON_MD_FIBER_MANUAL_RECORD,
                RECORDButton) {
            };

            void render(void* ctx) override;
        };

        class ButtonSpaceSwitch : public Button {
        public:
            ButtonSpaceSwitch(tsl::AppState* appState, float sc, int32_t as, int al, int space);

            void render(void* ctx) override;

            void callback(const InputEvent& e) override;

            int32_t space{};
        };

        class ButtonEnvelopeSwitch : public Button {
        public:
            ButtonEnvelopeSwitch(tsl::AppState* appState, float sc, int32_t as, int al, const char* name,
                int32_t _type = SPACE_GRAINENV1)
                : Button(appState, sc, as, al, name, name) {
                type = _type;
                textpadding = 62.5f;
            }

            void render(void* ctx) override;

            void callback(const InputEvent& e) override;

            void func(tsl::AppState* _appState) const;

        private:
            int32_t type{};
        };

    }


}
#endif



