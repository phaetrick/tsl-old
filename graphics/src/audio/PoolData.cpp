#ifdef GRAINSTORM
#include "audio/Recording.h"
#include "app.h"
#include "tools/SwapRingPool.h"
#include "tools/LockFreeList.h"

static tsl::SwapPagePool::Allocation getFromPool(tsl::AppState* _appState, size_t bytes) {
	tsl::SwapPagePool::Allocation alloc{};
	if (bytes == 0) return alloc;
	// Step 1: acquire memory from swap pool, evicting if needed
	constexpr int kMaxEvictions = 100;
	int evictions = 0;
	int deletedPos{};

	while (!(alloc = _STATE->swapPool.acquire(bytes)).ptr) {
		if (evictions >= kMaxEvictions) return alloc;
		bool evicted = false;
		deletedPos = 0;
		for (auto& rec : _STATE->recordings) {
			std::lock_guard lk(rec.mutex);
			if (rec.isValid()) {
				rec.release();
				evictions++;
				evicted = true;
				break;
			}
			++deletedPos;
		}
		if (!evicted) {
			LOGE("Swap error. Fail after %d evictions.", evictions);
			return alloc;
		}
	}

	
	LOGE("Swap op. %.3f MB. Deleted: %d (%d - %d)", bytes / (1024. * 1024.),

		evictions,
		deletedPos - evictions,
		deletedPos);
	return alloc;

}

uint16_t tsl::allocRecording(AppState* _appState, size_t numFrames, int channels) {
	auto bytes = numFrames * channels * sizeof(short);
	if (bytes == 0) return NO_SOUND_PRESENT;

	auto  alloc = getFromPool(_appState, bytes);
	if (alloc.ptr == nullptr)return INVALID_POOL_HANDLE;
	// Step 2: construct new entry in recpool
	auto pos = _appState->recordings.emplace(_appState);

	LOGE("New Pool Recording created at pos: %d",
		pos + 1);

	auto& pd = _appState->recordings[pos];

	std::lock_guard lk(pd.mutex);
	pd.allocation = alloc;
	pd.mem = static_cast<short*>(alloc.ptr);
	pd.off = numFrames;
	pd.channels = channels;
	return pos;
}

void tsl::Recording::saveForPool(bool copyBytes) {
	if (poolHandle >= tsl::NO_SOUND_PRESENT)return;
	auto& pd = _STATE->recordings[poolHandle];
	auto s = state.load();
	if (s) {
		pd.currentState = *s;
	}
	else pd.currentState = loopdummy.at(0);

	pd.positions = positions;

	if (copyBytes) {
		for (int i = 0; i < off; i++)
			for (int chan = 0; chan < channels; chan++)
				pd.mem[i * channels + chan] = buffer[chan][i];
	}
}

uint16_t tsl::Recording::pushSwap() {
	if (off == 0) {
		return tsl::NO_SOUND_PRESENT;
	}
	else {
		if (poolHandle < tsl::NO_SOUND_PRESENT) {
			auto& pd = _appState->recordings[poolHandle];
			std::lock_guard lk(pd.mutex);
			if (pd.isValid()) {
				saveForPool(false);
				return poolHandle;
			}
		}

		if ((poolHandle = allocRecording(_appState, off, channels)) < tsl::NO_SOUND_PRESENT) {
			auto& pd = _appState->recordings[poolHandle];
			pd.incRefCount();
			pd.fileName = fileName;
			pd.numEdits = numEdits;
			pd.lastEdit = lastEdit;
			saveForPool(true);

		}
		return poolHandle;
	}
}

