#pragma once
//
// Created by pr on 17.09.23.
//

#ifndef GRAINSTORM_PARAMS_H
#define GRAINSTORM_PARAMS_H
#include <atomic>
#include <string>
#include <functional>
#include <algorithm>
#include <span>
#include <string_view>
#include <cstdint>
#ifndef MYFLOAT
#define MYFLOAT double
#endif

enum ParameterType : uint8_t{
	ParameterType_bool = 0,
	ParameterType_double = 1,
	ParameterType_enum = 2,
};

inline constexpr double octaveCurveMin = 0.0625; // 1/16
inline constexpr double octaveCurveMax = 16.0;   // 16

namespace tsl {
	namespace graphics {
		class View;
	}
	struct AppState;
	namespace parameters {

		struct Value {
			operator double()    const { return f; }
			operator float()    const { return f; }
			operator int()      const { return i; }
			operator uint32_t() const { return u; }
			operator bool() const { return b; }

			Value& operator=(double    v) { f = v; return *this; }
			Value& operator=(float    v) { f = v; return *this; }
			Value& operator=(int      v) { i = v; return *this; }
			Value& operator=(uint32_t v) { u = v; return *this; }
			Value& operator=(bool v) { b = v; return *this; }

			bool operator==(const Value& o) const noexcept { return u == o.u; }
			bool operator!=(const Value& o) const noexcept { return u != o.u; }

			bool operator==(double    v) const noexcept { return f == (float)v; }
			bool operator==(float     v) const noexcept { return f == v; }
			bool operator==(int       v) const noexcept { return i == v; }
			bool operator==(uint32_t  v) const noexcept { return u == v; }
			bool operator==(bool      v) const noexcept { return b == v; }

			template<typename T>
			bool operator!=(T v) const noexcept { return !(*this == v); }

			union {
				float    f;
				int      i;
				uint32_t u;
				bool b;
			};
		};



		enum Eventtype : uint8_t {
			paramUpdate = 0,
			Power = 1,
			lfoMin = 2,
			lfoMax = 3,
			Recording = 4,
			lfoDest = 5,
			SpecialAction = 6,
			TextEvent = 7,
			Rerender = 8,
			Follower = 9,
			FxOrder = 10,
			Preset = 11,
			LoopFromExt = 12,
			NoParam = UINT8_MAX,
		};
		enum EventSubtype : uint8_t {
			fxOrderGrain = 0,
			fxOrderFx = 1,
			fxOrderSt = 2,
			normalTextEvent = 0,
			syncEvent = 1,
			LoopLoad1 = 0,
			LoopLoad2 = 1,
			LoopLoad3 = 2,
			LoopLoad4 = 3,
			LoopLoad5 = 4,
			LoopLoad6 = 5,
			LoopLoad7 = 6,
			LoopLoad8 = 7,
			LoopSave1 = 8,
			LoopSave2 = 9,
			LoopSave3 = 10,
			LoopSave4 = 11,
			LoopSave5 = 12,
			LoopSave6 = 13,
			LoopSave7 = 14,
			LoopSave8 = 15,
			offsetFromUi = 16,
			offStopFromUi = 17,
			offStartFromUi = 18,
			recordingChange = 19,
			offset = 20,
			offStart = 21,
			offStop = 22,
			waveformView = 23,
			bounceType = 24,
			playbackDir = 25,
			off = 28,
			playDirFromUi = 29,
			powerFx = 0,
			powerStereoFx = 1,
			powerGrainFx = 2,
			bypassFx = 3,
			bypassStereoFx = 4,
			bypassGrainFx = 5,
			powerTrack = 6,
			followerPower = 7,
			followerMin = 0,
			followerMax = 1,
			followerGain = 2,
			followerAtt = 3,
			followerDec = 4,
			followerSidechain = 5,
			sequencerState = 14,
			grainGenState = 15,
		};

		struct SequencerState {
			int16_t grainscount{};
			int16_t silencecount{};
			int8_t currentnote{};
			int8_t arpstep{};
			int8_t arpcycledir{1};
			int8_t reserved{};
			auto operator<=>(const SequencerState&) const = default;
			bool operator==(const SequencerState&) const = default;
		};

