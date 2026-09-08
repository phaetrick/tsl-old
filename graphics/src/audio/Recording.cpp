#include "audio/Recording.h"
#include "app.h"
#include "tools/SwapRingPool.h"
#include "tools/LockFreeList.h"

std::array<tsl::SavedRecordingState, tsl::numSavedPositions> tsl::loopdummy = { {
	{ 0., 0., 0., 0., 1., 0., 1. },
	{ 0., 0., 0., 0., 1., 0., 1. },
	{ 0., 0., 0., 0., 1., 0., 1. },
	{ 0., 0., 0., 0., 1., 0., 1. },
	{ 0., 0., 0., 0., 1., 0., 1. },
	{ 0., 0., 0., 0., 1., 0., 1. },
	{ 0., 0., 0., 0., 1., 0., 1. },
	{ 0., 0., 0., 0., 1., 0., 1. },
} };

tsl::RecordingState::RecordingState(const tsl::RecordingState& other) :
	waveformState{ other.waveformState.load(std::memory_order_acquire) },
	offset{ other.offset.load(std::memory_order_acquire) },
	off_start{ other.off_start.load(std::memory_order_acquire) },
	off_stop{ other.off_stop.load(std::memory_order_acquire) },
	bounceType{ other.bounceType.load(std::memory_order_acquire) },
	playbackDir{ other.playbackDir.load(std::memory_order_acquire) }
{

}

tsl::RecordingState& tsl::RecordingState::operator=(const tsl::RecordingState& other) {
	if (this == &other) return *this;
	waveformState.store(other.waveformState.load(std::memory_order_acquire), std::memory_order_release);
	offset.store(other.offset.load(std::memory_order_acquire), std::memory_order_release);
	off_start.store(other.off_start.load(std::memory_order_acquire), std::memory_order_release);
	off_stop.store(other.off_stop.load(std::memory_order_acquire), std::memory_order_release);
	bounceType.store(other.bounceType.load(std::memory_order_acquire), std::memory_order_release);
	playbackDir.store(other.playbackDir.load(std::memory_order_acquire), std::memory_order_release);
	return *this;
}

tsl::RecordingState& tsl::RecordingState::operator=(const tsl::SavedRecordingState& other) {
	waveformState.store(tsl::parameters::WaveformState{
		static_cast<float>(other[WaveformStartPosition]),
		static_cast<float>(other[WaveformZoom])
		}, std::memory_order_release);
	offset.store(other[ReadPos], std::memory_order_release);
	off_start.store(other[LoopStart], std::memory_order_release);
	off_stop.store(other[LoopStop], std::memory_order_release);
	bounceType.store(static_cast<int>(other[BounceType]), std::memory_order_release);
	playbackDir.store(other[PlaybackDirection], std::memory_order_release);
	return *this;
}

tsl::SavedRecordingState& tsl::SavedRecordingState::operator=(const tsl::RecordingState& other) {
	auto ws = other.waveformState.load(std::memory_order_acquire);
	(*this)[LoopStart] = other.off_start.load(std::memory_order_acquire);
	(*this)[LoopStop] = other.off_stop.load(std::memory_order_acquire);
	(*this)[ReadPos] = other.offset.load(std::memory_order_acquire);
	(*this)[BounceType] = other.bounceType.load(std::memory_order_acquire);
	(*this)[PlaybackDirection] = other.playbackDir.load(std::memory_order_acquire);
	(*this)[WaveformStartPosition] = ws.startPos;
	(*this)[WaveformZoom] = ws.zoom;
	return *this;
}




tsl::Recording::~Recording() {
#ifdef GRAINSTORM
	if (poolHandle < NO_SOUND_PRESENT) {
		auto& pd = _appState->recordings[poolHandle];
		std::lock_guard lk(pd.mutex);
		pd.decRefCount();
	}
#endif
}


tsl::Recording::Recording(const tsl::Recording& other)
{
	_appState = other._appState;
	sr = other.sr;
	channels = other.channels;
	//statusMessage = other.statusMessage;
	trackIndex = other.trackIndex;
	numEdits = other.numEdits;
	lastEdit = other.lastEdit;
	try {
		for (int i = 0; i < MAX_CHANNELS; ++i)
			buffer[i] = other.buffer[i];
		fileName = other.fileName;
		off = other.off;
		dorendering.store(true, std::memory_order_relaxed);
		isresizing.store(false, std::memory_order_relaxed);
		auto s = other.state.load();
		if (s != nullptr) {
			state.store(std::make_shared<RecordingState>(*s));
		}
		else state.store(std::make_shared<RecordingState>());
		positions = other.positions;
	}
	catch (const std::bad_alloc&) {
		for (int i = 0; i < MAX_CHANNELS; ++i)
			buffer[i].clear();
		off = 0;
		fileName = "";
		dorendering = false;
		positions = loopdummy;
		state = std::make_shared<RecordingState>();
		showToast(_STATE, "Out of memory.");
	}
}


