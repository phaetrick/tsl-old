#include "keyboard.h"
#include "app.h"
#include <IconsMaterialDesignReduced.h>
#include <SkFontMetrics.h>

using namespace tsl::graphics;

// =============================================================
// ===============  TextInput (caret-enabled)  =================
// =============================================================

bool TextInput::isValidNumber(const std::string& s) {
	if (s.empty()) return false;
	bool seenDigit = false, seenDot = false;
	size_t i = 0;
	if (s[i] == '-') { i++; if (i == s.size()) return false; }
	for (; i < s.size(); ++i) {
		char c = s[i];
		if (std::isdigit((unsigned char)c)) seenDigit = true;
		else if (c == '.') { if (seenDot) return false; seenDot = true; }
		else return false;
	}
	if (!seenDigit) return false;
	char* end = nullptr;
	double val = std::strtod(s.c_str(), &end);
	if (*end != '\0') return false;
	return std::isfinite(val);
}

// Sentinel vkey for the compact keyboard's dedicated ':' key. Not a real VK —
// hardware never sends it — so the OEM_1 (;/:) translation stays untouched for
// physical keyboards while the on-screen key types ':' directly, shift or not.
// The compact layout has no ';' anymore; it earned its slot for nothing.
// 0x100 is past the whole VK code space (0xFE is already VKEY_OEM_CLEAR).
static constexpr int VKEY_COLONKEY = 0x100;

// Extended VK → char mapper (US layout) for digits & OEM
// In keyboard.cpp - Updated VKtoCharExtended_US function:
static char VKtoCharExtended_US(int vk, bool shift) {
	switch (vk) {
	case VKEY_COLONKEY: return ':';
	case VKEY_1: return shift ? '!' : '1';
	case VKEY_2: return shift ? '@' : '2';  // Fixed: @ symbol
	case VKEY_3: return shift ? '#' : '3';
	case VKEY_4: return shift ? '$' : '4';
	case VKEY_5: return shift ? '%' : '5';
	case VKEY_6: return shift ? '^' : '6';
	case VKEY_7: return shift ? '&' : '7';
	case VKEY_8: return shift ? '*' : '8';
	case VKEY_9: return shift ? '(' : '9';
	case VKEY_0: return shift ? ')' : '0';
	case VKEY_OEM_MINUS:   return shift ? '_' : '-';
	case VKEY_OEM_PLUS:    return shift ? '+' : '=';
	case VKEY_OEM_1:       return shift ? ':' : ';';
	case VKEY_OEM_2:       return shift ? '?' : '/';
	case VKEY_OEM_3:       return shift ? '~' : '`';
	case VKEY_OEM_4:       return shift ? '{' : '[';
	case VKEY_OEM_5:       return shift ? '|' : '\\';
	case VKEY_OEM_6:       return shift ? '}' : ']';
	case VKEY_OEM_7:       return shift ? '"' : '\'';
	case VKEY_OEM_COMMA:   return shift ? '<' : ',';
	case VKEY_OEM_PERIOD:  return shift ? '>' : '.';
	case VKEY_SPACE:       return ' ';
	default:               return '\0';
	}
}

bool tsl::graphics::VKeyTypesCharacter(int vk) {
	if (vk == VKEY_BACK || vk == VKEY_DELETE) return true;
	if (VKtoCharExtended_US(vk, false) != '\0' || VKtoCharExtended_US(vk, true) != '\0') return true;
	return VKtoChar(vk, false) != '\0' || VKtoChar(vk, true) != '\0';
}


void TextInput::render(void* ctx) {
	auto c = (SkCanvas*)ctx;
	flush(c);

	SkPaint paint; paint.setAntiAlias(true);
	const float fs = _STATE->textsize2 * .9f;
	SkFont& font = _STATE->font_normal; font.setSize(fs);
	SkFontMetrics metrics{}; font.getMetrics(&metrics);

	const std::string s = text();
	const float	y1 = (height + fs) * .5f;
	const float padding = fs * .5; // Small left padding

	// Choose alignment method
	const float w1 = font.measureText(s.c_str(), (int)s.size(), SkTextEncoding::kUTF8);


	float x1 = 0.f;

	if (textAlign == ALIGN_LEFT) {
		if (w1 > width - 2 * padding)
			x1 = width - w1 - 2 * padding;
		// Left-aligned: text starts at left edge with small padding
		else x1 = padding;
	}
	else
	{
		if (w1 > width - padding)
			x1 = width - w1 - padding;
		else x1 = (width - w1) * 0.5f; // Center-aligned: text centered in the box	
	}

	if (highlight && !s.empty()) {
		SkPaint hl; hl.setAntiAlias(true); hl.setColor(skcol::fg);
		c->drawRect(SkRect::MakeXYWH(startx + x1,
			starty + (height - _STATE->textsize2) * .5f,
			w1, _STATE->textsize2 + metrics.fDescent), hl);
		paint.setColor(skcol::bg);
	}
	else {
		paint.setColor(skcol::text);
	}
	c->save();
	c->clipRect(SkRect::MakeXYWH(startx + padding, starty, width - 2 * padding + lw, height));
	if (!s.empty()) {
		c->drawSimpleText(s.c_str(), (int)s.size(), SkTextEncoding::kUTF8,
			startx + x1, starty + y1, font, paint);
	}

	if (hasFocus.load()) {
		size_t cur = cursor; if (cur > s.size()) cur = s.size();
		const float subW = font.measureText(s.substr(0, cur).c_str(), (int)cur, SkTextEncoding::kUTF8);

		auto elapsed = timer.elapsed();
		int alpha = (int)((.5f + sinf(TWOPI_F_P * elapsed) * .5f) * 255.f);

		SkPaint caret; caret.setColor(SkColorSetA(skcol::blue, alpha));
		c->drawRect(SkRect::MakeXYWH(
			startx + x1 + subW,
			starty + (height - _STATE->textsize2) * .5f,
			lw, _STATE->textsize2 + metrics.fDescent), caret);
	}
	c->restore();
}

float TextInput::getTextWidth() {
	const float fs = _STATE->textsize2 * .9f;
	SkFont font(_STATE->font_normal); font.setSize(fs);
	std::string s = text();
	return font.measureText(s.c_str(), (int)s.size(), SkTextEncoding::kUTF8);
}

float TextInput::getTextStartX() {
	const float fs = _STATE->textsize2 * .9f;
	if (textAlign == ALIGN_LEFT) {
		const float padding = fs * 0.1f;
		return startx + padding;
	}
	else {
		// For centered text, calculate where it actually starts
		SkFont font(_STATE->font_normal); font.setSize(fs);
		const std::string s = text();
		float x1 = 0.f, y1 = 0.f;
		measureTextFixed(width, height, font, (s.empty() ? " " : s.c_str()), &x1, &y1, fs);
		return startx + x1;
	}
}