void tsl::Recording::saveForPool(const RecordingDiff& diff, const SwapProgressSink* sink)
{
	if (poolHandle >= tsl::NO_SOUND_PRESENT)
		return;

	auto& pd = _STATE->recordings[poolHandle];
	std::lock_guard lk(pd.mutex);
	auto s = state.load(std::memory_order_acquire);
	if (s)
		pd.currentState = *s;
	else
		pd.currentState = loopdummy.at(0);

	pd.positions = positions;
	pd.diff = diff;

	// determine payload region
	size_t copyStart = 0;
	size_t copyEnd = 0;

	switch (diff.type) {

	case RecordingDiff::Modification:
		copyStart = std::min<size_t>(diff.point, off);
		copyEnd = std::min<size_t>(diff.point + diff.region, diff.oldSize);  // not off
		break;

	case RecordingDiff::Removal:
		copyStart = std::min<size_t>(diff.point, off);
		copyEnd = std::min<size_t>(diff.point + diff.region, off);
		break;

	case RecordingDiff::Replacement:
		copyStart = 0;
		copyEnd = std::min<size_t>(diff.oldSize, off);
		break;

	case RecordingDiff::Insertion:
	case RecordingDiff::Reversal:
		// metadata only — keep allocation alive
		//pd.off = 0;
		return;

	default:
		//pd.off = 0;
		return;
	}

	if (copyEnd <= copyStart) {
		//pd.off = 0;
		return;
	}
	/*
	* #include "zstd.h"
const size_t allocBytes = ZSTD_compressBound(rawBytes);
LOGE("Poolpush - Raw size: %f MB Compressed: %f MB", rawBytes / (1024. * 1024.), allocBytes / (1024. * 1024.));
// allocate based on compress bound
SwapPagePool::Allocation newAlloc = getFromPool(_STATE, allocBytes);
if (!newAlloc.ptr) return;
// serialize raw into a temp buffer
std::vector<short> tmp(payloadFrames * channels);
size_t dst1 = 0;
for (int chan = 0; chan < channels; chan++) {
	short prev = 0;
	for (size_t i = copyStart; i < copyEnd; i++) {
		short s = buffer[chan][i];
		tmp[dst1++] = s - prev;
		prev = s;
	}
}
// compress deltas
size_t compressed = ZSTD_compress(pd.mem, allocBytes, tmp.data(), rawBytes, 3);
LOGE("Compressed size %f", compressed / (1024. * 1024.));
_STATE->swapPool.release(newAlloc.token);
*/

	const size_t payloadFrames = copyEnd - copyStart;

	const size_t requiredBytes = payloadFrames * channels * sizeof(short);
	// LOGE("T %d P %d, R %d O %d P+r  %d ", diff.type, diff.point, diff.region, diff.oldSize, diff.point + diff.region);
	if (!pd.mem ||
		pd.off * channels * sizeof(short) < requiredBytes)
	{
		SwapPagePool::Allocation newAlloc = getFromPool(_STATE, requiredBytes);
		if (!newAlloc.ptr)
			return;

		if (pd.mem) {
			_STATE->swapPool.release(pd.allocation.token);
			pd.allocation.token = {};
		}

		pd.allocation = newAlloc;
		pd.mem = static_cast<short*>(newAlloc.ptr);
	}

	pd.off = payloadFrames;

	size_t dst = 0;
	/* Reported every 64k frames rather than per frame: the stores are cheap but
	   the panel only redraws once a frame, and this loop is the hot part of a
	   copy that can run for seconds. InfoPanel is perm, so it picks the values
	   up on its own -- no redraw from here, and none would be safe anyway,
	   since this runs with pd.mutex held. */
	constexpr size_t kProgressStride = 1u << 16;
	const size_t bytesPerFrame = (size_t)channels * sizeof(short);
	auto report = [&](size_t framesDone) {
		if (!sink) return;
		if (sink->fraction)
			sink->fraction->store(payloadFrames ? (float)framesDone / (float)payloadFrames : 1.f,
				std::memory_order_relaxed);
		if (sink->frames)
			sink->frames->store((long)framesDone, std::memory_order_relaxed);
		if (sink->bytes)
			sink->bytes->store((long)(framesDone * bytesPerFrame), std::memory_order_relaxed);
	};

	for (size_t i = copyStart; i < copyEnd; i++) {
		const size_t framesDone = i - copyStart;
		if ((framesDone & (kProgressStride - 1)) == 0)
			report(framesDone);
		for (int chan = 0; chan < channels; chan++)
			pd.mem[dst++] = buffer[chan][i];
	}
	report(payloadFrames);
}