tsl::Recording& tsl::Recording::operator=(const tsl::Recording& other) {
	if (this == &other) return *this;

	_appState = other._appState;
	sr = other.sr;
	channels = other.channels;
	//statusMessage = other.statusMessage;
	trackIndex = other.trackIndex;
	numEdits = other.numEdits;
	lastEdit = other.lastEdit;
	try {
		for (int i = 0; i < MAX_CHANNELS; ++i)
			buffer[i] = other.buffer[i];
		fileName = other.fileName;
		off = other.off;
		dorendering.store(true, std::memory_order_relaxed);
		ismoving.store(false, std::memory_order_relaxed);
		isresizing.store(false, std::memory_order_relaxed);
		auto s = other.state.load();
		if (s != nullptr) {
			state.store(std::make_shared<RecordingState>(*s));
		}
		else state.store(std::make_shared<RecordingState>());

		positions = other.positions;
	}
	catch (const std::bad_alloc&) {
		for (int i = 0; i < MAX_CHANNELS; ++i)
			buffer[i].clear();
		off = 0;
		fileName = "";
		dorendering = false;
		state.store(std::make_shared<RecordingState>());
		positions = loopdummy;
		showToast(_STATE, "Out of memory.");
	}
	return *this;
}


void tsl::adjustLoopPointsAfterCut(double& loopStart, double& loopEnd, double& readPos,
	double cutStart, double cutEnd)
{
	if (cutStart < loopStart)
		loopStart = cutStart + std::max(0.0, loopStart - cutEnd);

	if (cutStart < loopEnd)
		loopEnd = cutStart + std::max(0.0, loopEnd - cutEnd);

	if (loopEnd < loopStart)
		loopEnd = loopStart;

	if (readPos > cutStart)
		readPos = cutStart + std::max(0.0, readPos - cutEnd);

	if (readPos < loopStart)
		readPos = loopStart;
	else if (readPos > loopEnd)
		readPos = loopEnd;
}

void tsl::adjustLoopPointsAfterInsert(double& loopStart, double& loopEnd, double& readPos,
	double insertPos, double insertLength)
{
	if (insertPos < loopStart)
		loopStart += insertLength;

	if (insertPos <= loopEnd)
		loopEnd += insertLength;

	if (insertPos < readPos)
		readPos += insertLength;
}

void tsl::Recording::adjustPositions(double point, double length) {
	auto cut = std::abs(length);
	auto isCutting = length < 0;
	const double off_before = off;
	const double newLength = isCutting ? off_before - cut : point + cut > off_before ? point + cut : off_before;
	auto cur = state.load();
	if (cur) {
		double start = cur->off_start.load();
		double stop = cur->off_stop.load();
		double offset = cur->offset.load();
		if (isCutting)adjustLoopPointsAfterCut(start, stop, offset, point, point + cut);
		else adjustLoopPointsAfterInsert(start, stop, offset, point, cut);
		cur->off_start = start;
		cur->off_stop = stop;
		cur->offset = offset;
		if (off_before > 0 && newLength > 0 && isCutting) {
			auto wState = cur->waveformState.load();
			double startpos = wState.startPos;
			double firstSample = startpos * off_before;
			if (point < firstSample) {
				firstSample = std::max(firstSample - cut, 0.);
				wState.startPos = firstSample / newLength;
				cur->waveformState.store(wState);
			}
		}

	}
	for (auto& p : positions) {
		double start = p[LoopStart];
		double stop = p[LoopStop];
		double offset = p[ReadPos];
		if (isCutting)adjustLoopPointsAfterCut(start, stop, offset, point, point + cut);
		else adjustLoopPointsAfterInsert(start, stop, offset, point, cut);
		p[LoopStart] = start;
		p[LoopStop] = stop;
		p[ReadPos] = offset;

		if (off_before > 0 && newLength > 0 && isCutting) {
			double firstSample = p.at(WaveformStartPosition) * off_before;
			if (point < firstSample) {
				firstSample = std::max(firstSample - cut, 0.);
				p.at(WaveformStartPosition) = firstSample / newLength;

			}

		}
	}

}

