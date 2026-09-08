#include "fxOrder.h"
#include "app.h"
#include "track.h"
#include "grainstorm.h"
#include "button.h"
#include "degradation.h"

using namespace tsl::graphics;

void switcheffect(TRACK* track, int32_t space) {
	auto _appState = track->_appState;
	auto active = _STATE->active_track.load();
	auto tindex = track->index;
	if (active == tindex) {
		_DATA->views.active_spaces_buttons[GASMAIN]->setState(NORMAL);
		_DATA->views.active_spaces_array[(int)_STATE->params[track->index][AS].load()]->deldraw();
		_DATA->views.active_spaces_array[(int)_STATE->params[track->index][AS].load()]->delCB();
	}

	for (auto& val : grainmod2values) {
		if ((int)val == space) {
			_STATE->params[track->index][ASGRAN] = space;
			_STATE->params[track->index][AS] = SPACE_GRANULATION;
		}
	}
	for (auto& val : fxtypes2values) {
		if (val == space) {
			_STATE->params[track->index][ASFX] = space;
			_STATE->params[track->index][AS] = SPACE_FX;
		}
	}
	for (auto& val : reverbtypevalues) {
		if (val == space) {
			_STATE->params[track->index][ASSTFX] = space;
			_STATE->params[track->index][AS] = SPACE_REVERB;
		}
	}
	if (active == tindex) {
		tsl::graphics::TrackButton::func(_appState, tindex);
	}
}


tsl::graphics::EffectOrderView::EffectOrderView(tsl::AppState* appState) : View(appState, WRAP, 0, CENTER_ALIGN, 0),
ScrollViewBase(_appState) {};


void tsl::graphics::EffectOrderView::computeSize() {
	startx = 0;
	starty = 0;
	width = stopx = _STATE->windowWidth;
	height = stopy = _STATE->windowHeight;
	const float textsize = _STATE->textsize2;

	itemheight = _STATE->textsize1;

	SkFont font(_STATE->font_normal);

	font.setSize(textsize * .9f);

	{
		for (auto& s : titles) {
			SkRect bounds{};
			font.measureText(s, strlen(s), SkTextEncoding::kUTF8, &bounds);
			boxwidth = bounds.width() * 3 > boxwidth ? bounds.width() * 3 : boxwidth;
		}
	}
	boxwidth = parent != nullptr && parent->width > boxwidth ? parent->width.load() : boxwidth;

	for (auto& deque : { grainFX, monoFX, stereoFX })
		for (auto& p : deque) {
			SkRect bounds{};
			font.measureText(p.first.data(), p.first.size(), SkTextEncoding::kUTF8,
				&bounds);

			boxwidth = bounds.width() * 3 + textsize * 2 > boxwidth ? bounds.width() * 3 + textsize * 2 : boxwidth;
		}

	boxoffsetx = textsize * .5f + lw2;
	boxoffsety = boxoffsetx + itemheight;

	const float maxw = parent == nullptr ? width * .975f : parent->width.load();

	if (boxwidth + 2 * boxoffsetx > maxw)
		boxwidth = maxw - 2 * boxoffsetx;

	drawwidth = boxwidth + 2 * boxoffsetx;

	drawstartx = parent == nullptr ? startx + (width - drawwidth) * .5f : parent->startx.load();
	if (drawstartx < startx)
		drawstartx = startx;

	drawstopx = drawstartx + drawwidth;
	if (drawstopx > width) {
		drawstopx = width;
		drawstartx = drawstopx - drawwidth;
	}

	boxheight = itemheight * std::max(std::max(grainFX.size(), monoFX.size()), stereoFX.size());

	const float maxh = parent == nullptr ? height * .9f : DISTANCE(parent->stopy,
		_STATE->windowHeight -
		_STATE->textsize2);

	if (boxheight + boxoffsety + boxoffsetx > maxh) {
		maxoffset = -DISTANCE(boxheight + boxoffsety + boxoffsetx, maxh);
		drawheight = maxh;
		boxheight = maxh - boxoffsety - boxoffsetx;
	}
	else {
		maxoffset = 0;
		drawheight = boxheight + boxoffsety + boxoffsetx;
	}

	drawstarty = parent == nullptr ? starty + (height - drawheight) * .5f : parent->stopy -
		parent->height *
		.1f;
	drawstopy = drawstarty + drawheight;
	if (drawstopy > height) {
		drawstopy = height;
		drawstarty = drawstopy - drawheight;
	}
	centerText(font, boxwidth, itemheight, "STEREO FX", _titleX, _titleY);
}

