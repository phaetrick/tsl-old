#include "presetbrowser.h"
#include "app.h"
#include "grainstorm.h"
#include "preset.h"
#include "sequencer.h"
#include "gui.h"
#include <cmath>
#include <cstring>
#include <memory>

using namespace tsl::graphics;

// One browser per app, owned here. Every access is on the UI thread (activate()
// runs in UiTasksQueue, input and render are UI-side), so a plain shared_ptr is
// enough — this is the same "one view per app" argument Settings2's static owner
// slot rests on.
static std::shared_ptr<PresetBrowserView> gBrowser;

static constexpr const char* kResetLabel = "RESET SYNTH";

// Cuts a name down to `maxw` and marks the cut with a trailing "..".
//
// ".." and not "…" because that is what PresetSelector already puts on the same
// names in the same font — one convention for elided text, not two. UTF-8 safe:
// the loop advances whole code points, so a multi-byte character is never cut in
// half (a half-cut sequence draws as a replacement box, and user preset names
// come from filenames, which can be anything).
static std::string ellipsize(SkFont& font, const std::string& s, float maxw) {
    if (s.empty() || maxw <= 0.f)
        return s;
    auto w = [&](size_t n) {
        return font.measureText(s.data(), n, SkTextEncoding::kUTF8);
    };
    if (w(s.size()) <= maxw)
        return s;
    const float ew = font.measureText("..", 2, SkTextEncoding::kUTF8);
    size_t cut = 0;
    for (size_t i = 0; i < s.size();) {
        size_t next = i + 1;
        while (next < s.size() && ((unsigned char)s[next] & 0xC0) == 0x80)
            next++;
        if (w(next) + ew > maxw)
            break;
        cut = next;
        i = next;
    }
    return s.substr(0, cut) + "..";
}

PresetBrowserView::PresetBrowserView(tsl::AppState* appState)
    : View(appState, WRAP, 0, CENTER_ALIGN, 0), ScrollViewBase(_appState) {
    prio = 0;
    name = "PresetBrowser";
}

void PresetBrowserView::rebuild() {
    rows.clear();
    const auto& ps = _DATA->presets;
    for (int i = 0; i < (int)ps.size(); i++) {
        if (ps[i].category != activeCat) continue;
        // Same gate as the LOAD PRESET selector: with the factory bank hidden the
        // browser must not offer it either, or the two disagree about what exists.
        if (ps[i].isSystem && ps[i].date != LONG_MAX &&
            !_DATA->showFactoryPresets.load(std::memory_order_relaxed))
            continue;
        rows.push_back(Row{ps[i].name, std::string(), i, ps[i].uid});
    }
}

void PresetBrowserView::clampOffset() {
    float off = offset.load();
    if (off > 0.f) off = 0.f;
    else if (off < (float)maxoffset) off = (float)maxoffset;
    offset.store(off);
}

// Row under a y position in view coordinates, or -1 outside the list.
int PresetBrowserView::rowAt(float ypos) const {
    const float rel = ypos - boxoffsety - offset.load();
    if (rel < 0.f) return -1;
    const int r = (int)(rel / (float)itemheight);
    return r < (int)rows.size() ? r : -1;
}

// Category under a y position, or -1 outside the column. Deliberately NOT clamped
// to the last entry: with a 16-row box the column ends well above the bottom of
// the panel, and clamping would drop presets into USER from empty space.
int PresetBrowserView::catAt(float ypos) const {
    const float rel = ypos - boxoffsety;
    if (rel < 0.f) return -1;
    const int i = (int)(rel / catItemH);
    return i < kCatRows ? kCatFirst + i : -1;
}

void PresetBrowserView::init() { computeSize(); }

