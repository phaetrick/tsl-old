#include "LogView.h"
#include "skia.h"
#include "app.h"
#include "logger.h"

void tsl::graphics::LogView::delRecursiveDraw() {
	View::delRecursiveDraw();
	_appState->graphics.deleteWindow(windex);
	windex = -1;
	firstRun = true;
    needsRecalc = true;
}


// Add these member variables to your LogView class header:
// int scrollOffset = 0;  // Current scroll position (in lines)
// int maxScrollOffset = 0; // Maximum allowed scroll offset
// std::vector<std::string> cachedWrappedLines; // Cache wrapped lines
// bool needsRecalc = true; // Flag to recalculate wrapped lines

void tsl::graphics::LogView::render(void*) {
    auto w = _appState->windowWidth, h = _appState->windowHeight, x = 0, y = 0;
    auto canvas = _appState->graphics.getCanvas(windex, x, y, w, h);
    if (!canvas) return;

    std::vector<std::string> logs;
    bool logsChanged = tsl::logRingBuffer.snapshotFormatted(logs, firstRun);

    if (!logsChanged && !needsRecalc && cachedWrappedLines.empty()) return;

    flush(canvas);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(skcol::text);
    float fontsize = _STATE->textsize2 * 0.8f;
    SkFont font(_STATE->font_normal);
    font.setSize(fontsize);

    const float lineSpacing = fontsize * 0.85f;
    const float maxWidth = static_cast<float>(w) - 20.0f; // 10px margin on each side

    // Store previous max scroll offset to detect new messages
    int previousMaxScrollOffset = maxScrollOffset;
    bool wasAtBottom = (scrollOffset >= maxScrollOffset);

    // Recalculate wrapped lines if logs changed or window resized
    if (logsChanged || needsRecalc) {
        // Only process new logs if user hasn't scrolled or if they're at the bottom
        if (!userHasScrolled || wasAtBottom) {
            cachedWrappedLines.clear();

            // Flatten + wrap all logs into visual lines
            for (const auto& log : logs) {
                size_t start = 0;
                while (start < log.size()) {
                    size_t len = 1;
                    while (start + len <= log.size()) {
                        auto substr = log.substr(start, len);
                        if (font.measureText(substr.c_str(), substr.size(), SkTextEncoding::kUTF8) > maxWidth)
                            break;
                        len++;
                    }
                    if (len > 1) len--; // step back to last fitting char
                    cachedWrappedLines.push_back(log.substr(start, len));
                    start += len;
                }
            }

            // Update max scroll offset
            int maxLines = static_cast<int>(h / lineSpacing);
            maxScrollOffset = std::max(0, static_cast<int>(cachedWrappedLines.size()) - maxLines);

            // Auto-scroll to bottom when new messages arrive and user is at bottom
            if (logsChanged && autoScrollToBottom && (!userHasScrolled || wasAtBottom)) {
                scrollOffset = maxScrollOffset;
                userHasScrolled = false; // Reset user scroll flag when auto-scrolling
            }
            else {
                // Clamp current scroll offset if window was resized
                scrollOffset = std::min(scrollOffset, maxScrollOffset);
                scrollOffset = std::max(0, scrollOffset);
            }
        }
        else if (needsRecalc) {
            // Only recalculate on window resize, not on new messages when scrolled
            cachedWrappedLines.clear();

            // Flatten + wrap all logs into visual lines
            for (const auto& log : logs) {
                size_t start = 0;
                while (start < log.size()) {
                    size_t len = 1;
                    while (start + len <= log.size()) {
                        auto substr = log.substr(start, len);
                        if (font.measureText(substr.c_str(), substr.size(), SkTextEncoding::kUTF8) > maxWidth)
                            break;
                        len++;
                    }
                    if (len > 1) len--; // step back to last fitting char
                    cachedWrappedLines.push_back(log.substr(start, len));
                    start += len;
                }
            }

            // Update max scroll offset
            int maxLines = static_cast<int>(h / lineSpacing);
            maxScrollOffset = std::max(0, static_cast<int>(cachedWrappedLines.size()) - maxLines);

            // Clamp current scroll offset
            scrollOffset = std::min(scrollOffset, maxScrollOffset);
            scrollOffset = std::max(0, scrollOffset);
        }

        needsRecalc = false;
    }

    // Calculate visible lines
    int maxLines = static_cast<int>(h / lineSpacing);
    int startLine = scrollOffset;
    int endLine = std::min(startLine + maxLines, static_cast<int>(cachedWrappedLines.size()));

    // Render visible lines
    float yPos = lineSpacing;
    for (int i = startLine; i < endLine; ++i) {
        canvas->drawString(cachedWrappedLines[i].c_str(), 10, yPos, font, paint);
        yPos += lineSpacing;
    }

    // Draw scroll indicator if there are more lines than can fit
    if (maxScrollOffset > 0) {
        drawScrollIndicator(canvas, w, h, lineSpacing);
    }

    // Draw "scroll to bottom" button if user has scrolled up and there are new messages
    if (userHasScrolled && scrollOffset < maxScrollOffset) {
        drawScrollToBottomButton(canvas, w, h);
    }
}
void tsl::graphics::LogView::callback(const InputEvent& event) {
    // Gesture state lives in the class members (lastY, isDragging, hasDragged,
    // dragThreshold) -- function-statics here used to shadow them.

    if (event.action == ACTION_DOWN) {
        lastY = event.y;
        isDragging = true;
        hasDragged = false;

        // Check if user tapped the scroll-to-bottom button
        if (userHasScrolled && scrollOffset < maxScrollOffset) {
            auto w = _appState->windowWidth, h = _appState->windowHeight;
            const float buttonSize = 40.0f;
            const float buttonX = w - buttonSize - 20.0f;
            const float buttonY = h - buttonSize - 20.0f;

            if (event.x >= buttonX && event.x <= buttonX + buttonSize &&
                event.y >= buttonY && event.y <= buttonY + buttonSize) {
                buttonTouched = true;
                      }
        }
    }
    else if (isDragging && event.action == ACTION_MOVE) {
        float deltaY = event.y - lastY;
        float totalDelta = std::abs(event.y - lastY);

        // Only start scrolling if we've moved enough to indicate intent to scroll
        if (totalDelta > dragThreshold || hasDragged) {
            hasDragged = true;
            float fontsize = _STATE->textsize2 * 0.8f;
            const float lineSpacing = fontsize * 0.85f;

            // Convert pixel movement to line movement
            int linesDelta = static_cast<int>(-deltaY / lineSpacing);

            if (linesDelta != 0) {
                int oldScrollOffset = scrollOffset;
                scrollOffset += linesDelta;
                scrollOffset = std::max(0, std::min(scrollOffset, maxScrollOffset));

                // Mark that user has manually scrolled if they moved away from bottom
                if (scrollOffset != oldScrollOffset) {
                    userHasScrolled = true;
                    // If user scrolled to the very bottom, reset the flag
                    if (scrollOffset >= maxScrollOffset) {
                        userHasScrolled = false;
                    }
                }

                lastY = event.y;

                // Trigger redraw
                // You may need to call your redraw function here
                // redraw(); // Uncomment if you have a redraw function
            }
        }
    }
    else if (event.action == ACTION_UP) {
        // Exit the log view only if user didn't drag (i.e., it was a tap)
        if (!hasDragged && !buttonTouched) {
            delCB();
            deldraw();
            return;
        }
        if (buttonTouched) {
                scrollOffset = maxScrollOffset;
                userHasScrolled = false;
                needsRecalc = true; // Force recalculation to show latest messages
                buttonTouched = false;
        }
        isDragging = false;
        hasDragged = false;
    }
    if (event.action == ACTION_MOUSE_WHEEL) { // Assuming you have scroll wheel support
        int linesDelta = event.pointer_id > 0 ? -3 : 3; // 3 lines per scroll
        int oldScrollOffset = scrollOffset;
        scrollOffset += linesDelta;
        scrollOffset = std::max(0, std::min(scrollOffset, maxScrollOffset));

        // Mark that user has manually scrolled if they moved away from bottom
        if (scrollOffset != oldScrollOffset) {
            userHasScrolled = true;
            // If user scrolled to the very bottom, reset the flag
            if (scrollOffset >= maxScrollOffset) {
                userHasScrolled = false;
            }
        }
    }
}

