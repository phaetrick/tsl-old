#include "SettingsView.h"
#include "tools/PlatformPaths.h"
namespace tsl::graphics {

	// --- Material Design Color Palette ---
	const SkColor MATERIAL_DARK_BG_PRIMARY = SkColorSetRGB(0, 0, 0);
	const SkColor MATERIAL_DARK_BG_SURFACE = SkColorSetRGB(0, 0, 0);
	const SkColor MATERIAL_LIGHT_BLUE_500 = SkColorSetRGB(3, 169, 244);
	const SkColor MATERIAL_TEXT_PRIMARY = SkColorSetARGB(255, 224, 224, 224);
	const SkColor MATERIAL_TEXT_SECONDARY = SkColorSetARGB(153, 255, 255, 255);
	const SkColor MATERIAL_HIGHLIGHT = SkColorSetARGB(50, 3, 169, 244);
	const SkColor MATERIAL_CLICK_HIGHLIGHT = SkColorSetARGB(40, 255, 255, 255);
	const SkColor MATERIAL_CLICK_HIGHLIGHT_BLUE = SkColorSetARGB(60, 3, 169, 244);
	const SkColor MATERIAL_CONTROL_PRESSED = SkColorSetARGB(30, 255, 255, 255);
	const SkColor MATERIAL_BUTTON_NEUTRAL_BG = SkColorSetRGB(90, 90, 90);
	const SkColor MATERIAL_BORDER_LIGHT = SkColorSetRGB(120, 120, 120);
	const SkColor MATERIAL_CATEGORY_SEPARATOR_COLOR = SkColorSetRGB(95, 95, 95);
	const float MATERIAL_CORNER_RADIUS = 4.0f;

	// --- ScalableLayout Implementation ---
	ScalableLayout::ScalableLayout(tsl::AppState* appState)
		: _appState(appState), windowWidth(800.0f), windowHeight(600.0f),
		pluginWidth(800.0f), pluginHeight(600.0f),
		xStart(0.0f), xEnd(800.0f), yStart(0.0f), yEnd(600.0f),
		leftMargin(24.0f), rightMargin(24.0f),
		topMargin(18.0f), bottomMargin(18.0f),
		itemControlWidth(800.0f * 0.22f),
		controlHeight(20.0f),
		universalItemSpacing(18.0f),
		itemNameToDescSpacing(6.0f),
		descTextPaddingRight(800.0f * 0.03f),
		catTitleItemSpacing(27.0f),
		closeBtnSize(30.0f),
		closeBtnMargin(12.0f),
		scrollbarWidth(8.0f),
		scrollbarMargin(8.0f),
		dropdownOptionHeight(20.0f),
		maxVisibleOptions(5),
		checkboxSize(20.0f),
		checkboxPadding(7.0f),
		dropdownArrowSize(8.0f),
		dropdownTextPadding(10.0f)
	{
	}

	void ScalableLayout::init() {
		const float ox = _appState->graphics.xOffset;
		const float oy = _appState->graphics.yOffset;
		pluginWidth  = _appState->windowWidth;
		pluginHeight = _appState->windowHeight;
		windowWidth  = pluginWidth  + 2.0f * ox;
		windowHeight = pluginHeight + 2.0f * oy;

		// Safe area bounds in local canvas coords (after translate by ox,oy):
		//   screen left safe edge  = local (safeInsets.left - ox)
		//   screen right safe edge = local (pluginWidth + ox - safeInsets.right)
		const float il = _appState->safeInsets.left;
		const float ir = _appState->safeInsets.right;
		const float it = _appState->safeInsets.top;
		const float ib = _appState->safeInsets.bottom;
		xStart = il - ox;
		xEnd   = pluginWidth + ox - ir;
		yStart = it - oy;
		yEnd   = pluginHeight + oy - ib;

		const float safeW = xEnd - xStart;
		const float lh = _appState->textsize1; // standard line height

		leftMargin = safeW * 0.03f;
		rightMargin = safeW * 0.03f;
		topMargin = lh * 0.6f;
		bottomMargin = lh * 0.6f;

		itemControlWidth = safeW * 0.25f;
		controlHeight = lh;

		universalItemSpacing = lh * 0.6f;
		itemNameToDescSpacing = lh * 0.2f;
		descTextPaddingRight = safeW * 0.03f;
		catTitleItemSpacing = lh * 0.9f;

		checkboxSize = lh;
		checkboxPadding = lh * 0.35f;

		closeBtnSize = lh * 1.08f;
		closeBtnMargin = lh * 0.3f;

		scrollbarWidth = std::max(6.0f, safeW * 0.006f);
		scrollbarMargin = std::max(6.0f, safeW * 0.006f);

		dropdownArrowSize = lh * 0.35f;
		dropdownTextPadding = lh * 0.5f;
		dropdownOptionHeight = lh;
		maxVisibleOptions = 10;
	}

	float ScalableLayout::getCategoryFontSize() const {
		return _appState->textsize2 * 0.8f;
	}

	float ScalableLayout::getItemFontSize() const {
		return _appState->textsize2 * 0.9f;
	}

	float ScalableLayout::getDescFontSize() const {
		return _appState->textsize2 * 0.7f;
	}

	// --- Helper Functions ---
	std::vector<std::string> wrapText(const std::string& text, const SkFont& font, float maxWidth) {
		std::vector<std::string> lines;
		if (text.empty()) return lines;

		std::istringstream iss(text);
		std::string word, currentLine;

		while (iss >> word) {
			std::string testLine = currentLine.empty() ? word : currentLine + " " + word;
			float testLineWidth = font.measureText(testLine.c_str(), testLine.size(), SkTextEncoding::kUTF8);

			if (testLineWidth <= maxWidth) {
				currentLine = testLine;
			}
			else {
				if (!currentLine.empty()) {
					lines.push_back(currentLine);
				}
				currentLine = word;
			}
		}

		if (!currentLine.empty()) {
			lines.push_back(currentLine);
		}

		return lines;
	}

	std::string truncateText(const std::string& text, const SkFont& font, float maxWidth) {
		float textWidth = font.measureText(text.c_str(), text.size(), SkTextEncoding::kUTF8);
		if (textWidth <= maxWidth) return text;

		std::string ellipsis = "...";
		float ellipsisWidth = font.measureText(ellipsis.c_str(), ellipsis.size(), SkTextEncoding::kUTF8);
		if (maxWidth <= ellipsisWidth) return ellipsis;

		size_t length = text.length();
		while (length > 0) {
			std::string testString = text.substr(0, length);
			float currentWidth = font.measureText(testString.c_str(), testString.size(), SkTextEncoding::kUTF8);
			if (currentWidth + ellipsisWidth <= maxWidth) {
				return testString + ellipsis;
			}
			length--;
		}
		return ellipsis;
	}

	std::vector<std::string> splitAndTrimString(const std::string& s, char delimiter) {
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream tokenStream(s);

		while (std::getline(tokenStream, token, delimiter)) {
			token.erase(0, token.find_first_not_of(" \t\n\r"));
			token.erase(token.find_last_not_of(" \t\n\r") + 1);
			tokens.push_back(token);
		}
		return tokens;
	}

