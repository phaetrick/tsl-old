#pragma once
#include <vector>
#include <array>
#include <mutex>
#include <atomic>
#include <string>
#include <types.h>
#include <defines.h>
#include "tools/AtomicSharedPtr.h"
#include "tools/SwapRingPool.h"
#include "params.h"


namespace tsl {
	struct AppState;
	void
		adjustLoopPointsAfterCut(double& loopStart, double& loopEnd, double& readPos, double cutStart,
			double cutEnd);

	void adjustLoopPointsAfterInsert(double& loopStart, double& loopEnd, double& readPos,
		double insertPos, double insertLength);
	void adjustPositions(double positions[], double point, double length);


	enum PositionIndex {
		LoopStart,
		LoopStop,
		ReadPos,
		BounceType,
		PlaybackDirection,
		WaveformStartPosition,
		WaveformZoom,
		SizePositions
	};
	constexpr int numSavedPositions = 8;
	constexpr int positionsTotal = numSavedPositions * SizePositions;

	struct RecordingState;

	class SavedRecordingState : public std::array<double, SizePositions> {
	public:
		SavedRecordingState() = default;
		explicit SavedRecordingState(const RecordingState& other) { *this = other; }
		SavedRecordingState(std::initializer_list<double> il) {
			std::size_t i = 0;
			for (auto v : il) { if (i >= SizePositions) break; (*this)[i++] = v; }
		}
		SavedRecordingState& operator=(const RecordingState& other);
	};

	uint16_t allocRecording(AppState*, size_t numFrames, int channels);



	inline constexpr uint16_t INVALID_POOL_HANDLE = UINT16_MAX;
	inline constexpr uint16_t NO_SOUND_PRESENT = UINT16_MAX - 1;

	extern std::array<SavedRecordingState, numSavedPositions> loopdummy;

	struct WaveformPositionsUi;

	struct WaveformUndoRedo {
		uint32_t offset{};
		uint32_t loopBound{};
		int8_t playbackDir{ 1 };
		int8_t offsetChanged{};
	};

	struct RecordingDiff {
		enum Type : uint8_t {
			Unchanged = 0,
			Modification = 1,
			Insertion = 2,
			Removal = 3,
			Reversal = 4,
			Replacement = 5 // Complete Replacement for new file loads
		};
		Type type{};

		size_t oldSize{};
		size_t newSize{};
		size_t point{};
		size_t region{};

		RecordingDiff reverse() {
			RecordingDiff r{};
			r.oldSize = newSize;
			r.newSize = oldSize;
			switch (type) {
			case Removal:
				r.type = Insertion;
				r.point = point;
				r.region = region;
				return r;
			case Insertion:
				r.type = Removal;
				r.point = point;
				r.region = region;
				return r;
			case Modification:
				r.type = Modification;
				r.point = point;
				r.region = region;
				return r;
			case Replacement:
				r.type = Replacement;
				return r;  // sizes already swapped above

			case Reversal:
				r.type = Reversal;
				r.point = point;
				r.region = region;
				return r;
			case Unchanged:
			default: return *this;

			}
		}
		
	};

	struct alignas(64) AudioPoolData {
		explicit AudioPoolData(tsl::AppState* s) :_appState(s) {};
		bool isValid() { return allocation.token.valid(); }
		SwapPagePool::Allocation allocation{};
		void release();
		tsl::AppState* _appState{};
		size_t off{};
		size_t compressedSize{};  // 0 = uncompressed, >0 = compressed byte count
		int channels{};
		short* mem{};
		uint16_t numEdits{};
		uint16_t lastEdit{};
		RecordingDiff diff{};
		std::vector<WaveformUndoRedo> waveformUndoRedos[4]{};
		std::vector<SavedRecordingState> loadUndoRedos[4]{};
		int posWaveform[4]{};
		int posLoad[4]{};
		SavedRecordingState currentState{};
		std::array<SavedRecordingState, numSavedPositions> positions{};
		std::recursive_mutex mutex{};
		std::string fileName{};
		int refCount_{};
		void decRefCount();
		void incRefCount();

	};

	struct alignas(64) RecordingState {
		std::atomic<tsl::parameters::WaveformState> waveformState{};
		std::atomic<MYFLOAT> offset{};
		std::atomic<MYFLOAT> off_start{};
		std::atomic<MYFLOAT> off_stop{};
		std::atomic<int> bounceType{};
		std::atomic<int> playbackDir{ 1 };
		RecordingState() = default;
		explicit RecordingState(const SavedRecordingState& other) { *this = other; }
		RecordingState(const RecordingState& other);
		RecordingState& operator=(const RecordingState& other);
		RecordingState& operator=(const SavedRecordingState& other);

	};



	struct Recording {

		Recording() = delete;
		
		explicit Recording(tsl::AppState* appState);

		Recording(tsl::AppState* appState, std::string fileName_, int sr_, int channels_)
			: Recording(appState) {
			fileName = std::move(fileName_);
			sr = sr_;
			channels = channels_;
		}
		Recording(tsl::AppState* appState, std::vector<short> data[], int channels, std::string fileName_ = "User Created");



		Recording(const Recording& other);

		~Recording();


		Recording& operator=(const tsl::Recording& other);

		
		uint16_t pushSwap();
		static std::shared_ptr<Recording> makeFromSwap(tsl::AppState*, uint16_t pd);
		static std::shared_ptr<tsl::Recording> makeFromSwap(tsl::AppState* _appState, std::shared_ptr<tsl::Recording> oldRecording, uint16_t recNum);
		void saveForPool(bool copyBytes);
		/* Optional live counters for a long saveForPool copy. Every member may
		   be null and the caller owns all of them; they are only stored to.
		   Deliberately mirrors how the decoder reports (DecoderAndroid.cpp):
		   frames and bytes count UP toward the total rather than holding it, so
		   the same InfoPanel render path reads correctly for both. */
		struct SwapProgressSink {
			std::atomic<float>* fraction{};   // 0..1
			std::atomic<long>*  frames{};     // frames copied so far
			std::atomic<long>*  bytes{};      // bytes copied so far
		};

		void saveForPool(const RecordingDiff&, const SwapProgressSink* sink = nullptr);
		uint16_t pushSwap(const RecordingDiff&, const SwapProgressSink* sink = nullptr);
#if defined(HAS_AUDIO) && defined(DOES_SOUNDEDITING)
		bool edit(ParameterNum algorithm, RecordingDiff &, MYFLOAT positions[8 * 5] = nullptr);
#endif
		template<typename T>
		bool push(std::vector<unsigned char>& src, int srcchannels, RecordingDiff& diff);
		void finishRecording();
		void adjustPositions(double point, double length);
		void loadPositions(int slot);
		void savePositions(int slot);
		tsl::AppState* _appState{};
		size_t off{};
		int sr{}, channels{};
		int trackIndex{};
		uint16_t poolHandle{ tsl::NO_SOUND_PRESENT };
		uint16_t numEdits{};
		uint16_t lastEdit{};
		std::array<SavedRecordingState, numSavedPositions> positions;
		std::atomic_bool ismoving{};
		std::atomic_bool isresizing{};
		std::string fileName{};
		std::string statusMessage{};
		std::vector<short> buffer[MAX_CHANNELS];
		alignas(64) tsl::AtomicSharedPtr<RecordingState> state{};
		alignas(64) std::atomic<bool> dorendering{};
	};
}