void changeOrder(TRACK* t) {


}



void tsl::graphics::EffectOrderView::callback(const InputEvent& event) {
	int32_t action = event.action;
	int32_t _pointerid = event.pointer_id;
	float xpos = event.x - drawstartx;
	float ypos = event.y - drawstarty;
#ifdef PLATFORM_MOBILE
	velocityTracker.addMovement(event);
#endif
	switch (action) {
	case ACTION_DOWN: {
		if (xpos >= boxoffsetx && xpos < boxoffsetx + boxwidth && ypos >= boxoffsety &&
			ypos < boxoffsety + boxheight) {
			mode = INSIDE;;//_itemView.cb(event, pos);
			activeFx = { nullptr, (int)((xpos - boxoffsetx) / (boxwidth / 3)),
						(int)((ypos - boxoffsety - offset) / itemheight), 0, 0 };
			timer.reset();
			redraw();
		}
		else {
			mode = OUTSIDE;;//_itemView.cb(event, -1);
		}
		totalmoved = 0;
		lastXpos = xpos;
		lastYpos = ypos;
		pointerid = _pointerid;
#ifdef PLATFORM_MOBILE
		scroller[currentScroller.load()].forceFinished(true);
#endif
		break;
	}
	case ACTION_UP: {
		if (pointerid == _pointerid) {
			if (mode == MOVING) {
				auto tmp = activeFx.load();
				auto& deque = tmp.q == 0 ? grainFX : tmp.q == 1 ? monoFX : stereoFX;
				// The empty() check is load-bearing: size() - 1 underflows size_t
				// on an empty deque, and the .at() below would throw.
				if (tmp.name != nullptr && !deque.empty()) {
					auto ppos = (ypos - boxoffsety - offset - itemheight / 2) / itemheight;
					int pos = 0;
					if (ppos < 0) {
						pos = -1;
					}
					else if (ppos >= deque.size()) {
						pos = (int)deque.size() - 1;
					}
					else {
						pos = (int)floor(ppos);
					}
					if (deque.at(pos == -1 ? 0 : pos).second != tmp.id && deque.at(
						pos == -1 ? 0 : pos < deque.size() - 1 ? pos + 1
						: pos).second !=
						tmp.id) {
						auto t = _DATA->tracks[_STATE->active_track.load()];
						if (auto res = _DATA->snapShot.add_taskInt([=] {
							auto gainTask = _STATE->player.isPlaying() && _STATE->params[t->index][POWERTRACK].load() == 1.0;
							if (gainTask) {
								t->gainTask.setTargetAutoWait(tsl::gaintask::GainDown);
								auto token = _STATE->waitNotify.begin_wait();
								_DATA->toAudioThreadQueue.try_push([=]() {
									if (tmp.q == 0) {
										auto oldPos = t->fx_queue_grain[0].pos(tmp.id);
										for (int32_t i = 0; i < _STATE->channels; i++) {
											auto& current_q = t->fx_queue_grain[i];
											// Now work directly with current_q
											int move_result = current_q.move_element(tmp.id, pos);
											// ... handle move_result ...
										}
										auto newPos = t->fx_queue_grain[0].pos(tmp.id);
										_DATA->snapShot.add_task([this, fxNum = tmp.id, tindex = t->index, oldPos, newPos] {
											std::lock_guard lk(_DATA->snapShot);
											_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::FxOrder, fxNum, std::bit_cast<double>(tsl::parameters::FxOrderState{ newPos, oldPos }), tsl::parameters::powerGrainFx, 0, tsl::parameters::Event::History | tsl::parameters::Event::ToWorkerThread));
											_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Power, fxNum, std::bit_cast<double>(tsl::parameters::PowerState{ newPos, 1 }), tsl::parameters::powerGrainFx, 0, 0));
											});
									}
									else if (tmp.q == 1) {
										auto oldPos = t->fx_queue[0].pos(tmp.id);

										for (int32_t i = 0; i < _STATE->channels; i++) {
											auto& current_q = t->fx_queue[i];
											// Now work directly with current_q
											int move_result = current_q.move_element(tmp.id, pos);
											// ... handle move_result ...
										}
										auto newPos = t->fx_queue[0].pos(tmp.id);

										_DATA->snapShot.add_task([this, fxNum = tmp.id, tindex = t->index, oldPos, newPos] {
											std::lock_guard lk(_DATA->snapShot);
											_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::FxOrder, fxNum, std::bit_cast<double>(tsl::parameters::FxOrderState{ newPos, oldPos }), tsl::parameters::powerFx, 0, tsl::parameters::Event::History | tsl::parameters::Event::ToWorkerThread));
											_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Power, fxNum, std::bit_cast<double>(tsl::parameters::PowerState{ newPos, 1 }), tsl::parameters::powerFx, 0, 0));
											});

									}
									else { // tmp.q == 2 or any other default
										auto& current_q = t->fx_queue_stereo;
										// Now work directly with current_q
										auto oldPos = current_q.pos(tmp.id);
										int move_result = current_q.move_element(tmp.id, pos);
										auto newPos = current_q.pos(tmp.id);
										// ... handle move_result ...
										_DATA->snapShot.add_task([this, fxNum = tmp.id, tindex = t->index, oldPos, newPos] {
											std::lock_guard lk(_DATA->snapShot);
											_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::FxOrder, fxNum, std::bit_cast<double>(tsl::parameters::FxOrderState{ newPos, oldPos }), tsl::parameters::powerStereoFx, 0, tsl::parameters::Event::History | tsl::parameters::Event::ToWorkerThread));
											_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(tindex, tsl::parameters::Power, fxNum, std::bit_cast<double>(tsl::parameters::PowerState{ newPos, 1 }), tsl::parameters::powerStereoFx, 0, 0));
											});
									}
									t->gainTask.setTarget(-120, 0);
									_STATE->waitNotify.complete(token);
									});
								_STATE->waitNotify.wait_for_signal(token);
							}
							else {
								std::lock_guard lk(_DATA->snapShot);

								if (tmp.q == 0) {
									auto oldPos = t->fx_queue_grain[0].pos(tmp.id);
									for (int32_t i = 0; i < _STATE->channels; i++) {
										auto& current_q = t->fx_queue_grain[i];
										// Now work directly with current_q
										int move_result = current_q.move_element(tmp.id, pos);
										// ... handle move_result ...
									}
									auto newPos = t->fx_queue_grain[0].pos(tmp.id);
									_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(t->index, tsl::parameters::FxOrder, tmp.id, std::bit_cast<double>(tsl::parameters::FxOrderState{ newPos, oldPos }), tsl::parameters::powerGrainFx, 0, tsl::parameters::Event::History | tsl::parameters::Event::ToWorkerThread));
									_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(t->index, tsl::parameters::Power, tmp.id, std::bit_cast<double>(tsl::parameters::PowerState{ newPos, 1 }), tsl::parameters::powerGrainFx, 0, 0));
								}
								else if (tmp.q == 1) {
									auto oldPos = t->fx_queue[0].pos(tmp.id);

									for (int32_t i = 0; i < _STATE->channels; i++) {
										auto& current_q = t->fx_queue[i];
										// Now work directly with current_q
										int move_result = current_q.move_element(tmp.id, pos);
										// ... handle move_result ...
									}
									auto newPos = t->fx_queue[0].pos(tmp.id);

									_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(t->index, tsl::parameters::FxOrder, tmp.id, std::bit_cast<double>(tsl::parameters::FxOrderState{ newPos, oldPos }), tsl::parameters::powerFx, 0, tsl::parameters::Event::History | tsl::parameters::Event::ToWorkerThread));
									_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(t->index, tsl::parameters::Power, tmp.id, std::bit_cast<double>(tsl::parameters::PowerState{ newPos, 1 }), tsl::parameters::powerFx, 0, 0));
								}
								else { // tmp.q == 2 or any other default
									auto& current_q = t->fx_queue_stereo;
									// Now work directly with current_q
									auto oldPos = current_q.pos(tmp.id);
									int move_result = current_q.move_element(tmp.id, pos);
									auto newPos = current_q.pos(tmp.id);
									// ... handle move_result ...
									_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(t->index, tsl::parameters::FxOrder, tmp.id, std::bit_cast<double>(tsl::parameters::FxOrderState{ newPos, oldPos }), tsl::parameters::powerStereoFx, 0, tsl::parameters::Event::History | tsl::parameters::Event::ToWorkerThread));
									_DATA->snapShot.addEvent(tsl::parameters::Event::createEvent(t->index, tsl::parameters::Power, tmp.id, std::bit_cast<double>(tsl::parameters::PowerState{ newPos, 1 }), tsl::parameters::powerStereoFx, 0, 0));
								}
							}

							activate(t);



							}) > 0)
						{
							char text[100];
							snprintf(text, 100, "FX Order Queued. Pos %d.",
								res + 1);
							showToast(_STATE, text);
						}
					}
					else {
						tmp.name = nullptr;
						tmp.q = 10000;
						activeFx.store(tmp);
						pointerid = -1;
						mode = UNTOUCHED;
						redraw();
					}
				}
			}
			else if (mode == OUTSIDE) {
				deldraw();
				delCB();
				return;
			}
			else if (mode == INSIDE /*&& _itemView.cb(event, -1) == 1*/) {
				auto aX = activeFx.load();
				if (aX.name == nullptr && aX.q <= 2) {
					auto& q = aX.q == 0 ? grainFX : aX.q == 1 ? monoFX : stereoFX;
					if (aX.qpos < q.size()) {
						deldraw();
						delCB();
						_STATE->UiTasksQueue.add_task(switcheffect,
							_DATA->tracks[_STATE->active_track.load()],
							q.at(aX.qpos).second);
					}
				}
			}
			pointerid = -1;
			mode = UNTOUCHED;
		}
		break;
	}
	case ACTION_MOVE: {
		if (pointerid == _pointerid) {
			if (mode == MOVING) {
				auto tmp = activeFx.load();
				if (tmp.name != nullptr) {
					tmp.y = ypos;
					if (tmp.y < boxoffsety)
						tmp.y = boxoffsety;
					else if (tmp.y > boxoffsety + boxheight)
						tmp.y = boxoffsety + boxheight;
					activeFx.store(tmp);
					redraw();
				}

				float diffy = lastYpos - ypos;
				float off = offset;
				off -= diffy;
				if (off < maxoffset)
					off = maxoffset;
				else if (off > 0)
					off = 0;
				offset = off;
				lastXpos = xpos;
				lastYpos = ypos;
				timeLastAction.reset();
			}
			else if (mode == INSIDE || mode == OUTSIDE) {
				totalmoved += spacing(xpos, lastXpos, ypos, lastYpos);
				if (totalmoved > _STATE->textsize2) {
					if (mode == INSIDE) {
						lastXpos = xpos;
						lastYpos = ypos;
						//_itemView.cb(event, -1);
						if (maxoffset < 0)
							mode = MOVING;
						if (xpos >= boxoffsetx && xpos < boxoffsetx + boxwidth &&
							ypos >= boxoffsety &&
							ypos < boxoffsety + boxheight) {
							int32_t pos = (int)((ypos - boxoffsety - offset) / itemheight);
							if (pos >= std::max(std::max(grainFX.size(), monoFX.size()),
								stereoFX.size()))
								pos = std::max(std::max(grainFX.size(), monoFX.size()),
									stereoFX.size());
							int32_t q = (int)floorf(
								(xpos - boxoffsetx) / (boxwidth / 3.f));

							if (q == 0 && pos < grainFX.size())
								activeFx = { grainFX.at(pos).first.data(), q, pos,
											grainFX.at(pos).second,
											ypos };
							else if (q == 1 && pos < monoFX.size())
								activeFx = { monoFX.at(pos).first.data(), q, pos,
											monoFX.at(pos).second,
											ypos };
							else if (q == 2 && pos < stereoFX.size())
								activeFx = { stereoFX.at(pos).first.data(), q, pos,
											stereoFX.at(pos).second,
											ypos };
							else
								activeFx = { nullptr, 0, 0, 0, 0 };
							redraw();
							mode = MOVING;;//_itemView.cb(event, pos);
						}
					}
					else {
						mode = UNTOUCHED;
						pointerid = -1;
						redraw();
					}
				}
			}
		}
		break;
	case ACTION_KEY_UP: {
		if (event.pointer_id == VKEY_ESCAPE) {
			delCB();
			deldraw();
		}
	}
					  break;
	}

	default:
		break;
	}
}