	// --- AbstractPrefItem Implementation ---
	AbstractPrefItem::AbstractPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
		const std::string& name, const std::string& key,
		const std::string& description, const std::string& type)
		: _name(name), _key(key), _description(description), _type(type),
		_state(state), _mgr(mgr), specialAction(nullptr) {
	}

	void AbstractPrefItem::setAction(std::function<void()> action) {
		specialAction = action;
	}

	const std::string& AbstractPrefItem::getKey() const { return _key; }
	const std::string& AbstractPrefItem::getType() const { return _type; }
	const std::string& AbstractPrefItem::getName() const { return _name; }
	const std::string& AbstractPrefItem::getDescription() const { return _description; }
	const SkRect& AbstractPrefItem::getBounds() const { return _bounds; }
	const SkRect& AbstractPrefItem::getDisplayBounds() const { return _displayBounds; }

	bool AbstractPrefItem::contains(float x, float contentClickY) const {
		return _bounds.contains(x, contentClickY);
	}

	bool AbstractPrefItem::hasExpandedContentActive() const { return false; }
	bool AbstractPrefItem::isDropdownItem() const { return false; }

	void AbstractPrefItem::drawExpandedOptions(SkCanvas* canvas, float dropdownX,
		float dropdownY_on_canvas, float dropdownW, SkFont& font, const ScalableLayout& layout) {
	}

	SkRect AbstractPrefItem::getExpandedContentBounds(float controlH) const {
		return SkRect::MakeEmpty();
	}

	SkRect AbstractPrefItem::getExpandedContentCanvasBounds(float controlH, float mainScrollOffset) const {
		return SkRect::MakeEmpty();
	}

	float AbstractPrefItem::getExpandedContentScrollOffset() const { return 0.0f; }
	float AbstractPrefItem::getExpandedContentTotalHeight() const { return 0.0f; }
	float AbstractPrefItem::getExpandedContentVisibleHeight() const { return 0.0f; }

	bool AbstractPrefItem::handleExpandedContentTouch(float x, float y, float contentClickY,
		int pointerId, float& lastTouchY, float& dirtyRectTop, float& dirtyRectBottom,
		const ScalableLayout& layout) {
		return false;
	}

	bool AbstractPrefItem::closeExpandedContent(float mainScrollOffset,
		float& dirtyRectTop, float& dirtyRectBottom) {
		return false;
	}

	// --- BoolPrefItem Implementation ---
	BoolPrefItem::BoolPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
		const std::string& name, const std::string& key,
		const std::string& description, const std::string& initialValue)
		: AbstractPrefItem(state, mgr, name, key, description, "bool"), _value(initialValue) {
	}

	void BoolPrefItem::updateValueFromManager() {
		_value = _mgr->Get(_key, (_value == "true")) ? "true" : "false";
	}

	void BoolPrefItem::draw(SkCanvas* canvas, float currentY, float scrollOffset,
		SkFont& itemFont, SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) {
		SkPaint textPaint;
		textPaint.setColor(MATERIAL_TEXT_PRIMARY);

		float drawY = currentY - scrollOffset;
		canvas->drawSimpleText(_name.c_str(), _name.size(), SkTextEncoding::kUTF8,
			layout.textOriginX(), drawY, itemFont, textPaint);

		SkFontMetrics fm;
		itemFont.getMetrics(&fm);
		float textVisualCenter = currentY + (fm.fAscent + fm.fDescent) * 0.5f;

		float controlX_content = layout.controlOriginX();
		float controlH = layout.checkboxSize;
		float controlY_content = textVisualCenter - controlH * 0.5f;

		_bounds = SkRect::MakeXYWH(controlX_content, controlY_content, controlH, controlH);

		float itemContentHeight = itemFont.getSpacing() + layout.itemNameToDescSpacing;
		if (!_description.empty()) {
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.descWidth());
			itemContentHeight += (descLines.size() * itemDescFont.getSpacing());
		}
		_displayBounds = SkRect::MakeXYWH(layout.textOriginX(), currentY, layout.rowWidth(), itemContentHeight);
		_displayBounds.join(_bounds);

		if (!_description.empty()) {
			SkPaint itemDescPaint;
			itemDescPaint.setColor(MATERIAL_TEXT_SECONDARY);
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.descWidth());
			float currentDescY = currentY + itemFont.getSpacing() + layout.itemNameToDescSpacing - scrollOffset;
			for (const std::string& line : descLines) {
				canvas->drawSimpleText(line.c_str(), line.size(), SkTextEncoding::kUTF8,
					layout.textOriginX(), currentDescY, itemDescFont, itemDescPaint);
				currentDescY += itemDescFont.getSpacing();
			}
		}

		SkPaint borderPaint;
		borderPaint.setColor(MATERIAL_BORDER_LIGHT);
		borderPaint.setStyle(SkPaint::kStroke_Style);
		borderPaint.setStrokeWidth(1.0f);

		SkPaint surfacePaint;
		surfacePaint.setColor(MATERIAL_DARK_BG_SURFACE);

		float boxSize = layout.checkboxSize;
		SkRect boxRect = SkRect::MakeXYWH(
			controlX_content + (controlH - boxSize) / 2,
			controlY_content - scrollOffset,
			boxSize,
			boxSize
		);
		SkRRect boxRRect = SkRRect::MakeRectXY(boxRect, MATERIAL_CORNER_RADIUS, MATERIAL_CORNER_RADIUS);

		canvas->drawRRect(boxRRect, surfacePaint);

		if (isPressed) {
			SkPaint highlightPaint;
			highlightPaint.setColor(MATERIAL_CONTROL_PRESSED);
			canvas->drawRRect(boxRRect, highlightPaint);
		}

		canvas->drawRRect(boxRRect, borderPaint);

		if (_value == "true") {
			SkPaint checkPaint;
			checkPaint.setColor(MATERIAL_LIGHT_BLUE_500);
			checkPaint.setStrokeWidth(std::max(2.0f, boxSize * 0.1f));
			float checkPadding = layout.checkboxPadding;
			canvas->drawLine(boxRect.left() + checkPadding, boxRect.top() + checkPadding,
				boxRect.right() - checkPadding, boxRect.bottom() - checkPadding, checkPaint);
			canvas->drawLine(boxRect.right() - checkPadding, boxRect.top() + checkPadding,
				boxRect.left() + checkPadding, boxRect.bottom() - checkPadding, checkPaint);
		}
	}

	bool BoolPrefItem::processClick(float x, float y, float contentClickY,
		float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) {
		if (_bounds.contains(x, contentClickY)) {
			bool currentValue = (_value == "true");
			_value = currentValue ? "false" : "true";
			_mgr->Set(_key, !currentValue);
			dirtyRectTop = _displayBounds.top();
			dirtyRectBottom = _displayBounds.bottom();
			return true;
		}
		return false;
	}

	bool BoolPrefItem::handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) {
		return false;
	}

	// --- SliderPrefItem Implementation ---
	SliderPrefItem::SliderPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
		const std::string& name, const std::string& key,
		const std::string& description, const std::string& initialValue)
		: AbstractPrefItem(state, mgr, name, key, description, "slider"), _value(initialValue) {
	}

	void SliderPrefItem::updateValueFromManager() {
		int defaultValueInt = 0;
		std::istringstream defaultIss(_value);
		defaultIss >> defaultValueInt;
		int actualValueInt = _mgr->Get(_key, defaultValueInt);
		_value = std::to_string(static_cast<float>(actualValueInt) / 100.0f);
	}

	void SliderPrefItem::draw(SkCanvas* canvas, float currentY, float scrollOffset,
		SkFont& itemFont, SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) {
		SkPaint textPaint;
		textPaint.setColor(MATERIAL_TEXT_PRIMARY);

		float drawY = currentY - scrollOffset;
		canvas->drawSimpleText(_name.c_str(), _name.size(), SkTextEncoding::kUTF8,
			layout.textOriginX(), drawY, itemFont, textPaint);

		SkFontMetrics fm;
		itemFont.getMetrics(&fm);
		float textVisualCenter = currentY + (fm.fAscent + fm.fDescent) * 0.5f;

		float controlX_content = layout.controlOriginX();
		float controlW = layout.itemControlWidth;
		float controlH = layout.controlHeight;
		float controlY_content = textVisualCenter - controlH * 0.5f;

		_bounds = SkRect::MakeXYWH(controlX_content, controlY_content, controlW, controlH);

		float itemContentHeight = itemFont.getSpacing() + layout.itemNameToDescSpacing;
		if (!_description.empty()) {
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.descWidth());
			itemContentHeight += (descLines.size() * itemDescFont.getSpacing());
		}
		_displayBounds = SkRect::MakeXYWH(layout.textOriginX(), currentY, layout.rowWidth(), itemContentHeight);
		_displayBounds.join(_bounds);

		if (!_description.empty()) {
			SkPaint itemDescPaint;
			itemDescPaint.setColor(MATERIAL_TEXT_SECONDARY);
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.descWidth());
			float currentDescY = currentY + itemFont.getSpacing() + layout.itemNameToDescSpacing - scrollOffset;
			for (const std::string& line : descLines) {
				canvas->drawSimpleText(line.c_str(), line.size(), SkTextEncoding::kUTF8,
					layout.textOriginX(), currentDescY, itemDescFont, itemDescPaint);
				currentDescY += itemDescFont.getSpacing();
			}
		}

		SkRect controlBgRect = SkRect::MakeXYWH(controlX_content, controlY_content - scrollOffset, controlW, controlH);
		SkRRect controlBgRRect = SkRRect::MakeRectXY(controlBgRect, MATERIAL_CORNER_RADIUS, MATERIAL_CORNER_RADIUS);

		SkPaint bgPaint;
		bgPaint.setColor(MATERIAL_DARK_BG_SURFACE);
		canvas->drawRRect(controlBgRRect, bgPaint);

		if (isPressed) {
			SkPaint highlightPaint;
			highlightPaint.setColor(MATERIAL_CONTROL_PRESSED);
			canvas->drawRRect(controlBgRRect, highlightPaint);
		}

		SkPaint borderPaint;
		borderPaint.setColor(MATERIAL_BORDER_LIGHT);
		borderPaint.setStyle(SkPaint::kStroke_Style);
		borderPaint.setStrokeWidth(1.0f);
		canvas->drawRRect(controlBgRRect, borderPaint);

		float trackPadding = controlW * 0.05f;
		SkRect track = SkRect::MakeXYWH(controlX_content + trackPadding,
			controlY_content - scrollOffset + controlH / 2 - 2, controlW - 2 * trackPadding, 4);
		SkRRect trackRRect = SkRRect::MakeRectXY(track, 2.0f, 2.0f);
		SkPaint trackPaint;
		trackPaint.setColor(MATERIAL_BORDER_LIGHT);
		canvas->drawRRect(trackRRect, trackPaint);

		float sliderVal = 0.0f;
		std::istringstream iss(_value);
		iss >> sliderVal;
		sliderVal = std::clamp(sliderVal, 0.0f, 1.0f);

		float knobWidth = controlH * 0.4f;
		float knobX = controlX_content + trackPadding + sliderVal * (controlW - 2 * trackPadding);
		SkRect knobRect = SkRect::MakeXYWH(knobX - knobWidth / 2,
			controlY_content - scrollOffset + controlH * 0.2f, knobWidth, controlH * 0.6f);
		SkRRect knobRRect = SkRRect::MakeRectXY(knobRect, MATERIAL_CORNER_RADIUS, MATERIAL_CORNER_RADIUS);
		SkPaint knobPaint;
		knobPaint.setColor(MATERIAL_LIGHT_BLUE_500);
		canvas->drawRRect(knobRRect, knobPaint);
	}

	bool SliderPrefItem::processClick(float x, float y, float contentClickY,
		float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) {
		if (_bounds.contains(x, contentClickY)) {
			float pos_relative_to_content_control_left = x - _bounds.left();
			float sliderValueFloat = std::clamp(pos_relative_to_content_control_left / _bounds.width(), 0.0f, 1.0f);
			int sliderValueInt = static_cast<int>(std::round(sliderValueFloat * 100.0f));
			std::string newValue = std::to_string(sliderValueFloat);
			if (_value != newValue) {
				_value = newValue;
				_mgr->Set(_key, sliderValueInt);
				dirtyRectTop = _displayBounds.top();
				dirtyRectBottom = _displayBounds.bottom();
				return true;
			}
		}
		return false;
	}

	bool SliderPrefItem::handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) {
		return false;
	}

	// --- DropdownPrefItem Implementation ---
	DropdownPrefItem::DropdownPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
		const std::string& name, const std::string& key,
		const std::string& description, const std::string& initialValue,
		const std::string& displayOptionsStr, const std::string& optionValuesStr)
		: AbstractPrefItem(state, mgr, name, key, description, "dropdown"),
		_value(initialValue),
		_currentValueDisplayName("Error"),
		_isDropdownOpen(false),
		_dropdownScrollOffset(0.0f),
		_dropdownContentHeight(0.0f),
		_dropdownVisibleHeight(0.0f)
	{
		_displayOptions = splitAndTrimString(displayOptionsStr, ',');
		_optionValues = splitAndTrimString(optionValuesStr, ',');

		if (_displayOptions.empty() || _displayOptions.size() != _optionValues.size()) {
			fprintf(stderr, "Warning: Mismatch or missing displayOptions/optionValues for dropdown '%s'.\n", _name.c_str());
			_displayOptions.clear();
			_optionValues.clear();
		}
		else {
			_currentValueDisplayName = _displayOptions[0];
			for (size_t i = 0; i < _optionValues.size(); ++i) {
				if (_optionValues[i] == _value && i < _displayOptions.size()) {
					_currentValueDisplayName = _displayOptions[i];
					break;
				}
			}
		}
	}

	void DropdownPrefItem::updateValueFromManager() {
		std::string storedValue = _mgr->Get(_key, _value);
		_value = storedValue;

		_currentValueDisplayName = "N/A";
		for (size_t i = 0; i < _optionValues.size(); ++i) {
			if (_optionValues[i] == storedValue && i < _displayOptions.size()) {
				_currentValueDisplayName = _displayOptions[i];
				break;
			}
		}
	}

	void DropdownPrefItem::draw(SkCanvas* canvas, float currentY, float scrollOffset,
		SkFont& itemFont, SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) {
		_dropdownContentHeight = _displayOptions.size() * layout.dropdownOptionHeight;
		_dropdownVisibleHeight = std::min(static_cast<float>(layout.maxVisibleOptions) *
			layout.dropdownOptionHeight, _dropdownContentHeight);

		if (_isDropdownOpen) {
			float maxScroll = std::max(0.0f, _dropdownContentHeight - _dropdownVisibleHeight);
			_dropdownScrollOffset = std::clamp(_dropdownScrollOffset, 0.0f, maxScroll);
		}

		SkPaint textPaint;
		textPaint.setColor(MATERIAL_TEXT_PRIMARY);

		float drawY = currentY - scrollOffset;
		canvas->drawSimpleText(_name.c_str(), _name.size(), SkTextEncoding::kUTF8,
			layout.textOriginX(), drawY, itemFont, textPaint);

		SkFontMetrics fm;
		itemFont.getMetrics(&fm);
		float textVisualCenter = currentY + (fm.fAscent + fm.fDescent) * 0.5f;

		float controlX_content = layout.controlOriginX();
		float controlW = layout.itemControlWidth;
		float controlH = layout.controlHeight;
		float dropdownBoxHeight = layout.checkboxSize * 1.08f;
		float controlY_content = textVisualCenter - dropdownBoxHeight * 0.5f;

		// Shrink if scrollbar overlaps
		float scrollbarLeftEdge = layout.xEnd - layout.scrollbarWidth - layout.scrollbarMargin * 2;
		float controlRightEdge = controlX_content + controlW;
		if (controlRightEdge > scrollbarLeftEdge) {
			controlW = scrollbarLeftEdge - controlX_content;
		}

		float dropdownBoxWidth = controlW;
		float dropdownBoxY = controlY_content;

		_bounds = SkRect::MakeXYWH(controlX_content, controlY_content, controlW, dropdownBoxHeight);

		float itemContentHeight = itemFont.getSpacing() + layout.itemNameToDescSpacing;
		if (!_description.empty()) {
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.descWidth());
			itemContentHeight += (descLines.size() * itemDescFont.getSpacing());
		}
		_displayBounds = SkRect::MakeXYWH(layout.textOriginX(), currentY, layout.rowWidth(), itemContentHeight);
		_displayBounds.join(_bounds);

		if (!_description.empty()) {
			SkPaint itemDescPaint;
			itemDescPaint.setColor(MATERIAL_TEXT_SECONDARY);
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.descWidth());
			float currentDescY = currentY + itemFont.getSpacing() + layout.itemNameToDescSpacing - scrollOffset;
			for (const std::string& line : descLines) {
				canvas->drawSimpleText(line.c_str(), line.size(), SkTextEncoding::kUTF8,
					layout.textOriginX(), currentDescY, itemDescFont, itemDescPaint);
				currentDescY += itemDescFont.getSpacing();
			}
		}

		SkPaint borderPaint;
		borderPaint.setColor(MATERIAL_BORDER_LIGHT);
		borderPaint.setStyle(SkPaint::kStroke_Style);
		borderPaint.setStrokeWidth(1.0f);

		SkPaint surfacePaint;
		surfacePaint.setColor(MATERIAL_DARK_BG_SURFACE);

		SkRect dropRect = SkRect::MakeXYWH(
			controlX_content,
			dropdownBoxY - scrollOffset,
			dropdownBoxWidth,
			dropdownBoxHeight
		);
		SkRRect dropRRect = SkRRect::MakeRectXY(dropRect, MATERIAL_CORNER_RADIUS, MATERIAL_CORNER_RADIUS);

		canvas->drawRRect(dropRRect, surfacePaint);

		if (isPressed) {
			SkPaint highlightPaint;
			highlightPaint.setColor(MATERIAL_CONTROL_PRESSED);
			canvas->drawRRect(dropRRect, highlightPaint);
		}

		canvas->drawRRect(dropRRect, borderPaint);

		SkPaint valueTextPaint;
		valueTextPaint.setColor(MATERIAL_TEXT_PRIMARY);

		float textPadding = layout.dropdownTextPadding;
		float arrowWidth = dropdownBoxHeight * 0.6f;
		float dropdownTextMaxWidth = dropdownBoxWidth - (textPadding * 2 + arrowWidth);
		std::string displayedValue = truncateText(_currentValueDisplayName, itemFont, dropdownTextMaxWidth);

		SkFontMetrics dfm;
		itemFont.getMetrics(&dfm);
		float dropBoxCenterY = dropdownBoxY - scrollOffset + dropdownBoxHeight * 0.5f;
		float valueTextY = dropBoxCenterY - (dfm.fAscent + dfm.fDescent) * 0.5f;
		canvas->drawSimpleText(displayedValue.c_str(), displayedValue.size(), SkTextEncoding::kUTF8,
			controlX_content + textPadding, valueTextY, itemFont, valueTextPaint);

		SkPath arrow;
		SkPaint arrowPaint;
		arrowPaint.setColor(MATERIAL_TEXT_SECONDARY);
		float arrowSize = layout.dropdownArrowSize;
		float arrowCenterX = controlX_content + dropdownBoxWidth - arrowWidth / 2;
		float arrowCenterY = dropdownBoxY - scrollOffset + dropdownBoxHeight / 2;

		if (_isDropdownOpen) {
			arrow.moveTo(arrowCenterX - arrowSize / 2, arrowCenterY + arrowSize / 4);
			arrow.lineTo(arrowCenterX + arrowSize / 2, arrowCenterY + arrowSize / 4);
			arrow.lineTo(arrowCenterX, arrowCenterY - arrowSize / 4);
		}
		else {
			arrow.moveTo(arrowCenterX - arrowSize / 2, arrowCenterY - arrowSize / 4);
			arrow.lineTo(arrowCenterX + arrowSize / 2, arrowCenterY - arrowSize / 4);
			arrow.lineTo(arrowCenterX, arrowCenterY + arrowSize / 4);
		}
		arrow.close();
		canvas->drawPath(arrow, arrowPaint);
	}

	bool DropdownPrefItem::hasExpandedContentActive() const {
		return _isDropdownOpen;
	}

	bool DropdownPrefItem::isDropdownItem() const {
		return true;
	}

	void DropdownPrefItem::drawExpandedOptions(SkCanvas* canvas, float dropdownX,
		float dropdownY_on_canvas, float dropdownW, SkFont& font, const ScalableLayout& layout) {
		if (!_isDropdownOpen) return;

		SkPaint textPaint;
		textPaint.setColor(MATERIAL_TEXT_PRIMARY);

		float dropdownDisplayHeight = std::min(_dropdownContentHeight, _dropdownVisibleHeight);
		SkRect dropdownListRect = SkRect::MakeXYWH(dropdownX, dropdownY_on_canvas, dropdownW, dropdownDisplayHeight);
		SkRRect dropdownListRRect = SkRRect::MakeRectXY(dropdownListRect, MATERIAL_CORNER_RADIUS, MATERIAL_CORNER_RADIUS);

		SkPaint borderPaint;
		borderPaint.setColor(MATERIAL_BORDER_LIGHT);
		borderPaint.setStyle(SkPaint::kStroke_Style);
		borderPaint.setStrokeWidth(1.0f);

		SkPaint surfacePaint;
		surfacePaint.setColor(MATERIAL_DARK_BG_SURFACE);

		canvas->drawRRect(dropdownListRRect, surfacePaint);
		canvas->drawRRect(dropdownListRRect, borderPaint);

		canvas->save();
		canvas->clipRRect(dropdownListRRect, SkClipOp::kIntersect, true);

		float textPadding = dropdownW * 0.1f;
		float optionTextMaxWidth = dropdownW - (2 * textPadding);
		float drawOptionY_base = dropdownY_on_canvas - _dropdownScrollOffset;

		SkFontMetrics ofm;
		font.getMetrics(&ofm);

		for (size_t i = 0; i < _displayOptions.size(); ++i) {
			const std::string& optDisplayName = _displayOptions[i];
			const std::string& optActualValue = (i < _optionValues.size()) ? _optionValues[i] : "";

			SkRect optionRect = SkRect::MakeXYWH(dropdownX, drawOptionY_base + (i * layout.dropdownOptionHeight),
				dropdownW, layout.dropdownOptionHeight);

			if (optActualValue == _value) {
				SkPaint highlightPaint;
				highlightPaint.setColor(MATERIAL_CLICK_HIGHLIGHT_BLUE);
				SkRect paddedRect = optionRect;
				paddedRect.inset(2, 2);
				SkRRect highlightRRect = SkRRect::MakeRectXY(paddedRect, MATERIAL_CORNER_RADIUS - 1, MATERIAL_CORNER_RADIUS - 1);
				canvas->drawRRect(highlightRRect, highlightPaint);
			}

			float optCenterY = optionRect.top() + layout.dropdownOptionHeight * 0.5f;
			float optTextY = optCenterY - (ofm.fAscent + ofm.fDescent) * 0.5f;
			std::string displayedOption = truncateText(optDisplayName, font, optionTextMaxWidth);
			canvas->drawSimpleText(displayedOption.c_str(), displayedOption.size(), SkTextEncoding::kUTF8,
				dropdownX + textPadding, optTextY, font, textPaint);
		}
		canvas->restore();

		if (_dropdownContentHeight > _dropdownVisibleHeight && _displayOptions.size() > (size_t)layout.maxVisibleOptions) {
			SkPaint scrollbarPaint;
			scrollbarPaint.setColor(MATERIAL_TEXT_SECONDARY);

			float scrollAreaX = dropdownX + dropdownW - layout.scrollbarWidth - layout.scrollbarMargin;
			float scrollAreaY = dropdownY_on_canvas;
			float scrollAreaHeight = dropdownDisplayHeight;

			float scrollRatio = _dropdownVisibleHeight / _dropdownContentHeight;
			float thumbHeight = scrollRatio * scrollAreaHeight;
			float maxScroll = _dropdownContentHeight - _dropdownVisibleHeight;
			float thumbY = scrollAreaY + (maxScroll > 0 ? (_dropdownScrollOffset / maxScroll) *
				(scrollAreaHeight - thumbHeight) : 0);

			SkRect thumbRect = SkRect::MakeXYWH(scrollAreaX, thumbY, layout.scrollbarWidth, thumbHeight);
			SkRRect thumbRRect = SkRRect::MakeRectXY(thumbRect, layout.scrollbarWidth / 2, layout.scrollbarWidth / 2);
			canvas->drawRRect(thumbRRect, scrollbarPaint);
		}
	}

	bool DropdownPrefItem::processClick(float x, float y_on_canvas, float contentClickY,
		float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) {
		bool redrawNeeded = false;

		dirtyRectTop = _displayBounds.top();
		dirtyRectBottom = _displayBounds.bottom();

		if (_isDropdownOpen) {
			float controlX = _bounds.left();
			float controlH = _bounds.height();
			float dropdownListTop_content = _bounds.top() + controlH + 2;
			float dropdownListTop_on_canvas = dropdownListTop_content - (contentClickY - y_on_canvas);

			SkRect dropdownVisibleListBounds_on_canvas = SkRect::MakeXYWH(
				controlX,
				dropdownListTop_on_canvas,
				_bounds.width(),
				std::min(_dropdownContentHeight, _dropdownVisibleHeight)
			);

			if (dropdownVisibleListBounds_on_canvas.contains(x, y_on_canvas)) {
				float clickY_relative_to_dropdown_content = y_on_canvas -
					dropdownVisibleListBounds_on_canvas.top() + _dropdownScrollOffset;
				size_t clickedOptionIndex = static_cast<size_t>(clickY_relative_to_dropdown_content /
					layout.dropdownOptionHeight);

				if (clickedOptionIndex < _optionValues.size()) {
					std::string newSelectedValue = _optionValues[clickedOptionIndex];
					if (_value != newSelectedValue) {
						_value = newSelectedValue;
						_currentValueDisplayName = _displayOptions[clickedOptionIndex];

						int int_val = 0;
						std::istringstream iss(_value);
						iss >> int_val;

						if (!iss.fail() && iss.eof()) {
							_mgr->Set(_key, int_val);
						}
						else {
							_mgr->Set(_key, _value);
						}
						redrawNeeded = true;
					}
				}
			}

			_isDropdownOpen = false;
			_dropdownScrollOffset = 0.0f;

			float actualDrawnHeight = std::min(_dropdownContentHeight, _dropdownVisibleHeight);
			dirtyRectBottom = _displayBounds.bottom() + actualDrawnHeight + 10;
			redrawNeeded = true;

		}
		else {
			if (_bounds.contains(x, contentClickY)) {
				_isDropdownOpen = true;
				float actualDrawnHeight = std::min(_dropdownContentHeight, _dropdownVisibleHeight);
				dirtyRectBottom = _displayBounds.bottom() + actualDrawnHeight + 10;
				redrawNeeded = true;
			}
		}
		return redrawNeeded;
	}

	bool DropdownPrefItem::handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) {
		if (_isDropdownOpen) {
			float oldDropdownScrollOffset = _dropdownScrollOffset;
			float maxDropdownScroll = std::max(0.0f, _dropdownContentHeight - _dropdownVisibleHeight);
			_dropdownScrollOffset = std::clamp(_dropdownScrollOffset - dy, 0.0f, maxDropdownScroll);

			if (oldDropdownScrollOffset != _dropdownScrollOffset) {
				dirtyRectTop = _bounds.top() + _bounds.height() + 2;
				dirtyRectBottom = dirtyRectTop + _dropdownVisibleHeight;
				return true;
			}
		}
		return false;
	}

	SkRect DropdownPrefItem::getExpandedContentBounds(float controlH) const {
		if (!_isDropdownOpen) return SkRect::MakeEmpty();
		float dropdownListTop_content = _bounds.top() + controlH + 2;
		return SkRect::MakeXYWH(_bounds.left(), dropdownListTop_content, _bounds.width(), _dropdownContentHeight);
	}

	SkRect DropdownPrefItem::getExpandedContentCanvasBounds(float controlH, float mainScrollOffset) const {
		if (!_isDropdownOpen) return SkRect::MakeEmpty();
		float dropdownListTop_content = _bounds.top() + controlH + 2;
		float dropdownListTop_on_canvas = dropdownListTop_content - mainScrollOffset;
		float actualDrawnHeight = std::min(_dropdownContentHeight, _dropdownVisibleHeight);
		return SkRect::MakeXYWH(_bounds.left(), dropdownListTop_on_canvas, _bounds.width(), actualDrawnHeight);
	}

	float DropdownPrefItem::getExpandedContentScrollOffset() const {
		return _dropdownScrollOffset;
	}

	float DropdownPrefItem::getExpandedContentTotalHeight() const {
		return _dropdownContentHeight;
	}

	float DropdownPrefItem::getExpandedContentVisibleHeight() const {
		return _dropdownVisibleHeight;
	}

	bool DropdownPrefItem::handleExpandedContentTouch(float x, float y, float contentClickY,
		int pointerId, float& lastTouchY, float& dirtyRectTop, float& dirtyRectBottom,
		const ScalableLayout& layout) {
		SkRect dropdownCanvasBounds = getExpandedContentCanvasBounds(layout.dropdownOptionHeight, 0);
		if (dropdownCanvasBounds.contains(x, y)) {
			return true;
		}
		return false;
	}

	bool DropdownPrefItem::closeExpandedContent(float mainScrollOffset,
		float& dirtyRectTop, float& dirtyRectBottom) {
		if (_isDropdownOpen) {
			_isDropdownOpen = false;
			_dropdownScrollOffset = 0.0f;
			dirtyRectTop = _displayBounds.top();
			dirtyRectBottom = _displayBounds.bottom() + std::min(_dropdownContentHeight, _dropdownVisibleHeight);
			return true;
		}
		return false;
	}

	// --- ActionPrefItem Implementation ---
	ActionPrefItem::ActionPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
		const std::string& name, const std::string& key,
		const std::string& description, const std::string& type)
		: AbstractPrefItem(state, mgr, name, key, description, type) {
	}

	void ActionPrefItem::updateValueFromManager() {}

	void ActionPrefItem::draw(SkCanvas* canvas, float currentY, float scrollOffset,
		SkFont& itemFont, SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) {
		SkPaint textPaint;
		textPaint.setColor(MATERIAL_TEXT_PRIMARY);

		float drawY = currentY - scrollOffset;
		canvas->drawSimpleText(_name.c_str(), _name.size(), SkTextEncoding::kUTF8,
			layout.textOriginX(), drawY, itemFont, textPaint);

		float itemContentHeight = itemFont.getSpacing() + layout.itemNameToDescSpacing;
		if (!_description.empty()) {
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.rowWidth());
			itemContentHeight += (descLines.size() * itemDescFont.getSpacing());
		}
		_displayBounds = SkRect::MakeXYWH(layout.textOriginX(), currentY, layout.rowWidth(), itemContentHeight);
		_bounds = SkRect::MakeXYWH(layout.textOriginX(), currentY - 20, layout.rowWidth(), itemContentHeight);

		if (!_description.empty()) {
			SkPaint itemDescPaint;
			itemDescPaint.setColor(MATERIAL_TEXT_SECONDARY);
			std::vector<std::string> descLines = wrapText(_description, itemDescFont, layout.rowWidth());
			float currentDescY = currentY + itemFont.getSpacing() + layout.itemNameToDescSpacing - scrollOffset;
			for (const std::string& line : descLines) {
				canvas->drawSimpleText(line.c_str(), line.size(), SkTextEncoding::kUTF8,
					layout.textOriginX(), currentDescY, itemDescFont, itemDescPaint);
				currentDescY += itemDescFont.getSpacing();
			}
		}
	}

	bool ActionPrefItem::processClick(float x, float y, float contentClickY,
		float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) {
		if (_bounds.contains(x, contentClickY) && specialAction != nullptr) {
			specialAction();
			return false;
		}
		return false;
	}

	bool ActionPrefItem::handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) {
		return false;
	}

	// --- UrlPrefItem Implementation ---
	UrlPrefItem::UrlPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
		const std::string& name, const std::string& description, const std::string& url)
		: ActionPrefItem(state, mgr, name, "", description, "url"), _url(url) {
	}

	bool UrlPrefItem::processClick(float x, float y, float contentClickY,
		float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) {
		if (_bounds.contains(x, contentClickY) && !_url.empty()) {
			_state->openURL(_url);
			return false;
		}
		return false;
	}

	// --- Settings Implementation ---
	Settings::Settings(AppState* ss, const std::string& appName,
		std::function<void(const std::string& key, int value)> intChangeCallback)
		: View(ss, WRAP, 0, CENTER_ALIGN),
		layout(ss),
		scrollOffset(0),
		isScrolling(false),
		isDropdownScrolling(false),
		activePointerId(-1),
		lastTouchY(0),
		mgr(tsl::app::getStoragePath(appName)),
		fullRedrawNeeded(true),
		activeItem(nullptr),
		pressedItem(nullptr),
		activeDropdownScroller(nullptr),
		windex(-1),
		onHideCallback(nullptr),
		contentHeight(0),
		isClosing(false)
	{
		if (intChangeCallback)
			mgr.SetIntChangeCallback(intChangeCallback);
		prio = 0;
		init();
	}

	void Settings::delRecursiveDraw() {
		_STATE->graphics.deleteWindow(windex);
		windex = -1;
		View::delRecursiveDraw();
	}

	AbstractPrefItem* Settings::getByKey(const std::string& key) {
		for (auto it = categories.rbegin(); it != categories.rend(); ++it) {
			for (auto item_it = it->items.rbegin(); item_it != it->items.rend(); ++item_it) {
				AbstractPrefItem* item = item_it->get();
				if (item->getKey() == key) {
					return item;
				}
			}
		}
		return nullptr;
	}

	void Settings::show(OnHideCallback cb) {
		onHideCallback = cb;
		init();
		updateItemValuesFromManager();
		fullRedrawNeeded = true;
		dirtyRegion.setRect(SkIRect::MakeLTRB((int)layout.xStart, (int)layout.yStart, (int)layout.xEnd, (int)layout.yEnd));
		addDraw();
		addCB();
	}

	void Settings::showBlocking() {
		std::atomic<bool> completed{ false };
		show([&]() {
			completed.store(true, std::memory_order_release);
			completed.notify_one();
			});
		while (!completed.load(std::memory_order_acquire)) {
			completed.wait(false);
		}
		return;
	}

	void Settings::hide() {
		delCB();
		deldraw();
		lastTouchY = 0;
		isScrolling = false;
		isDropdownScrolling = false;
		activePointerId = -1;
		isClosing = false;
		scrollOffset = 0;
		activeItem = nullptr;
		pressedItem = nullptr;
		activeDropdownScroller = nullptr;
		dirtyRegion.setEmpty();
		fullRedrawNeeded = true;

		if (onHideCallback) {
			onHideCallback();
			onHideCallback = nullptr;
		}
	}

	void Settings::init() {
		width  = _STATE->windowWidth;
		height = _STATE->windowHeight;
		layout.init();
		startx = (int)layout.xStart;
		starty = (int)layout.yStart;
		stopx  = (int)layout.xEnd;
		stopy  = (int)layout.yEnd;
	}

	void Settings::loadFromXml(const std::string& xmlData) {
		parseXml(xmlData);
	}

	void Settings::render(void* c) {
		if (_STATE->windowHeight != height || _STATE->windowWidth != width) {
			init();
			fullRedrawNeeded = true;

			activeItem = nullptr;
			pressedItem = nullptr;
			activeDropdownScroller = nullptr;
			isScrolling = false;
			isDropdownScrolling = false;
			isClosing = false;
			activePointerId = -1;
			lastTouchY = 0;

			for (auto& cat : categories) {
				for (auto& item : cat.items) {
					if (item->isDropdownItem() && item->hasExpandedContentActive()) {
						float dummyTop = 0, dummyBottom = 0;
						item->closeExpandedContent(scrollOffset, dummyTop, dummyBottom);
					}
				}
			}
		}

		const float ox = _STATE->graphics.xOffset;
		const float oy = _STATE->graphics.yOffset;
		int fullW = width + (int)(2.0f * ox);
		int fullH = height + (int)(2.0f * oy);

		SkCanvas* canvas = _STATE->graphics.getCanvas(windex, -ox, -oy, fullW, fullH);
		if (!canvas) {
			return;
		}
		canvas->save();

		SkPaint bg;
		bg.setColor(MATERIAL_DARK_BG_PRIMARY);

		if (fullRedrawNeeded) {
			// Fill full canvas (including letterbox bars) then translate to plugin content space
			canvas->drawRect(SkRect::MakeWH(fullW, fullH), bg);
			canvas->translate(ox, oy);
		}
		else {
			if (dirtyRegion.isEmpty()) {
				canvas->restore();
				return;
			}
			// Translate first so dirty rects (in content space) map correctly
			canvas->translate(ox, oy);
			// Build clip path from dirty rects (clipPath respects current transform, clipRegion does not)
			SkPath dirtyClip;
			for (SkRegion::Iterator iter(dirtyRegion); !iter.done(); iter.next()) {
				SkRect r = SkRect::Make(iter.rect());
				dirtyClip.addRect(r);
				canvas->drawRect(r, bg);
			}
			canvas->clipPath(dirtyClip, SkClipOp::kIntersect, true);
		}

		float currentY = layout.topMargin + layout.closeBtnSize + layout.universalItemSpacing;

		SkFont catFont(_STATE->font_normal);
		catFont.setSize(_STATE->textsize2 * 0.8f);
		catFont.setEmbolden(true);

		SkFont itemFont(_STATE->font_normal);
		itemFont.setSize(_STATE->textsize2 * 0.9f);

		SkFont catDescFont(_STATE->font_normal);
		catDescFont.setSize(_STATE->textsize2 * 0.7f);
		catDescFont.setEmbolden(false);

		std::vector<AbstractPrefItem*> expandedDropdowns;

		for (size_t catIndex = 0; catIndex < categories.size(); ++catIndex) {
			auto& cat = categories[catIndex];

			SkPaint catPaint;
			catPaint.setColor(MATERIAL_LIGHT_BLUE_500);
			canvas->drawSimpleText(cat.name.c_str(), cat.name.size(), SkTextEncoding::kUTF8,
				layout.textOriginX(), currentY - scrollOffset, catFont, catPaint);

			currentY += catFont.getSpacing();

			if (!cat.description.empty()) {
				currentY += layout.itemNameToDescSpacing;

				SkPaint descPaint;
				descPaint.setColor(MATERIAL_TEXT_SECONDARY);

				std::vector<std::string> descLines = wrapText(cat.description, catDescFont, layout.descWidth());

				for (const std::string& line : descLines) {
					canvas->drawSimpleText(line.c_str(), line.size(), SkTextEncoding::kUTF8,
						layout.textOriginX(), currentY - scrollOffset, catDescFont, descPaint);
					currentY += catDescFont.getSpacing();
				}
			}

			currentY += layout.catTitleItemSpacing;

			for (size_t itemIndex = 0; itemIndex < cat.items.size(); ++itemIndex) {
				auto& item = cat.items[itemIndex];

				item->draw(canvas, currentY, scrollOffset, itemFont, catDescFont,
					pressedItem == item.get(), layout);

				if (item->hasExpandedContentActive()) {
					expandedDropdowns.push_back(item.get());
				}

				float itemHeight = calculateItemHeight(item.get(), itemFont, catDescFont, layout);
				currentY += itemHeight;

				if (itemIndex < cat.items.size() - 1) {
					currentY += layout.universalItemSpacing;
				}
			}

			if (catIndex < categories.size() - 1) {
				currentY += layout.universalItemSpacing;

				SkPaint separatorPaint;
				separatorPaint.setColor(MATERIAL_CATEGORY_SEPARATOR_COLOR);
				canvas->drawLine(layout.xStart, currentY - scrollOffset,
					layout.xStart + layout.windowWidth, currentY - scrollOffset, separatorPaint);

				currentY += layout.universalItemSpacing;
			}
		}

		contentHeight = currentY + layout.bottomMargin;
		contentHeight = std::max(height + 1.0f, contentHeight);

		for (AbstractPrefItem* dropdownItem : expandedDropdowns) {
			if (dropdownItem && dropdownItem->isDropdownItem() && dropdownItem->hasExpandedContentActive()) {
				SkRect itemBounds = dropdownItem->getBounds();
				float dropdownX = itemBounds.left();
				float dropdownY_content = itemBounds.bottom() + 2;
				float dropdownY_canvas = dropdownY_content - scrollOffset;
				float dropdownW = itemBounds.width();

				dropdownItem->drawExpandedOptions(canvas, dropdownX, dropdownY_canvas, dropdownW, itemFont, layout);
			}
		}

		if (contentHeight > height) {
			drawScrollbar(canvas, scrollOffset, contentHeight, height, layout);
		}

		float closeBtnRightEdge = layout.controlRightX();
		if (contentHeight > height) {
			closeBtnRightEdge -= (layout.scrollbarWidth + layout.scrollbarMargin * 2);
		}

		SkRect closeBtnRect = SkRect::MakeXYWH(
			closeBtnRightEdge - layout.closeBtnSize,
			layout.topMargin,
			layout.closeBtnSize,
			layout.closeBtnSize
		);
		closeBtnRRectBounds = SkRRect::MakeRectXY(closeBtnRect, MATERIAL_CORNER_RADIUS, MATERIAL_CORNER_RADIUS);

		SkPaint btnPaint;
		btnPaint.setColor(MATERIAL_BUTTON_NEUTRAL_BG);
		canvas->drawRRect(closeBtnRRectBounds, btnPaint);

		SkPaint xPaint;
		xPaint.setColor(MATERIAL_TEXT_PRIMARY);
		xPaint.setStrokeWidth(2.5f);
		float closePadding = layout.closeBtnSize * 0.28f;
		canvas->drawLine(closeBtnRect.left() + closePadding, closeBtnRect.top() + closePadding,
			closeBtnRect.right() - closePadding, closeBtnRect.bottom() - closePadding, xPaint);
		canvas->drawLine(closeBtnRect.right() - closePadding, closeBtnRect.top() + closePadding,
			closeBtnRect.left() + closePadding, closeBtnRect.bottom() - closePadding, xPaint);

		canvas->restore();

		dirtyRegion.setEmpty();
		fullRedrawNeeded = false;
	}

	void Settings::callback(const InputEvent& e) {
		float contentClickY = e.y + scrollOffset;

		switch (e.action) {
		case ACTION_DOWN: {
			if (pressedItem) {
				addDirtyRect(pressedItem->getDisplayBounds().roundOut().makeOffset(0, -scrollOffset));
			}

			SkRect boundingBox = closeBtnRRectBounds.getBounds();

			if (boundingBox.contains(e.x, e.y)) {
				isClosing = true;
				activePointerId = e.pointer_id;
				lastTouchY = e.y;
				addDirtyRect(closeBtnRRectBounds.getBounds().roundOut());
				return;
			}

			activePointerId = e.pointer_id;
			lastTouchY = e.y;
			isScrolling = false;
			isDropdownScrolling = false;
			activeDropdownScroller = nullptr;

			bool handledByDropdown = false;
			for (auto& cat : categories) {
				for (auto& item_ptr : cat.items) {
					if (item_ptr->isDropdownItem() && item_ptr->hasExpandedContentActive()) {
						SkRect itemBounds = item_ptr->getBounds();
						float dropdownX = itemBounds.left();
						float dropdownY_content = itemBounds.bottom() + 2;
						float dropdownY_canvas = dropdownY_content - scrollOffset;
						float dropdownW = itemBounds.width();

						float dropdownContentHeight = item_ptr->getExpandedContentTotalHeight();
						float dropdownVisibleHeight = item_ptr->getExpandedContentVisibleHeight();
						float actualDisplayHeight = std::min(dropdownContentHeight, dropdownVisibleHeight);

						SkRect expandedCanvasBounds = SkRect::MakeXYWH(dropdownX, dropdownY_canvas,
							dropdownW, actualDisplayHeight);

						if (expandedCanvasBounds.contains(e.x, e.y)) {
							if (actualDisplayHeight < dropdownContentHeight) {
								isDropdownScrolling = true;
								activeDropdownScroller = item_ptr.get();
								addDirtyRect(expandedCanvasBounds.roundOut());
								handledByDropdown = true;
								break;
							}
							else {
								activeItem = item_ptr.get();
								handledByDropdown = true;
								break;
							}
						}
					}
				}
				if (handledByDropdown) break;
			}

			if (!handledByDropdown) {
				activeItem = getItemAt(e.x, contentClickY);

				if (activeItem) {
					SkRect highlightArea = activeItem->getDisplayBounds();
					highlightArea.offset(0, -scrollOffset);
					addDirtyRect(highlightArea.roundOut());
					pressedItem = activeItem;
					redraw();
				}

				bool anyDropdownClosed = false;
				for (auto& cat : categories) {
					for (auto& item_ptr : cat.items) {
						if (item_ptr->isDropdownItem() && item_ptr->hasExpandedContentActive()) {
							SkRect itemControlBounds_content = item_ptr->getBounds();

							SkRect itemBounds = item_ptr->getBounds();
							float dropdownX = itemBounds.left();
							float dropdownY_content = itemBounds.bottom() + 2;
							float dropdownY_canvas = dropdownY_content - scrollOffset;
							float dropdownW = itemBounds.width();
							float actualDisplayHeight = std::min(item_ptr->getExpandedContentTotalHeight(),
								item_ptr->getExpandedContentVisibleHeight());
							SkRect expandedCanvasBounds = SkRect::MakeXYWH(dropdownX, dropdownY_canvas,
								dropdownW, actualDisplayHeight);

							if (!itemControlBounds_content.contains(e.x, contentClickY) &&
								!expandedCanvasBounds.contains(e.x, e.y)) {
								float dirtyTop = 0, dirtyBottom = 0;
								if (item_ptr->closeExpandedContent(scrollOffset, dirtyTop, dirtyBottom)) {
									addDirtyRect(SkIRect::MakeLTRB((int)layout.textOriginX(), (int)(dirtyTop - scrollOffset),
										(int)layout.controlRightX(), (int)(dirtyBottom - scrollOffset)));
									anyDropdownClosed = true;
								}
							}
						}
					}
				}
				if (anyDropdownClosed) {
					redraw();
				}
			}
		}
						break;

		case ACTION_MOVE:
			if (e.pointer_id == activePointerId) {
				float dy = e.y - lastTouchY;

				if (pressedItem && std::abs(dy) > 5) {
					addDirtyRect(pressedItem->getDisplayBounds().roundOut().makeOffset(0, -scrollOffset));
					pressedItem = nullptr;
					redraw();
				}

				if (!isScrolling && !isDropdownScrolling && std::abs(dy) > 5) {
					isScrolling = true;
					activeItem = nullptr;
				}

				if (isDropdownScrolling && activeDropdownScroller) {
					float dirtyTop = 0, dirtyBottom = 0;
					if (activeDropdownScroller->handleScroll(dy, dirtyTop, dirtyBottom)) {
						addDirtyRect(SkIRect::MakeLTRB((int)layout.textOriginX(), (int)(dirtyTop - scrollOffset),
							(int)layout.controlRightX(), (int)(dirtyBottom - scrollOffset)));
						redraw();
					}
				}
				else if (isScrolling) {
					float oldScrollOffset = scrollOffset;
					scrollOffset = std::clamp(scrollOffset - dy, 0.0f, std::max(0.0f, contentHeight - height));
					if (oldScrollOffset != scrollOffset) {
						addDirtyRect(SkIRect::MakeLTRB((int)layout.xStart, (int)layout.yStart, (int)layout.xEnd, (int)layout.yEnd));
						redraw();
					}
				}
				lastTouchY = e.y;

				if (std::abs(dy) > 5) {
					isClosing = false;
				}
			}
			break;

		case ACTION_UP:
			if (e.pointer_id != activePointerId) {
				break;
			}

			if (pressedItem) {
				addDirtyRect(pressedItem->getDisplayBounds().roundOut().makeOffset(0, -scrollOffset));
				pressedItem = nullptr;
				redraw();
			}

			if (isClosing) {
				hide();
				return;
			}

			if (!isScrolling && !isDropdownScrolling && activeItem) {
				float dirtyTop = 0, dirtyBottom = 0;

				if (activeItem->isDropdownItem() && activeItem->hasExpandedContentActive()) {
					if (activeItem->processClick(e.x, e.y, contentClickY, dirtyTop, dirtyBottom, layout)) {
						addDirtyRect(SkIRect::MakeLTRB((int)layout.textOriginX(), (int)(dirtyTop - scrollOffset),
							(int)layout.controlRightX(), (int)(dirtyBottom - scrollOffset)));
						redraw();
					}
				}
				else {
					if (activeItem->processClick(e.x, e.y, contentClickY, dirtyTop, dirtyBottom, layout)) {
						addDirtyRect(SkIRect::MakeLTRB((int)layout.textOriginX(), (int)(dirtyTop - scrollOffset),
							(int)layout.controlRightX(), (int)(dirtyBottom - scrollOffset)));
						redraw();
					}
				}
			}

			if (isDropdownScrolling && activeDropdownScroller) {
				if (activeDropdownScroller->isDropdownItem()) {
					SkRect itemBounds = activeDropdownScroller->getBounds();
					float dropdownY_canvas = itemBounds.bottom() + 2 - scrollOffset;
					float actualDisplayHeight = std::min(activeDropdownScroller->getExpandedContentTotalHeight(),
						activeDropdownScroller->getExpandedContentVisibleHeight());
					SkRect dropdownArea = SkRect::MakeXYWH(itemBounds.left(), dropdownY_canvas,
						itemBounds.width(), actualDisplayHeight);
					addDirtyRect(dropdownArea.roundOut());
					redraw();
				}
			}

			isScrolling = false;
			isDropdownScrolling = false;
			activeItem = nullptr;
			activeDropdownScroller = nullptr;
			activePointerId = -1;
			isClosing = false;
			break;

		case ACTION_MOUSE_WHEEL: {
			if (isScrolling || isDropdownScrolling) {
				break;
			}
			activeItem = nullptr;

			if (pressedItem) {
				addDirtyRect(pressedItem->getDisplayBounds().roundOut().makeOffset(0, -scrollOffset));
				pressedItem = nullptr;
			}

			float oldScrollOffset = scrollOffset;
			scrollOffset = std::clamp(scrollOffset - (e.pointer_id * 25), 0.0f,
				std::max(0.0f, contentHeight - height));
			if (oldScrollOffset != scrollOffset) {
				addDirtyRect(SkIRect::MakeLTRB((int)layout.xStart, (int)layout.yStart, (int)layout.xEnd, (int)layout.yEnd));
			}
			redraw();
			break;
		}

		default:
			break;
		}
	}

	// --- Private Methods ---
	float Settings::calculateItemHeight(AbstractPrefItem* item, const SkFont& itemFont,
		const SkFont& descFont, const ScalableLayout& layout) {
		float height = itemFont.getSpacing();

		if (!item->getDescription().empty()) {
			height += layout.itemNameToDescSpacing;

			std::vector<std::string> descLines = wrapText(item->getDescription(), descFont, layout.descWidth());
			height += (descLines.size() * descFont.getSpacing());
		}

		float minHeight = layout.controlHeight + layout.itemNameToDescSpacing;
		return std::max(height, minHeight);
	}

	void Settings::addDirtyRect(const SkIRect& rect) {
		SkIRect safeBounds = SkIRect::MakeLTRB((int)layout.xStart, (int)layout.yStart,
		                                        (int)layout.xEnd,   (int)layout.yEnd);
		SkIRect clippedRect = rect;
		if (!clippedRect.intersect(safeBounds)) {
			return;
		}
		dirtyRegion.op(clippedRect, SkRegion::kUnion_Op);
	}

	std::string Settings::getAttributeValue(const std::string& tag, const std::string& attr) {
		std::string search = attr + "=\"";
		size_t pos = tag.find(search);
		if (pos == std::string::npos) return "";
		pos += search.length();
		size_t endPos = tag.find("\"", pos);
		if (endPos == std::string::npos) return "";
		return tag.substr(pos, endPos - pos);
	}

	void Settings::parseXml(const std::string& xml) {
		categories.clear();
		size_t pos = 0;
		while ((pos = xml.find("<PrefCategory", pos)) != std::string::npos) {
			size_t endTagPos = xml.find(">", pos);
			if (endTagPos == std::string::npos) break;
			std::string catTag = xml.substr(pos, endTagPos - pos + 1);
			PrefCategory cat;
			cat.name = getAttributeValue(catTag, "name");
			cat.description = getAttributeValue(catTag, "description");
			pos = endTagPos + 1;

			size_t catEnd = xml.find("</PrefCategory>", pos);
			if (catEnd == std::string::npos) break;

			size_t prefPos = pos;
			while ((prefPos = xml.find("<Pref", prefPos)) != std::string::npos && prefPos < catEnd) {
				size_t prefEnd = xml.find("/>", prefPos);
				if (prefEnd == std::string::npos || prefEnd > catEnd) break;

				std::string prefTag = xml.substr(prefPos, prefEnd - prefPos + 2);
				std::string name = getAttributeValue(prefTag, "name");
				std::string key = getAttributeValue(prefTag, "key");
				std::string description = getAttributeValue(prefTag, "description");
				std::string type = getAttributeValue(prefTag, "type");
				std::string value = getAttributeValue(prefTag, "value");
				std::string url = getAttributeValue(prefTag, "url");
				std::string displayOptionsStr = getAttributeValue(prefTag, "displayOptions");
				std::string optionValuesStr = getAttributeValue(prefTag, "optionValues");

				if (type == "bool" && !name.empty() && !key.empty()) {
					std::string boolValue = value.empty() ? "false" : value;
					cat.items.push_back(std::make_unique<BoolPrefItem>(_STATE, &mgr, name, key, description, boolValue));
				}
				else if (type == "slider" && !name.empty() && !key.empty()) {
					std::string sliderValue = value.empty() ? "0.5" : value;
					cat.items.push_back(std::make_unique<SliderPrefItem>(_STATE, &mgr, name, key, description, sliderValue));
				}
				else if (type == "dropdown" && !name.empty() && !key.empty() &&
					!displayOptionsStr.empty() && !optionValuesStr.empty()) {
					std::string dropdownValue = value.empty() ? "0" : value;
					cat.items.push_back(std::make_unique<DropdownPrefItem>(_STATE, &mgr, name, key,
						description, dropdownValue, displayOptionsStr, optionValuesStr));
				}
				else if (type == "url" && !name.empty() && !url.empty()) {
					cat.items.push_back(std::make_unique<UrlPrefItem>(_STATE, &mgr, name, description, url));
				}
				else if (type == "action" && !name.empty()) {
					cat.items.push_back(std::make_unique<ActionPrefItem>(_STATE, &mgr, name, key, description));
				}
				prefPos = prefEnd + 2;
			}
			categories.push_back(std::move(cat));
			pos = catEnd + 15;
		}
	}

	void Settings::updateItemValuesFromManager() {
		for (auto& cat : categories) {
			for (auto& item : cat.items) {
				item->updateValueFromManager();
			}
		}
	}

	void Settings::drawScrollbar(SkCanvas* canvas, float currentScrollOffset,
		float totalContentHeight, float visibleHeight, const ScalableLayout& layout) {
		float scrollAreaX = layout.xEnd - layout.scrollbarWidth - layout.scrollbarMargin;
		float scrollAreaY = layout.topMargin;
		float scrollAreaHeight = height - layout.topMargin - layout.bottomMargin;

		if (totalContentHeight <= visibleHeight) {
			return;
		}

		SkPaint scrollbarPaint;
		scrollbarPaint.setColor(MATERIAL_TEXT_SECONDARY);

		float scrollRatio = visibleHeight / totalContentHeight;
		float thumbHeight = scrollRatio * scrollAreaHeight;
		float thumbY = scrollAreaY + (currentScrollOffset / (totalContentHeight - visibleHeight)) *
			(scrollAreaHeight - thumbHeight);

		SkRect thumbRect = SkRect::MakeXYWH(scrollAreaX, thumbY, layout.scrollbarWidth, thumbHeight);
		canvas->drawRect(thumbRect, scrollbarPaint);
	}

	AbstractPrefItem* Settings::getItemAt(float x, float contentClickY) {
		// First check if clicking on any expanded dropdown content
		for (auto it = categories.rbegin(); it != categories.rend(); ++it) {
			for (auto item_it = it->items.rbegin(); item_it != it->items.rend(); ++item_it) {
				AbstractPrefItem* item = item_it->get();
				if (item->hasExpandedContentActive()) {
					SkRect expandedCanvasBounds = item->getExpandedContentCanvasBounds(30, scrollOffset);
					if (expandedCanvasBounds.contains(x, contentClickY - scrollOffset)) {
						return item;
					}
				}
			}
		}

		// Then check regular item bounds
		for (auto it = categories.rbegin(); it != categories.rend(); ++it) {
			for (auto item_it = it->items.rbegin(); item_it != it->items.rend(); ++item_it) {
				AbstractPrefItem* item = item_it->get();
				if (item->contains(x, contentClickY)) {
					return item;
				}
			}
		}
		return nullptr;
	}

} // namespace tsl::graphics