#pragma once
// created 27.07.2022

#include "view.h"
#include "Input.h"
#include "logger.h"
#include "keydefines.h"
#include "tools/WaitNotify.h"

#include <string>
#include <mutex>
#include <limits>
#include <cctype>
#include <cmath>
#include <atomic>
#include <vector>
#include <chrono>
#include <thread>


// Skia types used in member fields
#include "include/core/SkRect.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkRRect.h"

namespace tsl {
	class WaitNotify;
}

namespace tsl {
	namespace graphics {


		template<size_t N = 100>
		class StringMutex {
		public:
			StringMutex() { buf[0] = '\0'; }
			explicit StringMutex(const char* text) { assign(text); }

			void operator=(const char* o) { std::lock_guard lk(mutex); assign(o); }
			void operator=(const std::string& o) { std::lock_guard lk(mutex); assign(o.c_str()); }
			void operator+=(const std::string& o) { std::lock_guard lk(mutex); append(o.c_str()); }
			void operator+=(char o) { std::lock_guard lk(mutex); char s[2]{ o,'\0' }; append(s); }
			void operator+=(const char* o) { std::lock_guard lk(mutex); append(o); }
			void operator--() { std::lock_guard lk(mutex); if (len > 0) buf[--len] = '\0'; }

			std::string operator()(size_t index = 0) { std::lock_guard lk(mutex); return index < len ? std::string(buf + index, len - index) : ""; }
			char        operator[](size_t index) { std::lock_guard lk(mutex); return index < len ? buf[index] : '\0'; }

			bool empty() { std::lock_guard lk(mutex); return len == 0; }
			bool contains(const std::string& s) { std::lock_guard lk(mutex); return std::string_view(buf, len).find(s) != std::string_view::npos; }
			size_t size() { std::lock_guard lk(mutex); return len; }
			const char* c_str() { std::lock_guard lk(mutex); return buf; } // unsafe without held lock, see note

			void insert(int pos, const std::string& s) {
				std::lock_guard lk(mutex);
				if (pos > (int)len) pos = (int)len;
				size_t slen = s.size();
				size_t newlen = std::min(len + slen, N - 1);
				slen = newlen - len;
				if (slen == 0) return;
				memmove(buf + pos + slen, buf + pos, len - pos + 1);
				memcpy(buf + pos, s.c_str(), slen);
				len = newlen;
			}

			void del(int pos) {
				std::lock_guard lk(mutex);
				if (len == 0 || pos >= (int)len) return;
				memmove(buf + pos, buf + pos + 1, len - pos);
				--len;
			}

			std::recursive_mutex mutex;

		private:
			char buf[N];
			size_t len = 0;

			void assign(const char* s) {
				len = std::min(strlen(s), N - 1);
				memcpy(buf, s, len);
				buf[len] = '\0';
			}
			void append(const char* s) {
				size_t slen = std::min(strlen(s), N - 1 - len);
				memcpy(buf + len, s, slen);
				len += slen;
				buf[len] = '\0';
			}
		};

		class TextInputPopup;
		class NumericalPopup;

		class TextInput : public View {
			friend TextInputPopup;
			friend NumericalPopup;
		public:
						TextInput(tsl::AppState* appState, tsl::graphics::KeyboardType type = tsl::graphics::KeyboardType::TYPE_CLASS_NUMBER)
				: View(appState, WRAP, 0, CENTER_ALIGN, 0), _type(type) {
			}
		public:
			enum TextAlign {
				ALIGN_CENTER = 0,
				ALIGN_LEFT = 1
			};

			void setTextAlign(TextAlign align) { textAlign = align; }
			float getTextWidth(); // Get current text width
			float getTextStartX(); // Get where text actually starts drawing

			void render(void* ctx) override;
			void callback(const InputEvent& e) override;

			std::string getText() { return text(); }

			void setText(std::string t) {
				std::lock_guard lk(text.mutex);
				text = t;
				cursor = text().size();
			}

			void setHighLight(bool hl) { highlight = hl; }
			void resetTimer() { timer.reset(); }
			// Hard cap on typed characters, clamped to the text buffer. The
			// default matches the old implicit buffer cap, so existing inputs
			// behave exactly as before.
			void setMaxLength(int n) { maxLen = n < 1 ? 1 : (n > 239 ? 239 : n); }