void tsl::graphics::EffectOrderView::render(void* ctx)
{
	auto canvas = _STATE->graphics.getCanvas(windowindex, drawstartx,
		drawstarty, drawwidth, drawheight);
	if (canvas == nullptr)
		return;
	SkPaint paint;
	canvas->clear(tsl::sk_colours::bg);
	//paint.setColor(_DATA->C_bg);
	//canvas->drawRect(SkRect::MakeXYWH(0, 0, drawwidth, drawheight), paint);
	paint.setColor(tsl::sk_colours::fg);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setAntiAlias(true);
	paint.setStrokeWidth(1.0);
	canvas->drawRect(SkRect::MakeXYWH(0.5, 0.5, drawwidth - 1, drawheight - 1),
		paint);
	paint.setStrokeWidth(lw);
	SkFont& font = _STATE->font_normal;
	const float textsize = _STATE->textsize2;
	font.setSize(textsize * .9f);
	paint.setStyle(SkPaint::kFill_Style);
	canvas->drawSimpleText("GRAIN FX", strlen("GRAIN FX"), SkTextEncoding::kUTF8, boxoffsetx,
		boxoffsetx + _titleY, font, paint);
	canvas->drawSimpleText("MONO FX", strlen("MONO FX"), SkTextEncoding::kUTF8,
		boxwidth / 3 + boxoffsetx,
		boxoffsetx + _titleY,
		font, paint);
	canvas->drawSimpleText("STEREO FX", strlen("STEREO FX"), SkTextEncoding::kUTF8,
		boxwidth / 3 * 2 + boxoffsetx,
		boxoffsetx + _titleY, font, paint);
	canvas->save();
	SkPath path;
	path.addRect(SkRect::MakeXYWH(lw, boxoffsety - lw, width - 2 * lw, boxheight + 2 * lw));
	canvas->clipPath(path);

#ifdef PLATFORM_MOBILE
	auto& s = scroller[currentScroller.load()];
	const auto elapsed = !s.isFinished() ? timeLastAction.elapsedReplace()
		: timeLastAction.elapsed();

	if (s.computeScrollOffset()) {
		;
		offset = s.getCurrY();
	}
#else
	const auto elapsed = timeLastAction.elapsed();

#endif

	const float off = offset;

	auto aX = activeFx.load();
	int32_t drawPos = -1000;
	if (aX.name != nullptr) {
		auto ppos = (aX.y - boxoffsety - offset - itemheight / 2) / itemheight;
		if (ppos < 0) {
			drawPos = -1;
		}
		else if (ppos >=
			(aX.q == 0 ? grainFX.size() : aX.q == 1 ? monoFX.size() : stereoFX.size())) {
			drawPos =
				(aX.q == 0 ? grainFX.size() : aX.q == 1 ? monoFX.size() : stereoFX.size()) -
				1;
		}
		else {
			drawPos = (int)floor(ppos);
		}
	}
	auto truncateText = [&](std::string_view text, float maxWidth) -> std::pair<std::string_view, bool> {
		float fullWidth = font.measureText(text.data(), text.size(), SkTextEncoding::kUTF8);
		if (fullWidth <= maxWidth)
			return { text, false };

		float ellipsisWidth = font.measureText("...", 3, SkTextEncoding::kUTF8);
		float budget = maxWidth - ellipsisWidth;

		size_t fitLen = text.size();
		while (fitLen > 0) {
			float w = font.measureText(text.data(), fitLen, SkTextEncoding::kUTF8);
			if (w <= budget) break;
			fitLen--;
		}
		return { std::string_view(text.data(), fitLen), true };
		};

	TRACK* track = _DATA->tracks[_STATE->active_track.load()];
	int32_t posx = 0;
	int32_t i = 0;
	for (auto& deque : { grainFX, monoFX, stereoFX }) {
		float sx = boxoffsetx + posx;
		float sy = boxoffsety + off;
		paint.setStyle(SkPaint::kFill_Style);
		int32_t pos = 0;
		for (auto& fx : deque) {
			if (sy >= boxoffsety - itemheight &&
				sy - itemheight <= boxoffsety + boxheight) {
				// Powered off but still in the queue awaiting fade-out/reap
				const bool dying = track->fxpower[fx.second].load(std::memory_order_acquire) == 0.0;
				paint.setColor(dying ? skcol::grey : skcol::fg);
				float x, y;
				canvas->save();
				canvas->translate(sx, sy);
				// in your loop, replace the drawSimpleText block:
				float maxWidth = boxwidth / 3.0f;
				auto [truncated, hasEllipsis] = truncateText(fx.first, maxWidth);

				centerText(font, maxWidth, itemheight, truncated.data(), x, y);
				canvas->drawSimpleText(truncated.data(), truncated.size(), SkTextEncoding::kUTF8, 0, y, font, paint);

				if (hasEllipsis) {
					float truncWidth = font.measureText(truncated.data(), truncated.size(), SkTextEncoding::kUTF8);
					canvas->drawSimpleText("...", 3, SkTextEncoding::kUTF8, truncWidth, y, font, paint);
				}
				if (aX.name != nullptr && i == aX.q) {
					if (pos == 0 && drawPos == -1 && fx.second != aX.id) {
						paint.setColor(skcol::blue);
						canvas->drawLine(0, 1, boxwidth / 3, 1, paint);
						paint.setColor(skcol::fg);
					}
					else if (pos == drawPos && fx.second != aX.id &&
						deque.at(pos < deque.size() - 1 ? pos + 1 : pos).second !=
						aX.id) {
						paint.setColor(skcol::blue);
						canvas->drawLine(0, itemheight, boxwidth / 3, itemheight, paint);
						paint.setColor(skcol::fg);
					}
				}
				else if (aX.qpos == pos && aX.q == i) {
					paint.setStyle(SkPaint::kStroke_Style);
					paint.setColor(skcol::fg);
					canvas->drawRect(
						SkRect::MakeXYWH(0, 0, boxwidth / 3, itemheight),
						paint);
					paint.setStyle(SkPaint::kFill_Style);
				}
				pos++;
				canvas->restore();
			}
			sy += itemheight;
		}
		posx += boxwidth / 3;
		i++;
	}

	const float maxoff = std::abs(maxoffset);
	if (maxoff > 0) {
		float alpha =
			elapsed > 1.0 ? 0.f : 255.f - 255.f * elapsed;
		paint.setColor(
			SkColorSetA(mode == 1000 ? skcol::blue_transparent : skcol::fg, alpha));
		const float length = boxheight / (1.f + maxoff / boxheight);
		const float pos =
			(1.f - DISTANCEF(maxoffset, off) / maxoff) * (boxheight - length);
		canvas->drawRect(
			SkRect::MakeXYWH(drawwidth - 3 * lw, boxoffsety + pos, 2 * lw, length),
			paint);
	}


	if (aX.name != nullptr) {
		float maxWidth = boxwidth / 3.0f;
		std::string_view text(aX.name);
		auto [truncated, hasEllipsis] = truncateText(text, maxWidth);

		float x, y;
		centerText(font, maxWidth, itemheight, truncated.data(), x, y);

		paint.setColor(skcol::fg);
		paint.setStyle(SkPaint::kStroke_Style);
		canvas->drawRect(
			SkRect::MakeXYWH(boxoffsetx + aX.q * boxwidth / 3,
				aX.y - itemheight * .5f,
				boxwidth / 3, itemheight), paint);
		paint.setStyle(SkPaint::kFill_Style);

		float drawX = aX.q * boxwidth / 3 + boxoffsetx;
		float drawY = aX.y - itemheight * .5f + y;

		canvas->drawSimpleText(truncated.data(), truncated.size(),
			SkTextEncoding::kUTF8, drawX, drawY, font, paint);

		if (hasEllipsis) {
			float truncWidth = font.measureText(truncated.data(), truncated.size(), SkTextEncoding::kUTF8);
			canvas->drawSimpleText("...", 3, SkTextEncoding::kUTF8,
				drawX + truncWidth, drawY, font, paint);
		}
	}canvas->restore();
}
void findFx(tsl::AppState* _STATE, TRACK* track, std::deque<std::pair<std::string_view, int>>& monoFX, std::deque<std::pair<std::string_view, int>>& stereoFX, std::deque<std::pair<std::string_view, int>>& grainFX) {
	for (auto it = track->fx_queue_grain[0].begin();
		it != track->fx_queue_grain[0].end(); ++it)
		grainFX.emplace_back(
			grainmods2[findIndexFloat(grainmod2values, it.operator*()->_id)],
			it.operator*()->_id);

	for (auto it = track->fx_queue[0].begin(); it != track->fx_queue[0].end(); ++it)
		monoFX.emplace_back(fxtypes2[findIndexFloat(fxtypes2values, it.operator*()->_id)],
			it.operator*()->_id);

	for (auto it = track->fx_queue_stereo.begin();
		it != track->fx_queue_stereo.end(); ++it)
		if (it.operator*()->_id != tsl::DEGRADATION_STEREO_ID)
			stereoFX.emplace_back(
				reverbtypes[findIndexFloat(reverbtypevalues, it.operator*()->_id)],
				it.operator*()->_id);

}
int tsl::graphics::EffectOrderView::activate(TRACK* track) {
	auto _appState = track->_appState;

	struct FxLists {
		std::deque<std::pair<std::string_view, int>> mono, stereo, grain;
	};
	auto lists = std::make_shared<FxLists>();

	auto gainTask = _STATE->player.isPlaying() && _STATE->params[track->index][POWERTRACK].load() == 1.0;
	if (gainTask) {
		auto token = _STATE->waitNotify.begin_wait();
		bool queued = _DATA->toAudioThreadQueue.try_push([track, token, lists] {
			findFx(track->_STATE, track, lists->mono, lists->stereo, lists->grain);
			track->_STATE->waitNotify.complete(token);
			});
		if (!queued || !_STATE->waitNotify.wait_for_signal(token, 500)) {
			showToast(track->_STATE, "Try again.");
			return 0;
		}
	}
	else
		findFx(_STATE, track, lists->mono, lists->stereo, lists->grain);

	if (lists->mono.empty() && lists->grain.empty() && lists->stereo.empty()) {
		showToast(track->_STATE, "No Effects Active.");
		return 0;
	}



	_STATE->UiTasksQueue.add_task([track, lists = std::move(lists)] {
		auto _appState = track->_appState;
		auto tmp = _DATA->effectsOrderView.load();
		if (tmp != nullptr) {
			tmp->delCB();
			tmp->deldraw();
		}
		else {
			tmp = std::make_shared<EffectOrderView>(track->_appState);
			_DATA->effectsOrderView = tmp;
		}
		tmp->grainFX = std::move(lists->grain);
		tmp->monoFX = std::move(lists->mono);
		tmp->stereoFX = std::move(lists->stereo);
		tmp->reset();
		tmp->computeSize();
		tmp->addDraw();
		tmp->addCB();
		});
	return 1;
}
void tsl::graphics::EffectOrderView::addRecursiveDraw()
{
	View::addRecursiveDraw();
}