uint16_t tsl::Recording::pushSwap(
	const RecordingDiff& diff, const SwapProgressSink* sink)
{
	if (off == 0)
		return tsl::NO_SOUND_PRESENT;

	if (poolHandle < tsl::NO_SOUND_PRESENT) {

		auto& pd =
			_appState->recordings[poolHandle];

		std::lock_guard lk(pd.mutex);

		if (pd.isValid()) {

			saveForPool(diff, sink);
			return poolHandle;
		}
	}

	// tiny initial allocation
	if ((poolHandle =
		allocRecording(
			_appState,
			1,
			channels))
		< tsl::NO_SOUND_PRESENT)
	{
		auto& pd =
			_appState->recordings[poolHandle];
		std::lock_guard lk(pd.mutex);

		pd.incRefCount();

		pd.fileName = fileName;
		pd.numEdits = numEdits;
		pd.lastEdit = lastEdit;

		saveForPool(diff, sink);
	}

	return poolHandle;
}

static void restoreSamples(
	const tsl::AudioPoolData& pd,
	size_t& src,
	std::shared_ptr<tsl::Recording>& rec,
	size_t dstIndex,
	int srcChan,
	int dstChan)
{
	if (srcChan == dstChan) {
		for (int chan = 0; chan < dstChan; chan++)
			rec->buffer[chan][dstIndex] = pd.mem[src++];
	}
	else if (srcChan < dstChan) {
		short last = 0;
		for (int chan = 0; chan < srcChan; chan++) {
			last = pd.mem[src++];
			rec->buffer[chan][dstIndex] = last;
		}
		for (int chan = srcChan; chan < dstChan; chan++)
			rec->buffer[chan][dstIndex] = last;
	}
	else {
		for (int chan = 0; chan < dstChan - 1; chan++)
			rec->buffer[chan][dstIndex] = pd.mem[src++];
		int32_t mix = 0;
		for (int chan = dstChan - 1; chan < srcChan; chan++)
			mix += pd.mem[src++];
		rec->buffer[dstChan - 1][dstIndex] = (short)std::clamp(
			mix, (int32_t)INT16_MIN, (int32_t)INT16_MAX);
	}
}

std::shared_ptr<tsl::Recording> tsl::Recording::makeFromSwap(tsl::AppState* _appState, uint16_t recNum) {
	if (recNum == INVALID_POOL_HANDLE || recNum == NO_SOUND_PRESENT) return {};
	auto& pd = _STATE->recordings[recNum];
	std::lock_guard lk(pd.mutex);
	if (!pd.isValid()) return {};

	const int srcChan = pd.channels;
	const int dstChan = _STATE->channels;
	const int frames = pd.off;

	auto rec = std::make_shared<Recording>(_appState);
	rec->sr = _STATE->sr;
	rec->channels = dstChan;
	rec->off = frames;
	rec->fileName = pd.fileName;
	rec->positions = pd.positions;
	rec->poolHandle = recNum;
	rec->lastEdit = pd.lastEdit;
	rec->numEdits = pd.numEdits;
	auto s = rec->state.load();
	if (s) {
		*s = pd.currentState;
	}
	try {
		for (int i = 0; i < dstChan; i++)
			rec->buffer[i].resize(frames);
	}
	catch (std::bad_alloc) { return {}; }

	if (srcChan == dstChan) {
		// fast path
		for (int i = 0; i < frames; i++)
			for (int chan = 0; chan < dstChan; chan++)
				rec->buffer[chan][i] = pd.mem[i * srcChan + chan];
	}
	else if (srcChan < dstChan) {
		// src has fewer channels — copy available, duplicate last into extras
		for (int i = 0; i < frames; i++) {
			for (int chan = 0; chan < srcChan; chan++)
				rec->buffer[chan][i] = pd.mem[i * srcChan + chan];
			// fill extra channels by repeating the last src channel
			for (int chan = srcChan; chan < dstChan; chan++)
				rec->buffer[chan][i] = pd.mem[i * srcChan + (srcChan - 1)];
		}
	}
	else {
		// src has more channels — downmix extras into last dst channel
		for (int i = 0; i < frames; i++) {
			for (int chan = 0; chan < dstChan - 1; chan++)
				rec->buffer[chan][i] = pd.mem[i * srcChan + chan];
			// mix remaining src channels into last dst channel
			int32_t mix = 0;
			for (int chan = dstChan - 1; chan < srcChan; chan++)
				mix += pd.mem[i * srcChan + chan];
			rec->buffer[dstChan - 1][i] = static_cast<short>(
				std::clamp(mix, (int32_t)INT16_MIN, (int32_t)INT16_MAX));
		}
	}
	pd.incRefCount();
	return rec;
}


