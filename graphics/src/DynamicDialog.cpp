#include "DynamicDialog.h"
#include "app.h"
#include <IconsMaterialDesignReduced.h>
#include <include/core/SkFontMetrics.h>
#include <sstream>

using namespace tsl::graphics;

DynamicDialog::DynamicDialog(tsl::AppState* appState)
	: View(appState, WRAP, 0, CENTER_ALIGN, 0), keyboard(appState) {
	perm = true;
}

// Helper methods for input pattern
float DynamicDialog::getDistanceMoved(float currentX, float currentY) const {
	if (!pendingAction_.isValid()) return 0.0f;

	float dx = currentX - pendingAction_.startX;
	float dy = currentY - pendingAction_.startY;
	return sqrtf(dx * dx + dy * dy);
}

DynamicDialog::PendingAction::Type DynamicDialog::getElementAt(float x, float y, int& elementIndex) {
	// Convert to relative coordinates
	float relX = x - this->x;
	float relY = y - this->y;

	// Check buttons first
	if (hasVisibleButtons()) {
		const int buttonY = h - buttonHeight - fieldSpacing;
		const int buttonCount = getButtonCount();
		const int buttonW = (w - fieldSpacing * (buttonCount + 1)) / buttonCount;

		switch (buttonMode) {
		case ButtonMode::CANCEL_ONLY: {
			if (showCancelButton && relX >= fieldSpacing && relX <= fieldSpacing + buttonW &&
				relY >= buttonY && relY <= buttonY + buttonHeight) {
				elementIndex = 0;
				return PendingAction::BUTTON_CLICK;
			}
			break;
		}
		case ButtonMode::OK_ONLY: {
			if (showOkButton && relX >= fieldSpacing && relX <= fieldSpacing + buttonW &&
				relY >= buttonY && relY <= buttonY + buttonHeight) {
				elementIndex = 1;
				return PendingAction::BUTTON_CLICK;
			}
			break;
		}
		case ButtonMode::OK_CANCEL: {
			const int cancelX = w - buttonW * 2 - fieldSpacing;
			const int okX = w - buttonW - fieldSpacing / 2;

			if (showCancelButton && relX >= cancelX && relX <= cancelX + buttonW &&
				relY >= buttonY && relY <= buttonY + buttonHeight) {
				elementIndex = 0;
				return PendingAction::BUTTON_CLICK;
			}

			if (showOkButton && relX >= okX && relX <= okX + buttonW &&
				relY >= buttonY && relY <= buttonY + buttonHeight) {
				elementIndex = 1;
				return PendingAction::BUTTON_CLICK;
			}

			break;
		}
		case ButtonMode::CUSTOM_BUTTON: {
			if (!customButtonLabel.empty() && relX >= fieldSpacing && relX <= fieldSpacing + buttonW &&
				relY >= buttonY && relY <= buttonY + buttonHeight) {
				elementIndex = 2;
				return PendingAction::BUTTON_CLICK;
			}
			break;
		}
		case ButtonMode::OK_CANCEL_CUSTOM: {
			// Same three-across geometry as drawButtons: custom, cancel, ok.
			if (relY < buttonY || relY > buttonY + buttonHeight) break;
			const int customX = fieldSpacing;
			const int cancelX = fieldSpacing * 2 + buttonW;
			const int okX = fieldSpacing * 3 + buttonW * 2;
			if (!customButtonLabel.empty() && relX >= customX && relX <= customX + buttonW) {
				elementIndex = 2;
				return PendingAction::BUTTON_CLICK;
			}
			if (showCancelButton && relX >= cancelX && relX <= cancelX + buttonW) {
				elementIndex = 0;
				return PendingAction::BUTTON_CLICK;
			}
			if (showOkButton && relX >= okX && relX <= okX + buttonW) {
				elementIndex = 1;
				return PendingAction::BUTTON_CLICK;
			}
			break;
		}
		}
	}

	// Check input fields - create event with ABSOLUTE coordinates
	// because isFieldArea expects absolute coordinates and converts them internally
	for (size_t i = 0; i < fields.size(); ++i) {
		const auto& field = fields[i];

		if (field->fieldType == FieldType::CHECKBOX) {
			// Pass absolute coordinates (x, y) not relative
			if (isFieldArea(tsl::graphics::InputEvent(nullptr, 0, 0, x, y), static_cast<int>(i))) {
				elementIndex = static_cast<int>(i);
				return PendingAction::CHECKBOX_TOGGLE;
			}
		}
		else if (field->fieldType == FieldType::TEXT_INPUT) {
			// Pass absolute coordinates (x, y) not relative
			if (isFieldArea(tsl::graphics::InputEvent(nullptr, 0, 0, x, y), static_cast<int>(i))) {
				elementIndex = static_cast<int>(i);
				return PendingAction::TEXT_FIELD_FOCUS;
			}
		}
		else if (field->fieldType == FieldType::SLIDER) {
			if (isFieldArea(tsl::graphics::InputEvent(nullptr, 0, 0, x, y), static_cast<int>(i))) {
				elementIndex = static_cast<int>(i);
				return PendingAction::SLIDER_DRAG;
			}
		}
	}

	elementIndex = -1;
	return PendingAction::PENDINGACTION_NONE;
}
// Add these new methods to your DynamicDialog.cpp file:
void DynamicDialog::addCheckbox(const std::string& label, bool defaultChecked, LabelPosition labelPos) {
	auto field = std::make_unique<InputField>(_appState, label, FieldType::CHECKBOX, false, labelPos);
	field->isChecked = defaultChecked;
	fields.push_back(std::move(field));
	needsFullRedraw = true;
}

void DynamicDialog::addTextDescription(const std::string& label, const std::string& description, LabelPosition labelPos) {
	auto field = std::make_unique<InputField>(_appState, label, FieldType::TEXT_DESCRIPTION, false, labelPos);
	field->description = description;
	fields.push_back(std::move(field));
	needsFullRedraw = true;
}

float DynamicDialog::sliderValueOf(const InputField& field) {
	if (field.sliderLog)
		return field.sliderMin * powf(field.sliderMax / field.sliderMin, field.sliderT);
	return field.sliderMin + (field.sliderMax - field.sliderMin) * field.sliderT;
}

float DynamicDialog::sliderTFromValue(const InputField& field, float value) {
	const float v = std::clamp(value, field.sliderMin, field.sliderMax);
	if (field.sliderLog)
		return logf(v / field.sliderMin) / logf(field.sliderMax / field.sliderMin);
	return (v - field.sliderMin) / (field.sliderMax - field.sliderMin);
}

void DynamicDialog::addSlider(const std::string& label, float minVal, float maxVal,
	float defaultVal, bool logScale, const std::string& unit) {
	auto field = std::make_unique<InputField>(_appState, label, FieldType::SLIDER, false, LabelPosition::LEFT);
	field->sliderMin = minVal;
	field->sliderMax = maxVal;
	field->sliderLog = logScale && minVal > 0.f && maxVal > minVal;
	field->sliderUnit = unit;
	field->sliderDefaultT = sliderTFromValue(*field, defaultVal);
	field->sliderT = field->sliderDefaultT;
	fields.push_back(std::move(field));
	needsFullRedraw = true;
}

float DynamicDialog::getSliderValue(const std::string& label) const {
	for (const auto& field : fields)
		if (field->label == label && field->fieldType == FieldType::SLIDER)
			return sliderValueOf(*field);
	return 0.f;
}

void DynamicDialog::setSliderValue(const std::string& label, float value) {
	for (auto& field : fields) {
		if (field->label == label && field->fieldType == FieldType::SLIDER) {
			field->sliderT = sliderTFromValue(*field, value);
			break;
		}
	}
}

