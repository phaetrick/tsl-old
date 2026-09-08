#pragma once
#include "keyboard.h"
#include "view.h"
#include "Input.h"
#include "logger.h"
#include "keydefines.h"
#include <string>
#include <functional>
#include <vector>
#include <map>

namespace tsl {
    namespace graphics {

        enum class LabelPosition {
            LEFT = 0,    // Label to the left of input (default)
            ABOVE = 1    // Label above input
        };

        enum class FieldType {
            TEXT_INPUT = 0,
            CHECKBOX = 1,
            TEXT_DESCRIPTION = 2,  // Multi-line read-only text
            SLIDER = 3             // Horizontal slider, drawn like Slider (slider.cpp)
        };

        struct InputField {
            std::string label;
            TextInput input;
            bool isPassword;
            LabelPosition labelPosition;
            char maskChar;
            FieldType fieldType;

            // Checkbox specific
            bool isChecked;

            // Text description specific
            std::string description;
            std::vector<std::string> descriptionLines;
            int descriptionHeight;

            // Slider specific. Value model is a normalized position t in
            // [0,1]; the mapping to min..max is linear or logarithmic. The
            // input's geometry box doubles as the slider row, so layout and
            // hit testing reuse the existing field machinery.
            float sliderMin{0.f}, sliderMax{1.f};
            float sliderT{0.f}, sliderDefaultT{0.f};
            bool sliderLog{false};
            std::string sliderUnit;

            // Original constructor (for backward compatibility)
            InputField(tsl::AppState* appState, const std::string& lbl, bool password = false, LabelPosition pos = LabelPosition::LEFT)
                : label(lbl), input(appState, tsl::graphics::KeyboardType::TYPE_CLASS_TEXT), isPassword(password),
                labelPosition(pos), maskChar('*'), fieldType(FieldType::TEXT_INPUT), isChecked(false), descriptionHeight(0) {
                input.setTextAlign(TextInput::ALIGN_LEFT);
            }

            // New constructor with field type
            InputField(tsl::AppState* appState, const std::string& lbl, FieldType type, bool password = false, LabelPosition pos = LabelPosition::LEFT)
                : label(lbl), input(appState, tsl::graphics::KeyboardType::TYPE_CLASS_TEXT), isPassword(password),
                labelPosition(pos), maskChar('*'), fieldType(type), isChecked(false), descriptionHeight(0) {
                input.setTextAlign(TextInput::ALIGN_LEFT);
            }
        };

        class DynamicDialog : public View {
        public:
            struct DialogResult {
                bool confirmed;
                std::vector<std::string> values;
                std::map<std::string, std::string> namedValues; // label -> value
                std::map<std::string, bool> checkboxValues; // label -> checked state
                std::map<std::string, float> sliderValues; // label -> mapped value
            };
            enum class ButtonMode {
                NO_BUTTONS = 0,     // No buttons
                OK_ONLY = 1,        // Only OK button
                CANCEL_ONLY = 2,    // Only Cancel button  
                OK_CANCEL = 3,      // Both OK and Cancel buttons
                CUSTOM_BUTTON = 4   // Custom button(s)
            };
            using OnCompleteCallback = std::function<void(const DialogResult&)>;

            explicit DynamicDialog(tsl::AppState* appState);

            // View overrides
            void init() override;
            void render(void* c) override;
            void callback(const InputEvent& e) override;
            void addRecursiveCB() override { hasFocus.store(true); View::addRecursiveCB(); }
            void delRecursiveCB() override { hasFocus.store(false); View::delRecursiveCB(); }

            // Configuration
            void setTitle(const std::string& t) { title = t; needsFullRedraw = true; }
            void setOkCancelLabels(const std::string& ok, const std::string& cancel) {
                okLabel = ok;
                cancelLabel = cancel;
                needsFullRedraw = true;
            }

            // Dynamic field management
            void addInputField(const std::string& label, bool isPassword = false, LabelPosition labelPos = LabelPosition::LEFT);
            void addInputField(const std::string& label, const std::string& defaultValue, bool isPassword = false, LabelPosition labelPos = LabelPosition::LEFT);
            void addCheckbox(const std::string& label, bool defaultChecked = false, LabelPosition labelPos = LabelPosition::LEFT);
            void addTextDescription(const std::string& label, const std::string& description, LabelPosition labelPos = LabelPosition::ABOVE);
            // Slider row drawn to the Slider (slider.cpp) convention: filled
            // line left of a full-height position tick, label + live value to
            // the right of the track. Relative dragging; a quick tap resets to
            // defaultVal. logScale maps equal travel to equal factors.
            void addSlider(const std::string& label, float minVal, float maxVal,
                float defaultVal, bool logScale = false, const std::string& unit = "");
            float getSliderValue(const std::string& label) const;
            void setSliderValue(const std::string& label, float value);

            void clearFields();
            void setFieldValue(int index, const std::string& value);
            void setFieldValue(const std::string& label, const std::string& value);
            void setFieldMaxLength(const std::string& label, int maxLen);
            void setCheckboxValue(const std::string& label, bool checked);
            std::string getFieldValue(int index) const;
            std::string getFieldValue(const std::string& label) const;
            bool getCheckboxValue(const std::string& label) const;
            int getFieldCount() const { return static_cast<int>(fields.size()); }

            // Label layout configuration
            void setLabelWidth(int width) { labelWidth = width; } // For LEFT positioned labels
            void setLabelSpacing(int spacing) { labelSpacing = spacing; } // Gap between label and input