std::shared_ptr<tsl::Recording>
tsl::Recording::makeFromSwap(
	tsl::AppState* _appState,
	std::shared_ptr<tsl::Recording> oldRecording,
	uint16_t recNum)
{
	if (recNum == INVALID_POOL_HANDLE ||
		recNum == NO_SOUND_PRESENT)
		return {};

	auto& pd = _STATE->recordings[recNum];
	std::lock_guard lk(pd.mutex);

	if (!pd.isValid())  // keep entry alive check
		return {};

	std::shared_ptr<Recording> rec;

	if (oldRecording) {
		rec = std::make_shared<Recording>(*oldRecording);
	}
	else {
		rec = std::make_shared<Recording>(_appState);
		rec->sr = _STATE->sr;
		rec->channels = _STATE->channels;
		rec->off = pd.diff.oldSize;
		try {
			for (int i = 0; i < rec->channels; i++)
				rec->buffer[i].resize(rec->off);
		}
		catch (std::bad_alloc&) { return {}; }
	}

	rec->fileName = pd.fileName;
	rec->positions = pd.positions;
	rec->lastEdit = pd.lastEdit;
	rec->numEdits = pd.numEdits;
	rec->poolHandle = recNum;  // add this

	auto s = rec->state.load();
	if (s) *s = pd.currentState;

	const auto& diff = pd.diff;
	const int srcChan = pd.channels;
	const int dstChan = rec->channels;
	switch (diff.type) {

		// ----------------------------------------------------------------
		// INSERTION undo — erase inserted region
		// ----------------------------------------------------------------
	case RecordingDiff::Insertion:
	{
		const size_t pos = std::min<size_t>(diff.point, rec->off);
		const size_t len = std::min<size_t>(diff.region, rec->off - pos);

		for (int chan = 0; chan < dstChan; chan++) {
			auto& buf = rec->buffer[chan];
			buf.erase(buf.begin() + pos, buf.begin() + pos + len);
		}
		rec->off -= len;
		break;
	}

	// ----------------------------------------------------------------
	// REMOVAL undo — reinsert deleted region
	// ----------------------------------------------------------------
	case RecordingDiff::Removal:
	{
		const size_t pos = std::min<size_t>(diff.point, rec->off);
		const size_t len = pd.off;  // payload frame count

		try {
			for (int chan = 0; chan < dstChan; chan++)
				rec->buffer[chan].resize(rec->off + len);
		}
		catch (std::bad_alloc&) { return {}; }

		for (int chan = 0; chan < dstChan; chan++) {
			auto& buf = rec->buffer[chan];
			std::memmove(
				buf.data() + pos + len,
				buf.data() + pos,
				(rec->off - pos) * sizeof(short));
		}

		size_t src = 0;
		for (size_t i = 0; i < len; i++) {
			const size_t dstIndex = pos + i;
			restoreSamples(pd, src, rec, dstIndex, srcChan, dstChan);
		}

		rec->off = diff.oldSize;
		break;
	}

	// ----------------------------------------------------------------
	// MODIFICATION undo — restore previous samples
	// ----------------------------------------------------------------
	case RecordingDiff::Modification:
	{
		// resize buffer down if paste shrunk it (or grew it)
		if (diff.oldSize != diff.newSize) {
			try {
				for (int chan = 0; chan < dstChan; chan++)
					rec->buffer[chan].resize(diff.oldSize);
			}
			catch (std::bad_alloc&) { return {}; }
		}
		const size_t start = std::min<size_t>(diff.point, diff.oldSize);
		const size_t end = std::min<size_t>(diff.point + diff.region, diff.oldSize);
		const size_t frames = (end > start) ? (end - start) : 0;

		size_t src = 0;
		for (size_t i = 0; i < frames; i++) {
			const size_t dstIndex = start + i;
			if (dstIndex >= diff.oldSize) break;
			restoreSamples(pd, src, rec, dstIndex, srcChan, dstChan);
		}

		rec->off = diff.oldSize;
		break;
	}

	// ----------------------------------------------------------------
	// REPLACEMENT undo — restore entire old buffer
	// ----------------------------------------------------------------
	case RecordingDiff::Replacement:
	{
		const size_t frames = pd.off;

		try {
			for (int chan = 0; chan < dstChan; chan++)
				rec->buffer[chan].resize(frames);
		}
		catch (std::bad_alloc&) { return {}; }

		size_t src = 0;
		for (size_t i = 0; i < frames; i++)
			restoreSamples(pd, src, rec, i, srcChan, dstChan);

		rec->off = diff.oldSize;
		break;
	}

	// ----------------------------------------------------------------
	// REVERSAL undo — just reverse the same region again
	// ----------------------------------------------------------------
	case RecordingDiff::Reversal:
	{
		const size_t pos = std::min<size_t>(diff.point, rec->off);
		const size_t len = std::min<size_t>(diff.region, rec->off - pos);

		for (int chan = 0; chan < dstChan; chan++) {
			auto& buf = rec->buffer[chan];
			std::reverse(buf.begin() + pos, buf.begin() + pos + len);
		}
		break;
	}

	default:
		break;
	}
	// after all diff type handling, before pd.incRefCount()
	size_t minBufSize = SIZE_MAX;
	for (int i = 0; i < rec->channels; i++)
		minBufSize = std::min(minBufSize, rec->buffer[i].size());

	if (rec->off > minBufSize) {
		showToast(_STATE, "This should never happen. Expected and actual bufsize mismatch after mod!");
		LOGE("This should never happen. Size mismatch between expected and actual buffer size! Type %d Exècted %d Actual %d", diff.type, rec->off, minBufSize);
	}
	rec->off = std::min(rec->off, minBufSize);
	pd.incRefCount();
	return rec;
}


void tsl::AudioPoolData::release() {
	std::lock_guard lk(mutex);
	if (isValid()) {
		_STATE->swapPool.release(allocation.token);
		allocation.token.count = 0;
		allocation.token.page = 0;
		allocation.ptr = nullptr;
		mem = nullptr;
		LOGE("Recording swap release.");
	}
};

void tsl::AudioPoolData::decRefCount() {
	std::lock_guard lk(mutex);
	if (isValid()) {
		if (refCount_ > 0) {
			refCount_--;

			if (refCount_ == 0) {
				release();
			}
			else LOGE("AudioPoolData refcount dec: %d", refCount_);
		}
		else LOGE("AudioPoolData refcount dec on 0. Should not happen.");

	}
	else LOGE("AudioPoolData refcount dec on invalid state: %d", refCount_);
}

void tsl::AudioPoolData::incRefCount() {
	std::lock_guard lk(mutex);
	if (isValid()) {
		refCount_++;
		LOGE("AudioPoolData refcount inc: %d", refCount_);
	}
	else LOGE("AudioPoolData refcount inc on invalid state:", refCount_);
};
#endif // GRAINSTORM