void tsl::graphics::LogView::drawScrollIndicator(SkCanvas* canvas, int w, int h, float lineSpacing) {
    if (cachedWrappedLines.empty() || maxScrollOffset <= 0) return;

    SkPaint scrollPaint;
    scrollPaint.setAntiAlias(true);
    scrollPaint.setColor(0x80FFFFFF); // Semi-transparent white

    // Draw scrollbar background
    const float scrollbarWidth = 4.0f;
    const float scrollbarX = w - scrollbarWidth - 5.0f;
    const float scrollbarHeight = h - 20.0f; // Leave some margin
    const float scrollbarY = 10.0f;

    SkRect scrollbarBg = SkRect::MakeXYWH(scrollbarX, scrollbarY, scrollbarWidth, scrollbarHeight);
    scrollPaint.setColor(0x40FFFFFF); // More transparent for background
    canvas->drawRect(scrollbarBg, scrollPaint);

    // Draw scroll thumb
    float totalLines = static_cast<float>(cachedWrappedLines.size());
    float visibleLines = h / lineSpacing;
    float thumbHeight = (visibleLines / totalLines) * scrollbarHeight;
    float thumbY = scrollbarY + (scrollOffset / totalLines) * scrollbarHeight;

    SkRect scrollThumb = SkRect::MakeXYWH(scrollbarX, thumbY, scrollbarWidth, thumbHeight);
    scrollPaint.setColor(0x80FFFFFF); // Less transparent for thumb
    canvas->drawRect(scrollThumb, scrollPaint);
}