// Updated TextInput::callback method for better caret positioning with left-aligned text:
void TextInput::callback(const InputEvent& e) {
	if (e.action == ACTION_KEY_UP) {
		const bool numeric = ((_type & tsl::graphics::KeyboardType::TYPE_CLASS_NUMBER) == tsl::graphics::KeyboardType::TYPE_CLASS_NUMBER);
		const bool allowSigned = ((_type & tsl::graphics::KeyboardType::TYPE_NUMBER_FLAG_SIGNED) != 0);
		const bool allowDecimal = ((_type & tsl::graphics::KeyboardType::TYPE_NUMBER_FLAG_DECIMAL) != 0);

		// Navigation keys (unchanged)
		if (e.pointer_id == VKEY_LEFT) { if (cursor > 0) cursor--; resetTimer(); setHighLight(false); return; }
		if (e.pointer_id == VKEY_RIGHT) { if (cursor < text().size()) cursor++; resetTimer(); setHighLight(false); return; }
		if (e.pointer_id == VKEY_HOME) { cursor = 0; resetTimer(); return; }
		if (e.pointer_id == VKEY_END) { cursor = text().size(); resetTimer(); return; }
		if (e.pointer_id == VKEY_RETURN || e.pointer_id == VKEY_ESCAPE) {
			resetTimer(); return;
		}

		std::lock_guard lk(text.mutex);
		auto clearIfHighlighted = [&]() {
			if (highlight) { highlight = false; text = ""; cursor = 0; }
			};

		// Deletion (unchanged)
		if (e.pointer_id == VKEY_BACK) { clearIfHighlighted(); if (cursor > 0) { text.del((int)cursor - 1); cursor--; } resetTimer(); return; }
		if (e.pointer_id == VKEY_DELETE) { clearIfHighlighted(); if (cursor < text().size()) { text.del((int)cursor); } resetTimer(); return; }

		// Numeric handling (unchanged)
		if (numeric) {
			if (e.pointer_id == VKEY_OEM_MINUS) {
				clearIfHighlighted();
				if (text.empty()) { text = "-"; cursor = 1; }
				else {
					if (text[0] == '-') { text.del(0); if (cursor > 0) --cursor; }
					else { text.insert(0, "-"); ++cursor; }
				}
				resetTimer(); return;
			}
			if (e.pointer_id == VKEY_OEM_PERIOD) {
				clearIfHighlighted();
				if (!text.contains(".")) {
					if (cursor == 0 || text.empty() || (text[0] == '-' && cursor == 1)) {
						text.insert((int)cursor, "0."); cursor += 2;
					}
					else {
						text.insert((int)cursor, "."); ++cursor;
					}
				}
				resetTimer(); return;
			}
		}

		// Generic character mapping - FIXED to use the updated function
		char ch = VKtoCharExtended_US(e.pointer_id, _STATE->shiftPressed);
		if (ch == '\0') ch = VKtoChar(e.pointer_id, _STATE->shiftPressed);

		if (numeric) {
			if (ch == '-') {
				clearIfHighlighted();
				if (allowSigned) {
					if (text.empty()) { text = "-"; cursor = 1; }
					else if (text[0] == '-') { text.del(0); if (cursor > 0) --cursor; }
					else { text.insert(0, "-"); ++cursor; }
				}
				resetTimer(); return;
			}
			if (ch == '.') {
				clearIfHighlighted();
				if (allowDecimal && !text.contains(".")) {
					if (cursor == 0 || text.empty() || (text[0] == '-' && cursor == 1)) {
						text.insert((int)cursor, "0."); cursor += 2;
					}
					else { text.insert((int)cursor, "."); ++cursor; }
				}
				resetTimer(); return;
			}
			if (ch != '\0' && std::isdigit((unsigned char)ch)) {
				clearIfHighlighted();
				if (text.size() >= (size_t)maxLen) { resetTimer(); return; }
				std::string tmp(1, ch);
				text.insert((int)cursor, tmp); ++cursor; resetTimer();
			}
			return;
		}

		// Text mode: accept any mapped character, up to the length cap
		if (ch != '\0') {
			clearIfHighlighted();
			if (text.size() >= (size_t)maxLen) { resetTimer(); return; }
			std::string tmp(1, ch);
			text.insert((int)cursor, tmp);
			++cursor;
			resetTimer();
		}
		return;
	}
	else if (e.action == ACTION_DOWN) {
		// Updated caret positioning for both alignment modes
		highlight = false;

		SkFont font(_STATE->font_normal);
		const float fs = _STATE->textsize2 * .9f;
		font.setSize(fs);
		std::string s = text();
		const float padding = fs * 0.5f;
		auto wt = getTextWidth();
		float textStartX;
		if (textAlign == ALIGN_LEFT) {
			textStartX = (wt > width - padding * 2) ? width - wt - padding * 2 : padding;
		}
		else {
			textStartX = (wt > width - padding) ? width - wt - padding : (width - wt) * .5f;
		}

		const float localX = e.x - startx;
		size_t n = s.size(), i = 0;
		for (; i <= n; ++i) {
			const float w = font.measureText(s.substr(0, i).c_str(), (int)i, SkTextEncoding::kUTF8);
			if (localX <= textStartX + w) break;
		}
		cursor = std::min(i, n);
		resetTimer();
	}
}

// =============================================================
// ====================  TextInputPopUp  =======================
// =============================================================
// Handle Enter/Esc uniformly. Returns true if it handled & closed.
void TextInputPopUp::wakeWaiter() {
	if (waitToken.ticket)
		_STATE->waitNotify.complete(waitToken);
	else
		_STATE->waitNotify.wake_thread(waitSlotId);
}

bool TextInputPopUp::handleConfirmCancelFromKeyboard(int vkey) {
	if (vkey == VKEY_RETURN) {
		if (onEnter() == 1) { wakeWaiter(); }
		return true;
	}
	if (vkey == VKEY_ESCAPE) {
		wakeWaiter();
		return true;
	}
	return false;
}
void TextInputPopUp::handleTapOrForwardToInput(const InputEvent& e) {
	if (e.x < x || e.x > x + w || e.y < y || e.y > y + h) { wakeWaiter(); return; }

	const int ix0 = x + textInput1.startx;
	const int iy0 = y + textInput1.starty;
	const int ix1 = ix0 + textInput1.width;
	const int iy1 = iy0 + textInput1.height;

	if (e.x >= ix0 && e.x <= ix1 && e.y >= iy0 && e.y <= iy1) {
		InputEvent eLocal = e;
		eLocal.x = e.x - x; // popup-local
		eLocal.y = e.y - y;
		textInput1.callback(eLocal);
	}
	else {
		textInput1.callback(e);
	}
}


void TextInputPopUp::addRecursiveDraw() {
	textInput1.setHighLight(true);
	View::addRecursiveDraw();
}

void TextInputPopUp::delRecursiveDraw() {
	_STATE->graphics.deleteWindow(windowindex);
	View::delRecursiveDraw();
}