void DynamicDialog::setCheckboxValue(const std::string& label, bool checked) {
	for (auto& field : fields) {
		if (field->label == label && field->fieldType == FieldType::CHECKBOX) {
			field->isChecked = checked;
			needsFullRedraw = true;
			break;
		}
	}
}

bool DynamicDialog::getCheckboxValue(const std::string& label) const {
	for (const auto& field : fields) {
		if (field->label == label && field->fieldType == FieldType::CHECKBOX) {
			return field->isChecked;
		}
	}
	return false;
}

void DynamicDialog::setCustomButton(const std::string& label, std::function<void()> callback) {
	customButtonLabel = label;
	customButtonCallback = callback;
	buttonMode = ButtonMode::CUSTOM_BUTTON;
	needsFullRedraw = true;
}

bool DynamicDialog::hasVisibleButtons() const {
	switch (buttonMode) {
	case ButtonMode::NO_BUTTONS: {
		return false;
	}
	case ButtonMode::OK_ONLY: {
		return showOkButton;
	}
	case ButtonMode::CANCEL_ONLY: {
		return showCancelButton;
	}
	case ButtonMode::OK_CANCEL: {
		return showOkButton || showCancelButton;
	}
	case ButtonMode::CUSTOM_BUTTON: {
		return !customButtonLabel.empty();
	}
	case ButtonMode::OK_CANCEL_CUSTOM: {
		return showOkButton || showCancelButton || !customButtonLabel.empty();
	}
	default: {
		return false;
	}
	}
}

int DynamicDialog::getButtonCount() const {
	switch (buttonMode) {
	case ButtonMode::NO_BUTTONS: {
		return 0;
	}
	case ButtonMode::OK_ONLY: {
		return showOkButton ? 1 : 0;
	}
	case ButtonMode::CANCEL_ONLY: {
		return showCancelButton ? 1 : 0;
	}
	case ButtonMode::OK_CANCEL: {
		return (showOkButton ? 1 : 0) + (showCancelButton ? 1 : 0);
	}
	case ButtonMode::CUSTOM_BUTTON: {
		return !customButtonLabel.empty() ? 1 : 0;
	}
	case ButtonMode::OK_CANCEL_CUSTOM: {
		return (showOkButton ? 1 : 0) + (showCancelButton ? 1 : 0) +
		       (!customButtonLabel.empty() ? 1 : 0);
	}
	default: {
		return 0;
	}
	}
}

void DynamicDialog::wrapTextDescription(InputField& field) {
	field.descriptionLines.clear();
	if (field.description.empty()) {
		field.descriptionHeight = 0;
		return;
	}

	SkFont& font = _STATE->font_normal;
	font.setSize(_STATE->textsize2 * 0.85f);

	const int maxWidth = w - (fieldSpacing * 2);
	std::string currentLine = "";
	std::istringstream words(field.description);
	std::string word;

	while (std::getline(words, word, ' ')) {
		std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
		float lineWidth = font.measureText(testLine.c_str(), testLine.size(), SkTextEncoding::kUTF8);

		if (lineWidth <= maxWidth) {
			currentLine = testLine;
		}
		else {
			if (!currentLine.empty()) {
				field.descriptionLines.push_back(currentLine);
				currentLine = word;
			}
			else {
				field.descriptionLines.push_back(word);
			}
		}
	}

	if (!currentLine.empty()) {
		field.descriptionLines.push_back(currentLine);
	}

	const int lineHeight = static_cast<int>(_STATE->textsize2 * 1.0f);
	field.descriptionHeight = lineHeight * field.descriptionLines.size();
}

int DynamicDialog::calculateFieldHeight(const InputField& field) {
	switch (field.fieldType) {
	case FieldType::TEXT_INPUT: {
		return fieldHeight;
	}
	case FieldType::CHECKBOX: {
		// Same as every other row: layoutFields advances by fieldHeight, and
		// a smaller value here made the height sum lie per checkbox.
		return fieldHeight;
	}
	case FieldType::TEXT_DESCRIPTION: {
		return field.descriptionHeight;
	}
	case FieldType::SLIDER: {
		return fieldHeight;
	}
	default: {
		return fieldHeight;
	}
	}
}

// Add these drawing methods:
void DynamicDialog::drawField(SkCanvas* c, InputField& field, int fieldIndex) {
	switch (field.fieldType) {
	case FieldType::TEXT_INPUT:
		// Handle password masking
		if (field.isPassword) {
			std::string originalText = field.input.getText();
			std::string maskedText(originalText.length(), field.maskChar);
			field.input.setText(maskedText);
			field.input.render(c);
			field.input.setText(originalText);
		}
		else {
			field.input.render(c);
		}
		break;
	case FieldType::CHECKBOX:
		drawCheckbox(c, field, field.input.starty);
		break;
	case FieldType::TEXT_DESCRIPTION:
		drawTextDescription(c, field, field.input.starty);
		break;
	}
}

void DynamicDialog::drawCheckbox(SkCanvas* c, InputField& field, int yPos) {
	// The label is the static pass's job (drawFieldLabel), same as for text
	// inputs - this draws only the box, at the input rect layoutFields gave
	// the row (which already accounts for a label wider than labelWidth).
	const int checkboxSize = static_cast<int>(_STATE->textsize2 * 0.8f);
	const int checkboxX = field.labelPosition == LabelPosition::LEFT
		? field.input.startx.load() : fieldSpacing;
	const int checkboxY = yPos + (field.input.height - checkboxSize) / 2;

	// Draw checkbox border
	SkPaint borderPaint;
	borderPaint.setAntiAlias(true);
	borderPaint.setStyle(SkPaint::kStroke_Style);
	borderPaint.setStrokeWidth(2.0f);
	borderPaint.setColor(skcol::border);
	c->drawRect(SkRect::MakeXYWH(checkboxX, checkboxY, checkboxSize, checkboxSize), borderPaint);

	// Draw check mark if checked
	if (field.isChecked) {
		SkPaint checkPaint;
		checkPaint.setAntiAlias(true);
		checkPaint.setStyle(SkPaint::kStroke_Style);
		checkPaint.setStrokeWidth(2.0f);
		checkPaint.setColor(skcol::text);

		const float margin = checkboxSize * 0.2f;
		const float x1 = checkboxX + margin;
		const float y1 = checkboxY + checkboxSize * 0.5f;
		const float x2 = checkboxX + checkboxSize * 0.4f;
		const float y2 = checkboxY + checkboxSize - margin;
		const float x3 = checkboxX + checkboxSize - margin;
		const float y3 = checkboxY + margin;

		c->drawLine(x1, y1, x2, y2, checkPaint);
		c->drawLine(x2, y2, x3, y3, checkPaint);
	}
}

const char* DynamicDialog::sliderFormat(const InputField&) {
	// Always one decimal: a log slider moves in sub-unit steps at the low end,
	// and an integer display looks dead there.
	return "%s %.1f %s";
}

// The track takes what the text leaves over, capped at the conventional
// 72.5% (slider.cpp). Sized against the WIDEST possible label — max value,
// not current — so the track never shifts while dragging, and the text can
// never run off the dialog edge (it did, caught on screen).
float DynamicDialog::sliderTrackWidth(const InputField& field) const {
	char widest[64];
	snprintf(widest, sizeof(widest), sliderFormat(field),
		field.label.c_str(), field.sliderMax, field.sliderUnit.c_str());
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9f);
	const float wtxt = font.measureText(widest, strlen(widest), SkTextEncoding::kUTF8);
	float trackW = field.input.width - wtxt - _STATE->textsize2 * 1.2f;
	trackW = std::min(trackW, field.input.width * .725f);
	return std::max(trackW, _STATE->textsize2 * 2.f);
}