            // Button configuration - NEW METHODS
            void setButtonMode(ButtonMode mode) { buttonMode = mode; needsFullRedraw = true; }
            void setDrawButtonRect(bool draw) { drawButtonRect = draw; needsFullRedraw = true; }
            void setShowOkButton(bool show) { showOkButton = show; needsFullRedraw = true; }
            void setShowCancelButton(bool show) { showCancelButton = show; needsFullRedraw = true; }
            void setCustomButton(const std::string& label, std::function<void()> callback = nullptr);

            void setAutoSubmitOnLastField(bool autoSubmit) { autoSubmitOnLast = autoSubmit; }
            void setPasswordMaskChar(const std::string& label, char ch);

            // Callbacks
            OnCompleteCallback onCompleteCallback;
            void setShowKeyboard(bool show) { showKeyboard = show; }
            void setStaticMode(bool isStatic) { staticMode = isStatic; }
            void setTitlePadding(int padding) { titlePadding = padding; needsFullRedraw = true; }

        private:
            bool showKeyboard{ true };     // Show/hide keyboard
            bool staticMode{ false };      // Static display mode (no interaction)
            bool cancelOnClickoutside{ false };
            int titlePadding{ 16 };        // Padding for title text
            std::vector<std::string> titleLines; // Multi-line title support

            // Button configuration
            ButtonMode buttonMode{ ButtonMode::OK_CANCEL };
            bool showOkButton{ true };
            bool showCancelButton{ true };
            std::string customButtonLabel;
            std::function<void()> customButtonCallback;

            // Rendering optimization
            bool needsFullRedraw{ true };      // Flag for full redraw
            bool firstFrame{ true };           // First frame flag

            // Input handling state for your pattern
            struct PendingAction {
                enum Type {
                    PENDINGACTION_NONE = 0,
                    BUTTON_CLICK,
                    CHECKBOX_TOGGLE,
                    TEXT_FIELD_FOCUS,
                    SLIDER_DRAG
                } type = PENDINGACTION_NONE;

                int elementIndex = -1;      // Which button/checkbox/field was clicked
                float startX = 0.0f;        // Initial click position; running drag anchor for sliders
                float startY = 0.0f;
                int pointerId = -1;         // Which pointer/finger
                long long downMs = 0;       // Press time, for the slider tap-reset
                float totalDrag = 0.0f;     // Accumulated |dx| of a slider drag

                void clear() {
                    type = PENDINGACTION_NONE;
                    elementIndex = -1;
                    startX = startY = 0.0f;
                    pointerId = -1;
                    downMs = 0;
                    totalDrag = 0.0f;
                }

                bool isValid() const {
                    return type != PENDINGACTION_NONE && elementIndex >= 0;
                }
            };

            PendingAction pendingAction_;
            static constexpr float CANCEL_DISTANCE_THRESHOLD = 30.0f; // pixels

            // Helper methods for input pattern
            float getDistanceMoved(float currentX, float currentY) const;
            PendingAction::Type getElementAt(float x, float y, int& elementIndex);

            // New methods
            void wrapTitle();
            void wrapTextDescription(InputField& field);
            void drawMultilineTitle(SkCanvas* c);
            int calculateTitleHeight();
            int calculateFieldHeight(const InputField& field);

        protected:
            void addRecursiveDraw() override;
            void delRecursiveDraw() override;

        private:
            int windowindex{ -1 };
            int oldwindowindex{ -1 };
            float labelBaseLine{};
            std::string title{ "Input Dialog" };
            std::string okLabel{ "OK" };
            std::string cancelLabel{ "Cancel" };

            // Layout
            int x{}, y{}, w{}, h{};
            int fieldHeight{};
            int fieldSpacing{};
            int titleHeight{};
            int buttonHeight{};

            // Dynamic fields
            std::vector<std::unique_ptr<InputField>> fields;
            AlphaKeyboard keyboard;

            // Hardware auto-repeat state: the vkey currently held down, and
            // whether its repeats already typed (then the release is silent).
            int  hwHeldVkey_{ -1 };
            bool hwRepeated_{ false };

            // State
            int currentFieldIndex{ 0 };
            bool hasButtons{ true };
            bool drawButtonRect{ false };
            bool autoSubmitOnLast{ true };
            int labelWidth{ 120 }; // Default width for LEFT positioned labels
            int labelSpacing{ 8 }; // Gap between label and input
            char passwordMaskChar{ '*' }; // Default password mask character

            // Internal methods
            void layoutFields();
            void setFieldFocus(int fieldIndex);
            void handleConfirmCancel(int vkey);
            void handleTap(const InputEvent& e);
            void nextField();
            void prevField();
            TextInput* getCurrentInput();
            bool isFieldArea(const InputEvent& e, int fieldIndex);
            void drawFieldLabel(SkCanvas* c, InputField& field, int yPos);
            void drawField(SkCanvas* c, InputField& field, int fieldIndex);
            void drawCheckbox(SkCanvas* c, InputField& field, int yPos);
            void drawSlider(SkCanvas* c, InputField& field);
            static float sliderValueOf(const InputField& field);
            static float sliderTFromValue(const InputField& field, float value);
            float sliderTrackWidth(const InputField& field) const;
            static const char* sliderFormat(const InputField& field);
            void drawTextDescription(SkCanvas* c, InputField& field, int yPos);
            void drawButtons(SkCanvas* c);
            void submitDialog();
            void cancelDialog();
            bool hasVisibleButtons() const;
            int getButtonCount() const;

        };

    } // namespace graphics
} // namespace tsl