void TextInputPopUp::init() {
	startx = 0; starty = 0;
	width = stopx = _STATE->windowWidth;
	height = stopy = _STATE->windowHeight;
	h = _STATE->textsize2 * (title.empty() ? 3.0 : 5.0);

	if (_STATE->windowWidth < _STATE->windowHeight) {
		x = _STATE->windowWidth / 8.0;
		if (alignY_ != -1)
			y = alignY_;
		else
			y = _STATE->windowHeight / 3.0 - _STATE->textsize2 * (title.empty() ? 1.5 : 2.5);
		textInput1.width = w = _STATE->windowWidth * .75f;
	}
	else {
		x = _STATE->windowWidth / 4.0;
		if (alignY_ != -1)
			y = alignY_;
		else
			y = _STATE->windowHeight / 6.0 - _STATE->textsize2 * (title.empty() ? 1.5 : 2.5);
		textInput1.width = w = _STATE->windowWidth * .5f;
	}
	textInput1.height = _STATE->textsize2 * 2.0f;

	if (title.empty()) {
		// Vertically center the input when there's no title
		textInput1.starty = (int)((h - textInput1.height) * 0.5f);
	}
	else {
		// Leave a title band, then place the input below it
		textInput1.starty = (int)(_STATE->textsize2 * 2.0f);
	}

	textInput1.stopy = textInput1.starty + textInput1.height; // <-- correct
	textInput1.startx = 0;
	textInput1.stopx = textInput1.startx + textInput1.width;
}

void TextInputPopUp::callback(const InputEvent& e) {
	if (e.action == ACTION_KEY_UP) {
		if (e.pointer_id == VKEY_RETURN && !isInside(e.x, e.y, x, y, w, h)) {
			if (onEnter() == 1) { wakeWaiter(); }
			return;
		}
		else if (e.pointer_id == VKEY_ESCAPE) {
			wakeWaiter();
		}
		else {
			textInput1.callback(e);
		}
	}
	else if (e.action == ACTION_DOWN) {
		if (e.x < x || e.x > x + w || e.y < y || e.y > y + h) {
			wakeWaiter();
		}
		else {
			textInput1.callback(e);
		}
	}
}

void TextInputPopUp::render(void*) {
	if (_STATE->windowWidth != width || _STATE->windowHeight != height) init();

	auto c = _STATE->graphics.getCanvas(windowindex, x, y, w, h);
	if (!c)
		return;

	// Match NumericalPopUp styling (was sk_colours::wbg there)
	c->clear(sk_colours::wbg);

	// Draw the input
	textInput1.render(c);

	// Title
	SkPaint paint; paint.setAntiAlias(true); paint.setColor(skcol::text);
	const float fs = _STATE->textsize2 * .9f;
	SkFont font(_STATE->font_normal); font.setSize(fs);

	if (!title.empty()) {
		float x1, y1;
		measureTextFixed(w, _STATE->textsize2 * 2.0f, font, title.c_str(), &x1, &y1, fs);
		c->drawSimpleText(title.c_str(), (int)title.size(), SkTextEncoding::kUTF8, x1, y1, font, paint);
	}

	// Border (match NumericalPopUp)
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setStrokeWidth(1.0f);
	paint.setColor(skcol::border);
	c->drawRect(SkRect::MakeXYWH(0.5f, 0.5f, (float)w - 1.0f, (float)h - 1.0f), paint);
}

// =============================================================
// =================  Numerical Keyboard/Popup  =================
// =============================================================

void NumericalKeyboard::init() {
	width = height = _STATE->textsize1 * 6;
	if (alignment == BOTTOM) {
		starty = _STATE->windowHeight - height - lw2;
		startx = _STATE->windowWidth * .5 - width * .5;
	}
	else if (alignment == RIGHT) {
		startx = _STATE->windowWidth - width - _STATE->textsize2;
		starty = (_STATE->windowHeight - height) * .5;
	}
	else if (alignment == LEFT) {
		startx = _STATE->textsize2;
		starty = (_STATE->windowHeight - height) * .5;
	}
	else { // TOP
		startx = _STATE->windowWidth * .5 - width * .5;
		starty = lw2;
	}
	stopx = startx + width;
	stopy = starty + height;
}

void NumericalKeyboard::render(void* /*_c*/) {
	auto c = _STATE->graphics.getCanvas(windowindex, startx, starty, width, height);
	if (!c)
		return;
	c->clear(skcol::wbg);

	SkFont font(_STATE->font_normal); font.setSize(height / 8);
	SkFont font2(_STATE->font_md_kb);    font2.setSize(height / 8);

	float x11, y11;
	centerText(font, width * .25f, height * .25f, "012456789B-.", x11, y11);

	SkPaint paint; paint.setAntiAlias(true);
	int row = 0, column = 0;
	auto act = activeel.load();

	for (int i = 0; i < 16; i++) {
		paint.setColor(act == i ? skcol::blue_transparent : skcol::text);
		char bla;

		if (i == 3) {
			float xx, yy; centerText(font2, width / 4, height / 4, ICON_MD_CHEVRON_LEFT, xx, yy);
			c->drawSimpleText(ICON_MD_CHEVRON_LEFT, (int)strlen(ICON_MD_CHEVRON_LEFT),
				SkTextEncoding::kUTF8, width * .75f + xx, yy, font2, paint);
		}
		else if (i == 11) {
			float xx, yy; if (act == 7 || act == 15) paint.setColor(skcol::blue_transparent);
			centerText(font2, width / 4, height / 4, ICON_MD_CHECK, xx, yy);
			c->drawSimpleText(ICON_MD_CHECK, (int)strlen(ICON_MD_CHECK),
				SkTextEncoding::kUTF8, width * .75f + xx, height * .5f + yy, font2, paint);
		}
		else if (i == 12 || i == 14) {
			bla = VKtoChar(vkeys[i], false);
			c->drawSimpleText(&bla, 1, SkTextEncoding::kUTF8,
				(i == 14 ? width * .5f + width * .11f : width * .1075f),
				height * .75f + (i == 14 ? y11 : height * .17f), font, paint);
		}
		else {
			bla = VKtoChar(vkeys[i], false);
			c->drawSimpleText(&bla, 1, SkTextEncoding::kUTF8,
				column * width * .25f + width * .09f,
				row * height * .25f + y11, font, paint);
		}

		if (++column == 4) { ++row; column = 0; }
	}

	borderWindow(c);
}

int NumericalKeyboard::cb(const InputEvent& e) {
	switch (e.action) {
	case ACTION_DOWN:
		if (isInside(e)) {
			xpos = e.x; ypos = e.y; pointerid = e.pointer_id;
			float ex = e.x - startx, ey = e.y - starty;
			int column = (int)(ex / (float)width * 4);
			int row = (int)(ey / (float)height * 4);
			auto el = 4 * row + column;
			activeel.store(el);
			redraw();
			return -2;
		}
		break;
	case ACTION_UP: {
		auto act = activeel.load();
		if (e.pointer_id == pointerid && act != -1) {
			pointerid = -1; activeel = -1;
			if (act > 15) act = 15; else if (act < 0) act = 0;
			redraw();
			return vkeys[act];
		}
	} break;
	case ACTION_MOVE:
		if (e.pointer_id == pointerid && spacing(xpos, e.x, ypos, e.y) > _STATE->textsize2) {
			pointerid = -1; activeel = -1; redraw(); return -2;
		}
		if (isInside(e)) return -2;
		break;
	default: break;
	}
	return -1;
}

void NumericalKeyboard::delRecursiveDraw() {
	View::delRecursiveDraw();
	_STATE->graphics.deleteWindow(windowindex);
}

void NumericalPopUp::init() {
	TextInputPopUp::init();
	keyboard.init();
}