// Slider (slider.cpp) drawing convention: thin line track, filled (blue) left
// of the position, fg right of it, a full-row vertical tick as the handle,
// text to the right of the track. Label and live value share that text slot.
void DynamicDialog::drawSlider(SkCanvas* c, InputField& field) {
	const float rowY = field.input.starty;
	const float rowH = field.input.height;
	const float trackX0 = field.input.startx;
	const float trackW = sliderTrackWidth(field);
	const float trackX1 = trackX0 + trackW;
	const float cy = rowY + rowH * .5f;
	const float offset = trackX0 + trackW * field.sliderT;

	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setStrokeWidth(lw);
	paint.setColor(skcol::blue);
	c->drawLine(trackX0, cy, offset, cy, paint);
	paint.setColor(skcol::fg);
	c->drawLine(offset, cy, trackX1, cy, paint);
	c->drawLine(offset, rowY + 1, offset, rowY + rowH - 1, paint);

	char text[64];
	const float value = sliderValueOf(field);
	snprintf(text, sizeof(text), sliderFormat(field),
		field.label.c_str(), value, field.sliderUnit.c_str());
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * .9f);
	SkRect bounds{};
	font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);
	c->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8,
		trackX1 + _STATE->textsize2 * .9f,
		rowY + rowH - (rowH - bounds.height()) * .5f, font, paint);
}

void DynamicDialog::drawTextDescription(SkCanvas* c, InputField& field, int yPos) {
	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setColor(skcol::text);

	SkFont& font = _STATE->font_normal;
	font.setSize(_STATE->textsize2 * 0.85f);

	int currentY = yPos;
	const int lineHeight = static_cast<int>(_STATE->textsize2 * 1.0f);

	// Draw label if present
	if (!field.label.empty() && field.labelPosition == LabelPosition::ABOVE) {
		SkFont& labelFont = _STATE->font_normal;
		labelFont.setSize(_STATE->textsize2 * 0.9f);
		c->drawSimpleText(field.label.c_str(), field.label.size(), SkTextEncoding::kUTF8,
			fieldSpacing, currentY + static_cast<int>(_STATE->textsize2 * 0.7f), labelFont, paint);
		currentY += static_cast<int>(_STATE->textsize2) + fieldSpacing / 2;
	}

	// Draw description lines
	for (const std::string& line : field.descriptionLines) {
		c->drawSimpleText(line.c_str(), line.size(), SkTextEncoding::kUTF8,
			fieldSpacing, currentY + static_cast<int>(_STATE->textsize2 * 0.7f), font, paint);
		currentY += lineHeight;
	}
}

// Update your drawButtons method:
void DynamicDialog::drawButtons(SkCanvas* c) {
	if (!hasVisibleButtons()) return;

	const int buttonY = h - buttonHeight - fieldSpacing;
	const int buttonCount = getButtonCount();
	const int buttonW = (w - fieldSpacing * (buttonCount + 1)) / buttonCount;

	SkPaint paint;
	paint.setAntiAlias(true);
	SkFont font(_STATE->font_normal);
	font.setSize(_STATE->textsize2 * 0.9f);

	int currentX = fieldSpacing;

	switch (buttonMode) {
	case ButtonMode::OK_ONLY: {
		if (showOkButton) {
			// Draw OK button
			if (drawButtonRect) {
				paint.setStyle(SkPaint::kStroke_Style);
				paint.setStrokeWidth(1.0f);
				paint.setColor(skcol::text);
				c->drawRect(SkRect::MakeXYWH(currentX, buttonY, buttonW, buttonHeight), paint);
			}
			paint.setStyle(SkPaint::kFill_Style);
			paint.setColor(skcol::text);
			float textX, textY;
			measureTextFixed(buttonW, buttonHeight, font, okLabel.c_str(), &textX, &textY, _STATE->textsize2 * 0.9f);
			c->drawSimpleText(okLabel.c_str(), okLabel.size(), SkTextEncoding::kUTF8,
				currentX + textX, buttonY + textY, font, paint);
		}
		break;
	}
	case ButtonMode::CANCEL_ONLY: {
		if (showCancelButton) {
			// Draw Cancel button
			if (drawButtonRect) {
				paint.setStyle(SkPaint::kStroke_Style);
				paint.setStrokeWidth(1.0f);
				paint.setColor(skcol::text);
				c->drawRect(SkRect::MakeXYWH(currentX, buttonY, buttonW, buttonHeight), paint);
			}
			paint.setStyle(SkPaint::kFill_Style);
			paint.setColor(skcol::text);
			float textX, textY;
			measureTextFixed(buttonW, buttonHeight, font, cancelLabel.c_str(), &textX, &textY, _STATE->textsize2 * 0.9f);
			c->drawSimpleText(cancelLabel.c_str(), cancelLabel.size(), SkTextEncoding::kUTF8,
				currentX + textX, buttonY + textY, font, paint);
		}
		break;
	}
	case ButtonMode::OK_CANCEL: {
		if (showOkButton && showCancelButton) {
			const int cancelX = w - buttonW * 2 - fieldSpacing;
			const int okX = w - buttonW - fieldSpacing / 2;
			float textX, textY;

			paint.setStyle(SkPaint::kFill_Style);
			paint.setColor(skcol::text);

			// Cancel Button
			if (drawButtonRect) {
				paint.setStyle(SkPaint::kStroke_Style);
				c->drawRect(SkRect::MakeXYWH(cancelX, buttonY, buttonW, buttonHeight), paint);
				paint.setStyle(SkPaint::kFill_Style);
			}
			measureTextFixed(buttonW, buttonHeight, font, cancelLabel.c_str(), &textX, &textY, _STATE->textsize2 * 0.9f);
			c->drawSimpleText(cancelLabel.c_str(), cancelLabel.size(), SkTextEncoding::kUTF8,
				cancelX + textX, buttonY + textY, font, paint);

			// OK Button
			if (drawButtonRect) {
				paint.setStyle(SkPaint::kStroke_Style);
				c->drawRect(SkRect::MakeXYWH(okX, buttonY, buttonW, buttonHeight), paint);
				paint.setStyle(SkPaint::kFill_Style);
			}
			measureTextFixed(buttonW, buttonHeight, font, okLabel.c_str(), &textX, &textY, _STATE->textsize2 * 0.9f);
			c->drawSimpleText(okLabel.c_str(), okLabel.size(), SkTextEncoding::kUTF8,
				okX + textX, buttonY + textY, font, paint);
		}
		break;
	}
	case ButtonMode::CUSTOM_BUTTON: {
		if (!customButtonLabel.empty()) {
			// Draw custom button
			if (drawButtonRect) {
				paint.setStyle(SkPaint::kStroke_Style);
				paint.setStrokeWidth(1.0f);
				paint.setColor(skcol::text);
				c->drawRect(SkRect::MakeXYWH(currentX, buttonY, buttonW, buttonHeight), paint);
			}
			paint.setStyle(SkPaint::kFill_Style);
			paint.setColor(skcol::text);
			float textX, textY;
			measureTextFixed(buttonW, buttonHeight, font, customButtonLabel.c_str(), &textX, &textY, _STATE->textsize2 * 0.9f);
			std::string str("Ok");
			float x1, y1;

			measureTextFixed(buttonW, buttonHeight, font, str.c_str(), &x1, &y1, _STATE->textsize2 * 0.9f);

			c->drawSimpleText(customButtonLabel.c_str(), customButtonLabel.size(), SkTextEncoding::kUTF8,
				currentX + textX, buttonY + y1, font, paint);
		}
		break;
	}
	case ButtonMode::OK_CANCEL_CUSTOM: {
		// Three across, same geometry as the hit test in getElementAt:
		// custom | cancel | ok.
		const char* labels[3] = { customButtonLabel.c_str(), cancelLabel.c_str(),
		                          okLabel.c_str() };
		const bool visible[3] = { !customButtonLabel.empty(), showCancelButton,
		                          showOkButton };
		for (int i = 0; i < 3; i++) {
			if (!visible[i]) continue;
			const int bx = fieldSpacing * (i + 1) + buttonW * i;
			if (drawButtonRect) {
				paint.setStyle(SkPaint::kStroke_Style);
				paint.setStrokeWidth(1.0f);
				paint.setColor(skcol::text);
				c->drawRect(SkRect::MakeXYWH(bx, buttonY, buttonW, buttonHeight), paint);
			}
			paint.setStyle(SkPaint::kFill_Style);
			paint.setColor(skcol::text);
			float textX, textY;
			measureTextFixed(buttonW, buttonHeight, font, labels[i], &textX, &textY, _STATE->textsize2 * 0.9f);
			c->drawSimpleText(labels[i], strlen(labels[i]), SkTextEncoding::kUTF8,
				bx + textX, buttonY + textY, font, paint);
		}
		break;
	}
	}
}