		struct GraingenState {
			int32_t count[2]{};
			auto operator<=>(const GraingenState&) const = default;
			bool operator==(const GraingenState&) const = default;
		};

		struct SamplePosState {
			uint32_t newPos{};
			uint32_t oldPos{};
		};


		struct ValueState {
			float newValue{};
			float oldValue{};
		};

		struct PowerState {
			int32_t pos{};
			uint32_t pow{};

			static double normalize(const PowerState& s) {
				uint64_t packed =
					(uint64_t(uint32_t(s.pos)) << 1) |
					(s.pow ? 1ULL : 0ULL);

				constexpr double MAX =
					double((1ULL << 33) - 1);

				return double(packed) / MAX;
			}

			static PowerState denormalize(double x) {
				constexpr uint64_t MAX =
					(1ULL << 33) - 1;

				uint64_t packed =
					uint64_t(std::llround(
						std::clamp(x, 0.0, 1.0) * MAX));

				PowerState s;

				s.pow =
					packed & 1;

				s.pos =
					int32_t(uint32_t(packed >> 1));

				return s;
			}
		};

		struct WaveformState {
			bool operator==(const WaveformState& o) const noexcept = default;
			bool operator!=(const WaveformState& o) const noexcept = default;
			float startPos{};
			float zoom{1.f};
		};

		struct FxOrderState {
			int32_t oldPos{};
			int32_t newPos{};
		};


		struct MidiState {
			uint16_t min{};
			uint16_t max{};
			uint8_t channel{};
			uint8_t num{};
			uint8_t type{}; //Control / Enum / Bool
			int8_t inc{}; // -1 / + 1 for  enum
		};

		enum SenderFlags : uint32_t {
			None = 0,
			FromUi = 1,
			FromHistory = 2,
		};


		struct Event {
			Event() : value(0.0) {}
			
			~Event();

			Event(const Event& other);

			Event& operator=(const Event& other);

			Event(Event&& other) noexcept;

			Event& operator=(Event&& other) noexcept;

			enum EventFlags : uint8_t {
				Redraw = 1 << 0,
				Info = 1 << 1,
				History = 1 << 2,
				NoInfo = 1 << 3,
				ToAudioThread = 1 << 4,
				DoOnChange = 1 << 5,
				ToWorkerThread = 1 << 6,
				FromDaw = 1 << 7,
			};

			

			void setup(tsl::AppState*, int tindex, uint16_t id, uint16_t *displayParam = nullptr, std::atomic<MYFLOAT> **ref = nullptr);

			Event apply(tsl::AppState*, SenderFlags from);
			
			void applyFromExt(tsl::AppState*, SenderFlags from);

			MYFLOAT getCurrentValue(tsl::AppState*) const;

			MYFLOAT getDefaultValue(tsl::AppState*) const;

			uint16_t getDisplayParam() const;

			void toNormalized(tsl::AppState*);

			void fromNormalized(tsl::AppState*);

			MYFLOAT onChange(tsl::AppState*);

			void afterChange(tsl::AppState*);

			std::atomic<MYFLOAT>* getRef(tsl::AppState*) const;
			
			void setFlag(EventFlags flag, bool val = true) {
				if (val) flags |= flag;
				else flags &= ~flag;
			}

			bool getFlag(EventFlags flag) const {
				return (flags & flag) != 0;
			}

			void toString(tsl::AppState*, int& pos, char buffer[], size_t bufferSize, bool renderCurrentValue = false);

#ifdef PLUGIN_MODE
			int getPluginIndex(tsl::AppState*) const;
#endif

			static Event createEvent(uint8_t trackIndex, uint8_t eventType, uint16_t paramIndex = {}, double value = {}, uint8_t subType = EventSubtype{}, uint16_t groudId = {}, uint8_t flags = Event::History | Event::Redraw);
			static Event createRerenderEvent(uint8_t trackIndex, uint16_t paramIndex = {}, uint16_t groudId = {});
			static Event createTextEvent(uint8_t trackIndex, const char* text, uint16_t paramIndex = {}, uint8_t subType = EventSubtype{}, uint16_t groudId = {});