void NumericalPopUp::callback(const InputEvent& e) {
	// 1) On-screen numeric keyboard first
	int ret = keyboard.cb(e);
	if (ret != -1) {
		if (ret == -2) {                // press/move consumed: forward a KEY_DOWN for visual typing feel
			auto e2 = e; e2.action = ACTION_KEY_DOWN; textInput1.callback(e2);
			return;
		}
		if (handleConfirmCancelFromKeyboard(ret)) return; // Enter/Esc handled & closed
		// otherwise, map vkey to TextInput
		auto e2 = e; e2.pointer_id = ret; e2.action = ACTION_KEY_UP; textInput1.callback(e2);
		return;
	}

	// 2) Hardware keys
	if (e.action == ACTION_KEY_UP) {
		if (handleConfirmCancelFromKeyboard(e.pointer_id)) return;
		textInput1.callback(e);
		return;
	}

	// 3) Tap to place caret or dismiss
	if (e.action == ACTION_DOWN) {
		handleTapOrForwardToInput(e);
		return;
	}
}

void NumericalPopUp::render(void*) {
	// Base draws popup (title + input + border) and handles resize-init
	TextInputPopUp::render(nullptr);

	// Keyboard draws in its own window
	keyboard.render(nullptr);
}


void NumericalPopUp::addRecursiveDraw() {
	textInput1.setHighLight(true);
	View::addRecursiveDraw();
}

void NumericalPopUp::delRecursiveDraw() {
	_STATE->graphics.deleteWindow(windowindex);
	keyboard.delRecursiveDraw();
	View::delRecursiveDraw();
}

// =============================================================
// ====================  AlphaKeyboard  ========================
// =============================================================


// Old version checked only 0xEF. PUA in U+E000..U+EFFF starts with 0xEE.
static inline bool isIconGlyphUtf8(const std::string& s) {
	if (s.empty()) return false;
	const unsigned char b = static_cast<unsigned char>(s[0]);
	return (b == 0xEE || b == 0xEF);
}

void AlphaKeyboard::init() {
	const float W = (float)_STATE->windowWidth;
	const float H = (float)_STATE->windowHeight;

	// Height still from fraction
	height = (int)std::clamp(H * kFracH, 160.0f, H);

	// === Width: prefer near-full width in portrait, leave a small margin ===
	if (W < H) { // portrait
		// Small visual margin; ties nicely to your typography size.
		const float sideMargin = std::max(8.0f, _STATE->textsize2 * 0.6f);
		width = (int)std::clamp(W - 2.0f * sideMargin, 240.0f, W);
	}
	else {     // landscape: keep your existing fraction behavior
		width = (int)std::clamp(W * kFracW, 240.0f, W);
	}

	// *** Always position relative to SCREEN here ***
	if (alignment == BOTTOM) {
		starty = (int)(H - height - lw2);
		startx = (int)((W - width) * 0.5f);
	}
	else if (alignment == RIGHT) {
		startx = (int)(W - width - _STATE->textsize2);
		starty = (int)((H - height) * 0.5f);
	}
	else if (alignment == LEFT) {
		startx = (int)(_STATE->textsize2);
		starty = (int)((H - height) * 0.5f);
	}
	else { // TOP
		startx = (int)((W - width) * 0.5f);
		starty = lw2;
	}

	stopx = startx + width;
	stopy = starty + height;

	// Compact layout when keys would be too narrow to tap comfortably.
	// On iOS: portrait = compact (phone), landscape = full (more room).
	// On desktop: use pixel-based heuristic (rarely triggers in practice).
#if defined(OS_IOS)
	compactMode = (W < H);
#else
	compactMode = ((float)width / 13.f < _STATE->textsize2 * 1.4f);
#endif

	layoutKeys();
}

void AlphaKeyboard::delRecursiveDraw() {
	View::delRecursiveDraw();
	_STATE->graphics.deleteWindow(windowindex);
}

// Sentinel vkey for the 123/ABC page-toggle key (not a real VK).
static constexpr int VKEY_PAGESWITCH = 0xFF;