// Update your submitDialog method to include checkbox values:
void DynamicDialog::submitDialog() {
	if (onCompleteCallback) {
		DialogResult result;
		result.confirmed = true;

		for (const auto& field : fields) {
			if (field->fieldType == FieldType::TEXT_INPUT) {
				std::string value = field->input.getText();
				result.values.push_back(value);
				result.namedValues[field->label] = value;
			}
			else if (field->fieldType == FieldType::CHECKBOX) {
				result.checkboxValues[field->label] = field->isChecked;
			}
			else if (field->fieldType == FieldType::SLIDER) {
				result.sliderValues[field->label] = sliderValueOf(*field);
			}
			// TEXT_DESCRIPTION fields don't have values to submit
		}

		onCompleteCallback(result);
	}
	else {
		delCB();
		deldraw();
	}
}

void DynamicDialog::wrapTitle() {
	titleLines.clear();
	if (title.empty()) return;

	SkFont& font = _STATE->font_normal;
	font.setSize(_STATE->textsize2 * 0.9f);

	const int maxWidth = w - (titlePadding * 2);
	std::string currentLine = "";
	std::istringstream words(title);
	std::string word;

	while (std::getline(words, word, ' ')) {
		std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
		float lineWidth = font.measureText(testLine.c_str(), testLine.size(), SkTextEncoding::kUTF8);

		if (lineWidth <= maxWidth) {
			currentLine = testLine;
		}
		else {
			if (!currentLine.empty()) {
				titleLines.push_back(currentLine);
				currentLine = word;
			}
			else {
				// Single word too long - force it on its own line
				titleLines.push_back(word);
			}
		}
	}

	if (!currentLine.empty()) {
		titleLines.push_back(currentLine);
	}
}

int DynamicDialog::calculateTitleHeight() {
	if (title.empty()) return 0;

	wrapTitle();
	// The RecyclerView title metric, the house convention: half a textsize of
	// outer margin, then a full row band (2.0 ts) holding the first line with
	// the text vertically centered; further lines add 1.2 ts each. Everything
	// derives from textsize2 so the dialog scales with the UI - the old fixed
	// 16 px padding was cramped and DPI-blind.
	const float ts = _STATE->textsize2;
	return static_cast<int>(ts * 0.5f + ts * 2.0f + ts * 1.2f * (titleLines.size() - 1));
}

void DynamicDialog::drawMultilineTitle(SkCanvas* c) {
	if (titleLines.empty()) return;

	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setColor(skcol::text);

	SkFont& font = _STATE->font_normal;
	font.setSize(_STATE->textsize2 * 0.9f);

	const int lineHeight = static_cast<int>(_STATE->textsize2 * 1.2f);
	const int totalTextHeight = lineHeight * static_cast<int>(titleLines.size());

	// Text block vertically centered inside the title band (below the outer
	// margin) - the RecyclerView convention; see calculateTitleHeight.
	const int margin = static_cast<int>(_STATE->textsize2 * 0.5f);
	int startY = margin + (titleHeight - margin - totalTextHeight) / 2;

	// Calculate available vertical space
	if (fields.empty()) {
		int availableHeight;
		if (hasVisibleButtons()) {
			// Space from top to start of buttons
			const int buttonY = h - buttonHeight - fieldSpacing;
			availableHeight = buttonY;
		}
		else {
			// Entire dialog height
			availableHeight = h;
		}

		// Center title vertically in available space
		startY = (availableHeight - totalTextHeight) / 2;
	}

	int currentY = startY + static_cast<int>(_STATE->textsize2 * 0.7f); // Start position

	// Calculate horizontal centering
	float textX = titlePadding;
	float maxLineWidth = 0.0f;
	for (const std::string& line : titleLines) {
		float lineWidth = font.measureText(line.c_str(), line.size(), SkTextEncoding::kUTF8);
		if (lineWidth > maxLineWidth) {
			maxLineWidth = lineWidth;
		}
	}
	textX = (w - maxLineWidth) * 0.5f;

	for (const std::string& line : titleLines) {
		c->drawSimpleText(line.c_str(), line.size(), SkTextEncoding::kUTF8,
			textX, currentY, font, paint);
		currentY += lineHeight;
	}
}
void DynamicDialog::init() {
	// Full window bookkeeping
	startx = 0;
	starty = 0;
	width = stopx = _STATE->windowWidth;
	height = stopy = _STATE->windowHeight;

	const bool portrait = (_STATE->windowWidth < _STATE->windowHeight);
	bool needsKeyboard = !fields.empty() && showKeyboard && !staticMode;

	// Dialog width — for landscape with keyboard, init keyboard first to get startx
	if (portrait) {
		w = static_cast<int>(_STATE->windowWidth * 0.85f);
		x = (_STATE->windowWidth - w) / 2;
	}
	else if (!portrait && needsKeyboard) {
		// Init keyboard early so we know startx before wrapping text
		const float naturalH = _STATE->textsize1 * 4.5f;
		const float hf = std::clamp(naturalH / (float)_STATE->windowHeight, 0.25f, 1.0f);
		keyboard.setSizeFractions(0.45f, hf);
		keyboard.alignment = AlphaKeyboard::RIGHT;
		keyboard.init();
		const float margin = _STATE->textsize2;
		w = static_cast<int>(keyboard.startx - 2 * margin);
		x = static_cast<int>(margin);
	}
	else {
		w = static_cast<int>(_STATE->windowWidth * 0.60f);
		x = (_STATE->windowWidth - w) / 2;
	}

	// Layout dimensions - all proportional to textsize2, never fixed pixels,
	// so the dialog keeps its shape across DPI and platforms.
	const int labelLine = static_cast<int>(_STATE->textsize2);
	titlePadding = static_cast<int>(_STATE->textsize2 * 0.5f);
	labelWidth = static_cast<int>(_STATE->textsize2 * 5.0f);
	labelSpacing = static_cast<int>(_STATE->textsize2 * 0.4f);
	titleHeight = calculateTitleHeight();
	fieldHeight = static_cast<int>(_STATE->textsize2 * 1.4f);
	fieldSpacing = static_cast<int>(_STATE->textsize2 * 0.7f);

	buttonHeight = hasVisibleButtons() ? static_cast<int>(_STATE->textsize2 * 2.0f) : 0;
	hasButtons = hasVisibleButtons(); // Update the old hasButtons flag

	// Calculate total height based on content
	int contentHeight = 0;

	if (fields.empty() && !hasVisibleButtons()) {
		contentHeight = titleHeight + (fieldSpacing * 4);
	}
	else {
		contentHeight = titleHeight;
		contentHeight += fieldSpacing * 2;

		for (auto& field : fields) {
			if (field->fieldType == FieldType::TEXT_DESCRIPTION) {
				wrapTextDescription(*field);
			}

			if (field->labelPosition == LabelPosition::ABOVE && field->fieldType != FieldType::TEXT_DESCRIPTION) {
				const int labelLine = static_cast<int>(_STATE->textsize2);
				contentHeight += labelLine + fieldSpacing / 2;
			}

			contentHeight += calculateFieldHeight(*field);
			contentHeight += fieldSpacing;
		}

		contentHeight += fieldSpacing * 2;

		if (hasVisibleButtons()) {
			contentHeight += fieldSpacing;
			contentHeight += buttonHeight;
			contentHeight += fieldSpacing;
		}
	}

	h = contentHeight;

	if (needsKeyboard) {
		if (portrait) {
			float wf = std::clamp(static_cast<float>(w) / static_cast<float>(_STATE->windowWidth), 0.30f, 0.98f);
			keyboard.setSizeFractions(wf, 0.36f);
			keyboard.alignment = AlphaKeyboard::BOTTOM;
			keyboard.init();
			y = (_STATE->windowHeight - keyboard.height - h) / 2;
			if (y < (int)(_STATE->textsize2 * 0.4f)) y = (int)(_STATE->textsize2 * 0.4f);
		} else {
			// Landscape: keyboard already initialized above; just position dialog vertically
			y = (_STATE->windowHeight - h) / 2;
			if (y < (int)(_STATE->textsize2 * 0.4f)) y = (int)(_STATE->textsize2 * 0.4f);
		}
	}
	else {
		// Center dialog without keyboard
		y = (_STATE->windowHeight - h) / 2;
		if (y < (int)(_STATE->textsize2 * 0.4f)) y = (int)(_STATE->textsize2 * 0.4f);
	}

	layoutFields();

	if (!fields.empty()) {
		auto& f = fields.at(0);
		SkFont font = _STATE->font_normal;
		font.setSize(_STATE->textsize2 * 0.9f);
		std::string str("SkjdfsdlkjKJ");
		float x1, y1;

		measureTextFixed(f->input.width, f->input.height, font, str.c_str(), &x1, &y1, _STATE->textsize2 * 0.9f);
		labelBaseLine = y1;
	}

	// Mark for redraw
	needsFullRedraw = true;
	firstFrame = true;
}