			bool operator==(const Event& other) const {
				return eventType == other.eventType &&
					trackIndex == other.trackIndex &&
					paramIndex == other.paramIndex &&
					subType == other.subType;
			}
			
			union { 
				uint16_t paramIndex{}; 
			    uint16_t poolHandle;
			};  // 2 bytes
			uint8_t eventType{};    // 1 byte
			uint8_t subType{};      // 1 byte
			uint8_t trackIndex{};   // 1 byte
			uint8_t flags{};        // 1 byte
			uint16_t groupId{};     // 2 bytes
			union {
				double value{};
				SequencerState sequencerState;
				GraingenState graingenState; 
				WaveformState waveformState;
				PowerState power;
				FxOrderState fxOrder;
				MidiState midiState;
				const char* text;
				tsl::AppState* _appState;
			}; // 8 bytes

			};

		void redrawEvent(tsl::AppState* _appState, Event& e, uint16_t viewid);

	}
	template<typename T, typename T2>
	constexpr
		T toNorm(T val, T2 min, T2 max) {
		static_assert(std::is_floating_point<T>::value && std::is_floating_point<T2>::value,
			"toNorm: Input types should be floating-point for correct normalization.");
		if (max == min) {
			return static_cast<T>(0.0);
		}
		T normalized_value = (static_cast<T>(val) - static_cast<T>(min)) / (static_cast<T>(max) - static_cast<T>(min));

		// Clamp the result to the [0.0, 1.0] range
		return std::clamp(normalized_value, static_cast<T>(0.0), static_cast<T>(1.0));
	}

}
struct Param {
	enum ParamFlags : uint32_t {
		//SpecialParam = 1 << 0,
		ConvertMs = 1 << 0,
		CastInt = 1 << 1,
		NoAssignment = 1 << 2,
		ToDAW = 1 << 3,
		OwnInitValue = 1 << 4,
		MidiParam = 1 << 5,
		NoValue = 1 << 6,
		HasOnChange = 1 << 7,
		HasAfterChange = 1 << 8,
		// Draw the knob's value fill from the CENTRE of its range instead of from
		// the min end — see fillRange below. Opt-in per parameter, because a range
		// straddling zero is not on its own enough to want it: a gain in dB runs
		// -60..+60 but its min is silence, so "how far up from nothing" is the
		// reading that belongs there.
		CentreFill = 1 << 9,

		// add more as needed
		// Flag2 = 1 << 1,
		// Flag3 = 1 << 2,
	};
	void setFlag(ParamFlags flag, bool val = true) {
		if (val) flags |= flag;
		else flags &= ~flag;
	}

	bool getFlag(ParamFlags flag) const {
		return (flags & flag) != 0;
	}

	// Where a knob's value fill starts and ends, as fractions of the ring.
	//
	// Normally it runs from the min end to the value, which reads as "how much" —
	// right for a level, a rate, an amount. A CentreFill parameter is one whose
	// zero means NEUTRAL rather than none (LFO DEPTH, the SUB's COARSE, the arp
	// steps): anchoring at min paints half the ring for a control that is audibly
	// doing nothing. Those anchor at the ZERO position and sweep toward the value
	// in whichever direction it lies, so off is an empty ring and the direction of
	// the fill carries the sign.
	//
	// Without the flag lo is 0 and hi is the value's position, which is what the
	// knob drew before this existed — every other control renders unchanged.
	void fillRange(double value, float& lo, float& hi) const {
		const double span = (double)max - (double)min;
		if (span <= 0.) { lo = hi = 0.f; return; }
		double pct = (value - (double)min) / span;
		pct = pct < 0. ? 0. : (pct > 1. ? 1. : pct);
		// The straddle test is belt and braces: a CentreFill flag on a range that
		// does not contain zero would otherwise anchor outside the ring.
		const double anchor = (getFlag(CentreFill) && min < 0. && max > 0.)
		                    ? -(double)min / span : 0.;
		lo = (float)(pct < anchor ? pct : anchor);
		hi = (float)(pct < anchor ? anchor : pct);
	}