void PresetBrowserView::computeSize() {
    startx = starty = 0;
    width = stopx = _STATE->windowWidth;
    height = stopy = _STATE->windowHeight;

    const float textsize = _STATE->textsize2;
    itemheight = _STATE->textsize1;
    barH = _STATE->textsize1;

    SkFont font(_STATE->font_normal);
    font.setSize(textsize * .9f);

    // Category column: the widest category name plus breathing room. Measured with
    // the font it is DRAWN with — settings2's section-title face (textsize2 * .8,
    // embolden), so the two dialogs label their sections identically.
    SkFont catFont(_STATE->font_normal);
    catFont.setSize(textsize * .8f);
    catFont.setEmbolden(true);
    catWidth = 0;
    for (int i = 0; i < kCatRows; i++) {
        SkRect b{};
        const char* s = Preset::categoryNames[kCatFirst + i];
        catFont.measureText(s, strlen(s), SkTextEncoding::kUTF8, &b);
        catWidth = std::max(catWidth, b.width());
    }
    catWidth += textsize * 1.6f;

    // RESET SYNTH sits in the bar opposite the close button; its label width is
    // the hit zone, and the bar's full height is the target.
    {
        SkRect b{};
        font.measureText(kResetLabel, strlen(kResetLabel), SkTextEncoding::kUTF8, &b);
        resetW = b.width() + textsize;
    }

    boxoffsetx = textsize * .5f + lw2;
    boxoffsety = boxoffsetx + barH;

    drawwidth = width * (parent == nullptr ? .92f : 1.f);
    boxwidth = drawwidth - 2 * boxoffsetx;
    listX = catWidth;

    drawstartx = startx + (width - drawwidth) * .5f;
    drawstopx = drawstartx + drawwidth;

    // FIXED SIZE, deliberately: the box does not grow or shrink with the row
    // count. It used to, which meant every category switch resized and recentred
    // the dialog under the finger — and a category that happened to fit could
    // never be scrolled, because there was nothing left to scroll. Size is a
    // property of the window now, and the content scrolls inside it.
    //
    // The row budget rather than a flat fraction of the height: this app derives
    // every text size from the window WIDTH (textsize1 = width * 0.05), so on a
    // phone in portrait 90% of the height is roughly 37 rows — a full-screen box
    // two thirds empty for every category in the bank. Sixteen is comfortably
    // more than the biggest one (13) and, being > CAT_COUNT, it also guarantees
    // the category column fits at full row height.
    // (see kCatRows: 16 > the biggest category and > the number of columns)
    static constexpr int kVisibleRows = 16;
    drawheight = std::min(height * .9f,
                          (float)(boxoffsety + boxoffsetx + itemheight * kVisibleRows));
    boxheight = drawheight - boxoffsety - boxoffsetx;
    if (boxheight < itemheight) {   // absurdly short window; keep the box drawable
        boxheight = itemheight;
        drawheight = boxheight + boxoffsety + boxoffsetx;
    }
    drawstarty = starty + (height - drawheight) * .5f;
    drawstopy = drawstarty + drawheight;

    const float contentH = (float)itemheight * (float)rows.size();
    maxoffset = contentH > (float)boxheight ? -(int)(contentH - (float)boxheight) : 0;
    // Put the list back where this category was left, in rows — so reopening the
    // browser, switching back to a category, or resizing the window all land on
    // the same preset rather than the same pixel.
    offset.store(-catTop[activeCat] * (float)itemheight);
    clampOffset();

    // All ten categories share the fixed box, so they get whatever height fits
    // rather than the list's row height — on a short window (landscape phone,
    // a squat plugin window) the bottom of the column would otherwise be cut off
    // and USER unreachable.
    catItemH = std::min((float)itemheight, (float)boxheight / (float)kCatRows);
    // ...and the category text shrinks with it, in the same proportion the list's
    // text has to its row, so a squat window packs the column instead of piling
    // ten full-size names on top of each other.
    catTextSize = textsize * .8f * std::min(1.f, catItemH / (float)itemheight);

    listW = drawwidth - listX - 2 * boxoffsetx;
    gripW = (float)itemheight;
    gripX = listX + boxoffsetx + listW - gripW;
    nameW = listW - gripW - textsize * .25f;

    // overscroll glow spans the list, not the whole panel — the category column
    // does not scroll, so a glow across it would be claiming something it isn't
    glowStartX = listX;
    glowWidth = drawwidth - listX;

    for (auto& r : rows)
        r.display = ellipsize(font, r.name, nameW);
}