void DynamicDialog::render(void* /*ctx*/) {
	if (_STATE->windowWidth != width || _STATE->windowHeight != height) {
		init();
	}

	auto c = _STATE->graphics.getCanvas(windowindex, x, y, w, h);
	// Before the oldwindowindex bookkeeping on purpose: a frame that got no
	// surface must leave the index mismatch in place so the first good frame
	// still triggers the full redraw.
	if (!c)
		return;
	if (windowindex != oldwindowindex) {
		oldwindowindex = windowindex;
		needsFullRedraw = true;
	}

	// Only draw static content on first frame or when needed
	if (firstFrame || needsFullRedraw) {
		c->clear(sk_colours::wbg);

		// Draw multi-line title
		drawMultilineTitle(c);

		// Draw field labels (static parts)
		for (const auto& field : fields) {
			drawFieldLabel(c, *field, field->input.starty);
		}

		// Draw buttons
		drawButtons(c);

		// Draw border
		SkPaint borderPaint;
		borderPaint.setAntiAlias(true);
		borderPaint.setStyle(SkPaint::kStroke_Style);
		borderPaint.setStrokeWidth(1.0f);
		borderPaint.setColor(skcol::border);
		c->drawRect(SkRect::MakeXYWH(0.5f, 0.5f, w - 1.0f, h - 1.0f), borderPaint);

		firstFrame = false;
		needsFullRedraw = false;
	}

	// Always render input fields for cursor animation
	if (!fields.empty()) {
		for (const auto& field : fields) {
			// Clear the input field area
			SkPaint clearPaint;
			clearPaint.setColor(sk_colours::wbg);
			clearPaint.setStyle(SkPaint::kFill_Style);
			c->drawRect(SkRect::MakeXYWH(field->input.startx - 1, field->input.starty - 1,
				field->input.width + 2, field->input.height + 2), clearPaint);

			// Sliders live-render like inputs (drag moves the tick), but have
			// no TextInput to draw.
			if (field->fieldType == FieldType::SLIDER) {
				drawSlider(c, *field);
				continue;
			}
			// Same for checkboxes and descriptions: rendering their dormant
			// TextInput instead painted an empty input box over the row (and
			// over the label) - drawCheckbox was never reached from here.
			if (field->fieldType == FieldType::CHECKBOX) {
				drawCheckbox(c, *field, field->input.starty);
				continue;
			}
			if (field->fieldType == FieldType::TEXT_DESCRIPTION) {
				drawTextDescription(c, *field, field->input.starty);
				continue;
			}

			// Handle password masking
			if (field->isPassword) {
				std::string originalText = field->input.getText();
				std::string maskedText(originalText.length(), field->maskChar);
				field->input.setText(maskedText);
				field->input.render(c);
				field->input.setText(originalText);
			}
			else {
				field->input.render(c);
			}
		}
	}

	// Only render keyboard if needed
	if (!fields.empty() && showKeyboard && !staticMode) {
		keyboard.render(nullptr);

		// Held-key repeat pulse (on-screen hold): honours the on-screen latch
		if (int rk = keyboard.consumeRepeatKeyPulse(); rk >= 0) {
			InputEvent rep{};
			rep.action = ACTION_KEY_UP;
			rep.pointer_id = rk;
			rep.mods = (keyboard.isShift() || keyboard.isCaps()) ? MOD_SHIFT : 0;
			if (auto current = getCurrentInput()) {
				current->callback(rep);
			}
		}
	}
}

