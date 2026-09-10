#pragma once
#ifndef _SETTINGS2_H
#define _SETTINGS2_H

// Settings dialog, second iteration: same XML vocabulary and public API as
// the old Settings (SettingsView.h), but drawn and operated like the rest of
// the app -- checkbox squares, arrow selectors with a RecyclerView option
// popup, list-row highlights, textsize2-based typography -- instead of the
// Material look. The old class stays untouched; apps switch by instantiating
// Settings2 (grainstorm Event/ApplyFromUi.cpp is the first user).

#include "settings.h"
#include "view.h"
#include "Input.h"
#include "RecyclerView.h"

#include <include/core/SkRect.h>

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

namespace tsl::graphics {

	// One preference row, parsed from a <Pref .../> tag.
	class Pref2 {
	public:
		enum class Kind : uint8_t { Bool, Dropdown, Slider, Action, Url };

		Pref2(Kind k, std::string _name, std::string _key, std::string _description)
			: kind(k), name(std::move(_name)), key(std::move(_key)),
			  description(std::move(_description)) {}

		void setAction(std::function<void()> a) { specialAction = std::move(a); }
		const std::string& getKey() const { return key; }
		const std::string& getName() const { return name; }

		// index of value in optionValues, -1 when it matches no option
		int optionIndex() const {
			for (size_t i = 0; i < optionValues.size(); ++i)
				if (optionValues[i] == value)
					return (int)i;
			return -1;
		}

		Kind kind;
		std::string name, key, description, url;
		std::string value;                        // persisted value, string form
		float sliderVal{ .5f };                   // Slider only, 0..1
		std::vector<std::string> displayOptions;  // Dropdown
		std::vector<std::string> optionValues;    // Dropdown
		std::function<void()> specialAction{};

		// layout cache, content coords (0 == top of the scrolled area);
		// filled by Settings2::layoutContent()
		float rowTop{}, rowH{};
		float nameBaseline{}, descBaseline0{};
		std::vector<std::string> descLines;
		SkRect ctrl{};                            // control box (checkbox/value/slider)
	};

	class Settings2 : public View, protected ScrollViewBase {
		using OnHideCallback = std::function<void()>;

	public:
		Settings2(AppState* ss, const std::string& appName = "Grainstorm",
			std::function<void(const std::string& key, int value)> intChangeCallback = {});

		Pref2* getByKey(const std::string& key);
		void loadFromXml(const std::string& xmlData);
		void show(OnHideCallback cb = nullptr);
		void showBlocking();
		void hide();

		void init() override;
		void render(void*) override;
		void callback(const InputEvent& e) override;

	protected:
		void addRecursiveCB() override;
		void delRecursiveCB() override;
		void delRecursiveDraw() override;

	private:
		struct Category {
			std::string name, description;
			std::vector<Pref2> items;
			// layout cache, content coords
			float titleBaseline{}, descBaseline0{};
			std::vector<std::string> descLines;
			float sepY{ -1 };                     // separator line below (-1 = none)
		};

		// Row of the dropdown option popup. The popup is a plain RecyclerView,
		// whose rows are constructed internally, so the owning Settings2 is
		// published here while its popup is open -- there is only one settings
		// view per app, a single slot is enough.
		class OptionRow : public TextViewBase<std::string> {
		public:
			explicit OptionRow(tsl::AppState* appState) : TextViewBase<std::string>(appState) {}
			void computeWidth(int index) override;
			void render(SkCanvas* c, int index) override;
			int32_t cb(const InputEvent& e, int index) override;
			static inline std::atomic<Settings2*> owner{ nullptr };
		};

		void layoutContent();
		void updateItemValuesFromManager();
		void parseXml(const std::string& xml);
		void applyOption(Pref2& p, int index);
		void openOptions(Pref2& p);
		void closeOptions();
		void onOptionPicked(int index);
		Pref2* prefAt(float x, float cy);
		void drawBar(SkCanvas* c);
		void drawPref(SkCanvas* c, Pref2& p, float yoff,
			SkFont& itemFont, SkFont& descFont);

		std::vector<Category> categories;
		settings::SettingsManager mgr;
		OnHideCallback onHideCallback{};
		int windex{ -1 };
		std::string title{ "SETTINGS" };

		// metrics, local canvas coords (== plugin content coords)
		float xStart{}, xEnd{}, yStart{}, yEnd{};
		float pad{}, barH{}, contentTop{}, contentHeightTotal{};

		// touch state on top of ScrollViewBase's
		Pref2* hotPref{};
		bool closeHot{};
		bool sliding{};

		std::shared_ptr<RecyclerView<OptionRow, std::string>> optionsRv;
		Pref2* rvTarget{};
	};

} // namespace tsl::graphics

#endif // _SETTINGS2_H