void PresetBrowserView::render(void* ctx) {
    // Belt as well as braces. init() covers the Android path, whose resize
    // handler re-inits every window owner; the desktop one (tsl::app::resize)
    // re-lays-out the root tree only and never reaches views like this, so a
    // host resizing the editor would otherwise leave the browser at the old
    // geometry until it was closed and reopened.
    if (width != _STATE->windowWidth || height != _STATE->windowHeight)
        computeSize();
    auto canvas = _STATE->graphics.getCanvas(windowindex, drawstartx, drawstarty,
                                             drawwidth, drawheight);
    if (canvas == nullptr)
        return;
    SkPaint paint;
    canvas->clear(tsl::sk_colours::bg);
    paint.setAntiAlias(true);
    paint.setColor(tsl::sk_colours::fg);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(1.0);
    canvas->drawRect(SkRect::MakeXYWH(0.5, 0.5, drawwidth - 1, drawheight - 1), paint);
    paint.setStrokeWidth(lw);

    SkFont& font = _STATE->font_normal;
    const float textsize = _STATE->textsize2;
    font.setSize(textsize * .9f);
    paint.setStyle(SkPaint::kFill_Style);

    // ── title bar: Settings2::drawBar, measure for measure — centred embolden
    //    title, a barH square close button that fills blue while pressed, X inset
    //    by .30 with round caps, and a hairline under the whole bar ──
    {
        font.setEmbolden(true);
        SkRect b{};
        font.measureText("PRESETS", 7, SkTextEncoding::kUTF8, &b);
        canvas->drawSimpleText("PRESETS", 7, SkTextEncoding::kUTF8,
                               drawwidth * .5f - b.centerX(),
                               barH * .5f - b.centerY(), font, paint);
        font.setEmbolden(false);

        // RESET SYNTH: loads Default, which is why INIT is no longer a column.
        // Not embolden — the bar's bold face is the title, and an action that
        // reads as a heading invites being read as one.
        {
            const SkRect rbtn = SkRect::MakeXYWH(0, 0, resetW, barH);
            if (resetHot) {
                paint.setStyle(SkPaint::kFill_Style);
                paint.setColor(skcol::blue_transparent);
                canvas->drawRect(rbtn, paint);
            }
            paint.setStyle(SkPaint::kFill_Style);
            paint.setColor(skcol::fg);
            SkRect rb{};
            font.measureText(kResetLabel, strlen(kResetLabel), SkTextEncoding::kUTF8, &rb);
            canvas->drawSimpleText(kResetLabel, strlen(kResetLabel), SkTextEncoding::kUTF8,
                                   resetW * .5f - rb.centerX(), barH * .5f - rb.centerY(),
                                   font, paint);
            // Divider after it: without one the label reads as part of the bar
            // rather than as something to press. Same hairline as the other two,
            // spanning the bar from the frame's top edge to the bar divider.
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setStrokeWidth(1.0f);
            const float rx = std::floor(resetW) + .5f;
            canvas->drawLine(rx, .5f, rx, std::floor(barH) + .5f, paint);
            paint.setStrokeWidth(lw);
            paint.setStyle(SkPaint::kFill_Style);
        }

        const SkRect btn = SkRect::MakeXYWH(drawwidth - barH, 0, barH, barH);
        if (closeHot) {
            paint.setStyle(SkPaint::kFill_Style);
            paint.setColor(skcol::blue_transparent);
            canvas->drawRect(btn, paint);
        }
        SkRect xArea = btn;
        xArea.inset(barH * .30f, barH * .30f);
        paint.setColor(skcol::fg);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(lw);
        paint.setStrokeCap(SkPaint::kRound_Cap);
        canvas->drawLine(xArea.fLeft, xArea.fTop, xArea.fRight, xArea.fBottom, paint);
        canvas->drawLine(xArea.fRight, xArea.fTop, xArea.fLeft, xArea.fBottom, paint);
        paint.setStrokeCap(SkPaint::kButt_Cap);
        paint.setStrokeWidth(1.0f);
        // Snapped to a half pixel, edge to edge. barH is a whole number, and a
        // 1px antialiased line centred ON a pixel boundary is split across two
        // rows at half coverage each — which is why this read as a paler grey
        // than the frame around it, drawn at the same width and colour but on
        // the .5 offsets. Same reason the vertical divider below is snapped.
        canvas->drawLine(0, std::floor(barH) + .5f, drawwidth, std::floor(barH) + .5f, paint);
        paint.setStrokeWidth(lw);
        paint.setStyle(SkPaint::kFill_Style);
    }

    // ── the divider between categories and list ──
    // Drawn here, outside the content clip, because it has to run the WHOLE height
    // of the panel: from the centre of the bar divider to the centre of the frame's
    // bottom edge, so the three lines meet instead of leaving a stub at each end.
    // Same hairline treatment as the bar divider — it used to be `lw` wide (3px on
    // mobile), which read as a scrollbar track that never goes away.
    {
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(1.0f);
        const float x = std::floor(listX) + .5f;
        canvas->drawLine(x, std::floor(barH) + .5f, x, drawheight - .5f, paint);
        paint.setStrokeWidth(lw);
        paint.setStyle(SkPaint::kFill_Style);
    }

    canvas->save();
    SkPath clip;
    clip.addRect(SkRect::MakeXYWH(lw, boxoffsety - lw, drawwidth - 2 * lw, boxheight + 2 * lw));
    canvas->clipPath(clip);

#ifdef PLATFORM_MOBILE
    auto& sc = scroller[currentScroller.load()];
    const auto elapsed = !sc.isFinished() ? timeLastAction.elapsedReplace()
                                          : timeLastAction.elapsed();
    if (sc.computeScrollOffset())
        offset = sc.getCurrY();
#else
    const auto elapsed = timeLastAction.elapsed();
#endif

    auto d = drag.load();
    // Edge auto-scroll while reordering. It lives here and not in the move
    // handler because a finger parked against the edge stops producing move
    // events, and that is exactly when the list has to keep coming.
    if (mode == MOVING && d.row >= 0 && maxoffset < 0) {
        const float edge = (float)itemheight;
        float off = offset.load();
        if (d.y < boxoffsety + edge) off += (float)itemheight * .25f;
        else if (d.y > boxoffsety + boxheight - edge) off -= (float)itemheight * .25f;
        offset.store(off);
        clampOffset();
    }
    const float off = offset.load();
    if (itemheight > 0)
        catTop[activeCat] = -off / (float)itemheight;

    // ── category column (never scrolls: it is the frame, not the content) ──
    // A row being carried over the column is a recategorise; the target lights up.
    const int dropCat = (d.row >= 0 && d.x < listX) ? catAt(d.y) : -1;
    font.setSize(catTextSize);
    font.setEmbolden(true);
    for (int i = 0; i < kCatRows; i++) {
        const int c = kCatFirst + i;
        const float y = boxoffsety + i * catItemH;
        if (c == dropCat || c == pressCat) {
            paint.setColor(skcol::blue_transparent);
            canvas->drawRect(SkRect::MakeXYWH(0, y, listX, catItemH), paint);
        }
        // fg, not grey: same weight as the preset names, as settings2 draws its
        // section titles. active marks the one being shown.
        paint.setColor(c == activeCat ? skcol::active : skcol::fg);
        float tx, ty;
        centerText(font, catWidth, catItemH, Preset::categoryNames[c], tx, ty);
        canvas->drawSimpleText(Preset::categoryNames[c], strlen(Preset::categoryNames[c]),
                               SkTextEncoding::kUTF8, boxoffsetx, y + ty, font, paint);
    }
    font.setEmbolden(false);
    font.setSize(textsize * .9f);
    paint.setColor(skcol::fg);

    // ── drop indicator position ──
    int drawPos = -1000;
    if (d.row >= 0 && d.x >= listX && !rows.empty()) {
        float ppos = (d.y - boxoffsety - off - itemheight * .5f) / itemheight;
        if (ppos < 0) drawPos = -1;
        else if (ppos >= (float)rows.size()) drawPos = (int)rows.size() - 1;
        else drawPos = (int)floor(ppos);
    }

    // ── the list ──
    if (rows.empty()) {
        // Reachable with the factory bank switched off, where every factory
        // category is empty — an unexplained blank panel reads as a broken view.
        paint.setColor(skcol::lighter_grey);
        float tx, ty;
        centerText(font, nameW, itemheight, "(empty)", tx, ty);
        canvas->drawSimpleText("(empty)", 7, SkTextEncoding::kUTF8,
                               listX + boxoffsetx, boxoffsety + ty, font, paint);
    }
    const int pressed = (mode == INSIDE) ? pressRow : -1;
    float sy = boxoffsety + off;
    for (int r = 0; r < (int)rows.size(); r++) {
        if (sy >= boxoffsety - itemheight && sy - itemheight <= boxoffsety + boxheight) {
            canvas->save();
            canvas->translate(listX + boxoffsetx, sy);
            if (r == pressed) {
                // full width of the list column, divider to frame — the text is
                // inset by boxoffsetx but the highlight is the row, not the text
                paint.setColor(skcol::blue_transparent);
                canvas->drawRect(SkRect::MakeXYWH(-boxoffsetx, 0,
                                                  listW + 2 * boxoffsetx, itemheight), paint);
            }
            const bool isCurrent = _DATA->presets[rows[r].index].date == _DATA->presetDate;
            // the row being carried greys out where it came from
            paint.setColor(r == d.row ? skcol::lighter_grey
                                      : (isCurrent ? skcol::active : skcol::fg));
            float tx, ty;
            centerText(font, nameW, itemheight, rows[r].display.c_str(), tx, ty);
            canvas->drawSimpleText(rows[r].display.c_str(), rows[r].display.size(),
                                   SkTextEncoding::kUTF8, 0, ty, font, paint);
            // grip: the handle a reorder drag must start on
            {
                paint.setColor(r == d.row ? skcol::active : skcol::lighter_grey);
                const float gx = listW - gripW * .5f, gw = gripW * .22f;
                const float gy = itemheight * .5f, gs = itemheight * .16f;
                paint.setStrokeWidth(lw);
                paint.setStyle(SkPaint::kStroke_Style);
                for (int i = -1; i <= 1; i++)
                    canvas->drawLine(gx - gw, gy + i * gs, gx + gw, gy + i * gs, paint);
                paint.setStyle(SkPaint::kFill_Style);
            }
            if (d.row >= 0) {
                const float l = -boxoffsetx, rgt = listW + boxoffsetx;
                if (r == 0 && drawPos == -1 && d.row != 0) {
                    paint.setColor(skcol::blue);
                    canvas->drawLine(l, 1, rgt, 1, paint);
                } else if (r == drawPos && d.row != r && d.row != r + 1) {
                    paint.setColor(skcol::blue);
                    canvas->drawLine(l, itemheight, rgt, itemheight, paint);
                }
            }
            canvas->restore();
        }
        sy += itemheight;
    }

    // end-of-list glow (ScrollViewBase::updateOverscroll — it both draws and decays,
    // 0.96 per frame, which is why it has to run every frame `perm` gives us)
    if (maxoffset < 0)
        updateOverscroll(canvas);

    // scrollbar
    const float maxoff = std::abs((float)maxoffset);
    if (maxoff > 0 && elapsed < 2.0) {
        float alpha = elapsed > 1.0 ? 255.f - 255.f * (float)(elapsed - 1.) : 255.f;
        paint.setColor(SkColorSetA(skcol::fg, (U8CPU)alpha));
        const float length = boxheight / (1.f + maxoff / boxheight);
        const float pos = (1.f - DISTANCEF(maxoffset, off) / maxoff) * (boxheight - length);
        canvas->drawRect(SkRect::MakeXYWH(drawwidth - 3 * lw, boxoffsety + pos, 2 * lw, length), paint);
    }

    // ── the row being carried, drawn last so it rides above everything ──
    //
    // Over the list it keeps full width and its x, because it has to line up with
    // the drop line. Over the category column it shrinks to a name-sized chip
    // CENTRED ON THE POINTER: parked in the list it looked like nothing was
    // happening, and a full-width row would bury the column it is being dropped on.
    // Filled rather than outlined either way — it is a card you are holding, and
    // it has to cover what it passes over.
    if (d.row >= 0 && d.row < (int)rows.size()) {
        const std::string& nm = rows[d.row].display;
        const bool overCats = d.x < listX;
        float fw = listW, fx = listX + boxoffsetx, tpad = 0.f;
        if (overCats) {
            tpad = itemheight * .5f;
            fw = font.measureText(nm.c_str(), nm.size(), SkTextEncoding::kUTF8) + 2 * tpad;
            fx = d.x - fw * .5f;
            if (fx < lw) fx = lw;
            else if (fx + fw > drawwidth - lw) fx = drawwidth - lw - fw;
        }
        const float fy = d.y - itemheight * .5f;
        const SkRect card = SkRect::MakeXYWH(fx, fy, fw, itemheight);
        paint.setColor(skcol::bg);
        canvas->drawRect(card, paint);
        paint.setColor(skcol::fg);
        paint.setStyle(SkPaint::kStroke_Style);
        paint.setStrokeWidth(lw);
        canvas->drawRect(card, paint);
        paint.setStyle(SkPaint::kFill_Style);
        float tx, ty;
        centerText(font, fw, itemheight, nm.c_str(), tx, ty);
        canvas->drawSimpleText(nm.c_str(), nm.size(), SkTextEncoding::kUTF8,
                               fx + tpad, fy + ty, font, paint);
    }
    canvas->restore();

    // Stay in the draw queue while anything is still moving: the fling, the
    // edge auto-scroll and the scrollbar fade all animate without further input.
    perm = !(elapsed > 2.0 && mode != MOVING);
}

