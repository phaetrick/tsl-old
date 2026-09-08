//
// Created by pr on 25.01.22.
//

#include "Toast.h"
#include "app.h"
#include <IconsMaterialDesignReduced.h>
#include <include/core/SkFont.h>
#include <utility>

using namespace tsl::graphics;
void showToast(tsl::AppState* appState, const char* string) {
    tsl::graphics::showToast3(appState, string);
}

// Helper function to find the best break point in a string
size_t findBestBreakPoint(const std::string& text, size_t maxSize) {
    if (maxSize >= text.size()) return text.size();

    // Look for whitespace within the last 20% of maxSize
    size_t searchStart = std::max(size_t(1), maxSize - maxSize / 5);

    for (size_t i = maxSize; i >= searchStart; --i) {
        char c = text[i - 1];
        if (c == ' ' || c == '\t' || c == '-') {
            return i;
        }
    }

    // Look for punctuation
    for (size_t i = maxSize; i >= searchStart; --i) {
        char c = text[i - 1];
        if (c == ',' || c == ';' || c == '.' || c == '!' || c == '?') {
            return i;
        }
    }

    // Look for camelCase or snake_case breaks
    for (size_t i = maxSize; i >= searchStart; --i) {
        char c = text[i - 1];
        if (i < text.size()) {
            char next = text[i];
            // camelCase break (lowercase to uppercase)
            if (islower(c) && isupper(next)) return i;
            // snake_case break
            if (c == '_') return i;
            // Number to letter break
            if (isdigit(c) && isalpha(next)) return i;
            if (isalpha(c) && isdigit(next)) return i;
        }
    }

    // As last resort, use maxSize (hard break)
    return maxSize;
}

void Toast::prepare(const std::string &text, float seconds) {
    lines.clear();
    _ns = static_cast<uint64_t>(seconds * 1e9);
    height = width = 0;
    _time = 0;
    perm = true;

    fontsize = _STATE->textsize2 * .9f;
    const float offset = fontsize * .5f;

    SkFont font(_STATE->font_normal);
    font.setSize(fontsize);

    const float w = _STATE->windowWidth - 2 * offset;

    // Split by newlines first
    std::vector<std::string> paragraphs;
    auto filesize = text.size();
    auto line_beg = text.c_str();
    auto line_end = text.c_str() + filesize;

    for (; auto line_pos = std::find(line_beg, line_end, '\n'); line_beg = line_pos + 1) {
        paragraphs.emplace_back(line_beg, line_pos);
        if (line_pos == line_end) break;
    }

    std::vector<std::string> strings;

    // Process each paragraph
    for (const auto& paragraph : paragraphs) {
        if (paragraph.empty()) {
            strings.push_back(""); // Preserve empty lines
            continue;
        }

        std::string remaining = paragraph;

        while (!remaining.empty()) {
            // Binary search for maximum characters that fit
            size_t left = 1, right = remaining.size();
            size_t bestFit = 1;

            while (left <= right) {
                size_t mid = (left + right) / 2;

                SkRect bounds{};
                font.measureText(remaining.c_str(), mid, SkTextEncoding::kUTF8, &bounds);

                if (bounds.width() <= w) {
                    bestFit = mid;
                    left = mid + 1;
                } else {
                    right = mid - 1;
                }
            }

            // Find the best break point within bestFit characters
            size_t breakPoint = findBestBreakPoint(remaining, bestFit);

            std::string line = remaining.substr(0, breakPoint);

            // Trim trailing whitespace from the line
            while (!line.empty() && isspace(line.back())) {
                line.pop_back();
            }

            strings.push_back(line);

            // Remove the processed part from remaining text
            remaining = remaining.substr(breakPoint);

            // Trim leading whitespace from remaining text
            while (!remaining.empty() && isspace(remaining.front())) {
                remaining.erase(0, 1);
            }
        }
    }

    // Rest of your existing code for positioning and layout...
    float maxwidth = 0;
    float sx = 0;

    for(const auto& st : strings){
        SkRect bounds{};
        font.measureText(st.c_str(), st.size(), SkTextEncoding::kUTF8, &bounds);
        if(bounds.width() > maxwidth) {
            maxwidth = bounds.width();
            sx = maxwidth * .5f - bounds.centerX();
        }
        auto ypos = SkFloatToScalar(bounds.height() * .5f - bounds.centerY());
        lines.push_back({st, height + ypos + offset});
        height += bounds.height() + offset;
    }
    height += offset;

    startx = (_STATE->windowWidth - maxwidth) * .5f - offset;
    width = maxwidth + 2 * offset;
    stopx = startx + width;
    x = sx + offset;

    starty = (_STATE->windowHeight - height) * .5f;
    stopy = starty + height;

    // Handle overflow cases...
    while(starty < 0){
        if (lines.empty()) break;
        SkRect bounds{};
        font.measureText(lines.back().string.c_str(), lines.back().string.size(), SkTextEncoding::kUTF8, &bounds);
        float minus = bounds.height() + offset;
        starty += minus;
        height -= minus;
        stopy = starty + height;
        lines.pop_back();
    }

    while(stopy > _STATE->windowHeight) {
        if (lines.empty()) break;
        SkRect bounds{};
        font.measureText(lines.back().string.c_str(), lines.back().string.size(),
                         SkTextEncoding::kUTF8, &bounds);
        float minus = bounds.height() + offset;
        stopy -= minus;
        height -= minus;
        lines.pop_back();
    }

    if(stopy < _STATE->windowHeight * .9f){
        stopy = _STATE->windowHeight * .9f;
        starty = stopy - height;
    }
}

