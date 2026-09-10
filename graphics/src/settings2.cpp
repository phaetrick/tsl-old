#include "settings2.h"
#include "app.h"
#include "defines.h"
#include "keydefines.h"
#include "tools/PlatformPaths.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkFont.h>
#include <include/core/SkFontMetrics.h>
#include <include/core/SkPaint.h>
#include <include/core/SkPath.h>

#include <sstream>
#include <algorithm>
#include <cmath>

namespace tsl::graphics {

	// --- local helpers -----------------------------------------------------

	static std::string attrValue(const std::string& tag, const std::string& attr) {
		std::string search = attr + "=\"";
		size_t pos = tag.find(search);
		if (pos == std::string::npos) return "";
		pos += search.length();
		size_t endPos = tag.find('"', pos);
		if (endPos == std::string::npos) return "";
		return tag.substr(pos, endPos - pos);
	}

	static std::vector<std::string> splitTrim(const std::string& s, char delimiter) {
		std::vector<std::string> tokens;
		std::string token;
		std::istringstream stream(s);
		while (std::getline(stream, token, delimiter)) {
			token.erase(0, token.find_first_not_of(" \t\n\r"));
			token.erase(token.find_last_not_of(" \t\n\r") + 1);
			tokens.push_back(token);
		}
		return tokens;
	}

	static std::vector<std::string> wrapLines(const std::string& text, const SkFont& font,
		float maxWidth) {
		std::vector<std::string> lines;
		if (text.empty()) return lines;
		std::istringstream stream(text);
		std::string word, current;
		while (stream >> word) {
			std::string test = current.empty() ? word : current + " " + word;
			if (font.measureText(test.c_str(), test.size(), SkTextEncoding::kUTF8) <= maxWidth)
				current = test;
			else {
				if (!current.empty())
					lines.push_back(current);
				current = word;
			}
		}
		if (!current.empty())
			lines.push_back(current);
		return lines;
	}

	// keep colours and caps in sync with CheckBoxView::render (grainstorm)
	static void drawCheckBox(SkCanvas* canvas, const SkRect& box, bool checked, bool hot,
		float lw) {
		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setStrokeWidth(lw);
		if (hot) {
			paint.setStyle(SkPaint::kFill_Style);
			paint.setColor(skcol::blue_transparent);
			canvas->drawRect(box, paint);
		}
		paint.setColor(skcol::fg);
		paint.setStyle(SkPaint::kStroke_Style);
		paint.setStrokeCap(SkPaint::kRound_Cap);
		canvas->drawRect(box, paint);
		if (checked) {
			SkRect mark = box;
			mark.inset(lw * .5f, lw * .5f);
			canvas->drawLine(mark.fLeft, mark.fBottom, mark.fRight, mark.fTop, paint);
			canvas->drawLine(mark.fLeft, mark.fTop, mark.fRight, mark.fBottom, paint);
		}
	}


	// --- OptionRow (dropdown popup rows) ------------------------------------

