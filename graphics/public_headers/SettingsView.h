#pragma once
#ifndef _SETTINGS_VIEW_H
#define _SETTINGS_VIEW_H

#include "settings.h"
#include "view.h"
#include "Input.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkFont.h"
#include "include/core/SkFontMetrics.h"
#include "include/core/SkRect.h"
#include "include/core/SkRRect.h"
#include "include/core/SkPath.h"
#include "include/core/SkPoint.h"
#include "include/core/SkRegion.h"

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <atomic>
#include <cmath>
#include <map>
#include <memory>

#include "app.h"

namespace tsl::graphics {

	// --- Material Design Color Palette ---
	extern const SkColor MATERIAL_DARK_BG_PRIMARY;
	extern const SkColor MATERIAL_DARK_BG_SURFACE;
	extern const SkColor MATERIAL_LIGHT_BLUE_500;
	extern const SkColor MATERIAL_TEXT_PRIMARY;
	extern const SkColor MATERIAL_TEXT_SECONDARY;
	extern const SkColor MATERIAL_HIGHLIGHT;
	extern const SkColor MATERIAL_CLICK_HIGHLIGHT;
	extern const SkColor MATERIAL_CLICK_HIGHLIGHT_BLUE;
	extern const SkColor MATERIAL_CONTROL_PRESSED;
	extern const SkColor MATERIAL_BUTTON_NEUTRAL_BG;
	extern const SkColor MATERIAL_BORDER_LIGHT;
	extern const SkColor MATERIAL_CATEGORY_SEPARATOR_COLOR;
	extern const float MATERIAL_CORNER_RADIUS;

	// --- Scalable Layout Helper ---
	struct ScalableLayout {
		float windowWidth;   // full drawable width  (pluginW + 2*xOffset)
		float windowHeight;
		float pluginWidth;   // plugin content width
		float pluginHeight;
		float xStart;        // local x of safe-area left  edge
		float xEnd;          // local x of safe-area right edge
		float yStart;        // local y of safe-area top   edge
		float yEnd;          // local y of safe-area bottom edge
		float leftMargin;
		float rightMargin;
		float topMargin;
		float bottomMargin;
		float itemControlWidth;
		float controlHeight;
		float universalItemSpacing;
		float itemNameToDescSpacing;
		float descTextPaddingRight;
		float catTitleItemSpacing;
		float closeBtnSize;
		float closeBtnMargin;
		float scrollbarWidth;
		float scrollbarMargin;
		float dropdownOptionHeight;
		int maxVisibleOptions;
		float checkboxSize;
		float checkboxPadding;
		float dropdownArrowSize;
		float dropdownTextPadding;

		ScalableLayout(tsl::AppState* appState);
		void init();
		float getCategoryFontSize() const;
		float getItemFontSize() const;
		float getDescFontSize() const;

		// All positions in local canvas coords; span the full safe area.
		float textOriginX()    const { return xStart + leftMargin; }
		float controlRightX()  const { return xEnd   - rightMargin; }
		float controlOriginX() const { return controlRightX() - itemControlWidth; }
		float descWidth()      const { return controlOriginX() - textOriginX() - descTextPaddingRight; }
		float rowWidth()       const { return controlRightX() - textOriginX(); }

	private:
		tsl::AppState* _appState;
	};

	// --- Helper functions ---
	std::vector<std::string> wrapText(const std::string& text, const SkFont& font, float maxWidth);
	std::string truncateText(const std::string& text, const SkFont& font, float maxWidth);
	std::vector<std::string> splitAndTrimString(const std::string& s, char delimiter);

	// --- Abstract Base Class ---
	class AbstractPrefItem {
	protected:
		std::string _name;
		std::string _key;
		std::string _description;
		std::string _type;
		SkRect _bounds;
		SkRect _displayBounds;
		AppState* _state;
		tsl::settings::SettingsManager* _mgr;
		std::function<void()> specialAction;

	public:
		AbstractPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
			const std::string& name, const std::string& key,
			const std::string& description, const std::string& type);
		virtual ~AbstractPrefItem() = default;