void tsl::adjustPositions(double positions[], double point, double length) {

	auto cut = std::abs(length);
	auto isCutting = length < 0;
	for (int i = 0; i < 8 * 5; i += 5) {
		auto& offStart = positions[i], & offstop = positions[i + 1], & offset = positions[i +
			2];
		if (isCutting)adjustLoopPointsAfterCut(offStart, offstop, offset, point, point + cut);
		else adjustLoopPointsAfterInsert(offStart, offstop, offset, point, cut);
	}
}

tsl::Recording::Recording(tsl::AppState* appState)
	: positions{ loopdummy }
	, state{ std::make_shared<RecordingState>() }
	, _appState{ appState }
{


}

tsl::Recording::Recording(tsl::AppState* appState, std::vector<short> data[], int channels_, std::string fileName_) : Recording(appState)
{
	fileName = std::move(fileName_);
	channels = channels_;
	for (int i = 0; i < channels; i++) {
		buffer[i] = std::move(data[i]);
	}
	finishRecording();
	sr = _STATE->sr;
}
//Recording(tsl::AppState* appState, std::vector<short> data[], int channels, std::string fileName_ = "User Created");


void tsl::Recording::finishRecording() {
	const size_t maxSize = (size_t)_STATE->sr * 420;
	size_t afterSize = maxSize;
	for (int32_t i = 0; i < channels; i++) {
		if (buffer[i].size() > maxSize) buffer[i].resize(maxSize);
		afterSize = std::min(afterSize, buffer[i].size());
	}
	adjustPositions(0, afterSize);
	off = afterSize;
}



template<typename T>
bool tsl::Recording::push(std::vector<unsigned char>& src, int srcchans, RecordingDiff& diff) {
	auto buf = (T*)src.data();
	size_t total_samples = src.size() / sizeof(T);
	size_t stereo_pairs = total_samples / srcchans;
	const size_t maxFrames = (size_t)(_STATE->sr * 420);
	const size_t actualFrames = std::min(stereo_pairs, maxFrames > off ? maxFrames - off : 0);

	diff.type = RecordingDiff::Insertion;
	diff.oldSize = off;
	diff.point = off;
	diff.region = actualFrames;
	try {
		for (auto chan = 0; chan < channels; chan++)
			buffer[chan].resize(off + stereo_pairs);

		for (size_t smpl = 0; smpl < stereo_pairs; smpl++) {
			if (off + smpl >= (size_t)(_STATE->sr * 420)) break;
			int16_t data[2]{};
			if (channels == 1 && srcchans == 2) {
				data[0] = static_cast<int16_t>(
					(static_cast<double>(buf[smpl * 2]) +
						static_cast<double>(buf[smpl * 2 + 1]) + 1) / 2.);
			}
			else if (channels == 2 && srcchans == 1) {
				data[0] = data[1] = static_cast<int16_t>(buf[smpl]);
			}
			else {
				data[0] = static_cast<int16_t>(buf[smpl * srcchans]);
				data[1] = static_cast<int16_t>(buf[smpl * srcchans + 1]);
			}
			for (int i = 0; i < channels; i++)
				buffer[i][off + smpl] = data[i];
		}
	}
	catch (std::bad_alloc&) {
		showToast(_STATE, "Out of memory.");
		diff.type = RecordingDiff::Insertion;
		diff.region = 0;
		diff.oldSize = diff.newSize = off;
		return false;
	}

	size_t written = std::min(stereo_pairs, (size_t)(_STATE->sr * 420) - off);

	diff.type = RecordingDiff::Insertion;
	diff.oldSize = off;
	diff.point = off;
	diff.region = written;

	adjustPositions(off, written);
	off += written;

	diff.newSize = off;

	return true;
}
template bool tsl::Recording::push<int16_t>(std::vector<unsigned char>&, int, RecordingDiff& diff);
template bool tsl::Recording::push<float>(std::vector<unsigned char>&, int, RecordingDiff& diff);