// Rewrites `order` for the active category so the dragged preset sits at `to`,
// then re-sorts, persists and refreshes the selector. The deque is only
// REORDERED here (never resized), matching what savePresetToFile already does.
void PresetBrowserView::commitDrop(int from, int to) {
    if (from == to || from < 0 || from >= (int)rows.size()) return;
    std::vector<int> idx;
    for (auto& r : rows) idx.push_back(r.index);
    const int moved = idx[from];
    idx.erase(idx.begin() + from);
    if (to > (int)idx.size()) to = (int)idx.size();
    idx.insert(idx.begin() + to, moved);
    for (int i = 0; i < (int)idx.size(); i++)
        _DATA->presets[idx[i]].order = i;

    Preset::sortAll(_appState);
    Preset::saveOrder(_appState);
    rebuild();
    computeSize();
    redraw();

    auto* sel = _DATA->views.presetSelector;
    if (sel != nullptr) { sel->addRecursiveDraw(); sel->redraw(); }
}

// Recategorises the dragged preset and parks it at the END of the target, then
// re-sorts (which also compacts the hole it left behind), persists and refreshes.
// The view stays where it is rather than following the preset across: the user is
// filing, and yanking them into another category mid-tidy loses their place. The
// toast is the receipt.
void PresetBrowserView::moveToCategory(int row, int cat) {
    if (row < 0 || row >= (int)rows.size()) return;
    if (cat < 0 || cat >= Preset::CAT_COUNT) return;
    const int index = rows[row].index;
    if (_DATA->presets[index].category == cat) return;

    int last = -1;
    for (const auto& p : _DATA->presets)
        if (p.category == cat) last = std::max(last, p.order);
    _DATA->presets[index].category = cat;
    _DATA->presets[index].order = last + 1;

    Preset::sortAll(_appState);
    Preset::saveOrder(_appState);
    rebuild();
    computeSize();
    redraw();

    auto* sel = _DATA->views.presetSelector;
    if (sel != nullptr) { sel->addRecursiveDraw(); sel->redraw(); }

    char text[64];
    snprintf(text, sizeof(text), "Moved to %s", Preset::categoryNames[cat]);
    showToast(_STATE, text);
}

