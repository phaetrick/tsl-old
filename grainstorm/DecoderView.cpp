#include "DecoderView.h"
#include "track.h"
#include "waveform.h"
#include "grainstorm.h"
#include "Decdata.h"

using namespace tsl::graphics;

void DecoderView::init() {
	width = _STATE->windowWidth * .5f;
	height = _STATE->windowHeight * .5f;
	startx = (_STATE->windowWidth - width) * .5f;
	starty = (_STATE->windowHeight - height) * .5f;
	stopx = startx + width;
	stopy = starty + height;
}

void DecoderView::callback(const InputEvent& event) {
	float xpos = event.x;
	float ypos = event.y;
	int32_t action = event.action;
	switch (action) {
	case ACTION_DOWN:
		lastx = xpos;
		lasty = ypos;
		dragid = event.pointer_id;
		break;

	case ACTION_UP:
		if (event.pointer_id == dragid)
			dragid = -1;
		break;
	case ACTION_MOVE:
		// Only the finger that started the drag moves the window -- a second
		// finger used to apply its delta against the first one's lastx/lasty.
		if (event.pointer_id == dragid) {
			startx += (xpos - lastx);
			stopx = startx + width;
			starty += (ypos - lasty);
			stopy = starty + height;
			lastx = xpos;
			lasty = ypos;
		}
		break;
	case ACTION_KEY_UP:
		if (event.pointer_id == VKEY_ESCAPE) {
			_DATA->decodingStop = true;
		}
		break;
	default:
		break;
	}
}
#ifdef USE_IMGUI
#include <imgui.h>


void
DecoderView::render(void* context) {

	DecData& d = _DATA->decData;


	if (d.offset == 0 || d.buffer[0].empty()) {
		return;
	}
	TRACK* track = d.track;
	View* view = track->waveform;
	const int32_t channels = _STATE->channels;

	const long threeseconds = _STATE->sr * 15;

	ImGui::SetNextWindowPos(
		ImVec2(startx, starty),
		ImGuiCond_Always);
	ImGui::SetNextWindowSize(
		ImVec2(width, height),
		ImGuiCond_Always);
	bool test = true;
	if (!ImGui::Begin("Decode", &test, ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoMouseInputs |
		ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDecoration)) {
		ImGui::End();
		return;
	}
	ImVec2 canvas_pos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
	ImVec2 canvas_size = ImGui::GetContentRegionAvail();        // Resize canvas to what's available
	if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
	if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

	const float width = canvas_size.x;
	const float height = canvas_size.y;


	const float draw_offset = width / (float)channels;
	const float halb = 0.5f * draw_offset;
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	draw_list->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y +
		canvas_size.y));// clip lines within the canvas (i


	const long actual_offset = d.offset.load();

	const int32_t end = height;
	const float frames_per_bin = threeseconds / (float)height;

	int32_t x = 0;
	long startframe = actual_offset - threeseconds;

	const float gain = .9;

	long frames_per_buf = (long)(floor(frames_per_bin));


	long offset = 0;


	while (x++ < end) {
		short min[2]{}, max[2]{};
		{
			std::lock_guard lk(d.mutex);
			for (int32_t frame = 0; frame < frames_per_buf; frame++) {
				for (int32_t i = 0; i < channels; i++) {
					const short sample_val1 =
						startframe >= actual_offset || startframe < 0
						? 0 : d.buffer[i][startframe];
					//const short sample_val2 = data2[frame];
					max[i] = MAX(max[i], sample_val1);
					min[i] = MIN(min[i], sample_val1);
				};
				startframe++;
			}
		}


		for (int32_t i = 0; i < channels; i++) {
			float minon1 = halb - ABS(halb * min[i] * CONVMYFLT * gain);
			float maxoff1 =
				halb + ABS(halb * max[i] * CONVMYFLT * gain);

			draw_list->AddLine(
				ImVec2(canvas_pos.x + i * draw_offset + minon1, canvas_pos.y + x),
				ImVec2(canvas_pos.x + i * draw_offset + maxoff1, canvas_pos.y + x),
				IM_COL32(255, 255, 255, 255), 2.0);
		}

		offset += frames_per_buf;

		frames_per_buf = (long)(
			floor((x + 2) * frames_per_bin) - offset);
	}

	ImGui::End();
}
void DecoderView::delRecursiveDraw()
{
	View::delRecursiveDraw();
	visible.notify_one();
};