void DynamicDialog::callback(const InputEvent& e) {
	// In static mode, only allow dismissal by clicking outside
	if (e.action == ACTION_DOWN && cancelOnClickoutside &&
		(e.x < x || e.x > x + w || e.y < y || e.y > y + h)) {
		cancelDialog();
		return;
	}
	else if (staticMode) {
		return;
	}

	// Implement your input pattern here
	switch (e.action) {
	case ACTION_DOWN: {
		int elementIndex = -1;
		PendingAction::Type actionType = getElementAt(e.x, e.y, elementIndex);
		if (actionType != PendingAction::PENDINGACTION_NONE) {
			// Store the pending action
			pendingAction_.type = actionType;
			pendingAction_.elementIndex = elementIndex;
			pendingAction_.startX = e.x;
			pendingAction_.startY = e.y;
			pendingAction_.pointerId = e.pointer_id;
			pendingAction_.downMs = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now().time_since_epoch()).count();

			// Visual feedback could be added here if desired
			if (actionType == PendingAction::BUTTON_CLICK) {
				// Could highlight button
				needsFullRedraw = true;
			}
			else if (actionType == PendingAction::TEXT_FIELD_FOCUS) {
				auto& field = fields[pendingAction_.elementIndex];
				setFieldFocus(pendingAction_.elementIndex);
				InputEvent localEvent = e;
				localEvent.x = e.x - x;
				localEvent.y = e.y - y;
				field->input.callback(localEvent);
			}

		}
		break;
	}

	case ACTION_MOVE: {
		if (pendingAction_.isValid() && e.pointer_id == pendingAction_.pointerId) {
			// Sliders drag RELATIVE (the slider.cpp convention: the thumb
			// never jumps to the finger), with startX as the running anchor.
			// They are exempt from the drag-cancel threshold — the drag IS
			// the interaction.
			if (pendingAction_.type == PendingAction::SLIDER_DRAG) {
				auto& field = fields[pendingAction_.elementIndex];
				const float trackW = sliderTrackWidth(*field);
				if (trackW > 0.f) {
					field->sliderT = std::clamp(
						field->sliderT + (e.x - pendingAction_.startX) / trackW, 0.f, 1.f);
					pendingAction_.totalDrag += fabsf(e.x - pendingAction_.startX);
					pendingAction_.startX = e.x;
				}
				break;
			}
			float distanceMoved = getDistanceMoved(e.x, e.y);

			if (distanceMoved > _STATE->textsize2) {
				// User dragged too far - cancel the pending action
				pendingAction_.clear();
				needsFullRedraw = true;
			}
		}
		break;
	}

	case ACTION_UP: {
		// A release on the on-screen keyboard belongs to the KEYBOARD, and the
		// keyboard window lies OUTSIDE the dialog rect: letting it fall through
		// to the tap fallback below read as "tap outside" and CANCELLED the
		// dialog while the key was still being delivered - the teardown ran
		// under this very callback, so the key event misfired into freed state
		// (caught live on mac: every on-screen key click closed the dialog on
		// up, and ':' arrived as '<'). Route it AlphaPopUp's way: keyboard
		// first. A valid pendingAction means the gesture STARTED on a dialog
		// element (e.g. a slider drag ending over the keyboard) - that release
		// stays with the dialog.
		if (!fields.empty() && showKeyboard && !staticMode && !pendingAction_.isValid()) {
			int ret = keyboard.cb(e);
			if (ret != -1) {
				if (ret == -2) return; // consumed visually
				if (ret == VKEY_SHIFT || ret == VKEY_CAPITAL) return; // latch lives in the keyboard
				if (ret == VKEY_RETURN || ret == VKEY_ESCAPE || ret == VKEY_TAB ||
					ret == VKEY_UP || ret == VKEY_DOWN) {
					handleConfirmCancel(ret);
					return;
				}
				InputEvent e2 = e;
				e2.action = ACTION_KEY_UP;
				e2.pointer_id = ret;
				// On-screen keys carry the latch state with the injected event
				e2.mods = (keyboard.isShift() || keyboard.isCaps()) ? MOD_SHIFT : 0;
				if (auto current = getCurrentInput()) {
					current->callback(e2);
				}
				// One-shot shift
				if (keyboard.isShift() && !keyboard.isCaps())
					keyboard.toggleShift(false);
				return;
			}
		}
		if (pendingAction_.isValid() && e.pointer_id == pendingAction_.pointerId) {
			// Execute the pending action
			switch (pendingAction_.type) {
			case PendingAction::BUTTON_CLICK: {
				// Handle button clicks based on elementIndex
				if (pendingAction_.elementIndex == 0) { // Cancel button
					cancelDialog();
				}
				else if (pendingAction_.elementIndex == 1) { // OK button
					submitDialog();
				}
				else if (pendingAction_.elementIndex == 2) { // Custom button
					if (customButtonCallback) {
						customButtonCallback();
					}
				}
				break;
			}

			case PendingAction::CHECKBOX_TOGGLE: {
				if (pendingAction_.elementIndex < static_cast<int>(fields.size())) {
					auto& field = fields[pendingAction_.elementIndex];
					if (field->fieldType == FieldType::CHECKBOX) {
						field->isChecked = !field->isChecked;
						needsFullRedraw = true;
					}
				}
				break;
			}

			case PendingAction::TEXT_FIELD_FOCUS: {
				if (pendingAction_.elementIndex < static_cast<int>(fields.size())) {
					auto& field = fields[pendingAction_.elementIndex];
					if (field->fieldType == FieldType::TEXT_INPUT) {
						setFieldFocus(pendingAction_.elementIndex);
						InputEvent localEvent = e;
						localEvent.x = e.x - x;
						localEvent.y = e.y - y;
						field->input.callback(localEvent);
					}
				}
				break;
			}

			case PendingAction::SLIDER_DRAG: {
				// The slider.cpp convention: a quick tap resets to default.
				// "Tap" needs BOTH short time and no real travel — on time
				// alone a fast flick reads as a tap and resets the value it
				// just set (caught live with a synthetic drag).
				if (pendingAction_.elementIndex < static_cast<int>(fields.size())) {
					auto& field = fields[pendingAction_.elementIndex];
					const long long nowMs =
						(long long)std::chrono::duration_cast<std::chrono::milliseconds>(
							std::chrono::steady_clock::now().time_since_epoch()).count();
					if (field->fieldType == FieldType::SLIDER &&
						nowMs - pendingAction_.downMs < 300 &&
						pendingAction_.totalDrag < _STATE->textsize2 * .5f)
						field->sliderT = field->sliderDefaultT;
				}
				break;
			}
			}

			pendingAction_.clear();
		}
		else {
			// Fallback to old behavior if no pending action
			handleTap(e);
		}
		break; // fall through to keyboard handling below
	}
	}

	// Normal interaction mode - only handle keyboard if we have fields and keyboard is enabled
	if (!fields.empty() && showKeyboard) {
		// Handle on-screen keyboard
		int ret = keyboard.cb(e);
		if (ret != -1) {
			if (ret == -2) return; // consumed visually

			if (ret == VKEY_SHIFT || ret == VKEY_CAPITAL) return; // latch lives in the keyboard

			if (ret == VKEY_RETURN || ret == VKEY_ESCAPE || ret == VKEY_TAB ||
				ret == VKEY_UP || ret == VKEY_DOWN) {
				handleConfirmCancel(ret);
				return;
			}

			InputEvent e2 = e;
			e2.action = ACTION_KEY_UP;
			e2.pointer_id = ret;
			// On-screen keys carry the latch state with the injected event
			e2.mods = (keyboard.isShift() || keyboard.isCaps()) ? MOD_SHIFT : 0;

			if (auto current = getCurrentInput()) {
				current->callback(e2);
			}

			// One-shot shift
			if (keyboard.isShift() && !keyboard.isCaps())
				keyboard.toggleShift(false);
			return;
		}

		// Hardware keyboard: edits (characters, backspace, arrows) commit on
		// KEY_DOWN with THAT event's own modifier snapshot — OS auto-repeats
		// each type, and chord release order cannot flip the character.
		// Modifier edges only drive the on-screen label latch (preview).
		// Return/Escape/Tab/field-nav stay on KEY_UP so a press that closes
		// the dialog cannot leak its release into the view focused next.
		if (e.action == ACTION_KEY_DOWN) {
			keyboard.setHardwareKeyHighlight(e.pointer_id, true);

			// Alt latches like Shift: the layouts have no Alt layer
			if (e.pointer_id == VKEY_SHIFT || e.pointer_id == VKEY_LSHIFT || e.pointer_id == VKEY_RSHIFT ||
				e.pointer_id == VKEY_MENU || e.pointer_id == VKEY_LMENU || e.pointer_id == VKEY_RMENU) {
				if (!keyboard.isShift()) keyboard.toggleShift(true);
				return;
			}
			if (e.pointer_id == VKEY_CAPITAL) {
				return;
			}
			if (KeyEventEdits(e.pointer_id, e.keychar)) {
				InputEvent e2 = e;
				e2.action = ACTION_KEY_UP;
				e2.mods = FoldHwShift(e.mods, keyboard.isCaps());
				if (auto current = getCurrentInput()) {
					current->callback(e2);
				}
			}
			return;
		}
		else if (e.action == ACTION_KEY_UP) {
			keyboard.setHardwareKeyHighlight(e.pointer_id, false);

			if (e.pointer_id == VKEY_SHIFT || e.pointer_id == VKEY_LSHIFT || e.pointer_id == VKEY_RSHIFT ||
				e.pointer_id == VKEY_MENU || e.pointer_id == VKEY_LMENU || e.pointer_id == VKEY_RMENU) {
				if (keyboard.isShift()) keyboard.toggleShift(false);
				return;
			}

			if (e.pointer_id == VKEY_CAPITAL) {
				keyboard.toggleCaps();
				return;
			}

			if (e.pointer_id == VKEY_RETURN || e.pointer_id == VKEY_ESCAPE ||
				e.pointer_id == VKEY_TAB || e.pointer_id == VKEY_UP || e.pointer_id == VKEY_DOWN) {
				handleConfirmCancel(e.pointer_id, (e.mods & (MOD_SHIFT | MOD_ALT)) != 0);
				return;
			}
			return; // typing already happened on KEY_DOWN
		}
	}
	else {
		// No on-screen keyboard - hardware keys drive the fields directly.
		// Same convention as above: edits on KEY_DOWN, confirm/nav on KEY_UP.
		if (!fields.empty() && e.action == ACTION_KEY_DOWN) {
			if (KeyEventEdits(e.pointer_id, e.keychar)) {
				InputEvent e2 = e;
				e2.action = ACTION_KEY_UP;
				e2.mods = FoldHwShift(e.mods, false);
				if (auto current = getCurrentInput()) {
					current->callback(e2);
				}
			}
			return;
		}
		if (!fields.empty() && e.action == ACTION_KEY_UP) {
			if (e.pointer_id == VKEY_RETURN || e.pointer_id == VKEY_ESCAPE ||
				e.pointer_id == VKEY_TAB || e.pointer_id == VKEY_UP || e.pointer_id == VKEY_DOWN) {
				handleConfirmCancel(e.pointer_id, (e.mods & (MOD_SHIFT | MOD_ALT)) != 0);
				return;
			}
			return; // typing already happened on KEY_DOWN
		}
	}
}