void AlphaKeyboard::layoutKeys() {
	struct KDef { const char* txt; const char* txtShift; int vkey; bool special; float units; };

	std::vector<std::vector<KDef>> rows;

	if (!compactMode) {
		// ── Full layout (large screens / landscape) ──────────────────────────
		// Row 4: shift keys removed, just z-row + symbols
		rows = {
			{{"1","!",VKEY_1,false,1},{"2","@",VKEY_2,false,1},{"3","#",VKEY_3,false,1},{"4","$",VKEY_4,false,1},
			 {"5","%",VKEY_5,false,1},{"6","^",VKEY_6,false,1},{"7","&",VKEY_7,false,1},{"8","*",VKEY_8,false,1},
			 {"9","(",VKEY_9,false,1},{"0",")",VKEY_0,false,1},{"-","_",VKEY_OEM_MINUS,false,1},{"=","+",VKEY_OEM_PLUS,false,1},
			 {ICON_MD_KEYBOARD_BACKSPACE,"",VKEY_BACK,true,1.7f}},

			{{"q","Q",VKEY_Q,false,1},{"w","W",VKEY_W,false,1},{"e","E",VKEY_E,false,1},{"r","R",VKEY_R,false,1},
			 {"t","T",VKEY_T,false,1},{"y","Y",VKEY_Y,false,1},{"u","U",VKEY_U,false,1},{"i","I",VKEY_I,false,1},
			 {"o","O",VKEY_O,false,1},{"p","P",VKEY_P,false,1},{"[","{",VKEY_OEM_4,false,1},{"]","}",VKEY_OEM_6,false,1}},

			{{ICON_MD_KEYBOARD_CAPSLOCK,ICON_MD_KEYBOARD_CAPSLOCK,VKEY_CAPITAL,true,1.6f},
			 {"a","A",VKEY_A,false,1},{"s","S",VKEY_S,false,1},{"d","D",VKEY_D,false,1},{"f","F",VKEY_F,false,1},
			 {"g","G",VKEY_G,false,1},{"h","H",VKEY_H,false,1},{"j","J",VKEY_J,false,1},{"k","K",VKEY_K,false,1},
			 // ':' outright, no semicolon — nothing in the apps wants ';' and
			 // the AI prompt prefixes (sfx: / m:) want ':' one tap away.
			 // Hardware keyboards still type ';' through VKtoChar untouched.
			 {"l","L",VKEY_L,false,1},{":",":",VKEY_COLONKEY,false,1},{"'","\"",VKEY_OEM_7,false,1},
			 {ICON_MD_KEYBOARD_RETURN,"",VKEY_RETURN,true,1.9f}},

			{{"z","Z",VKEY_Z,false,1},{"x","X",VKEY_X,false,1},{"c","C",VKEY_C,false,1},{"v","V",VKEY_V,false,1},
			 {"b","B",VKEY_B,false,1},{"n","N",VKEY_N,false,1},{"m","M",VKEY_M,false,1},{",","<",VKEY_OEM_COMMA,false,1},
			 {".",">",VKEY_OEM_PERIOD,false,1},{"/","?",VKEY_OEM_2,false,1}},

			{{"","",VKEY_SPACE,true,6.0f}}
		};
	} else if (!numPage) {
		// ── Compact letters page ──────────────────────────────────────────────
		// Row 0: q-p (10 keys, no del) — reference row for keyWUnit
		// Row 1: a-l + del (del=1.0 so row 1 == same width as row 0)
		rows = {
			{{"q","Q",VKEY_Q,false,1},{"w","W",VKEY_W,false,1},{"e","E",VKEY_E,false,1},{"r","R",VKEY_R,false,1},
			 {"t","T",VKEY_T,false,1},{"y","Y",VKEY_Y,false,1},{"u","U",VKEY_U,false,1},{"i","I",VKEY_I,false,1},
			 {"o","O",VKEY_O,false,1},{"p","P",VKEY_P,false,1}},

			{{"a","A",VKEY_A,false,1},{"s","S",VKEY_S,false,1},{"d","D",VKEY_D,false,1},{"f","F",VKEY_F,false,1},
			 {"g","G",VKEY_G,false,1},{"h","H",VKEY_H,false,1},{"j","J",VKEY_J,false,1},{"k","K",VKEY_K,false,1},
			 {"l","L",VKEY_L,false,1},{ICON_MD_KEYBOARD_BACKSPACE,"",VKEY_BACK,true,1.0f}},

			{{ICON_MD_KEYBOARD_CAPSLOCK,ICON_MD_KEYBOARD_CAPSLOCK,VKEY_CAPITAL,true,1.6f},
			 {"z","Z",VKEY_Z,false,1},{"x","X",VKEY_X,false,1},{"c","C",VKEY_C,false,1},{"v","V",VKEY_V,false,1},
			 {"b","B",VKEY_B,false,1},{"n","N",VKEY_N,false,1},{"m","M",VKEY_M,false,1}},

			{{"123","123",VKEY_PAGESWITCH,true,2.0f},
			 {"","",VKEY_SPACE,true,5.0f},
			 {ICON_MD_KEYBOARD_RETURN,"",VKEY_RETURN,true,2.0f}}
		};
	} else {
		// ── Compact numbers page ──────────────────────────────────────────────
		// Row 0: 1-0 (10 keys, no del) — reference row for keyWUnit
		// Row 1: symbols + del (del=1.0 so row 1 == same width as row 0)
		// txtShift must match what VKtoChar actually emits with caps on — caps
		// state persists from the letters page, and these keys used to show
		// their unshifted glyph while typing the shifted one (';' shown, ':'
		// typed; '1' shown, '!' typed). Same pairs as the full layout, and the
		// only way to reach ':' on phones (the sfx: prompt prefix).
		rows = {
			{{"1","!",VKEY_1,false,1},{"2","@",VKEY_2,false,1},{"3","#",VKEY_3,false,1},{"4","$",VKEY_4,false,1},
			 {"5","%",VKEY_5,false,1},{"6","^",VKEY_6,false,1},{"7","&",VKEY_7,false,1},{"8","*",VKEY_8,false,1},
			 {"9","(",VKEY_9,false,1},{"0",")",VKEY_0,false,1}},

			{{"-","_",VKEY_OEM_MINUS,false,1},{"=","+",VKEY_OEM_PLUS,false,1},
			 {"[","{",VKEY_OEM_4,false,1},{"]","}",VKEY_OEM_6,false,1},
			 {":",":",VKEY_COLONKEY,false,1},{"'","\"",VKEY_OEM_7,false,1},
			 {",","<",VKEY_OEM_COMMA,false,1},{".",">",VKEY_OEM_PERIOD,false,1},
			 {"/","?",VKEY_OEM_2,false,1},{ICON_MD_KEYBOARD_BACKSPACE,"",VKEY_BACK,true,1.0f}},

			{{"ABC","ABC",VKEY_PAGESWITCH,true,2.0f},
			 // Caps here too, or the shifted symbols above are only reachable
			 // by toggling caps on the letters page first. Row was 9 units
			 // against the 10-unit reference row, so the extra key fits.
			 {ICON_MD_KEYBOARD_CAPSLOCK,ICON_MD_KEYBOARD_CAPSLOCK,VKEY_CAPITAL,true,1.0f},
			 {"","",VKEY_SPACE,true,5.0f},
			 {ICON_MD_KEYBOARD_RETURN,"",VKEY_RETURN,true,2.0f}}
		};
	}

	// Lock for the duration of keys/dirty rebuild — render() may be reading concurrently.
	std::lock_guard lk(keysMutex_);
	keys.clear();

	// Outer padding so the keyboard content doesn’t hug the frame
	const float padX = width * 0.035f;
	const float padY = height * 0.045f;

	const float gridW = std::max(1.0f, width - 2.0f * padX);
	const float gridH = std::max(1.0f, height - 2.0f * padY);

	const float Gx = gridW * colGap;
	const float Gy = gridH * rowGap;

	// Always use 4-row geometry so row height and positions are stable across pages.
	const float rowH = (gridH - Gy * 3.f) / 4.f;

	// Fixed unit width: derived from row 0 (widest reference row).
	// Shorter rows are centered using the same unit width.
	float refUnits = 0.f;
	for (auto& kd : rows[0]) refUnits += kd.units;
	const float refGaps = Gx * ((float)rows[0].size() - 1.f);
	const float keyWUnit = (gridW - refGaps) / refUnits;

	// Row slot indices: compact letters = [0,1,2,3], compact numbers = [0,1,_,3]
	// (slot 2 left empty so rows keep the same y positions across pages)
	const bool skipSlot2 = (compactMode && numPage); // numbers page has no row at slot 2

	for (int r = 0; r < (int)rows.size(); ++r) {
		// Map logical row index to slot: numbers page skips slot 2, bottom bar → slot 3
		int slot = r;
		if (skipSlot2 && r == 2) slot = 3; // bottom bar goes to slot 3

		const float y = padY + slot * (rowH + Gy);

		// Bottom bar (row with VKEY_PAGESWITCH as first key): expand to full grid width.
		// Space key fills whatever is left after the fixed-width side keys.
		const bool isBottomBar = compactMode && (rows[r][0].vkey == VKEY_PAGESWITCH);
		if (isBottomBar) {
			const float sideW = keyWUnit * 2.0f; // width of 123/ABC and return keys
			float x = padX;
			for (auto& kd : rows[r]) {
				Key k;
				k.special = kd.special;
				k.vkey = kd.vkey;
				k.label = kd.txt ? kd.txt : "";
				k.labelShift = kd.txtShift ? kd.txtShift : k.label;

				float w;
				if (kd.vkey == VKEY_SPACE) {
					w = gridW - 2.0f * sideW - 2.0f * Gx;
				} else {
					w = sideW;
				}
				k.bounds = SkRect::MakeXYWH(x, y, w, rowH);
				keys.emplace_back(std::move(k));
				x += w + Gx;
			}
			continue;
		}

		float rowUnits = 0.f;
		for (auto& kd : rows[r]) rowUnits += kd.units;
		const float rowW = keyWUnit * rowUnits + Gx * ((float)rows[r].size() - 1.f);
		// Center shorter rows within the grid
		float x = padX + (gridW - rowW) * 0.5f;

		for (auto& kd : rows[r]) {
			Key k;
			k.special = kd.special;
			k.vkey = kd.vkey;
			k.label = kd.txt ? kd.txt : "";
			k.labelShift = kd.txtShift ? kd.txtShift : k.label;

			float w = keyWUnit * kd.units;
			k.bounds = SkRect::MakeXYWH(x, y, w, rowH);
			keys.emplace_back(std::move(k));

			x += w + Gx;
		}
	}
	dirty.assign(keys.size(), 1);
	anyDirty.store(true);
	needsFullClear.store(true);
}