			static bool isValidNumber(const std::string& s);
		private:
			tsl::AtomicTimer  timer{};
			bool       highlight{};
			StringMutex<240> text{ "dsf" };
			int        maxLen{ 99 };
			int        _type{};
			std::atomic<int>     cursor{ 0 };   // insertion caret
			TextAlign textAlign = ALIGN_CENTER; // Add this member
		};

		class TextInputPopUp : public View {
		public:
			TextInputPopUp(tsl::AppState* appState, tsl::graphics::KeyboardType type = tsl::graphics::KeyboardType::TYPE_CLASS_NUMBER)
				: View(appState, WRAP, 0, CENTER_ALIGN, 0), _type(type), textInput1(_appState, type) {
				perm = true;
				textInput1.hasFocus = true;
			}

			void init() override;
			void callback(const InputEvent&) override;
			void render(void*) override;
			virtual int onEnter() {
				auto s = textInput1.getText();
				for (char const& c : s) if (std::isdigit(c) == 0 && c != '.' && c != '-') return 1;
				return 1;
			}

			void setTitle(const std::string& t) { title = t; }
			void setText(const std::string& t) {
				textInput1.setText(t);
			}
			int x{}, y{}, w{}, h{};
			void setSlotId(int slotId) { waitSlotId = slotId; }
			void setToken(tsl::ThreadSignal::WaitToken t) { waitToken = t; }
			tsl::ThreadSignal::WaitToken token() const { return waitToken; }
			float getHeight();
		protected:
			// Prefers the token rendezvous when one was set; falls back to the
			// legacy slot wake for un-migrated callers.
			void wakeWaiter();
			int windowindex{ -1 };
			int waitSlotId{};
			tsl::ThreadSignal::WaitToken waitToken{};
			void delRecursiveDraw() override;
			void addRecursiveCB() override { hasFocus.store(true);  View::addRecursiveCB(); }
			void delRecursiveCB() override { hasFocus.store(false); View::delRecursiveCB(); }
			void addRecursiveDraw() override;

			uint32_t  _type{};
			std::string title;
			TextInput   textInput1;

			// Handle Enter/Esc uniformly. Returns true if it handled & closed.
			bool handleConfirmCancelFromKeyboard(int vkey);

			// Shared caret tap logic used by both popups.
			void handleTapOrForwardToInput(const InputEvent& e);
			int alignY_{-1};
		};

		class NumericalKeyboard : public View {
		public:
			NumericalKeyboard(tsl::AppState* appState) : View(appState, WRAP, 0, CENTER_ALIGN, 0) {}
			enum align { BOTTOM = 0, TOP, LEFT, RIGHT };

			void init() override;
			void render(void* _c) override;
			int  cb(const InputEvent& e);
			void delRecursiveDraw() override;

			int alignment{ BOTTOM };

		private:
			int windowindex{ -1 };
			float xpos{ -1 }, ypos{ -1 };
			int pointerid{ -1 };
			std::atomic<int> activeel{ -1 };
			static constexpr KeyboardCode vkeys[16]{
				VKEY_1, VKEY_2, VKEY_3, VKEY_BACK,
				VKEY_4, VKEY_5, VKEY_6, VKEY_RETURN,
				VKEY_7, VKEY_8, VKEY_9, VKEY_RETURN,
				VKEY_OEM_MINUS, VKEY_0, VKEY_OEM_PERIOD, VKEY_RETURN
			};
		};

		class NumericalPopUp : public TextInputPopUp {
		public:
			NumericalPopUp(tsl::AppState* appState) : TextInputPopUp(appState), keyboard(appState) {
				perm = true; textInput1.hasFocus = true;
			}
			void init() override;
			void callback(const InputEvent&) override;
			void render(void*) override;

		protected:
			void addRecursiveDraw() override;
			void delRecursiveDraw() override;

		private:
			NumericalKeyboard keyboard;
		};

		// -----------------------------
		// Full QWERTY keyboard view
		// -----------------------------

		class AlphaKeyboard : public View {
		public:
			static float clamp01(float v) { if (v < 0.2f) return 0.2f; if (v > 1.0f) return 1.0f; return v; }

			enum align { BOTTOM = 0, TOP, LEFT, RIGHT };

			explicit AlphaKeyboard(tsl::AppState* appState) : View(appState, WRAP, 0, CENTER_ALIGN, 0) {}

			void init() override;                // positions itself at bottom-of-screen when alignment==BOTTOM
			void render(void* c) override;
			void delRecursiveDraw() override;