// Loads Default. Default is the only member of CAT_INIT and has no column of its
// own any more, so this button is the whole of its UI. Found by its LONG_MAX date
// rather than by index: that is the marker the rest of the file already treats as
// "this is Default", and it survives any reordering.
//
// Leaves the browser open, because tapping a preset row does — one rule for
// loading a sound, whichever control started it. The green `active` name going
// plain (nothing in the list is current any more) is the visible half of the
// feedback; the synth resetting under you is the rest.
void PresetBrowserView::resetSynth() {
    int index = -1;
    for (int i = 0; i < (int)_DATA->presets.size(); i++)
        if (_DATA->presets[i].date == LONG_MAX) { index = i; break; }
    if (index < 0) return;
    auto* _appState = this->_appState;
    if (!_DATA->toAudioThreadQueue.push([_appState, index]() {
            sequencer::loadPreset(_appState, index);
        })) {
        showToast(_STATE, "Too many tasks");
        return;
    }
    redraw();
    auto* sel = _DATA->views.presetSelector;
    if (sel != nullptr) { sel->addRecursiveDraw(); sel->redraw(); }
}

void PresetBrowserView::loadRow(int row) {
    if (row < 0 || row >= (int)rows.size()) return;
    auto* _appState = this->_appState;
    const int index = rows[row].index;
    if (!_DATA->toAudioThreadQueue.push([_appState, index]() {
            sequencer::loadPreset(_appState, index);
        })) {
        showToast(_STATE, "Too many tasks");
        return;
    }
    redraw();
    auto* sel = _DATA->views.presetSelector;
    if (sel != nullptr) { sel->addRecursiveDraw(); sel->redraw(); }
}