void AlphaKeyboard::syncGlobalShiftFlag() {
	_STATE->shiftPressed = (shiftPressed.load() || capsLock.load());
}

void AlphaKeyboard::toggleShift(bool onOff) {
	shiftPressed.store(onOff);
	syncGlobalShiftFlag();
	markAllDirty();      // NEW
	redraw();
}

void AlphaKeyboard::toggleCaps() {
	capsLock.store(!capsLock.load());
	syncGlobalShiftFlag();
	markAllDirty();      // NEW
	redraw();
}

void AlphaKeyboard::startRepeat() {
	repeating = true;
	repeatFired = false;
	pressStart = std::chrono::steady_clock::now();
	lastRepeat = pressStart;
}
void AlphaKeyboard::stopRepeat() { repeating = false; }
void AlphaKeyboard::tickRepeat() {
	if (!repeating) return;
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - pressStart).count();
	if (elapsed < REPEAT_DELAY_MS) return;
	auto step = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRepeat).count();
	if (step >= REPEAT_RATE_MS) { lastRepeat = now; redraw(); }
}

// One repeat pulse of the HELD key: returns its vkey, or -1 when nothing is
// due. Generalizes the old backspace-only pulse - every typeable key repeats.
int AlphaKeyboard::consumeRepeatKeyPulse() {
	if (!repeating) return -1;
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - pressStart).count();
	if (elapsed < REPEAT_DELAY_MS) return -1;
	auto step = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRepeat).count();
	if (step < REPEAT_RATE_MS) return -1;
	lastRepeat = now;
	int idx = activeIndex.load();
	std::lock_guard lk(keysMutex_);
	if (idx >= 0 && idx < (int)keys.size() && VKeyTypesCharacter(keys[idx].vkey)) {
		repeatFired = true;
		return keys[idx].vkey;
	}
	return -1;
}

void AlphaKeyboard::drawKey(SkCanvas* c, const Key& k, SkFont& font, SkFont& fontIcon) {
	SkPaint p; p.setAntiAlias(true);

	// Clear key area (no per-key frames)
	SkPaint clear; clear.setColor(sk_colours::wbg);
	c->drawRect(k.bounds, clear);

	// Like numerical: change ONLY glyph color on press/latch
	const bool latched = (k.vkey == VKEY_SHIFT && shiftPressed.load()) || (k.vkey == VKEY_CAPITAL && capsLock.load());
	const SkColor glyphColor = (k.pressed || latched) ? skcol::blue_transparent : skcol::text;

	// Spacebar: draw at 60% of row height, centered vertically.
	if (k.vkey == VKEY_SPACE) {
		constexpr float kSBH = 0.60f; // 60% of row height
		const float h = k.bounds.height();
		const float cy = k.bounds.centerY();
		const float nh = std::max(6.0f, h * kSBH);         // avoid too tiny bars
		SkRect sb = SkRect::MakeXYWH(k.bounds.left(),
			cy - nh * 0.5f,
			k.bounds.width(),
			nh);

		SkPaint br; br.setAntiAlias(true);
		br.setStyle(SkPaint::kStroke_Style);
		br.setStrokeWidth(1.0f);
		br.setColor(skcol::fg);

		const float r = std::max(3.0f, std::min(sb.width(), sb.height()) * 0.12f);
		c->drawRRect(SkRRect::MakeRectXY(sb, r, r), br);
		return; // nothing else to draw for spacebar
	}


	// Label
	const bool shifted = (shiftPressed.load() || capsLock.load());
	const std::string& txt = shifted ? (k.labelShift.empty() ? k.label : k.labelShift) : k.label;
	if (txt.empty()) return;

	p.setStyle(SkPaint::kFill_Style);
	p.setColor(glyphColor);

	// Special keys: icon glyphs use height-based sizing; plain text uses same size as letter keys.
	if (k.special) {
		const bool isIcon = isIconGlyphUtf8(txt);
		SkFont* pf = isIcon ? &fontIcon : &font;

		const float prevSize = pf->getSize();
		const float pad = std::max(2.0f, std::min(k.bounds.width(), k.bounds.height()) * 0.12f);
		const float maxW = std::max(1.0f, k.bounds.width() - 2.0f * pad);
		const float maxH = std::max(1.0f, k.bounds.height() - 2.0f * pad);

		const float baseSize = isIcon
			? std::max(10.0f, k.bounds.height() * 0.54f)
			: std::min(_STATE->textsize2 * 0.9f, k.bounds.height() * 0.75f);
		pf->setSize(baseSize);

		SkRect b;
		pf->measureText(txt.c_str(), (int)txt.size(), SkTextEncoding::kUTF8, &b);

		float bw = std::max(1.0f, b.width());
		float bh = std::max(1.0f, b.height());
		float scale = std::min(maxW / bw, maxH / bh);
		if (scale < 1.0f) {
			pf->setSize(baseSize * scale);
			pf->measureText(txt.c_str(), (int)txt.size(), SkTextEncoding::kUTF8, &b);
		}

		const float tx = k.bounds.centerX() - (b.left() + b.right()) * 0.5f;
		const float ty = k.bounds.centerY() - (b.top() + b.bottom()) * 0.5f;

		c->drawSimpleText(txt.c_str(), (int)txt.size(), SkTextEncoding::kUTF8, tx, ty, *pf, p);
		pf->setSize(prevSize);
		return;
	}

	// Normal keys: match TextButton sizing (textsize2 * 0.9), capped to fit key height.
	SkFont& pf = font;
	const float prevSize = pf.getSize();
	float target = std::min(_STATE->textsize2 * 0.9f, k.bounds.height() * 0.75f);
	pf.setSize(target);

	float tw = pf.measureText(txt.c_str(), (int)txt.size(), SkTextEncoding::kUTF8);
	SkFontMetrics m{}; pf.getMetrics(&m);
	float tx = k.bounds.centerX() - tw * 0.5f;
	float ty = k.bounds.centerY() + (pf.getSize() * 0.30f);

	c->drawSimpleText(txt.c_str(), (int)txt.size(), SkTextEncoding::kUTF8, tx, ty, pf, p);
	pf.setSize(prevSize);
}