	enum class ParamCurve {
		Linear,
		Log10,      // dB-style
		Octave,     // base-2
		Exponential // if you need other bases
	};

	Param& operator=(const Param& other)
	{
		if (this != &other)
		{
			min = other.min;
			max = other.max;
			if(!this->getFlag(OwnInitValue))
			    initvalue = other.initvalue;
			progress = other.progress;
			view = other.view;
			name = other.name;
			category = other.category;
			valuename = other.valuename;
			type = other.type;
			digits = other.digits;
			flags = other.flags;
			paramCurve = other.paramCurve;
			values = other.values;
			names = other.names;
			offset = other.offset;

			//progressFunc = other.progressFunc;

			// Copy atomics explicitly
#ifdef PLUGIN_MODE
			pluginIndex = other.pluginIndex;
#ifdef GRAINSTORM
			for (int i = 0; i < 4; i++)
				paramChanging[i].store(other.paramChanging[i].load(std::memory_order_relaxed),
					std::memory_order_relaxed);
#else
			paramChanging[0].store(other.paramChanging[0].load(std::memory_order_relaxed),
				std::memory_order_relaxed);
#endif
#endif
		}
		return *this;
	}

	MYFLOAT  toDisplay(int sr, MYFLOAT  value) const;
	MYFLOAT  getMin(int sr) const;
	MYFLOAT  getMax(int sr) const;
	std::string toString(float value) const;
	MYFLOAT  fromDisplay(int sr, MYFLOAT  val) const;
	int inputType() const;
	MYFLOAT toNormalized(MYFLOAT val);
	MYFLOAT fromNormalized(MYFLOAT normalized);
	MYFLOAT  toDSP(int sr, MYFLOAT  value);
	float getDisplayNameWidth(float maxCharWidth);
	float getDisplayWidthValue(double sr, float maxCharWidth);
	/*
	std::function<MYFLOAT(MYFLOAT, MYFLOAT, int)> progressFunc = [](MYFLOAT input, MYFLOAT prog, int sign) {
		return input + sign * prog;
		};
		*/
	static double incLogDelta(double logarithmic, double deltaLinear) {
		if (deltaLinear == 0.0) return 0.0; // Nothing to change

		// Convert log value back to linear
		double x = pow(10.0, logarithmic / 20.0);

		// Prevent invalid result (e.g., x + deltaLinear <= 0)
		if (x + deltaLinear <= 0.0) return 0.0;

		// Calculate new log delta
		double delta = 20.0 * log10((x + deltaLinear) / x);

		return delta;
	}

	static double incLinDelta(double linear, double deltaLinear) {
		if (deltaLinear == 0.0) return 0.0; // No change
		if (linear <= 0.0 || linear + deltaLinear <= 0.0) return 0.0; // Prevent invalid log

		double delta = 20.0 * log10((linear + deltaLinear) / linear);
		return delta;
	}

	MYFLOAT min{}, max{}, initvalue{}, progress{};
	uint32_t flags{};
	uint16_t paramOffset{};
	uint16_t offsetFact{ 1 };
	const char* name{};
	const char* valuename{};
	const char* category{}; 
	const char* subcategory{};
	int digits{};
	MYFLOAT offset{};

	tsl::graphics::View* view{};
	ParameterType type{ ParameterType_bool };
	ParamCurve paramCurve{ParamCurve::Linear};
	
	std::span<const std::string_view> names;
	std::span<const float> values;

#if defined(STANDALONE_MODE) || defined(PLUGIN_MODE) || defined(OS_IOS)
#ifdef GRAINSTORM
	std::atomic<int> paramChanging[4];
#else
	std::atomic<int> paramChanging[1];
#endif
	int pluginIndex{ -1 };
#endif
};

struct TrackSpecificControl {
	std::atomic<MYFLOAT> lfo_min{}, lfo_max{};
};


#endif //GRAINSTORM_PARAMS_H