	void Settings2::OptionRow::computeWidth(int index) {
		const auto& s = _values.at(index);
		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2 * .9f);
		SkRect bounds{};
		font.measureText(s.c_str(), s.size(), SkTextEncoding::kUTF8, &bounds);
		width = bounds.width();
	}

	void Settings2::OptionRow::render(SkCanvas* c, int index) {
		const auto& s = _values.at(index);
		flush(c);
		TextViewBase::render(c, index);
		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2 * .9f);
		float x, y;
		centerText(font, this, s.c_str(), x, y);
		auto* own = owner.load();
		const bool isactive = own != nullptr && own->rvTarget != nullptr &&
			index == own->rvTarget->optionIndex();
		SkPaint paint;
		paint.setColor(isactive ? skcol::active : skcol::fg);
		c->drawString(s.c_str(), _STATE->textsize2 * .5f, y, font, paint);
	}

	int32_t Settings2::OptionRow::cb(const InputEvent& e, int index) {
		if (e.action == ACTION_UP) {
			auto idx = indexhot.load();
			if (idx >= 0 && idx < (int)_values.size()) {
				if (auto* own = owner.load())
					own->onOptionPicked(idx);
			}
			else
				return 0;
		}
		return TextViewBase::cb(e, index);
	}

	// --- Settings2 -----------------------------------------------------------

	Settings2::Settings2(AppState* ss, const std::string& appName,
		std::function<void(const std::string& key, int value)> intChangeCallback)
		: View(ss, WRAP, 0, CENTER_ALIGN, 0), ScrollViewBase(ss),
		mgr(tsl::app::getStoragePath(appName)) {
		if (intChangeCallback)
			mgr.SetIntChangeCallback(std::move(intChangeCallback));
		prio = 0;
		name = "Settings2";
		init();
	}

	Pref2* Settings2::getByKey(const std::string& key) {
		for (auto it = categories.rbegin(); it != categories.rend(); ++it)
			for (auto item = it->items.rbegin(); item != it->items.rend(); ++item)
				if (item->key == key)
					return &*item;
		return nullptr;
	}

	void Settings2::loadFromXml(const std::string& xmlData) {
		parseXml(xmlData);
		layoutContent();
	}

	void Settings2::show(OnHideCallback cb) {
		onHideCallback = std::move(cb);
		init();
		updateItemValuesFromManager();
		addDraw();
		addCB();
	}

	void Settings2::showBlocking() {
		std::atomic<bool> completed{ false };
		show([&]() {
			completed.store(true, std::memory_order_release);
			completed.notify_one();
			});
		while (!completed.load(std::memory_order_acquire))
			completed.wait(false);
	}

	void Settings2::hide() {
		closeOptions();
		delCB();
		deldraw();
		offset = 0;
		if (onHideCallback) {
			onHideCallback();
			onHideCallback = nullptr;
		}
	}

	void Settings2::init() {
		width = _STATE->windowWidth;
		height = _STATE->windowHeight;
		const float ox = _STATE->graphics.xOffset;
		const float oy = _STATE->graphics.yOffset;
		xStart = _STATE->safeInsets.left - ox;
		xEnd = width + ox - _STATE->safeInsets.right;
		yStart = _STATE->safeInsets.top - oy;
		yEnd = height + oy - _STATE->safeInsets.bottom;
		pad = _STATE->textsize2;
		barH = _STATE->textsize1;
		contentTop = yStart + barH + lw2;
		startx = (int)xStart;
		starty = (int)yStart;
		stopx = (int)xEnd;
		stopy = (int)yEnd;
		layoutContent();
	}

	void Settings2::layoutContent() {
		const float ts2 = _STATE->textsize2;
		SkFont catFont(_STATE->font_normal);
		catFont.setSize(ts2 * .8f);
		catFont.setEmbolden(true);
		SkFont itemFont(_STATE->font_normal);
		itemFont.setSize(ts2 * .9f);
		SkFont descFont(_STATE->font_normal);
		descFont.setSize(ts2 * .7f);

		SkFontMetrics fm{};
		itemFont.getMetrics(&fm);

		const float catSp = catFont.getSpacing();
		const float itemSp = itemFont.getSpacing();
		const float descSp = descFont.getSpacing();
		const float gap = ts2 * .6f;
		const float descGap = ts2 * .2f;
		const float textX = xStart + pad;
		const float rowRight = xEnd - pad;

		float cursor = ts2 * .6f;

		for (size_t ci = 0; ci < categories.size(); ++ci) {
			auto& cat = categories[ci];
			cat.titleBaseline = cursor + catSp;
			cursor += catSp;
			cat.descLines = wrapLines(cat.description, descFont, rowRight - textX);
			if (!cat.descLines.empty()) {
				cat.descBaseline0 = cursor + descGap + descSp;
				cursor += descGap + cat.descLines.size() * descSp;
			}
			cursor += ts2 * .9f;

			for (auto& p : cat.items) {
				// control geometry first: its left edge caps the text width
				float ctrlW = 0, ctrlH = 0;
				switch (p.kind) {
				case Pref2::Kind::Bool:
					ctrlW = ctrlH = ts2 * 1.125f;
					break;
				case Pref2::Kind::Dropdown: {
					// value text + the DropDownView-style triangle, right-aligned;
					// sized for the longest option so descriptions never run under it
					ctrlH = ts2 * 1.5f;
					float maxw = 0;
					SkRect b{};
					for (auto& o : p.displayOptions) {
						itemFont.measureText(o.c_str(), o.size(), SkTextEncoding::kUTF8, &b);
						maxw = std::max(maxw, b.width());
					}
					ctrlW = std::min(maxw + ts2 * .8f, (xEnd - xStart) * .45f);
					ctrlW = std::max(ctrlW, ts2 * 1.8f);
					break;
				}
				case Pref2::Kind::Slider:
					ctrlW = (xEnd - xStart) * .25f;
					ctrlH = ts2 * 1.2f;
					break;
				default:
					break;
				}

				const bool fullRow = p.kind == Pref2::Kind::Action || p.kind == Pref2::Kind::Url;
				const float descW = fullRow ? rowRight - textX
					: rowRight - ctrlW - ts2 * .5f - textX;

				p.rowTop = cursor;
				p.descLines = wrapLines(p.description, descFont, descW);
				float textH = itemSp;
				if (!p.descLines.empty())
					textH += descGap + p.descLines.size() * descSp + ts2 * .3f;
				p.rowH = std::max(textH, 2 * ts2);

				if (p.descLines.empty())
					p.nameBaseline = p.rowTop + p.rowH * .5f - (fm.fAscent + fm.fDescent) * .5f;
				else
					p.nameBaseline = p.rowTop + itemSp * .85f;
				p.descBaseline0 = p.nameBaseline + descGap + descSp;

				if (ctrlW > 0) {
					// centred on the name line, like the old view
					const float cy = p.descLines.empty()
						? p.rowTop + p.rowH * .5f
						: p.nameBaseline + (fm.fAscent + fm.fDescent) * .5f;
					p.ctrl = SkRect::MakeXYWH(rowRight - ctrlW, cy - ctrlH * .5f, ctrlW, ctrlH);
				}
				else
					p.ctrl = SkRect::MakeEmpty();

				cursor += p.rowH + gap;
			}

			if (ci + 1 < categories.size()) {
				cursor += gap * .5f;
				cat.sepY = cursor;
				cursor += gap * 1.2f;
			}
			else
				cat.sepY = -1;
		}

		contentHeightTotal = cursor + ts2;

		const float visibleH = yEnd - contentTop;
		maxoffset = contentHeightTotal > visibleH ? -(int)(contentHeightTotal - visibleH) : 0;
		if (offset < maxoffset)
			offset = maxoffset;
		if (offset > 0)
			offset = 0;

		// overscroll glow + wheel step geometry (ScrollViewBase)
		boxoffsetx = (int)xStart;
		boxoffsety = (int)contentTop;
		boxwidth = (int)(xEnd - xStart);
		boxheight = (int)visibleH;
		glowStartX = xStart;
		glowWidth = xEnd - xStart;
		itemheight = _STATE->textsize1;
	}

	void Settings2::updateItemValuesFromManager() {
		for (auto& cat : categories)
			for (auto& p : cat.items) {
				switch (p.kind) {
				case Pref2::Kind::Bool:
					p.value = mgr.Get(p.key, p.value == "true") ? "true" : "false";
					break;
				case Pref2::Kind::Dropdown:
					p.value = mgr.Get(p.key, p.value);
					break;
				case Pref2::Kind::Slider:
					p.sliderVal = mgr.Get(p.key, (int)std::lround(p.sliderVal * 100.f)) / 100.f;
					break;
				default:
					break;
				}
			}
	}

	void Settings2::parseXml(const std::string& xml) {
		categories.clear();
		size_t pos = 0;
		while ((pos = xml.find("<PrefCategory", pos)) != std::string::npos) {
			size_t endTagPos = xml.find('>', pos);
			if (endTagPos == std::string::npos) break;
			std::string catTag = xml.substr(pos, endTagPos - pos + 1);
			Category cat;
			cat.name = attrValue(catTag, "name");
			cat.description = attrValue(catTag, "description");
			pos = endTagPos + 1;

			size_t catEnd = xml.find("</PrefCategory>", pos);
			if (catEnd == std::string::npos) break;

			size_t prefPos = pos;
			while ((prefPos = xml.find("<Pref", prefPos)) != std::string::npos && prefPos < catEnd) {
				size_t prefEnd = xml.find("/>", prefPos);
				if (prefEnd == std::string::npos || prefEnd > catEnd) break;

				std::string prefTag = xml.substr(prefPos, prefEnd - prefPos + 2);
				std::string name = attrValue(prefTag, "name");
				std::string key = attrValue(prefTag, "key");
				std::string description = attrValue(prefTag, "description");
				std::string type = attrValue(prefTag, "type");
				std::string value = attrValue(prefTag, "value");
				std::string url = attrValue(prefTag, "url");
				std::string displayOptionsStr = attrValue(prefTag, "displayOptions");
				std::string optionValuesStr = attrValue(prefTag, "optionValues");

				if (type == "bool" && !name.empty() && !key.empty()) {
					auto& p = cat.items.emplace_back(Pref2::Kind::Bool, name, key, description);
					p.value = value.empty() ? "false" : value;
				}
				else if (type == "slider" && !name.empty() && !key.empty()) {
					auto& p = cat.items.emplace_back(Pref2::Kind::Slider, name, key, description);
					p.value = value.empty() ? "0.5" : value;
					std::istringstream iss(p.value);
					iss >> p.sliderVal;
					p.sliderVal = std::clamp(p.sliderVal, 0.f, 1.f);
				}
				else if (type == "dropdown" && !name.empty() && !key.empty() &&
					!displayOptionsStr.empty() && !optionValuesStr.empty()) {
					auto& p = cat.items.emplace_back(Pref2::Kind::Dropdown, name, key, description);
					p.value = value.empty() ? "0" : value;
					p.displayOptions = splitTrim(displayOptionsStr, ',');
					p.optionValues = splitTrim(optionValuesStr, ',');
					if (p.displayOptions.empty() ||
						p.displayOptions.size() != p.optionValues.size()) {
						fprintf(stderr,
							"Warning: Mismatch or missing displayOptions/optionValues for dropdown '%s'.\n",
							name.c_str());
						p.displayOptions.clear();
						p.optionValues.clear();
					}
				}
				else if (type == "url" && !name.empty() && !url.empty()) {
					auto& p = cat.items.emplace_back(Pref2::Kind::Url, name, "", description);
					p.url = url;
				}
				else if (type == "action" && !name.empty()) {
					cat.items.emplace_back(Pref2::Kind::Action, name, key, description);
				}
				prefPos = prefEnd + 2;
			}
			categories.push_back(std::move(cat));
			pos = catEnd + 15;
		}
	}

	void Settings2::applyOption(Pref2& p, int index) {
		if (index < 0 || index >= (int)p.optionValues.size())
			return;
		if (p.value == p.optionValues[index])
			return;
		p.value = p.optionValues[index];
		int iv = 0;
		std::istringstream iss(p.value);
		iss >> iv;
		if (!iss.fail() && iss.eof())
			mgr.Set(p.key, iv);
		else
			mgr.Set(p.key, p.value);
	}

	void Settings2::openOptions(Pref2& p) {
		if (p.displayOptions.empty())
			return;
		if (!optionsRv)
			optionsRv = std::make_shared<RecyclerView<OptionRow, std::string>>(_appState);
		OptionRow::owner.store(this);
		rvTarget = &p;
		optionsRv->setTitle(p.name);
		optionsRv->setValues(p.displayOptions);
		optionsRv->computeSize();
		optionsRv->addDraw();
		optionsRv->addCB();
	}

	void Settings2::closeOptions() {
		if (optionsRv) {
			optionsRv->deldraw();
			optionsRv->delCB();
		}
		rvTarget = nullptr;
	}

	void Settings2::onOptionPicked(int index) {
		if (rvTarget != nullptr)
			applyOption(*rvTarget, index);
		rvTarget = nullptr;
		redraw();
	}

	Pref2* Settings2::prefAt(float x, float cy) {
		if (x < xStart + pad * .5f || x > xEnd - pad * .5f)
			return nullptr;
		for (auto& cat : categories)
			for (auto& p : cat.items)
				if (cy >= p.rowTop && cy < p.rowTop + p.rowH)
					return &p;
		return nullptr;
	}

	// --- drawing -------------------------------------------------------------

	void Settings2::drawBar(SkCanvas* canvas) {
		SkPaint paint;
		paint.setAntiAlias(true);
		paint.setColor(skcol::fg);

		SkFont font(_STATE->font_normal);
		font.setSize(_STATE->textsize2 * .9f);
		font.setEmbolden(true);
		SkRect bounds{};
		font.measureText(title.c_str(), title.size(), SkTextEncoding::kUTF8, &bounds);
		canvas->drawSimpleText(title.c_str(), title.size(), SkTextEncoding::kUTF8,
			(xStart + xEnd) * .5f - bounds.centerX(),
			yStart + barH * .5f - bounds.centerY(), font, paint);

		// keep colours and caps in sync with the dismiss button in
		// FloatingView::render
		SkRect btn = SkRect::MakeXYWH(xEnd - barH, yStart, barH, barH);
		if (closeHot) {
			paint.setStyle(SkPaint::kFill_Style);
			paint.setColor(skcol::blue_transparent);
			canvas->drawRect(btn, paint);
		}
		float inset = barH * .30f;
		SkRect xArea = btn;
		xArea.inset(inset, inset);
		paint.setColor(skcol::fg);
		paint.setStyle(SkPaint::kStroke_Style);
		paint.setStrokeWidth(lw);
		paint.setStrokeCap(SkPaint::kRound_Cap);
		canvas->drawLine(xArea.fLeft, xArea.fTop, xArea.fRight, xArea.fBottom, paint);
		canvas->drawLine(xArea.fRight, xArea.fTop, xArea.fLeft, xArea.fBottom, paint);

		paint.setStrokeWidth(1.0f);
		// Snapped to a half pixel: a 1px antialiased line centred on a pixel
		// boundary splits across two rows at half coverage and reads as a paler
		// grey than everything else drawn in fg. contentTop - lw2 * .5f lands on
		// a .5 only when lw2 is 1 (desktop) AND yStart is whole — on mobile lw2
		// is 1.5, so the bar divider was always soft there.
		const float divY = std::floor(contentTop - lw2 * .5f) + .5f;
		canvas->drawLine(xStart, divY, xEnd, divY, paint);
	}

	void Settings2::drawPref(SkCanvas* canvas, Pref2& p, float yoff,
		SkFont& itemFont, SkFont& descFont) {
		const float ts2 = _STATE->textsize2;
		const float textX = xStart + pad;
		SkPaint paint;
		paint.setAntiAlias(true);

		const bool isHot = hotPref == &p;

		// hot hugs the actionable thing, never the whole row: for action/url
		// rows that is the name itself
		if (isHot && (p.kind == Pref2::Kind::Action || p.kind == Pref2::Kind::Url)) {
			SkRect nb{};
			itemFont.measureText(p.name.c_str(), p.name.size(), SkTextEncoding::kUTF8, &nb);
			SkFontMetrics fm{};
			itemFont.getMetrics(&fm);
			const float boxH = ts2 * 1.5f;
			const float cy = p.nameBaseline + yoff + (fm.fAscent + fm.fDescent) * .5f;
			paint.setColor(skcol::bghot);
			canvas->drawRoundRect(SkRect::MakeLTRB(textX - ts2 * .3f, cy - boxH * .5f,
				textX + nb.width() + ts2 * .3f, cy + boxH * .5f),
				boxH * .1f, boxH * .1f, paint);
		}

		paint.setColor(skcol::fg);
		canvas->drawSimpleText(p.name.c_str(), p.name.size(), SkTextEncoding::kUTF8,
			textX, p.nameBaseline + yoff, itemFont, paint);

		if (!p.descLines.empty()) {
			paint.setColor(skcol::lighter_grey);
			const float descSp = descFont.getSpacing();
			for (size_t i = 0; i < p.descLines.size(); ++i)
				canvas->drawSimpleText(p.descLines[i].c_str(), p.descLines[i].size(),
					SkTextEncoding::kUTF8, textX,
					p.descBaseline0 + i * descSp + yoff, descFont, paint);
		}

		switch (p.kind) {
		case Pref2::Kind::Bool: {
			SkRect box = p.ctrl.makeOffset(0, yoff);
			drawCheckBox(canvas, box, p.value == "true", isHot, lw);
			break;
		}
		case Pref2::Kind::Dropdown: {
			// current value + a DropDownView-style triangle, right-aligned; a
			// settings choice opens the option list, it is not an effect-param
			// switcher, so there are no cycle arrows
			SkRect ctrl = p.ctrl.makeOffset(0, yoff);
			const float triW = ts2 * .45f;
			const float triH = ts2 * .28f;
			const float triGap = ts2 * .35f;

			int idx = p.optionIndex();
			const std::string& shown = idx >= 0 ? p.displayOptions[idx] : p.value;
			const float maxw = ctrl.width() - triW - triGap;
			std::string text = View::truncateText(itemFont, shown, maxw);
			SkRect b{};
			itemFont.measureText(text.c_str(), text.size(), SkTextEncoding::kUTF8, &b);

			const float triRight = ctrl.fRight;
			const float triLeft = triRight - triW;
			const float textLeft = triLeft - triGap - b.width();

			if (isHot) {
				paint.setColor(skcol::bghot);
				canvas->drawRoundRect(SkRect::MakeLTRB(textLeft - ts2 * .3f, ctrl.fTop,
					triRight + ts2 * .2f, ctrl.fBottom),
					ctrl.height() * .1f, ctrl.height() * .1f, paint);
			}

			paint.setColor(skcol::fg);
			canvas->drawSimpleText(text.c_str(), text.size(), SkTextEncoding::kUTF8,
				textLeft, ctrl.centerY() - b.centerY(), itemFont, paint);

			SkPath tri;
			const float tcy = ctrl.centerY();
			tri.moveTo(triLeft, tcy - triH * .5f);
			tri.lineTo(triRight, tcy - triH * .5f);
			tri.lineTo((triLeft + triRight) * .5f, tcy + triH * .5f);
			tri.close();
			paint.setStyle(SkPaint::kFill_Style);
			canvas->drawPath(tri, paint);
			break;
		}
		case Pref2::Kind::Slider: {
			SkRect ctrl = p.ctrl.makeOffset(0, yoff);
			paint.setColor(skcol::blue_transparent);
			canvas->drawRect(SkRect::MakeLTRB(ctrl.fLeft,
				ctrl.fTop, ctrl.fLeft + ctrl.width() * p.sliderVal, ctrl.fBottom), paint);
			paint.setColor(skcol::fg);
			paint.setStyle(SkPaint::kStroke_Style);
			paint.setStrokeWidth(lw2);
			canvas->drawRect(ctrl, paint);
			break;
		}
		default:
			break;
		}
	}

	void Settings2::render(void*) {
		if (_STATE->windowHeight != height || _STATE->windowWidth != width)
			init();

		const float ox = _STATE->graphics.xOffset;
		const float oy = _STATE->graphics.yOffset;
		const int fullW = width + (int)(2.0f * ox);
		const int fullH = height + (int)(2.0f * oy);

		SkCanvas* canvas = _STATE->graphics.getCanvas(windex, -ox, -oy, fullW, fullH);
		if (canvas == nullptr)
			return;
		canvas->save();
		canvas->clear(skcol::bg);
		canvas->translate(ox, oy);

#ifdef PLATFORM_MOBILE
		auto& s = scroller[currentScroller.load()];
		const auto elapsed = !s.isFinished() ? timeLastAction.elapsedReplace()
			: timeLastAction.elapsed();
		if (s.computeScrollOffset())
			offset = s.getCurrY();
#else
		const auto elapsed = timeLastAction.elapsed();
#endif

		drawBar(canvas);

		canvas->save();
		canvas->clipRect(SkRect::MakeLTRB(xStart, contentTop, xEnd, yEnd));
		const float yoff = contentTop + offset;

		SkFont catFont(_STATE->font_normal);
		catFont.setSize(_STATE->textsize2 * .8f);
		catFont.setEmbolden(true);
		SkFont itemFont(_STATE->font_normal);
		itemFont.setSize(_STATE->textsize2 * .9f);
		SkFont descFont(_STATE->font_normal);
		descFont.setSize(_STATE->textsize2 * .7f);

		SkPaint paint;
		paint.setAntiAlias(true);

		for (auto& cat : categories) {
			paint.setColor(skcol::fg);
			canvas->drawSimpleText(cat.name.c_str(), cat.name.size(), SkTextEncoding::kUTF8,
				xStart + pad, cat.titleBaseline + yoff, catFont, paint);
			if (!cat.descLines.empty()) {
				paint.setColor(skcol::lighter_grey);
				const float descSp = descFont.getSpacing();
				for (size_t i = 0; i < cat.descLines.size(); ++i)
					canvas->drawSimpleText(cat.descLines[i].c_str(), cat.descLines[i].size(),
						SkTextEncoding::kUTF8, xStart + pad,
						cat.descBaseline0 + i * descSp + yoff, descFont, paint);
			}
			for (auto& p : cat.items)
				drawPref(canvas, p, yoff, itemFont, descFont);
			if (cat.sepY >= 0) {
				paint.setColor(skcol::frame);
				paint.setStrokeWidth(1.0f);
				canvas->drawLine(xStart, cat.sepY + yoff, xEnd, cat.sepY + yoff, paint);
			}
		}

		const float maxoff = std::abs((float)maxoffset);
		if (maxoff)
			updateOverscroll(canvas);

		if (maxoff > 0 && elapsed < 2.0) {
			float alpha = elapsed > 1.0 ? 255.f - 255.f * (elapsed - 1.) : 255.f;
			paint.setColor(SkColorSetA(skcol::fg, (int)alpha));
			const float visibleH = yEnd - contentTop;
			const float length = visibleH / (1.f + maxoff / visibleH);
			const float pos = (1.f - DISTANCEF((float)maxoffset, offset.load()) / maxoff) *
				(visibleH - length);
			canvas->drawRect(SkRect::MakeXYWH(xEnd - 2 * lw, contentTop + pos, 2 * lw, length),
				paint);
		}
		if (elapsed > 2.0 && mode != MOVING)
			perm = false;
		else
			perm = true;

		canvas->restore();
		canvas->restore();
	}

	// --- input ---------------------------------------------------------------

	void Settings2::callback(const InputEvent& e) {
#ifdef PLATFORM_MOBILE
		velocityTracker.addMovement(e);
#endif
		switch (e.action) {
		case ACTION_DOWN: {
			pointerid = e.pointer_id;
			lastXpos = e.x;
			lastYpos = e.y;
			totalmoved = 0;
			overscrollTop = 0.0f;
			overscrollBottom = 0.0f;
#ifdef PLATFORM_MOBILE
			scroller[currentScroller.load()].forceFinished(true);
#endif
			timeLastAction.reset();
			closeHot = false;
			hotPref = nullptr;
			sliding = false;

			if (e.y < contentTop) {
				mode = OUTSIDE;
				if (e.x >= xEnd - barH) {
					closeHot = true;
					redraw();
				}
				break;
			}

			mode = INSIDE;
			const float cy = e.y - contentTop - offset;
			hotPref = prefAt(e.x, cy);
			if (hotPref != nullptr) {
				auto& p = *hotPref;
				if (p.kind == Pref2::Kind::Slider) {
					if (p.ctrl.contains(e.x, cy)) {
						sliding = true;
						p.sliderVal = std::clamp((e.x - p.ctrl.fLeft) / p.ctrl.width(), 0.f, 1.f);
					}
					else
						hotPref = nullptr;
				}
				redraw();
			}
			break;
		}

		case ACTION_MOVE: {
			if (pointerid != e.pointer_id)
				break;
			if (sliding && hotPref != nullptr) {
				hotPref->sliderVal =
					std::clamp((e.x - hotPref->ctrl.fLeft) / hotPref->ctrl.width(), 0.f, 1.f);
				timeLastAction.reset();
				redraw();
				break;
			}
			if (mode == MOVING) {
				float diffy = lastYpos - e.y;
				float off = offset;
				off -= diffy;
				if (off < maxoffset) {
					float overscroll = maxoffset - off;
					overscrollBottom = std::max(overscrollBottom.load(),
						std::min(overscroll * 0.5f, maxOverscroll));
					off = maxoffset;
				}
				else if (off > 0) {
					overscrollTop = std::max(overscrollTop.load(),
						std::min(off * 0.5f, maxOverscroll));
					off = 0;
				}
				offset = off;
				lastXpos = e.x;
				lastYpos = e.y;
				timeLastAction.reset();
				redraw();
			}
			else if (mode == INSIDE || mode == OUTSIDE) {
				totalmoved = spacing(e.x, lastXpos, e.y, lastYpos);
				if (totalmoved > _STATE->touchSlop()) {
					if (hotPref != nullptr || closeHot) {
						hotPref = nullptr;
						closeHot = false;
						redraw();
					}
					if (mode == INSIDE && maxoffset < 0) {
						mode = MOVING;
						lastXpos = e.x;
						lastYpos = e.y;
					}
					else {
						mode = UNTOUCHED;
						pointerid = -1;
					}
					timeLastAction.reset();
				}
			}
			break;
		}

		case ACTION_UP: {
			if (pointerid != e.pointer_id)
				break;
			if (closeHot) {
				closeHot = false;
				pointerid = -1;
				mode = UNTOUCHED;
				if (e.x >= xEnd - barH && e.y < contentTop) {
					hide();
					return;
				}
				redraw();
				break;
			}
			if (sliding && hotPref != nullptr) {
				hotPref->sliderVal =
					std::clamp((e.x - hotPref->ctrl.fLeft) / hotPref->ctrl.width(), 0.f, 1.f);
				mgr.Set(hotPref->key, (int)std::lround(hotPref->sliderVal * 100.f));
			}
			else if (mode == INSIDE && hotPref != nullptr) {
				auto& p = *hotPref;
				switch (p.kind) {
				case Pref2::Kind::Bool: {
					const bool current = p.value == "true";
					p.value = current ? "false" : "true";
					mgr.Set(p.key, !current);
					break;
				}
				case Pref2::Kind::Dropdown:
					openOptions(p);
					break;
				case Pref2::Kind::Action:
					if (p.specialAction)
						p.specialAction();
					break;
				case Pref2::Kind::Url:
					if (!p.url.empty() && _STATE->openURL) {
						std::string url = p.url;
						_STATE->openURL(url);
					}
					break;
				default:
					break;
				}
			}
			else if (mode == MOVING && maxoffset < 0) {
				float diffy = lastYpos - e.y;
				float off = offset.load() - diffy;
				if (off < maxoffset) {
					overscrollBottom = std::min((maxoffset - off) * 0.3f, maxOverscroll);
					off = maxoffset;
				}
				else if (off > 0) {
					overscrollTop = std::min(off * 0.3f, maxOverscroll);
					off = 0;
				}
				offset.store(off);
#ifdef PLATFORM_MOBILE
				float vx, vy;
				if (velocityTracker.getVelocity(pointerid, &vx, &vy) &&
					(_STATE->mMinimumFlingVelocity <= 0 ||
						std::abs(vy) >= _STATE->mMinimumFlingVelocity)) {
					if (_STATE->mMaximumFlingVelocity > 0 &&
						std::abs(vy) > _STATE->mMaximumFlingVelocity)
						vy = ISNEG(vy) ? -_STATE->mMaximumFlingVelocity
						: _STATE->mMaximumFlingVelocity;
					int cur = currentScroller.load();
					if (++cur >= numScrollers)
						cur = 0;
					scroller[cur].fling(0, off, 0, vy, 0, 0, maxoffset, 0);
					currentScroller.store(cur);
				}
#endif
			}
			hotPref = nullptr;
			sliding = false;
			mode = UNTOUCHED;
			pointerid = -1;
			timeLastAction.reset();
			redraw();
			break;
		}

		case ACTION_MOUSE_WHEEL: {
			if (maxoffset < 0 && mode == UNTOUCHED) {
				float off = offset;
				off += e.pointer_id * itemheight;
				if (off < maxoffset) {
					overscrollBottom = std::min((maxoffset - off) * 0.3f, maxOverscroll);
					off = maxoffset;
				}
				else if (off > 0) {
					overscrollTop = std::min(off * 0.3f, maxOverscroll);
					off = 0;
				}
				offset = off;
				timeLastAction.reset();
				redraw();
			}
			break;
		}

		case ACTION_KEY_UP:
			if (e.pointer_id == VKEY_ESCAPE)
				hide();
			break;

		default:
			break;
		}
	}

	// --- queue plumbing --------------------------------------------------------

	void Settings2::addRecursiveCB() {
		hasFocus.store(true);
		View::addRecursiveCB();
	}

	void Settings2::delRecursiveCB() {
		hasFocus.store(false);
		pointerid = -1;
		mode = UNTOUCHED;
		totalmoved = 0;
		hotPref = nullptr;
		closeHot = false;
		sliding = false;
		overscrollTop = 0.0f;
		overscrollBottom = 0.0f;
#ifdef PLATFORM_MOBILE
		velocityTracker.clear();
#endif
		View::delRecursiveCB();
	}

	void Settings2::delRecursiveDraw() {
		_STATE->graphics.deleteWindow(windex);
		windex = -1;
		View::delRecursiveDraw();
	}

} // namespace tsl::graphics