// Keep the original handleTap as fallback
void DynamicDialog::handleTap(const InputEvent& e) {
	// Check if tap is outside dialog
	if (e.x < x || e.x > x + w || e.y < y || e.y > y + h) {
		// The on-screen keyboard is its own window outside the dialog rect;
		// a release there (e.g. between two keys) is not a dismissal.
		if (!fields.empty() && showKeyboard && !staticMode &&
			e.x >= keyboard.startx && e.x <= keyboard.stopx &&
			e.y >= keyboard.starty && e.y <= keyboard.stopy)
			return;
		cancelDialog();
		return;
	}

	// Check button area
	if (hasVisibleButtons()) {
		const int buttonY = y + h - buttonHeight - fieldSpacing;
		const int buttonCount = getButtonCount();
		const int buttonW = (w - fieldSpacing * (buttonCount + 1)) / buttonCount;

		switch (buttonMode) {
		case ButtonMode::OK_ONLY: {
			if (showOkButton && e.x >= x + fieldSpacing && e.x <= x + fieldSpacing + buttonW &&
				e.y >= buttonY && e.y <= buttonY + buttonHeight) {
				submitDialog();
				return;
			}
			break;
		}
		case ButtonMode::CANCEL_ONLY: {
			if (showCancelButton && e.x >= x + fieldSpacing && e.x <= x + fieldSpacing + buttonW &&
				e.y >= buttonY && e.y <= buttonY + buttonHeight) {
				cancelDialog();
				return;
			}
			break;
		}
		case ButtonMode::OK_CANCEL: {
			// Positions must match drawButtons/getElementAt: cancel LEFT, ok
			// RIGHT (they were swapped here - the visual CANCEL submitted when
			// a drag past the threshold dropped the tap into this fallback).
			const int cancelX = x + w - buttonW * 2 - fieldSpacing;
			const int okX = x + w - buttonW - fieldSpacing / 2;

			if (showOkButton && e.x >= okX && e.x <= okX + buttonW &&
				e.y >= buttonY && e.y <= buttonY + buttonHeight) {
				submitDialog();
				return;
			}

			if (showCancelButton && e.x >= cancelX && e.x <= cancelX + buttonW &&
				e.y >= buttonY && e.y <= buttonY + buttonHeight) {
				cancelDialog();
				return;
			}
			break;
		}
		case ButtonMode::CUSTOM_BUTTON: {
			if (!customButtonLabel.empty() && e.x >= x + fieldSpacing && e.x <= x + fieldSpacing + buttonW &&
				e.y >= buttonY && e.y <= buttonY + buttonHeight) {
				if (customButtonCallback) {
					customButtonCallback();
				}
				return;
			}
			break;
		}
		case ButtonMode::OK_CANCEL_CUSTOM: {
			// custom | cancel | ok - same geometry as getElementAt/drawButtons.
			if (e.y < buttonY || e.y > buttonY + buttonHeight) break;
			const int customX = x + fieldSpacing;
			const int cancelX = x + fieldSpacing * 2 + buttonW;
			const int okX = x + fieldSpacing * 3 + buttonW * 2;
			if (!customButtonLabel.empty() && e.x >= customX && e.x <= customX + buttonW) {
				if (customButtonCallback) customButtonCallback();
				return;
			}
			if (showCancelButton && e.x >= cancelX && e.x <= cancelX + buttonW) {
				cancelDialog();
				return;
			}
			if (showOkButton && e.x >= okX && e.x <= okX + buttonW) {
				submitDialog();
				return;
			}
			break;
		}
		}
	}

	// Check which field was tapped
	for (int i = 0; i < static_cast<int>(fields.size()); ++i) {
		if (isFieldArea(e, i)) {
			auto& field = fields[i];

			if (field->fieldType == FieldType::CHECKBOX) {
				// Toggle checkbox
				field->isChecked = !field->isChecked;
				needsFullRedraw = true;
				return;
			}
			else if (field->fieldType == FieldType::TEXT_INPUT) {
				setFieldFocus(i);
				InputEvent localEvent = e;
				localEvent.x = e.x - x;
				localEvent.y = e.y - y;
				fields[i]->input.callback(localEvent);
				return;
			}
			// TEXT_DESCRIPTION fields don't respond to taps
		}
	}
}

void DynamicDialog::delRecursiveDraw() {
	_STATE->graphics.deleteWindow(windowindex);

	// Only cleanup keyboard if it was initialized
	if (!fields.empty() && showKeyboard && !staticMode) {
		keyboard.delRecursiveDraw();
	}

	View::delRecursiveDraw();
}

void DynamicDialog::addInputField(const std::string& label, bool isPassword, LabelPosition labelPos) {
	auto field = std::make_unique<InputField>(_appState, label, isPassword, labelPos);
	field->maskChar = passwordMaskChar;
	fields.push_back(std::move(field));

	// Set focus on first field
	if (fields.size() == 1) {
		fields[0]->input.hasFocus = true;
		currentFieldIndex = 0;
	}

	needsFullRedraw = true;
}

void DynamicDialog::addInputField(const std::string& label, const std::string& defaultValue,
	bool isPassword, LabelPosition labelPos) {
	addInputField(label, isPassword, labelPos);
	if (!fields.empty()) {
		fields.back()->input.setText(defaultValue);
	}
}

void DynamicDialog::setPasswordMaskChar(const std::string& label, char ch) {
	for (auto& field : fields) {
		if (field->label == label && field->isPassword) {
			field->maskChar = ch;
			break;
		}
	}
}

void DynamicDialog::clearFields() {
	fields.clear();
	currentFieldIndex = 0;
	needsFullRedraw = true;
}

void DynamicDialog::setFieldValue(int index, const std::string& value) {
	if (index >= 0 && index < static_cast<int>(fields.size())) {
		fields[index]->input.setText(value);
	}
}

void DynamicDialog::setFieldValue(const std::string& label, const std::string& value) {
	for (auto& field : fields) {
		if (field->label == label) {
			field->input.setText(value);
			break;
		}
	}
}

void DynamicDialog::setFieldMaxLength(const std::string& label, int maxLen) {
	for (auto& field : fields) {
		if (field->label == label && field->fieldType == FieldType::TEXT_INPUT) {
			field->input.setMaxLength(maxLen);
			break;
		}
	}
}

std::string DynamicDialog::getFieldValue(int index) const {
	if (index >= 0 && index < static_cast<int>(fields.size())) {
		return fields[index]->input.getText();
	}
	return "";
}

std::string DynamicDialog::getFieldValue(const std::string& label) const {
	for (const auto& field : fields) {
		if (field->label == label) {
			return field->input.getText();
		}
	}
	return "";
}