void PresetBrowserView::callback(const InputEvent& event) {
    const int32_t action = event.action;
    const int32_t _pointerid = event.pointer_id;
    const float xpos = event.x - drawstartx;
    const float ypos = event.y - drawstarty;
#ifdef PLATFORM_MOBILE
    velocityTracker.addMovement(event);
#endif
    switch (action) {
    case ACTION_DOWN: {
        pressRow = -1;
        pressGrip = false;
        totalmoved = 0;
        lastXpos = xpos;
        lastYpos = ypos;
        pointerid = _pointerid;
        timeLastAction.reset();
        overscrollTop.store(0.f);
        overscrollBottom.store(0.f);
#ifdef PLATFORM_MOBILE
        scroller[currentScroller.load()].forceFinished(true);
#endif
        if (ypos >= 0 && ypos < barH && xpos >= 0 && xpos < resetW) {  // RESET SYNTH
            resetHot = true;
            mode = UNTOUCHED;
            redraw();
            return;
        }
        if (ypos >= 0 && ypos < barH && xpos > drawwidth - barH) {   // the close X
            // Pressed now, closed on the way up — settings2's close button
            // behaves this way and shows the same blue fill while held.
            closeHot = true;
            mode = UNTOUCHED;
            redraw();
            return;
        }
        if (xpos >= 0 && xpos < listX && ypos >= boxoffsety &&
            ypos < boxoffsety + boxheight) {                   // category column
            // Held now, switched on the way up, like every other press in this
            // view — that is what gives it a hot state to draw.
            pressCat = catAt(ypos);
            // UNTOUCHED and not OUTSIDE: OUTSIDE closes the browser on the way up.
            mode = UNTOUCHED;
            redraw();
            return;
        }
        if (xpos >= listX && xpos <= drawwidth && ypos >= boxoffsety &&
            ypos < boxoffsety + boxheight) {                   // the list
            mode = INSIDE;
            pressRow = rowAt(ypos);
            pressGrip = pressRow >= 0 && xpos >= gripX;
            redraw();
        } else if (xpos < 0 || xpos > drawwidth || ypos < 0 || ypos > drawheight) {
            mode = OUTSIDE;                                    // tap-away closes
        } else {
            mode = UNTOUCHED;
        }
        break;
    }
    case ACTION_MOVE: {
        if (pointerid != _pointerid) break;
        if (mode == MOVING) {
            auto d = drag.load();
            if (d.row >= 0) {                                  // reordering
                d.x = xpos;
                d.y = ypos < boxoffsety ? boxoffsety
                    : (ypos > boxoffsety + boxheight ? boxoffsety + boxheight : ypos);
                drag.store(d);
            } else {                                           // scrolling
                // RecyclerView's rubber band: the offset still stops dead at the
                // end, but the overshoot is banked as glow so the list says so.
                float off = offset.load() - (lastYpos - ypos);
                if (off < (float)maxoffset) {
                    overscrollBottom.store(std::max(overscrollBottom.load(),
                        std::min(((float)maxoffset - off) * .5f, maxOverscroll)));
                    off = (float)maxoffset;
                } else if (off > 0.f) {
                    overscrollTop.store(std::max(overscrollTop.load(),
                        std::min(off * .5f, maxOverscroll)));
                    off = 0.f;
                }
                offset.store(off);
            }
            lastXpos = xpos;
            lastYpos = ypos;
            timeLastAction.reset();
            redraw();
        } else if (closeHot || resetHot || pressCat >= 0) {
            if (spacing(xpos, lastXpos, ypos, lastYpos) > _STATE->touchSlop()) {
                closeHot = resetHot = false;
                pressCat = -1;
                pointerid = -1;
                redraw();
            }
        } else if (mode == INSIDE || mode == OUTSIDE) {
            // lastXpos/lastYpos still hold the down point, so this is displacement
            // from it — the same fix RecyclerView carries; accumulating would add
            // the whole distance again on every move event.
            totalmoved = spacing(xpos, lastXpos, ypos, lastYpos);
            if (totalmoved > _STATE->touchSlop()) {
                if (mode == INSIDE && pressGrip) {
                    lastXpos = xpos;
                    lastYpos = ypos;
                    mode = MOVING;
                    drag.store(Drag(pressRow, xpos, ypos));
                } else if (mode == INSIDE && maxoffset < 0) {
                    lastXpos = xpos;
                    lastYpos = ypos;
                    mode = MOVING;                             // drag.row stays -1
                } else {
                    mode = UNTOUCHED;
                    pointerid = -1;
                }
                pressRow = mode == MOVING ? pressRow : -1;
                timeLastAction.reset();
                redraw();
            }
        }
        break;
    }
    case ACTION_UP: {
        if (pointerid != _pointerid) break;
        if (closeHot || resetHot) {
            const bool doReset = resetHot;
            closeHot = resetHot = false;
            pointerid = -1;
            // resetSynth redraws itself; the close path must NOT, or it would
            // re-queue the view behind the deldraw hideIfOpen has just scheduled.
            if (doReset) resetSynth();          // loads Default, stays open
            else hideIfOpen(_appState);
            return;
        }
        if (pressCat >= 0) {
            if (pressCat != activeCat) {
                activeCat = pressCat;
                rebuild();
                computeSize();   // restores that category's own scroll position
            }
            pressCat = -1;
            pointerid = -1;
            mode = UNTOUCHED;
            redraw();
            return;
        }
        auto d = drag.load();
        if (mode == MOVING && d.row >= 0 && d.x < listX) {
            moveToCategory(d.row, catAt(d.y));   // dropped on the category column
        } else if (mode == MOVING && d.row >= 0 && !rows.empty()) {
            // Same measure the blue line is drawn from: `slot` is the row the line
            // sits UNDER, so the insertion point in the untouched list is slot + 1
            // (slot -1 means "above row 0"). commitDrop erases first and then
            // inserts, so a target below the dragged row shifts down by one.
            float ppos = (ypos - boxoffsety - offset.load() - itemheight * .5f) / itemheight;
            int slot;
            if (ppos < 0) slot = -1;
            else if (ppos >= (float)rows.size()) slot = (int)rows.size() - 1;
            else slot = (int)floor(ppos);
            const int insertAt = slot + 1;
            const int to = insertAt > d.row ? insertAt - 1 : insertAt;
            commitDrop(d.row, to);
        } else if (mode == MOVING) {
#ifdef PLATFORM_MOBILE
            float vx, vy;
            if (maxoffset < 0 &&
                velocityTracker.getVelocity(pointerid, &vx, &vy) &&
                (_STATE->mMinimumFlingVelocity <= 0 ||
                 std::abs(vy) >= _STATE->mMinimumFlingVelocity)) {
                if (_STATE->mMaximumFlingVelocity > 0 &&
                    std::abs(vy) > _STATE->mMaximumFlingVelocity)
                    vy = ISNEG(vy) ? -_STATE->mMaximumFlingVelocity
                                   : _STATE->mMaximumFlingVelocity;
                int cur = currentScroller.load();
                if (++cur >= numScrollers) cur = 0;
                scroller[cur].fling(0, offset.load(), 0, vy, 0, 0, maxoffset, 0);
                currentScroller.store(cur);
            }
#endif
        } else if (mode == INSIDE && pressRow >= 0) {
            loadRow(pressRow);
        } else if (mode == OUTSIDE) {
            hideIfOpen(_appState);
        }
        drag.store(Drag());
        pressRow = -1;
        pressGrip = false;
        pressCat = -1;
        closeHot = resetHot = false;
        mode = UNTOUCHED;
        pointerid = -1;
        timeLastAction.reset();
        redraw();
        break;
    }
    case ACTION_MOUSE_WHEEL: {
        if (maxoffset < 0 && mode == UNTOUCHED) {
            float off = offset.load() + event.pointer_id * (float)itemheight;
            if (off < (float)maxoffset) {
                overscrollBottom.store(std::min(((float)maxoffset - off) * .3f, maxOverscroll));
                off = (float)maxoffset;
            } else if (off > 0.f) {
                overscrollTop.store(std::min(off * .3f, maxOverscroll));
                off = 0.f;
            }
            offset.store(off);
            timeLastAction.reset();
            redraw();
        }
        break;
    }
    case ACTION_KEY_UP: {
        if (event.pointer_id == VKEY_ESCAPE)
            hideIfOpen(_appState);
        break;
    }
    default:
        break;
    }
}