void AlphaKeyboard::render(void* /*_c*/) {
	auto canvas = _STATE->graphics.getCanvas(windowindex, startx, starty, width, height);
	// Before the oldwindowindex bookkeeping: a frame with no surface must keep
	// the index mismatch so the first good frame still repaints in full.
	if (!canvas)
		return;
	// Losing the GL surface (Android background -> foreground) destroys every
	// window surface; getCanvas() then hands back a fresh, blank one under a new
	// index. Nothing is dirty at that point, so the partial redraw below would
	// leave the keyboard empty -- repaint all of it instead.
	if (windowindex != oldwindowindex) {
		oldwindowindex = windowindex;
		markAllDirty();
		needsFullClear.store(true);
	}

	SkFont& font = _STATE->font_normal;
	SkFont& fontIcon = _STATE->font_md_kb;

	tickRepeat();

	if (needsFullClear.load()) {
		// After a layout change the old page content may persist; clear the whole keyboard area.
		SkPaint bg; bg.setColor(sk_colours::wbg);
		canvas->drawRect(SkRect::MakeXYWH(0, 0, (float)width, (float)height), bg);
		needsFullClear.store(false);
	}

	if (anyDirty.load()) {
		std::lock_guard lk(keysMutex_);
		for (int i = 0; i < (int)keys.size(); ++i) {
			if (!dirty[i]) continue;
			canvas->save();
			canvas->clipRect(keys[i].bounds, true);
			drawKey(canvas, keys[i], font, fontIcon);
			canvas->restore();
			dirty[i] = 0;
		}
		anyDirty.store(std::any_of(dirty.begin(), dirty.end(), [](uint8_t d) { return d != 0; }));
	}

	// Border is cheap; keep drawing it (or guard with anyDirty if you prefer)
	borderWindow(canvas);
}


void AlphaKeyboard::updateActiveFromPoint(float ex, float ey) {
	const float lx = ex - startx, ly = ey - starty;
	int idx = -1;
	{
		std::lock_guard lk(keysMutex_);
		for (int i = 0; i < (int)keys.size(); ++i) {
			if (keys[i].bounds.contains(lx, ly)) { idx = i; break; }
		}
	}
	int prev = activeIndex.load();
	if (prev != idx) {
		{
			std::lock_guard lk(keysMutex_);
			if (prev >= 0 && prev < (int)keys.size()) {
				keys[prev].pressed = false;
				if (prev < (int)dirty.size()) dirty[prev] = 1;
			}
			if (idx >= 0 && idx < (int)keys.size()) {
				keys[idx].pressed = true;
				if (idx < (int)dirty.size()) dirty[idx] = 1;
			}
			anyDirty.store(true);
		}
		activeIndex.store(idx);
		redraw();
	}
}


int AlphaKeyboard::cb(const InputEvent& e) {
	switch (e.action) {
	case ACTION_DOWN: {
		if (!isInside(e)) return -1;
		pointerid = e.pointer_id; downX = e.x; downY = e.y;
		updateActiveFromPoint(e.x, e.y);
		{
			// Every typeable key repeats when held, not just backspace.
			std::lock_guard lk(keysMutex_);
			int idx = activeIndex.load();
			if (idx >= 0 && idx < (int)keys.size() && VKeyTypesCharacter(keys[idx].vkey))
				startRepeat();
		}
		return -2;
	}
	case ACTION_MOVE: {
		if (e.pointer_id != pointerid) return -1;
		if (!isInside(e)) {
			int idx = activeIndex.exchange(-1);
			{
				std::lock_guard lk(keysMutex_);
				if (idx >= 0 && idx < (int)keys.size()) {
					keys[idx].pressed = false;
					if (idx < (int)dirty.size()) dirty[idx] = 1;
					anyDirty.store(true);
				}
			}
			stopRepeat(); redraw(); return -2;
		}
		{
			// Sliding onto another key restarts the repeat clock for THAT key,
			// so the new key waits its own initial delay.
			const int prev = activeIndex.load();
			updateActiveFromPoint(e.x, e.y);
			const int cur = activeIndex.load();
			if (cur != prev) {
				std::lock_guard lk(keysMutex_);
				if (cur >= 0 && cur < (int)keys.size() && VKeyTypesCharacter(keys[cur].vkey))
					startRepeat();
				else
					stopRepeat();
			}
		}
		return -2;
	}
	case ACTION_UP: {
		if (e.pointer_id != pointerid) return -1;
		int idx = activeIndex.exchange(-1);
		pointerid = -1; stopRepeat();
		const bool firedRepeats = repeatFired;
		repeatFired = false;

		int vkey = -1;
		{
			std::lock_guard lk(keysMutex_);
			if (idx < 0 || idx >= (int)keys.size()) { redraw(); return -1; }
			keys[idx].pressed = false;
			if (idx < (int)dirty.size()) dirty[idx] = 1;
			anyDirty.store(true);
			vkey = keys[idx].vkey;
		}
		redraw();
		// The hold already typed via repeat pulses - the release adds nothing.
		if (firedRepeats) return -2;

		if (vkey == VKEY_CAPITAL) { toggleCaps(); return -2; }
		if (vkey == VKEY_PAGESWITCH) {
			numPage = !numPage;
			layoutKeys();
			redraw();
			return -2;
		}
		return vkey;
	}
	default: break;
	}
	return -1;
}

void AlphaKeyboard::setHardwareKeyHighlight(int vkey, bool down) {
	bool changed = false;
	{
		std::lock_guard lk(keysMutex_);
		for (int i = 0; i < (int)keys.size(); ++i) {
			auto& k = keys[i];
			if (k.vkey == vkey && k.pressed != down) {
				k.pressed = down;
				if (i < (int)dirty.size()) dirty[i] = 1;
				anyDirty.store(true);
				changed = true;
			}
		}
	}
	if (changed) redraw();
}


// =============================================================
// ====================  AlphaPopUp  ===========================
// =============================================================


static inline bool isShiftKey(int vk) {
	return vk == VKEY_SHIFT || vk == VKEY_LSHIFT || vk == VKEY_RSHIFT;
}

AlphaPopUp::AlphaPopUp(tsl::AppState* appState)
	: TextInputPopUp(appState, tsl::graphics::KeyboardType::TYPE_CLASS_TEXT),
	keyboard(appState) {
	perm = true;
	textInput1.hasFocus = true;

}