#else
void DecoderView::delRecursiveDraw()
{
	View::delRecursiveDraw();
	_appState->graphics.deleteWindow(windex);
	windex = -1;
	visible_.notify_one();
};

#define PACK4_8_TO_U32(a, b, c, d) \
    ( (uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d) )


void
DecoderView::render(void* context) {
	const long actual_offset = decData.offset.load();

	if (actual_offset == 0) {
		return;
	}
	const int32_t channels = decData.channels;

	const long threeseconds = decData.sr * 15;


	const float draw_offset = width / (float)channels;
	const float halb = 0.5f * draw_offset;



	const int32_t end = height;
	const float frames_per_bin = threeseconds / (float)height;

	int32_t x = 0;
	long startframe = actual_offset - threeseconds;

	const float gain = .9;

	long frames_per_buf = (long)(floor(frames_per_bin));


	long offset = 0;

	auto canvas = _appState->graphics.getCanvas(windex, startx, starty, width, height);
	if (!canvas) {
		return;
	}
	SkPath waveformPath;
	waveformPath.setFillType(SkPathFillType::kWinding);

	while (x++ < end) {
		short min[2]{}, max[2]{};
		{
			std::lock_guard lk(decData.mtx);
			/* Bound on the live size, not on actual_offset. That was sampled
			   before the lock, and while the decoder only ever grows the buffer
			   (so it could not go stale downwards), the FINISH path moves the
			   buffers out entirely -- tsl::Recording's array constructor does
			   buffer[i] = std::move(data[i]) -- leaving empty vectors while
			   decData.offset still reads millions of frames. Indexing that gave
			   a fault address of exactly 2 * startframe: a null data pointer.

			   Safe before this session because deldraw() removed this view
			   under queue_draw's mutex, which the render walk held throughout,
			   so the decoder blocked until the frame ended. Removal is deferred
			   now, so one more render() can land after the move. */
			long limit[MAX_CHANNELS]{};
			for (int32_t i = 0; i < channels; i++)
				limit[i] = static_cast<long>(decData.buffer[i].size());

			for (int32_t frame = 0; frame < frames_per_buf; frame++) {
				for (int32_t i = 0; i < channels; i++) {
					const short sample_val1 =
						startframe >= limit[i] || startframe < 0
						? 0 : decData.buffer[i][startframe];
					//const short sample_val2 = data2[frame];
					max[i] = MAX(max[i], sample_val1);
					min[i] = MIN(min[i], sample_val1);
				};
				startframe++;
			}
		}


		for (int32_t i = 0; i < channels; i++) {
			float minon1 = halb - ABS(halb * min[i] * CONVMYFLT * gain);
			float maxoff1 =
				halb + ABS(halb * max[i] * CONVMYFLT * gain);
			waveformPath.moveTo(i * draw_offset + minon1, x);
			waveformPath.lineTo(i * draw_offset + maxoff1, x);
		}

		offset += frames_per_buf;

		frames_per_buf = (long)(
			floor((x + 2) * frames_per_bin) - offset);
	}
	View v{ _STATE,WRAP,0,CENTER_ALIGN };
	v.startx = v.starty = 0;
	v.width = v.stopx = width.load();
	v.height = v.stopy = height.load();
	canvas->clear(skcol::wbg);
	SkPaint paint;
	paint.setColor(skcol::waveform);
	paint.setStrokeWidth(2.0f);
	paint.setStyle(SkPaint::kStroke_Style);
	paint.setAntiAlias(true);
	canvas->drawPath(waveformPath, paint);
	paint.setColor(SK_Colour(255, 255, 255, 90));
	v.drawRect(canvas, SK_Colour(255, 255, 255, 90));
}



#endif