			// returns: -2 consumed (press/move), -1 ignore, otherwise VKEY_*
			int  cb(const InputEvent& e);

			void setSizeFractions(float wf, float hf) { kFracW = clamp01(wf); kFracH = clamp01(hf); }
			int  alignment = BOTTOM;

			bool isShift() const { return shiftPressed.load(); }
			bool isCaps()  const { return capsLock.load(); }
			void toggleShift(bool onOff = true);
			void toggleCaps();
			// Returns the held key's vkey when a repeat is due, else -1.
			int  consumeRepeatKeyPulse();
			void setHardwareKeyHighlight(int vkey, bool down);
			void markKeyDirty(int idx) {
				std::lock_guard lk(keysMutex_);
				if (idx >= 0 && idx < (int)dirty.size()) { dirty[idx] = 1; anyDirty.store(true); }
			}
			void markAllDirty() {
				std::lock_guard lk(keysMutex_);
				if (dirty.empty()) return;
				std::fill(dirty.begin(), dirty.end(), 1);
				anyDirty.store(true);
			}
		private:
			struct Key {
				SkRect      bounds{};
				int         vkey{ 0 };
				std::string label;
				std::string labelShift;
				bool        special{ false };
				bool        pressed{ false };
			};

			void layoutKeys();
			void drawKey(SkCanvas* c, const Key& k, SkFont& font, SkFont& fontIcon);
			void updateActiveFromPoint(float ex, float ey);
			void syncGlobalShiftFlag();
			void startRepeat();
			void stopRepeat();
			void tickRepeat();

		private:
			int windowindex{ -1 };
			// Backing surfaces are destroyed when the GL/Metal surface is lost
			// (Android background -> foreground). getCanvas() then hands out a
			// fresh, blank one under a new index; see AlphaKeyboard::render.
			int oldwindowindex{ -1 };
			std::vector<Key> keys;
			// NEW: dirty tracking
			std::vector<uint8_t> dirty;
			std::atomic<bool> anyDirty{ true };
			std::atomic<int> activeIndex{ -1 };
			mutable std::recursive_mutex keysMutex_;

			int   pointerid{ -1 };
			float downX{ -1 }, downY{ -1 };

			float kFracW = 0.92f;
			float kFracH = 0.38f;

			float rowGap = 0.008f;
			float colGap = 0.008f;
			float corner = 0.16f;

			std::atomic<bool> shiftPressed{ false };
			std::atomic<bool> capsLock{ false };
			bool compactMode{ false };
			bool numPage{ false };     // compact mode: false=letters, true=numbers
			std::atomic<bool> needsFullClear{ false };

			static constexpr int REPEAT_DELAY_MS = 450;
			static constexpr int REPEAT_RATE_MS = 70;
			bool repeating{ false };
			// A pulse was consumed for this press: the release emits nothing,
			// or a held key would end on one extra character.
			bool repeatFired{ false };
			std::chrono::steady_clock::time_point pressStart{};
			std::chrono::steady_clock::time_point lastRepeat{};
		};


		// ====================  AlphaPopUp (now from TextInputPopUp)  ====================

		class AlphaPopUp : public TextInputPopUp {
		public:
			struct InputResult { bool confirmed; std::string text; };
			using OnCompleteCallback = std::function<int(const InputResult&)>;
			OnCompleteCallback onCompleteCallback{};
			AlphaPopUp(tsl::AppState* appState);
			// View overrides
			void init() override;          // mirrors NumericalPopUp: init base + keyboard
			void render(void* c) override; // draws title/input + keyboard + border
			void callback(const InputEvent& e) override;
			std::function<void()> onCancel; // optional callback when cancelled (e.g. to restore focus)
		protected:
			void addRecursiveDraw() override;
			void addRecursiveCB() override;
			void delRecursiveDraw() override;
			int onEnter() override {
				if (onCompleteCallback) {
					auto s = textInput1.getText();
					InputResult ir{ !s.empty(), s };
					return onCompleteCallback(ir);
				}
				return 1; // keep the existing "close on enter" behaviour via base helper
			}
			View* childView_{};
		private:
			// IMPORTANT: do NOT redeclare windowindex here; use the one from TextInputPopUp
			AlphaKeyboard keyboard;
			// Hardware auto-repeat state: the vkey currently held down, and
			// whether its repeats already typed (then the release is silent).
			int  hwHeldVkey_{ -1 };
			bool hwRepeated_{ false };
		};
	} // namespace graphics
} // namespace tsl