void Toast::render(void *ctx) {
    if(height == 0 || width == 0){
        return;
    }
    auto canvas = _STATE->graphics.getCanvas(windowindex, startx,
                                               starty, width, height);
    if (canvas == nullptr){
            return;
    }
    double passed;
    if(_time == 0) {
        _time = tsl::time::nanosecondsSinceEpoch();
        passed = 0;
    }
    else
       passed = (double) (tsl::time::nanosecondsSinceEpoch() - _time) / (double) _ns ;
    if(passed < 0. || passed > 1.)
        passed = 0;
    else if(passed < .1) passed *= 10.;
    else if (passed > .9) passed = (.1 - (passed - .9)) * 10.;
    else passed = 1.;

    auto alpha = (uint8_t) (passed * 255.);
//LOGE("%d %g", alpha, passed);

    canvas->clear(SkColorSetA(tsl::sk_colours::bg, alpha));

    SkFont font(_STATE->font_normal);
    font.setSize(fontsize);
    SkPaint paint;
    paint.setColor(SkColorSetA(tsl::sk_colours::fg, alpha));
    paint.setAntiAlias(true);

    const float offset = fontsize * .5f;

    for(const auto& s : lines){
        canvas->drawSimpleText(s.string.c_str(), s.string.size(), SkTextEncoding::kUTF8, x, s.y, font, paint);
       // canvas->drawString(s.c_str(), 0, y, font, paint);
    }
    paint.setColor(SkColorSetA(tsl::sk_colours::grey, alpha>>1u));
    paint.setStrokeWidth(lw);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawRect(
            SkRect::MakeXYWH(lw2, lw2, width - lw, height - lw),
            paint);
}

void Toast::delRecursiveDraw() {
    View::delRecursiveDraw();
    _STATE->graphics.deleteWindow(windowindex);
	windowindex = -1;
};


void toast(tsl::AppState *_appState, const std::string text, const float seconds) {
    _appState->toast.prepare(text, seconds);
    _appState->toast.addDraw();
    using std::chrono::operator""ns;
    std::this_thread::sleep_for(seconds * 1000000000ns);
    _appState->toast.deldraw();
}

void tsl::graphics::showToast3(tsl::AppState* appState, const std::string text, const float seconds) {
    appState->toastQueue.add_task([=] {toast(appState, std::move(text), seconds); });
}