void AlphaPopUp::init() {

	const bool portrait = (_STATE->windowWidth < _STATE->windowHeight);

	if (portrait) {
		const float wf = std::clamp(float(w) / float(_STATE->windowWidth), 0.30f, 0.98f);
		keyboard.setSizeFractions(wf, 0.36f);
		keyboard.alignment = AlphaKeyboard::BOTTOM;
	} else {
		// Landscape: keyboard on the right, natural height (rows at textsize1), ~45% width
		// rowH = height * 0.222 (after padY=4.5% and rowGap=0.8%), so height = textsize1 / 0.222
		const float naturalH = _STATE->textsize1 * 4.5f;
		const float hf_landscape = std::clamp(naturalH / (float)_STATE->windowHeight, 0.25f, 1.0f);
		keyboard.setSizeFractions(0.45f, hf_landscape);
		keyboard.alignment = AlphaKeyboard::RIGHT;
	}
	keyboard.init();

	if (childView_ != nullptr) {
		const auto tmpH = _STATE->textsize2 * (title.empty() ? 3.0 : 5.0);
		const float limit = portrait ? (float)keyboard.starty : (float)_STATE->windowHeight;
		const auto maxHeight = limit - tmpH - 3 * _STATE->textsize2;
		auto childH = childView_->heightEstimate(maxHeight);
		if (childH < maxHeight) {
			auto normalPos = portrait ? _STATE->windowHeight / 3.0 : _STATE->windowHeight / 6.0;
			auto limitPos = portrait ? (double)keyboard.starty : (double)_STATE->windowHeight;
			if (childH < DISTANCE(normalPos, limitPos) - 2 * _STATE->textsize2)
				alignY_ = -1;
			else
				alignY_ = limitPos - 2 * _STATE->textsize2 - childH - tmpH;
		}
		else
			alignY_ = _STATE->textsize2;
	}
	else alignY_ = (int)(_STATE->textsize2);
	TextInputPopUp::init();

	// Landscape: override text box to use the left portion (keyboard occupies the right)
	if (!portrait) {
		const float margin = _STATE->textsize2;
		x = margin;
		w = keyboard.startx - 2 * margin;
		textInput1.width = w;
		textInput1.startx = 0;
		textInput1.stopx = w;
		if (childView_ != nullptr)
			y = margin * 2.0f;
		else {
			y = (_STATE->windowHeight - h) * 0.5f;
			if (y < margin) y = margin;
		}
	}

	// Keep current text, caret at end, blink reset
	const std::string cur = textInput1.getText();
	textInput1.setHighLight(false);
	textInput1.setText(cur);
	textInput1.hasFocus = true;
	textInput1.resetTimer();
	if (childView_ != nullptr) {
		// In landscape the keyboard is to the right; left side uses full screen height for the list
		const float childStopy = portrait ? (keyboard.starty - _STATE->textsize2) : (_STATE->windowHeight - _STATE->textsize2);
		childView_->starty = y + h + _STATE->textsize2;
		childView_->stopy = childStopy;
		childView_->height = childView_->stopy - childView_->starty;
		childView_->startx = x;
		childView_->stopx = x + w;
		childView_->width = w;
		childView_->init();
	}
}

void AlphaPopUp::render(void* c) {
	// Base popup (title + input + border, handles resize-init)
	TextInputPopUp::render(nullptr);

	// Keyboard in its own window
	keyboard.render(nullptr);

	// Held-key repeat pulse: any typeable key, honouring the shift state
	if (int rk = keyboard.consumeRepeatKeyPulse(); rk >= 0) {
		_STATE->shiftPressed = (keyboard.isShift() || keyboard.isCaps());
		InputEvent rep{}; rep.action = ACTION_KEY_UP; rep.pointer_id = rk;
		textInput1.callback(rep);
	}
}

void AlphaPopUp::callback(const InputEvent& e) {
	// 1) On-screen alpha keyboard first
	int ret = keyboard.cb(e);
	if (ret != -1) {
		if (ret == -2) return; // press/move consumed visually only

		// Caps / page-switch: handled inside keyboard, no character output
		if (ret == VKEY_CAPITAL) return;

		// Temporary shift state while injecting the key
		_STATE->shiftPressed = (keyboard.isShift() || keyboard.isCaps());
		InputEvent e2 = e; e2.action = ACTION_KEY_UP; e2.pointer_id = ret;
		textInput1.callback(e2);
		_STATE->shiftPressed = (keyboard.isShift() || keyboard.isCaps());

		// Confirm/Cancel via base helper (uses overridden onEnter() here)
		handleConfirmCancelFromKeyboard(ret);
	}

	// 2) Hardware modifier keys
	auto isHwShift = [](int vk) { return vk == VKEY_SHIFT || vk == VKEY_LSHIFT || vk == VKEY_RSHIFT; };
	if (e.action == ACTION_KEY_DOWN) {
		keyboard.setHardwareKeyHighlight(e.pointer_id, true);
		if (isHwShift(e.pointer_id)) { keyboard.toggleShift(true);  return; }
		if (e.pointer_id == VKEY_CAPITAL) { return; }
		// OS auto-repeat arrives as additional KEY_DOWNs while a key is held;
		// typing only on KEY_UP silently dropped them all. The repeats type
		// here, and the eventual release is swallowed below so a held key does
		// not end on one extra character.
		if (VKeyTypesCharacter(e.pointer_id)) {
			if (e.pointer_id == hwHeldVkey_) {
				_STATE->shiftPressed = _STATE->shiftPressed || keyboard.isShift() || keyboard.isCaps();
				InputEvent e2 = e; e2.action = ACTION_KEY_UP;
				textInput1.callback(e2);
				hwRepeated_ = true;
			}
			else hwHeldVkey_ = e.pointer_id;
		}
	}
	else if (e.action == ACTION_KEY_UP) {
		keyboard.setHardwareKeyHighlight(e.pointer_id, false);
		if (isHwShift(e.pointer_id)) { keyboard.toggleShift(false); return; }
		if (e.pointer_id == VKEY_CAPITAL) { keyboard.toggleCaps(); return; }
	}
	// 3) Regular hardware keys → input or confirm/cancel
	if (e.action == ACTION_KEY_UP) {
		if (e.pointer_id == hwHeldVkey_) hwHeldVkey_ = -1;
		if (handleConfirmCancelFromKeyboard(e.pointer_id)) return; // calls onEnter() override above
		if (hwRepeated_) { hwRepeated_ = false; return; } // repeats already typed this key
		_STATE->shiftPressed = _STATE->shiftPressed || keyboard.isShift() || keyboard.isCaps();
		textInput1.callback(e);
		return;
	}

	// 4) Tap to place caret or dismiss
	if (e.action == ACTION_DOWN) {
		handleTapOrForwardToInput(e);
		return;
	}
}

// DEADLOCK FIX. This runs from View::addDraw(), i.e. with queue_draw ALREADY
// held. It used to call toForeground(), which takes queue_callback -- giving
// this thread the order draw -> callback, the exact inverse of the input
// thread's callback -> draw (handleMotionEvent holds queue_callback for the
// whole dispatch, and the view callbacks it runs call redraw()/deldraw()).
//
// Caught live on device: the save dialog is an AlphaPopUp, so tapping SAVE
// PROJECT while still touching left the worker in addDraw() holding queue_draw
// and waiting for queue_callback, and the input thread in redraw() holding
// queue_callback and waiting for queue_draw. Touch died permanently, rendering
// stopped, audio carried on.
//
// Each half now runs under the lock it already has: the window reorder here,
// the callback-queue raise in addRecursiveCB() below.
void AlphaPopUp::addRecursiveDraw() {
	textInput1.setHighLight(true);
	toForegroundDrawDirect(windowindex);
	View::addRecursiveDraw();
}

void AlphaPopUp::addRecursiveCB() {
	toForegroundCBDirect();   // queue_callback is held by View::addCB()
	// TextInputPopUp::, NOT View:: -- the base sets hasFocus, which is what
	// findFocusedView() routes hardware key events by. Skipping it leaves the
	// name field unable to receive typed characters on desktop (on Android the
	// on-screen keyboard routes through touch, so it looks fine there).
	TextInputPopUp::addRecursiveCB();
}

void AlphaPopUp::delRecursiveDraw() {
	_STATE->graphics.deleteWindow(windowindex);
	keyboard.delRecursiveDraw();
	View::delRecursiveDraw();
}