void DynamicDialog::layoutFields() {
	const int labelLine = static_cast<int>(_STATE->textsize2);
	int currentY = titleHeight + fieldSpacing * 2;

	for (auto& field : fields) {
		if (field->fieldType == FieldType::SLIDER) {
			// Slider row takes the full width; its label is drawn inside the
			// row (right of the track), so no label band on either side.
			field->input.startx = fieldSpacing;
			field->input.starty = currentY;
			field->input.width = w - (fieldSpacing * 2);
		}
		else if (field->labelPosition == LabelPosition::ABOVE) {
			// Label above: input takes full width
			field->input.startx = fieldSpacing;
			field->input.starty = currentY + labelLine + fieldSpacing / 2;
			field->input.width = w - (fieldSpacing * 2);
		}
		else {
			// Label left: input starts after label + spacing. A label wider
			// than the labelWidth column pushes its input right instead of
			// running under it ("USE TRACK SOUND" was cut by the input rect).
			SkFont font(_STATE->font_normal);
			font.setSize(_STATE->textsize2 * 0.9f);
			const int measured = (int)font.measureText(
				field->label.c_str(), field->label.size(), SkTextEncoding::kUTF8);
			field->input.startx = fieldSpacing +
				std::max(labelWidth, measured + labelSpacing) + labelSpacing;
			field->input.starty = currentY;
			field->input.width = w - field->input.startx - fieldSpacing;
		}

		field->input.height = fieldHeight;
		field->input.stopx = field->input.startx + field->input.width;
		field->input.stopy = field->input.starty + field->input.height;
		field->input.init();

		// Calculate next Y position
		if (field->labelPosition == LabelPosition::ABOVE) {
			currentY += labelLine + fieldSpacing / 2 + fieldHeight + fieldSpacing;
		}
		else {
			currentY += fieldHeight + fieldSpacing;
		}
	}
}

void DynamicDialog::setFieldFocus(int fieldIndex) {
	if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) return;

	// Clear all focus
	for (auto& field : fields) {
		field->input.hasFocus = false;
		if (field->fieldType == FieldType::TEXT_INPUT)field->input.setHighLight(false);
	}

	// Set focus on specified field
	currentFieldIndex = fieldIndex;
	fields[currentFieldIndex]->input.hasFocus = true;
	fields[currentFieldIndex]->input.resetTimer();

}

// Focus cycling visits TEXT_INPUT fields only: sliders, checkboxes and
// descriptions have no caret, and giving one focus would silently swallow
// typed characters into its dormant TextInput.
void DynamicDialog::nextField() {
	const int n = static_cast<int>(fields.size());
	int next = currentFieldIndex;
	for (int step = 0; step < n; step++) {
		next = (next + 1) % n;
		if (fields[next]->fieldType == FieldType::TEXT_INPUT) break;
	}
	setFieldFocus(next);
}

void DynamicDialog::prevField() {
	const int n = static_cast<int>(fields.size());
	int prev = currentFieldIndex;
	for (int step = 0; step < n; step++) {
		prev = (prev + n - 1) % n;
		if (fields[prev]->fieldType == FieldType::TEXT_INPUT) break;
	}
	setFieldFocus(prev);
}

TextInput* DynamicDialog::getCurrentInput() {
	if (currentFieldIndex >= 0 && currentFieldIndex < static_cast<int>(fields.size())) {
		return &fields[currentFieldIndex]->input;
	}
	return nullptr;
}

void DynamicDialog::cancelDialog() {
	if (onCompleteCallback) {
		DialogResult result;
		result.confirmed = false;
		onCompleteCallback(result);
	}
	else {
		delCB();
		deldraw();
	}
}

void DynamicDialog::handleConfirmCancel(int vkey, bool shiftTab) {
	if (vkey == VKEY_RETURN) {
		// "Last field" means last TEXT field: trailing sliders/checkboxes
		// cannot take a caret, so Return on the final typed field submits.
		int lastText = static_cast<int>(fields.size()) - 1;
		while (lastText > 0 && fields[lastText]->fieldType != FieldType::TEXT_INPUT)
			lastText--;
		if (currentFieldIndex == lastText &&
			(autoSubmitOnLast || !hasButtons)) {
			submitDialog();
		}
		else {
			nextField();
			if (fields[currentFieldIndex]->fieldType == FieldType::TEXT_INPUT)fields[currentFieldIndex]->input.setHighLight(true);
		}
		return;
	}

	if (vkey == VKEY_ESCAPE) {
		cancelDialog();
		return;
	}

	if (vkey == VKEY_TAB) {
		if (shiftTab) {
			prevField();
		}
		else {
			nextField();
		}
		if (fields[currentFieldIndex]->fieldType == FieldType::TEXT_INPUT)fields[currentFieldIndex]->input.setHighLight(true);

		return;
	}

	if (vkey == VKEY_UP) {
		prevField();
		return;
	}

	if (vkey == VKEY_DOWN) {
		nextField();
		return;
	}
}

bool DynamicDialog::isFieldArea(const InputEvent& e, int fieldIndex) {
	if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) return false;

	const auto& field = fields[fieldIndex];
	const int relX = e.x - x;
	const int relY = e.y - y;

	if (field->fieldType == FieldType::SLIDER) {
		// A thin track needs a fat hit band: the row plus a spacing above and
		// below. Dragging is relative, so a generous grab cannot jump the value.
		return (relX >= field->input.startx && relX <= field->input.stopx &&
			relY >= field->input.starty - fieldSpacing &&
			relY <= field->input.stopy + fieldSpacing);
	}
	if (field->labelPosition == LabelPosition::ABOVE) {
		// For above labels, include label area above the input
		const int labelHeight = static_cast<int>(_STATE->textsize2);
		return (relX >= field->input.startx && relX <= field->input.stopx &&
			relY >= field->input.starty - labelHeight - fieldSpacing / 2 &&
			relY <= field->input.stopy);
	}
	else {
		// For left labels, include label area to the left of input
		return (relX >= fieldSpacing && relX <= field->input.stopx &&
			relY >= field->input.starty && relY <= field->input.stopy);
	}
}

void DynamicDialog::drawFieldLabel(SkCanvas* c, InputField& field, int yPos) {
	// Sliders draw their own label inside the row, right of the track.
	if (field.fieldType == FieldType::SLIDER) return;
	SkPaint paint;
	paint.setAntiAlias(true);
	paint.setColor(skcol::text);

	const float fs = _STATE->textsize2 * 0.9f;
	SkFont& font = _STATE->font_normal;
	font.setSize(fs);

	if (field.labelPosition == LabelPosition::ABOVE) {
		// Draw label above the input field
		float labelY = yPos - fieldSpacing / 2 - fs * 0.3f; // Slightly above the input
		c->drawSimpleText(field.label.c_str(), field.label.size(), SkTextEncoding::kUTF8,
			field.input.startx, labelY, font, paint);
	}
	else {
		// Draw label to the left of the input field using baseline alignment
		float labelY = field.input.starty + labelBaseLine;
		if (field.fieldType == FieldType::CHECKBOX) {
			// No inner text to baseline-align with: put the text's optical
			// center on the box center (box = row center). Cap height from
			// font metrics, not ink bounds - measured bounds proved loose on
			// this backend and sat the label near the row bottom.
			SkFontMetrics fm{};
			font.getMetrics(&fm);
			const float capH = fm.fCapHeight > 0.f ? fm.fCapHeight : fs * .7f;
			labelY = field.input.starty + field.input.height * .5f + capH * .5f;
		}
		c->drawSimpleText(field.label.c_str(), field.label.size(), SkTextEncoding::kUTF8,
			fieldSpacing, labelY, font, paint);
	}
}

void DynamicDialog::addRecursiveDraw() {
	if (auto current = getCurrentInput()) {
		current->setHighLight(true);
	}
	View::addRecursiveDraw();
}