#if defined(HAS_AUDIO) && defined(DOES_SOUNDEDITING)
bool tsl::Recording::edit(ParameterNum alg, RecordingDiff& diff, MYFLOAT positions[8 * 5]) {
	try {
		auto statetmp = state.load(std::memory_order_acquire);
		if (!statetmp)return false;
		const auto offStart = std::clamp(static_cast<size_t>(statetmp->off_start.load()), size_t{0}, off);
		const auto offStop  = std::clamp(static_cast<size_t>(statetmp->off_stop.load()),  size_t{0}, off);
		const auto offset_  = std::clamp(static_cast<size_t>(statetmp->offset.load()),    size_t{0}, off);
		const auto dist = std::clamp(offStop - offStart, size_t{0}, off);


		if (alg == EDITORMAXIMIZE) {

			if (dist <= 0)
				return false;

			double peak = 0.0;

			// Find peak only inside selection
			for (int32_t i = 0; i < channels; i++) {
				auto& buf = buffer[i];

				for (size_t s = offStart; s < offStop; s++) {
					peak = std::max(
						peak,
						std::abs((double)buf[s]));
				}
			}

			peak *= CONVMYFLT;

			if (peak > 0.0 && peak < 0.99) {

				double gain = 1.0 / peak;

				// Apply gain only inside selection
				for (int32_t i = 0; i < channels; i++) {
					auto& buf = buffer[i];

					for (size_t s = offStart; s < offStop; s++) {

						double v = buf[s] * gain;

						v = std::clamp(v, -32768.0, 32767.0);

						buf[s] = (int16_t)v;
					}
				}
			}
			else {
				return false;
			}
		}
		else if (alg == EDITORREVERSE) {
			if (dist <= 0) return false;


			for (int32_t i = 0; i < _STATE->channels; i++) {
				auto& buf = buffer[i];
				std::reverse(buf.begin() + offStart, buf.begin() + offStop);
			}

		}
		else if (alg == EDITORFADEIN || alg == EDITORFADEOUT) {
			if (dist <= 0) return false;


			for (int32_t i = 0; i < channels; i++) {
				auto& buf = buffer[i];

				for (size_t s = 0; s < dist; s++) {
					double t = static_cast<double>(s) / static_cast<double>(dist);

					double gain = (alg == EDITORFADEIN) ? t : (1.0 - t);

					double v = buf[offStart + s] * gain;
					buf[offStart + s] = (int16_t)v;
				}
			}
		}
		else if (alg == EDITORINSERTSILENCE) {
			const size_t maxSize = (size_t)_STATE->sr * 420;
			const size_t actualInsert = std::min(static_cast<size_t>(_STATE->sr),
				(offset_ < maxSize) ? (maxSize - offset_) : 0);
			if (actualInsert == 0) return false;


			for (int32_t i = 0; i < channels; i++) {
				auto& buf = buffer[i];
				const size_t pos = offset_;
				const size_t oldSize = buf.size();
				const size_t tailLen = (oldSize > pos) ? (oldSize - pos) : 0;
				const size_t tailDest = pos + actualInsert;
				const size_t movable = std::min(tailLen,
					(tailDest < maxSize) ? (maxSize - tailDest) : 0);
				const size_t newSize = std::min(tailDest + movable, maxSize);

				buf.resize(newSize);

				if (movable > 0)
					std::memmove(buf.data() + tailDest,
						buf.data() + pos,
						movable * sizeof(int16_t));

				std::memset(buf.data() + pos, 0, actualInsert * sizeof(int16_t));
			}
		}
		else if (alg == EDITORCOPY || alg == EDITORCUT) {
			if (dist <= 0) return false;
			for (int32_t i = 0; i < channels; i++) {
				_STATE->editorCopyBuf[i].assign(buffer[i].begin() + offStart, buffer[i].begin() + offStop);
				if (alg == EDITORCUT) {
					buffer[i].erase(buffer[i].begin() + offStart, buffer[i].begin() + offStop);
				}
			}
		}
		else if (alg == EDITORINSERT) {
			const size_t maxSize = (size_t)_STATE->sr * 420;
			for (int i = 0; i < channels; i++)
				if (_STATE->editorCopyBuf[i].empty()) return false;

			for (int i = 0; i < channels; i++) {
				auto& buf = buffer[i];
				auto& src = _STATE->editorCopyBuf[i];
				const size_t pos = offset_; // already clamped to <= buf.size()
				const size_t oldSize = buf.size();
				const size_t insertCount = src.size();

				if (pos >= maxSize) continue;

				// How many samples fit after pos within the cap
				const size_t roomAfterPos = maxSize - pos;
				const size_t actualInsert = std::min(insertCount, roomAfterPos);

				if (actualInsert == 0) continue;

				// Tail = samples currently sitting at [pos, oldSize)
				const size_t tailLen = (oldSize > pos) ? (oldSize - pos) : 0;

				// Where the tail would land
				const size_t tailDest = pos + actualInsert;

				// How many tail samples survive within the cap
				const size_t movable = std::min(tailLen,
					(tailDest < maxSize) ? (maxSize - tailDest) : 0);

				const size_t newSize = std::min(tailDest + movable, maxSize);
				buf.resize(newSize);

				if (movable > 0)
					std::memmove(buf.data() + tailDest,
						buf.data() + pos,
						movable * sizeof(int16_t));

				std::memcpy(buf.data() + pos,
					src.data(),
					actualInsert * sizeof(int16_t));
			}

		}
		else if (alg == EDITORPASTE) {
			for (int i = 0; i < channels; i++)
				if (_STATE->editorCopyBuf[i].empty()) return false;

			size_t maxSize = sr * 420;
			int32_t peak = 0;

			size_t maxPaste = 0;

			for (int i = 0; i < channels; i++) {
				maxPaste = std::max(
					maxPaste,
					_STATE->editorCopyBuf[i].size());
			}

			for (int32_t i = 0; i < channels; i++) {
				auto& src = _STATE->editorCopyBuf[i];
				auto& dst = buffer[i];
				size_t esize = src.size();

				// non-overlapping region of dst before offset
				for (size_t j = 0; j < offset_ && j < dst.size(); j++)
					peak = std::max(peak, (int32_t)std::abs(dst[j]));

				// overlapping region
				for (size_t smpl = 0; smpl < esize; smpl++) {
					size_t idx = offset_ + smpl;
					if (idx >= maxSize) break;
					int32_t d = (idx < dst.size()) ? dst[idx] : 0;
					int32_t mixed = d + src[smpl];
					peak = std::max(peak, std::abs(mixed));
				}

				// non-overlapping region of dst after paste region
				size_t endIdx = offset_ + esize;
				for (size_t j = endIdx; j < dst.size(); j++)
					peak = std::max(peak, (int32_t)std::abs(dst[j]));
			}

			double gain = (peak > 32767) ? (32767.0 / peak) : 1.0;
			for (int32_t i = 0; i < channels; i++) {
				auto& src = _STATE->editorCopyBuf[i];
				auto& dst = buffer[i];
				size_t esize = src.size();
				size_t newSize = std::min(std::max(dst.size(), offset_ + esize), maxSize);
				dst.resize(newSize);

				for (size_t smpl = 0; smpl < esize; smpl++) {
					size_t idx = offset_ + smpl;
					if (idx >= newSize) break;
					int32_t d = (idx < dst.size()) ? dst[idx] : 0;
					int32_t mixed = (int32_t)((d + src[smpl]) * gain);
					dst[idx] = (int16_t)std::clamp(mixed, -32768, 32767);
				}
			}
		}

		// ensure no channel exceeds max
		const size_t maxSize = (size_t)_STATE->sr * 420;
		size_t afterSize = maxSize;
		for (int32_t i = 0; i < channels; i++) {
			if (buffer[i].size() > maxSize) buffer[i].resize(maxSize);
			afterSize = std::min(afterSize, buffer[i].size());
		}

		ptrdiff_t ddist = (ptrdiff_t)afterSize - (ptrdiff_t)off;

		diff.oldSize = off;
		diff.newSize = afterSize;

		switch (alg) {
		case EDITORMAXIMIZE:
		case EDITORFADEIN:
		case EDITORFADEOUT:
		case EDITORGRAINENV:
			diff.type = RecordingDiff::Modification;
			diff.point = offStart;
			diff.region = offStop - offStart;
			break;

		case EDITORREVERSE:
			diff.type = RecordingDiff::Reversal;
			diff.point = offStart;
			diff.region = offStop - offStart;
			break;

		case EDITORINSERT:
		case EDITORINSERTSILENCE:
			diff.type = RecordingDiff::Insertion;
			diff.point = offset_;
			diff.region = afterSize - off;
			break;

		case EDITORCUT:
			diff.type = RecordingDiff::Removal;
			diff.point = offStart;
			diff.region = off - afterSize;
			break;

		case EDITORPASTE:
			diff.type = RecordingDiff::Modification;
			diff.point = offset_;
			diff.region = std::min(offset_ + _STATE->editorCopyBuf[0].size(), afterSize);
			break;

		default:
			diff.type = RecordingDiff::Unchanged;
			break;
		}
		if (alg == EDITORPASTE) {
			// only the region beyond original end grew
			if (ddist > 0) {
				adjustPositions(off, ddist);
				off = afterSize;
			}
		}
		else if (ddist != 0) {
			if (alg == EDITORCUT)
				adjustPositions((double)offStart, (double)ddist);
			else adjustPositions((double)offset_, (double)ddist);
			off = afterSize;

		}
		return true;
	}
	catch (const std::bad_alloc&) {
		showToast(_STATE, "Out of memory.");
		return false;
	}
}
#endif // DOES_SOUNDEDITING && HAS_AUDIO