void tsl::graphics::LogView::drawScrollToBottomButton(SkCanvas* canvas, int w, int h) {
    const float buttonSize = 40.0f;
    const float buttonX = w - buttonSize - 20.0f;
    const float buttonY = h - buttonSize - 20.0f;

    SkPaint buttonPaint;
    buttonPaint.setAntiAlias(true);

    // Draw button background (semi-transparent circle)
    buttonPaint.setColor(0x80000000); // Semi-transparent black
    canvas->drawCircle(buttonX + buttonSize / 2, buttonY + buttonSize / 2, buttonSize / 2, buttonPaint);

    // Draw button border
    buttonPaint.setStyle(SkPaint::kStroke_Style);
    buttonPaint.setStrokeWidth(2.0f);
    buttonPaint.setColor(0xFFFFFFFF); // White border
    canvas->drawCircle(buttonX + buttonSize / 2, buttonY + buttonSize / 2, buttonSize / 2 - 1, buttonPaint);

    // Draw down arrow
    buttonPaint.setStyle(SkPaint::kFill_Style);
    buttonPaint.setColor(0xFFFFFFFF); // White arrow

    float centerX = buttonX + buttonSize / 2;
    float centerY = buttonY + buttonSize / 2;
    float arrowSize = 8.0f;

    SkPath arrowPath;
    arrowPath.moveTo(centerX - arrowSize, centerY - arrowSize / 2);
    arrowPath.lineTo(centerX + arrowSize, centerY - arrowSize / 2);
    arrowPath.lineTo(centerX, centerY + arrowSize / 2);
    arrowPath.close();

    canvas->drawPath(arrowPath, buttonPaint);
}

void tsl::graphics::LogView::init() {
	startx = 0; starty = 0;
	stopx = width = _appState->windowWidth;
	stopy = height = _appState->windowHeight;
    needsRecalc = true;
}