void tsl::graphics::EffectOrderView::delRecursiveDraw()
{
	_STATE->graphics.deleteWindow(windowindex);
	View::delRecursiveDraw();
};

void tsl::graphics::EffectOrderView::addRecursiveCB()
{
	hasFocus.store(true);

	View::addRecursiveCB();
}

void tsl::graphics::EffectOrderView::delRecursiveCB() {
	hasFocus.store(false);
	pointerid = -1;
	mode = UNTOUCHED;
	totalmoved = 0;
#ifdef PLATFORM_MOBILE
	velocityTracker.clear();
#endif
	activeFx.store(bla(nullptr, -1, 0, 0, 0.f));
	View::delRecursiveCB();
};
void tsl::graphics::EffectOrderView::reset() {
	hasFocus.store(false);
	pointerid = -1;
	mode = UNTOUCHED;
	totalmoved = 0;
#ifdef PLATFORM_MOBILE
	velocityTracker.clear();
#endif
	activeFx.store(bla(nullptr, -1, 0, 0, 0.f));
}

tsl::graphics::FXView::FXView(tsl::AppState* appState) : TextViewBase<std::pair<std::string, int>>(appState) {};

void tsl::graphics::FXView::render(SkCanvas* c, int32_t index) {
	flush(c);
	SkFont& font = _STATE->font_normal;
	SkPaint paint;
	paint.setColor(skcol::fg);
	float x, y;
	const auto& s = _values.at(index);
	font.setSize(_STATE->textsize2 * .9f);
	font.setEmbolden(s.second == -1);

	centerText(font, this, s.first.c_str(), x, y);
	c->drawString(s.first.c_str(), _STATE->textsize2 * .5f, y, font, paint);
	if (s.second != -1)
		TextViewBase::render(c, index);
}

void tsl::graphics::FXView::computeWidth(int32_t index) {
	const auto& s = _values.at(index);
	SkFont& font = _STATE->font_normal;
	font.setSize(_STATE->textsize2 * .9);
	SkRect bounds{};
	font.measureText(s.first.c_str(), s.first.size(),
		SkTextEncoding::kUTF8, &bounds);
	width = bounds.width();
}

int32_t tsl::graphics::FXView::cb(const InputEvent& e, int index) {
	if (e.action == ACTION_UP) {
		auto tmp = indexhot.load();
		if (tmp >= 0 && tmp < _values.size()) {
			const auto& s = _values.at(tmp);
			if (s.second == -1)
				return 0;
			_STATE->UiTasksQueue.add_task(switcheffect,
				_DATA->tracks[_STATE->active_track.load()],
				s.second);
		}
		else return 0;
	}
	return TextViewBase::cb(e, index);
}