// Gesture state only. Deliberately does NOT touch `offset`: activate() calls this
// on every open, and computeSize() is about to restore the remembered position.
void PresetBrowserView::reset() {
    hasFocus.store(false);
    pointerid = -1;
    mode = UNTOUCHED;
    totalmoved = 0;
    pressRow = -1;
    pressGrip = false;
    pressCat = -1;
    closeHot = resetHot = false;
#ifdef PLATFORM_MOBILE
    velocityTracker.clear();
    scroller[currentScroller.load()].forceFinished(true);
#endif
    drag.store(Drag());
}

void PresetBrowserView::addRecursiveDraw() { timeLastAction.reset(); View::addRecursiveDraw(); }
void PresetBrowserView::delRecursiveDraw() {
    _STATE->graphics.deleteWindow(windowindex);
    View::delRecursiveDraw();
}
void PresetBrowserView::addRecursiveCB() { hasFocus.store(true); View::addRecursiveCB(); }
void PresetBrowserView::delRecursiveCB() {
    hasFocus.store(false);
    pointerid = -1;
    mode = UNTOUCHED;
    totalmoved = 0;
    pressRow = -1;
    pressGrip = false;
    pressCat = -1;
    closeHot = resetHot = false;
#ifdef PLATFORM_MOBILE
    velocityTracker.clear();
#endif
    drag.store(Drag());
    View::delRecursiveCB();
}

void PresetBrowserView::activate(tsl::AppState* appState) {
    auto* _appState = appState;
    _STATE->UiTasksQueue.add_task([_appState]() {
        if (gBrowser == nullptr)
            gBrowser = std::make_shared<PresetBrowserView>(_appState);
        else {
            gBrowser->delCB();
            gBrowser->deldraw();
        }
        // Open on the category the current preset lives in — the browser should
        // come up where the user already is, not always on LEADS.
        for (const auto& p : _DATA->presets)
            if (p.date == _DATA->presetDate) {
                // Default lives in the one category with no column; fall back to
                // the first real one rather than opening on nothing.
                gBrowser->activeCat = std::max(p.category, kCatFirst);
                break;
            }
        gBrowser->reset();
        gBrowser->rebuild();
        gBrowser->computeSize();
        gBrowser->addDraw();
        gBrowser->addCB();
    });
}

void PresetBrowserView::hideIfOpen(tsl::AppState* appState) {
    auto* _appState = appState;
    if (gBrowser == nullptr) return;
    _STATE->UiTasksQueue.add_task([]() {
        if (gBrowser != nullptr) { gBrowser->delCB(); gBrowser->deldraw(); }
    });
}