		void setAction(std::function<void()> action);
		virtual void draw(SkCanvas* canvas, float currentY, float scrollOffset,
			SkFont& itemFont, SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) = 0;
		virtual bool processClick(float x, float y, float contentClickY,
			float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) = 0;
		virtual bool handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) = 0;

		const std::string& getKey() const;
		const std::string& getType() const;
		const std::string& getName() const;
		const std::string& getDescription() const;
		const SkRect& getBounds() const;
		const SkRect& getDisplayBounds() const;

		virtual void updateValueFromManager() = 0;
		bool contains(float x, float contentClickY) const;
		virtual bool hasExpandedContentActive() const;
		virtual bool isDropdownItem() const;
		virtual void drawExpandedOptions(SkCanvas* canvas, float dropdownX, float dropdownY_on_canvas,
			float dropdownW, SkFont& font, const ScalableLayout& layout);
		virtual SkRect getExpandedContentBounds(float controlH) const;
		virtual SkRect getExpandedContentCanvasBounds(float controlH, float mainScrollOffset) const;
		virtual float getExpandedContentScrollOffset() const;
		virtual float getExpandedContentTotalHeight() const;
		virtual float getExpandedContentVisibleHeight() const;
		virtual bool handleExpandedContentTouch(float x, float y, float contentClickY,
			int pointerId, float& lastTouchY, float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout);
		virtual bool closeExpandedContent(float mainScrollOffset, float& dirtyRectTop, float& dirtyRectBottom);
	};

	// --- BoolPrefItem ---
	class BoolPrefItem : public AbstractPrefItem {
	private:
		std::string _value;
	public:
		BoolPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
			const std::string& name, const std::string& key,
			const std::string& description, const std::string& initialValue);

		void updateValueFromManager() override;
		void draw(SkCanvas* canvas, float currentY, float scrollOffset, SkFont& itemFont,
			SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) override;
		bool processClick(float x, float y, float contentClickY,
			float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) override;
		bool handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) override;
	};

	// --- SliderPrefItem ---
	class SliderPrefItem : public AbstractPrefItem {
	private:
		std::string _value;
	public:
		SliderPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
			const std::string& name, const std::string& key,
			const std::string& description, const std::string& initialValue);

		void updateValueFromManager() override;
		void draw(SkCanvas* canvas, float currentY, float scrollOffset, SkFont& itemFont,
			SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) override;
		bool processClick(float x, float y, float contentClickY,
			float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) override;
		bool handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) override;
	};

	// --- DropdownPrefItem ---
	class DropdownPrefItem : public AbstractPrefItem {
	private:
		std::string _value;
		std::string _currentValueDisplayName;
		std::vector<std::string> _displayOptions;
		std::vector<std::string> _optionValues;
		bool _isDropdownOpen;
		float _dropdownScrollOffset;
		float _dropdownContentHeight;
		float _dropdownVisibleHeight;

	public:
		DropdownPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
			const std::string& name, const std::string& key,
			const std::string& description, const std::string& initialValue,
			const std::string& displayOptionsStr, const std::string& optionValuesStr);

		void updateValueFromManager() override;
		void draw(SkCanvas* canvas, float currentY, float scrollOffset, SkFont& itemFont,
			SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) override;
		bool hasExpandedContentActive() const override;
		bool isDropdownItem() const override;
		void drawExpandedOptions(SkCanvas* canvas, float dropdownX, float dropdownY_on_canvas,
			float dropdownW, SkFont& font, const ScalableLayout& layout) override;
		bool processClick(float x, float y_on_canvas, float contentClickY,
			float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) override;
		bool handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) override;
		SkRect getExpandedContentBounds(float controlH) const override;
		SkRect getExpandedContentCanvasBounds(float controlH, float mainScrollOffset) const override;
		float getExpandedContentScrollOffset() const override;
		float getExpandedContentTotalHeight() const override;
		float getExpandedContentVisibleHeight() const override;
		bool handleExpandedContentTouch(float x, float y, float contentClickY,
			int pointerId, float& lastTouchY, float& dirtyRectTop, float& dirtyRectBottom,
			const ScalableLayout& layout) override;
		bool closeExpandedContent(float mainScrollOffset, float& dirtyRectTop, float& dirtyRectBottom) override;
	};

	// --- ActionPrefItem ---
	class ActionPrefItem : public AbstractPrefItem {
	public:
		ActionPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
			const std::string& name, const std::string& key,
			const std::string& description, const std::string& type = "action");

		void updateValueFromManager() override;
		void draw(SkCanvas* canvas, float currentY, float scrollOffset, SkFont& itemFont,
			SkFont& itemDescFont, bool isPressed, const ScalableLayout& layout) override;
		bool processClick(float x, float y, float contentClickY,
			float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) override;
		bool handleScroll(float dy, float& dirtyRectTop, float& dirtyRectBottom) override;
	};

	// --- UrlPrefItem ---
	class UrlPrefItem : public ActionPrefItem {
	private:
		std::string _url;
	public:
		UrlPrefItem(AppState* state, tsl::settings::SettingsManager* mgr,
			const std::string& name, const std::string& description, const std::string& url);

		bool processClick(float x, float y, float contentClickY,
			float& dirtyRectTop, float& dirtyRectBottom, const ScalableLayout& layout) override;
	};

	// --- Settings View Class ---
	class Settings : public View {
		using OnHideCallback = std::function<void()>;

	protected:
		void delRecursiveDraw();

	private:
		struct PrefCategory {
			std::string name;
			std::string description;
			std::vector<std::unique_ptr<AbstractPrefItem>> items;
		};

		int windex;
		OnHideCallback onHideCallback;
		SkRRect closeBtnRRectBounds;
		std::vector<PrefCategory> categories;
		float scrollOffset;
		float lastTouchY;
		bool isScrolling;
		bool isDropdownScrolling;
		int activePointerId;
		bool isClosing;
		AbstractPrefItem* activeItem;
		AbstractPrefItem* pressedItem;
		AbstractPrefItem* activeDropdownScroller;
		settings::SettingsManager mgr;
		float contentHeight;
		SkRegion dirtyRegion;
		bool fullRedrawNeeded;
		ScalableLayout layout;

		float calculateItemHeight(AbstractPrefItem* item, const SkFont& itemFont,
			const SkFont& descFont, const ScalableLayout& layout);
		void addDirtyRect(const SkIRect& rect);
		std::string getAttributeValue(const std::string& tag, const std::string& attr);
		void parseXml(const std::string& xml);
		void updateItemValuesFromManager();
		void drawScrollbar(SkCanvas* canvas, float currentScrollOffset,
			float totalContentHeight, float visibleHeight, const ScalableLayout& layout);
		AbstractPrefItem* getItemAt(float x, float contentClickY);
		AbstractPrefItem* getItemAt(float x, float contentClickY, float canvasY);
	public:
		Settings(AppState* ss, const std::string& appName = "Grainstorm",
			std::function<void(const std::string& key, int value)> intChangeCallback = {});

		AbstractPrefItem* getByKey(const std::string& key);
		void show(OnHideCallback cb = nullptr);
		void showBlocking();
		void hide();
		void init() override;
		void loadFromXml(const std::string& xmlData);
		void render(void* c) override;
		void callback(const InputEvent& e) override;
	};

} // namespace tsl::graphics

#endif // _SETTINGS_VIEW_H