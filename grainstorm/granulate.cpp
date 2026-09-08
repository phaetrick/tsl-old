#include "granulate.h"
#include "granulate_fft.h"
#include "pv.h"
#include "track.h"
#include "grainstorm.h"
#define OSCBNK_PHSMAX   0x80000000U    /* max. phase   */
#define OSCBNK_PHSMSK   0x7FFFFFFFU    /* phase mask   */
#define OSCBNK_RNDPHS   0               /* 31 bit rand -> phase bit shift */

/* convert floating point phase value to integer */
#define OSCBNK_PHS2INT(x)((uint32_t) /*MYFLT2LRND*/((x) * (float) OSCBNK_PHSMAX) & OSCBNK_PHSMSK)

static constexpr int32_t WSOLAIDEAL = 1500;//1500
static constexpr int32_t WSOLACHECK = 4096 - WSOLAIDEAL;//4096 - WSOLAIDEAL;

static inline int32_t irand(const int a, const int e) {
	double r = e - a + 1;
	return a + (int)(r * rand() / (RAND_MAX + 1.0));
}


void granulate(TRACK* track, int32_t channel, const bool isMultiThreaded) {
	auto _appState = track->_appState;
	auto ringMask = _STATE->ringMask;
	auto ringPosInternal = (_STATE->ringPos + 1) & ringMask;
	const auto currentBufSize = _STATE->currentBufSize;
	const auto source = track->source;

	// if (!filebuffer1 || source->off <= 0) return;
	auto& fx_queue_grain = track->fx_queue_grain[channel];


	const MYFLOAT sr = _STATE->sr;


	auto ringbuffer = track->ringbuffer[channel];
	auto grainbuffer = track->grain_buffer[channel];

	const bool wsola = _STATE->params[track->index][WSOLA] == 1.0;
	if (wsola && track->corrWSOLA[channel] == nullptr) {
		track->corrWSOLA[channel] = std::make_unique<CrossCorrelation<MYFLOAT >>(WSOLAIDEAL,
			WSOLACHECK);
	}






	bool pv_power = track->pv_power_tmp;
	bool cross_power = track->cross_power_tmp;

	auto pvfunc = pv_funcs[track->pv_func_tmp];//track->fx_func2.load();
	auto crossfunc = cross_funcs[track->cross_func_tmp];


	const LFO* grainamp_lfo = track->lfo[PREGAIN];
	const bool dograinamplfo = grainamp_lfo && grainamp_lfo->powertmp;
	MYFLOAT grainamp = 0;
	MYFLOAT grainamp_range = 0;
	if (dograinamplfo) {
		MYFLOAT a = _STATE->controls[track->index][PREGAIN].lfo_min.load();
		MYFLOAT b = _STATE->controls[track->index][PREGAIN].lfo_max.load();
		grainamp_range = b - a;
		grainamp = a;
	}
	else
		grainamp = _STATE->params[track->index][PREGAIN].load();

	MYFLOAT pan_range = 0;
	const LFO* pan_lfo = _STATE->channels == 2 ? track->lfo[GRAINPAN].load() : nullptr;
	MYFLOAT pan_lfo_phinc;
	const bool dopanlfo = pan_lfo && pan_lfo->powertmp;
	MYFLOAT pan_const = .5;
	if (dopanlfo) {
		MYFLOAT a = _STATE->controls[track->index][GRAINPAN].lfo_min.load();
		MYFLOAT b = _STATE->controls[track->index][GRAINPAN].lfo_max.load();
		pan_range = b - a;
		pan_const = a;
		pan_lfo_phinc = LOG2NORMALF(pan_lfo->freq()) / (double)sr;
	}
	else
		pan_const = _STATE->channels == 2 ? _STATE->params[track->index][GRAINPAN].load() : .5;


	const auto off_start = track->off_start_tmp;
	const auto off_stop = track->off_stop_tmp;
	MYFLOAT playbackspeed_dir = track->playbackDirTmp;
	MYFLOAT fileoffset = track->offset_tmp;
	const int32_t bounce_type = track->bounce_type_tmp;

	const LFO* speed_lfo = track->speed_lfo_tmp;

#if defined(PLUGIN_MODE) || defined(OS_IOS)
	const bool sync_to_host =
		track->syncDawTmp;

	const bool stopped =
		sync_to_host
		? !_DATA->hostTimeSnapshot.running
		: track->stopped_tmp;

	const double loopsPerBar = track->loopsPerBarTmp;
	const double beatsPerBar = _DATA->hostTimeSnapshot.beatsPerBar;
	const double loopBeats = beatsPerBar / loopsPerBar;

	if (sync_to_host && track->requestLoopPosTmp)
	{
		double phase = std::fmod(_DATA->hostTimeSnapshot.ppqNow / loopBeats, 1.0);
		fileoffset = off_start + phase * (off_stop - off_start);
	}

	double bpm = std::max(1.0, _DATA->hostTimeSnapshot.bpm);
	double sampleLen = off_stop - off_start;
	double secondsPerLoop = (60.0 / bpm) * loopBeats;
	double hostSpeed =
		sampleLen / (secondsPerLoop * _STATE->sr);

	const bool dospeedlfo = !sync_to_host && !stopped && speed_lfo != nullptr;

#else

	const bool stopped = track->stopped_tmp;
	const bool dospeedlfo = !stopped && speed_lfo != nullptr;

#endif
	MYFLOAT speed_range = 0;
	MYFLOAT playbackSpeed = 1.0;
	if (dospeedlfo) {
		auto a =
			track->speed_lfo_min_tmp, b =
			track->speed_lfo_max_tmp;
		speed_range = b - a;
		playbackSpeed = a;
	}
	else {
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		playbackSpeed =
			stopped ? 0.0f :
			sync_to_host
			? (MYFLOAT)hostSpeed
			: track->speed_tmp;
#else
		playbackSpeed = stopped ? 0 :
			track->speed_tmp;
#endif
	}

	MYFLOAT graindens = _STATE->params[track->index][DENSITY].load();
	auto randomoffset_const = (uint32_t)_STATE->params[track->index][RNDREAD].load();
	auto randomoffset = (uint32_t)randomoffset_const;
	const bool integerenvcycles = _STATE->params[track->index][INTEGERENVCYCLES].load() == 1.0;





	const LFO* awin_lfo = track->lfo[AWINCYLCES];
	const bool doawinlfo = awin_lfo && awin_lfo->power();
	MYFLOAT window_freq_anal_const = 1;
	MYFLOAT awin_range = 0;
	if (doawinlfo) {
		auto a = _STATE->controls[track->index][AWINCYLCES].lfo_min.load(), b =
			_STATE->controls[track->index][AWINCYLCES].lfo_max.load();
		if (integerenvcycles) {
			a = (int)a;
			b = (int)b;
		}
		awin_range = b - a;
		window_freq_anal_const = a;
	}
	else  window_freq_anal_const =
		integerenvcycles ? (int)_STATE->params[track->index][AWINCYLCES].load()
		: _STATE->params[track->index][AWINCYLCES].load();


	MYFLOAT grainsize_range = 0;
	const LFO* grainsize_lfo = track->lfo[GRAINSIZE];
	const bool dograinsizelfo =
		grainsize_lfo && grainsize_lfo->power() && !(pv_power || cross_power);
	MYFLOAT grainsize_const = track->grainsize_tmp;
	if (dograinsizelfo) {
		auto a = _STATE->controls[track->index][GRAINSIZE].lfo_min.load(), b =
			_STATE->controls[track->index][GRAINSIZE].lfo_max.load();
		grainsize_range = b - a;
		grainsize_const = a;
	}


	MYFLOAT offset_range = 0;
	MYFLOAT offset_min = 0;
	const LFO* offset_lfo = track->lfo[WRITEOFFSET];
	const bool dooffsetlfo = offset_lfo && offset_lfo->power();
	if (dooffsetlfo) {
		auto a = _STATE->controls[track->index][WRITEOFFSET].lfo_min.load(), b =
			_STATE->controls[track->index][WRITEOFFSET].lfo_max.load();
		offset_range = b - a;
		offset_min = a;
	}

	MYFLOAT readoffset_range = 0;
	MYFLOAT readoffset_const = 0, readoffset = 0;
	const LFO* readoffset_lfo = track->lfo[READOFFSET_LFO];
	const bool doreadoffsetlfo = readoffset_lfo && readoffset_lfo->power();
	if (doreadoffsetlfo) {
		auto a = _STATE->controls[track->index][READOFFSET_LFO].lfo_min.load(), b =
			_STATE->controls[track->index][READOFFSET_LFO].lfo_max.load();
		readoffset_range = b - a;
		readoffset = readoffset_const = a;
	}


	MYFLOAT rndread_range = 0;
	const LFO* rndread_lfo = track->lfo[RNDREAD];
	const bool dorndreadlfo = rndread_lfo && rndread_lfo->power();
	if (dorndreadlfo) {
		auto a = _STATE->controls[track->index][RNDREAD].lfo_min.load(), b =
			_STATE->controls[track->index][RNDREAD].lfo_max.load();
		rndread_range = b - a;
		randomoffset_const = randomoffset = (uint32_t)a;
	}


	MYFLOAT pitch_range = 0;
	MYFLOAT pitchlfomin = 0;
	const LFO* pitch_lfo = track->lfo[PITCH];
	const bool dopitchlfo = pitch_lfo && pitch_lfo->power();
	if (dopitchlfo) {
		MYFLOAT a = _STATE->controls[track->index][PITCH].lfo_min.load();
		MYFLOAT b = _STATE->controls[track->index][PITCH].lfo_max.load();
		pitchlfomin = a;
		pitch_range = b - a;
	}
	const MYFLOAT pitch_const = pow(2., _STATE->params[track->index][PITCH].load());

	MYFLOAT pitch_min_const = _STATE->params[track->index][PITCHMIN].load(), pitch_max_const = _STATE->params[track->index][PITCHMAX].load();
	MYFLOAT pitch_min = pitch_min_const, pitch_max = pitch_max_const;

	const int32_t intervals_min = (int)_STATE->params[track->index][SEMITONES_MIN].load(), intervals_max = (int)_STATE->params[track->index][SEMITONES_MAX].load(), interval_size = (int)_STATE->params[track->index][SEMITONES].load();


	bool randompitch = false;
	bool randomintervals = false;
	if (track->fxpower[SPACE_PITCH]) {
		randomintervals =
			(intervals_min != 0 || intervals_max != 0) && interval_size != 0;
		randompitch = pitch_min != 0 || pitch_max != 0;
	}

	MYFLOAT pitch_min_range = 0;
	const LFO* pitch_min_lfo = track->lfo[PITCHMIN];
	const bool dopitchminlfo = pitch_min_lfo && pitch_min_lfo->power();
	if (dopitchminlfo) {
		randompitch = true;
		auto a = _STATE->controls[track->index][PITCHMIN].lfo_min.load(), b =
			_STATE->controls[track->index][PITCHMIN].lfo_max.load();
		pitch_min_range = b - a;
		pitch_min_const = pitch_min = a;
	}

	MYFLOAT pitch_max_range = 0;
	const LFO* pitch_max_lfo = track->lfo[PITCHMAX];
	const bool dopitchmaxlfo = pitch_max_lfo && pitch_max_lfo->power();
	if (dopitchmaxlfo) {
		randompitch = true;
		auto a = _STATE->controls[track->index][PITCHMAX].lfo_min.load(), b = _STATE->controls[track->index][PITCHMAX].lfo_max.load();
		pitch_max_range = b - a;
		pitch_max_const = pitch_max = a;
	}

	MYFLOAT semitones_range = 0;
	const LFO* semitones_lfo = track->lfo[SEMITONES_LFO];
	MYFLOAT semitoneslfomin = 0;
	const bool dosemitoneslfo = semitones_lfo && semitones_lfo->power();
	if (dosemitoneslfo) {
		auto a = _STATE->controls[track->index][SEMITONES_LFO].lfo_min.load(),
			b = _STATE->controls[track->index][SEMITONES_LFO].lfo_max.load();
		semitoneslfomin = a;
		semitones_range = b - a;
	}

	MYFLOAT looppos_range = 0;
	const LFO* looppos_lfo = track->looppos_lfo_tmp;
	MYFLOAT loopposlfomin = 0;

	const bool doloopposlfo =
#if defined(PLUGIN_MODE) || defined(OS_IOS)
		sync_to_host ? false :
#endif
		!stopped && looppos_lfo != nullptr;

	if (doloopposlfo) {
		const MYFLOAT r = DISTANCE(off_start, off_stop);
		MYFLOAT a = track->looppos_lfo_min_tmp,
			b = track->looppos_lfo_max_tmp;
		loopposlfomin = std::min(off_start, off_stop) + a * r;
		looppos_range = (b - a) * r;
	}

	MYFLOAT gliss_range = 0;
	const LFO* gliss_lfo = track->lfo[GRAINGLISS];
	MYFLOAT glissconst;

	const bool doglisslfo = gliss_lfo && gliss_lfo->power();
	if (doglisslfo) {
		MYFLOAT a = _STATE->controls[track->index][GRAINGLISS].lfo_min.load(),
			b = _STATE->controls[track->index][GRAINGLISS].lfo_max.load();
		glissconst = a;
		gliss_range = b - a;
	}
	else
		glissconst = _STATE->params[track->index][GRAINGLISS].load();


	MYFLOAT gpol_range = 0;
	const LFO* gpol_lfo = track->lfo[GRAINENVINTERPOL];
	MYFLOAT gpolconst;

	const bool dogpollfo = gpol_lfo && gpol_lfo->power();
	if (dogpollfo) {
		MYFLOAT a = _STATE->controls[track->index][GRAINENVINTERPOL].lfo_min.load(),
			b = _STATE->controls[track->index][GRAINENVINTERPOL].lfo_max.load();
		gpolconst = a;//std::min(a, b);
		gpol_range = b - a;
	}
	else
		gpolconst = _STATE->params[track->index][GRAINENVINTERPOL].load();

	MYFLOAT phase_range = 0;
	const LFO* phase_lfo = track->lfo[ENVPHASE];
	MYFLOAT phaseconst;

	const bool phaselfo = phase_lfo && phase_lfo->power();
	if (phaselfo) {
		MYFLOAT a = _STATE->controls[track->index][ENVPHASE].lfo_min.load(),
			b = _STATE->controls[track->index][ENVPHASE].lfo_max.load();
		phaseconst = a;
		phase_range = b - a;
	}
	else
		phaseconst = _STATE->params[track->index][ENVPHASE].load();



	auto tindex = track->index;

	//LOGE("%g %g", step_length_swin, step_point_swin);

	const bool bandlimited_env = track->oldbandlimitedgrainenv == 1.0;
	auto& bandlimitedEnvtable1 = track->bandlimitedEnvtable[0];
	auto& bandlimitedEnvtable2 = track->bandlimitedEnvtable[1];
	auto& bandlimitedEnvtable3 = track->bandlimitedEnvtable[2];
	const auto& window1 = track->grainenv[0];
	const auto& window2 = track->grainenv[1];
	const auto& window3 = track->grainenv[2];


	const bool envonly = _STATE->params[track->index][NOINPUTFROMTRACK].load() == 1.0;

	auto hanningwin = _DATA->hanningwin;

	auto filebuffer = source->filebuffer.load();
	auto state = filebuffer ? filebuffer->state.load() : nullptr;
	const auto off = state == nullptr ? 0 : filebuffer->off;
	auto filebuffer1 = off > 0 ? filebuffer->buffer[channel].data() : nullptr;

	const bool isresizing = filebuffer == nullptr || filebuffer->isresizing.load();

	const auto panenv = _DATA->pan[channel];

	const auto grainsizemin = (int)_STATE->parameters[GRAINSIZE].min;


	const auto reverseParam = _STATE->params[track->index][REVERSEGRAINS].load();
	const bool hqresampling = _STATE->params[track->index][HQ_RESAMPLING] == 1.0;
#ifdef IS_MULTITHREADED
	auto& cross_barrier = track->cross_barrier[channel];
#endif

#if defined DOES_INPUT_RESAMPLING
	const auto dawGrainInputGain = dbToLinear60(_STATE->params[track->index][GRAININPUT_GAIN_DAW].load());
	const auto grainGain = dbToLinear60(_STATE->params[track->index][INPUT_GAIN_SAMPLER].load());
	const auto inputMask = _STATE->inputMask;
	const auto inputSize = inputMask + 1;
	auto inBuf = _STATE->inputBuf[channel];
#endif

	for (int smpl = 0; smpl < currentBufSize; smpl++) {
		MYFLOAT seqgain, seqpitch, seqsize;
		const MYFLOAT speed = dospeedlfo ? playbackSpeed + speed_lfo->buf[smpl] * speed_range : playbackSpeed;

		if (track->graingen.tick(channel, smpl, seqgain, seqpitch, seqsize)) {

			const MYFLOAT linearGain = dbToLinear60(dograinamplfo ? grainamp +
				grainamp_lfo->buf[smpl] * grainamp_range : grainamp) *
				seqgain;


			const bool silence = linearGain == 0;

			if (cross_power) {
#ifdef IS_MULTITHREADED
				if (isMultiThreaded) {
					cross_barrier.loop_continue(smpl, sr / graindens, silence);
					if (silence)
						track->destinationz->sem_cross[channel].wait();
				}
				else {
#endif
					track->stepLengthTmp[channel] = sr / graindens;
					track->silenceTmp[channel] = silence;
					compute_fft(track->destinationz, channel, false);
#ifdef IS_MULTITHREADED
				}
#endif
			}


			if (!silence) {
				track->step_point_grain[channel].store(smpl);

				if (cross_power || pv_power)
					seqsize = 1.;

				MYFLOAT grainsize = (dograinsizelfo ? grainsize_const +
					grainsize_lfo->buf[smpl] *
					grainsize_range
					: grainsize_const) * seqsize;

				if (grainsize < grainsizemin)
					grainsize = grainsizemin;

				MYFLOAT window_freq_anal = doawinlfo ? window_freq_anal_const +
					(integerenvcycles ? static_cast<int>(awin_lfo->buf[smpl] *
						awin_range) :
						awin_lfo->buf[smpl] * awin_range) : window_freq_anal_const;
				if (window_freq_anal < 1)
					window_freq_anal = 1;

				const MYFLOAT lfo_write_offset = dooffsetlfo ?
					offset_min +
					offset_lfo->buf[smpl] * offset_range : 0;


				if (dorndreadlfo) {
					randomoffset = (uint32_t)(randomoffset_const +
						rndread_lfo->buf[smpl] * rndread_range);
				}

				if (dopitchminlfo) {
					pitch_min = pitch_min_const +
						pitch_min_lfo->buf[smpl] * pitch_min_range;

				}

				if (dopitchmaxlfo) {
					pitch_max = pitch_max_const +
						pitch_max_lfo->buf[smpl] * pitch_max_range;

				}

				auto pitch = pitch_const * seqpitch * (dopitchlfo ? pow(2.,
					pitchlfomin +
					pitch_lfo->buf[smpl] *
					pitch_range)
					: 1) * (randomintervals ? pow(2.,
						irand(intervals_min,
							intervals_max) *
						interval_size /
						12.)
						: 1) *
					(randompitch ? pow(2., tsl::random::randomfloat(pitch_min,
						pitch_max)) : 1) *
					(dosemitoneslfo ? pow(2.,
						(round(semitoneslfomin +
							semitones_lfo->buf[smpl] *
							semitones_range) /
							12.))
						: 1);
				if (pitch > 8.)
					pitch = 8.;
				else if (pitch < .125)
					pitch = .125;

				track->pitchfact[channel] = pitch;
				track->glissfact[channel] = pow(2., doglisslfo ? glissconst +
					gliss_lfo->buf[smpl] * gliss_range
					: glissconst);

				if (!envonly && (off != 0
#if defined DOES_INPUT_RESAMPLING
                    || dawGrainInputGain > 0.0f
#endif
                )) {
					auto gliss = pitch * track->glissfact[channel];
					if (gliss > 8.)
						gliss = 8.;
					else if (gliss < .125)
						gliss = .125;

					const auto pitchinc = (gliss - pitch) / (double)grainsize;

					if (doreadoffsetlfo) {
						readoffset = readoffset_const +
							readoffset_lfo->buf[smpl] * readoffset_range;
					}

					if (doloopposlfo)
						fileoffset = loopposlfomin + looppos_range * looppos_lfo->buf[smpl];

					auto tempoffset =
						(randomoffset ? fileoffset + readoffset + irand(0, randomoffset) :
							fileoffset + readoffset);
#if defined DOES_INPUT_RESAMPLING
					auto tempoffsetDaw =
						(randomoffset ? _STATE->inputPos + smpl - readoffset - irand(0, randomoffset) :
							_STATE->inputPos + smpl - readoffset);

#endif

					if (wsola
						) {
						auto idealoffset =
							static_cast<int>(track->tmpoffset[channel].load() +
								sr / graindens * playbackspeed_dir);


						MYFLOAT mul = WINDOW_SIZE / (MYFLOAT)WSOLAIDEAL;
						for (int i = 0; i < WSOLAIDEAL; i++) {
							int o = idealoffset + i;
							track->fft_help1[channel][i] =
								o >= off || o < 0 ? 0.0 : (MYFLOAT)filebuffer1[o] *
								hanningwin[(int)(i * mul)];
						}
						mul = WINDOW_SIZE / (MYFLOAT)WSOLACHECK;
						for (int i = 0; i < WSOLACHECK; i++) {
							auto o = static_cast<int>(tempoffset + i);
							track->fft_help2[channel][i] =
								o >= off || o < 0 ? 0.0 : (MYFLOAT)filebuffer1[o] *
								hanningwin[(int)(i * mul)];
						}
						auto shift = track->corrWSOLA[channel]->Compute(
							track->fft_help1[channel],
							track->fft_help2[channel]);
						tempoffset += shift;
					}
					track->tmpoffset[channel].store(tempoffset);

					int forward = playbackspeed_dir == 1 ||
						(playbackspeed_dir == -1 && bounce_type == BOUNCE_NO_REVERSE)
						? 1
						: -1;
					if (tsl::random::randomfloat(0, 1) < reverseParam) forward *= -1;


					if (pitch != 1. || pitchinc != 0.) {
						if (pitchinc == 0 && hqresampling) {
							if (track->rst[channel] == nullptr)
								track->rst[channel] =
								std::make_unique<PolyPhaseResampler<MYFLOAT, 32, 256 >>();
							auto& rs = track->rst[channel];
							rs->initRounded(pitch);
							if (forward == 1) {
								auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
								auto inputPos = (static_cast<int>(tempoffsetDaw - grainsize * pitch)) & inputMask;
#endif
								for (int i = 0; i < grainsize; i++) {
									while (rs->isWriteNeeded()) {
										if (to >= off || to < 0) {
											rs->writeNextFrame(0.0
#if defined DOES_INPUT_RESAMPLING
												+ inBuf[inputPos] * dawGrainInputGain
#endif											
											);
										}
										else {
											rs->writeNextFrame(filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
												* grainGain + inBuf[inputPos] * dawGrainInputGain
#endif											
											);
										}
										to++;
#if defined DOES_INPUT_RESAMPLING
										inputPos = (inputPos + 1) & inputMask;
#endif											
									}
									grainbuffer[i] = rs->readNextFrame();
								}
							}
							else {
								auto to = static_cast<int>(tempoffset + grainsize * pitch);
#if defined DOES_INPUT_RESAMPLING
								auto inputPos = static_cast<int>(tempoffsetDaw) & inputMask;
#endif
								for (int i = 0; i < grainsize; i++) {
									while (rs->isWriteNeeded()) {
										if (to >= off || to < 0) {
											rs->writeNextFrame(0.0
#if DOES_INPUT_RESAMPLING
												+ inBuf[inputPos] * dawGrainInputGain
#endif											
											);
										}
										else {
											rs->writeNextFrame(filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
												* grainGain + inBuf[inputPos] * dawGrainInputGain
#endif																						
											);
										}
										to--;
#if defined DOES_INPUT_RESAMPLING
										inputPos = (inputPos - 1) & inputMask;
#endif
									}
									grainbuffer[i] = rs->readNextFrame();
								}
							}
							rs->clear();
						}
						else {
							if (forward) {
#if DOES_INPUT_RESAMPLING
								auto inputPos = tempoffsetDaw - grainsize * pitch;
								// initial wrap
								while (inputPos < 0.0)
									inputPos += inputSize;
								while (inputPos >= inputSize)
									inputPos -= inputSize;
#endif
								for (int i = 0; i < grainsize; i++) {
									auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
									int   i0 = static_cast<int>(inputPos);
									int   i1 = (i0 + 1) & inputMask;
									double frac = inputPos - static_cast<double>(i0);
									auto inputSample = (inBuf[i0] +
										frac *
										(inBuf[i1] -
											inBuf[i0])) * dawGrainInputGain;
#endif
									if (to + 1 >= off || to < 0) {
										grainbuffer[i] = 0.0;
#if defined DOES_INPUT_RESAMPLING
										grainbuffer[i] += inputSample;
										tempoffset += pitch;
										inputPos += pitch;
										while (inputPos >= inputSize)
											inputPos -= inputSize;
										pitch += pitchinc;
										if (pitch > 8.)pitch = 8.;
										else if (pitch < .125)pitch = .125;
#endif

										continue;
									}
									int v1 = to;
									int v2 = v1 + 1;

									grainbuffer[i] = (MYFLOAT)(filebuffer1[v1] +
										(tempoffset - v1) *
										(filebuffer1[v2] -
											filebuffer1[v1])) *
										CONVMYFLT
#ifdef DOES_INPUT_RESAMPLING
										* grainGain + inputSample;
#endif
									;

									tempoffset += pitch;
#ifdef DOES_INPUT_RESAMPLING
									inputPos += pitch;
									while (inputPos >= inputSize)
										inputPos -= inputSize;
#endif
									pitch += pitchinc;
									if (pitch > 8.)pitch = 8.;
									else if (pitch < .125)pitch = .125;
								}
							}
							else {
								tempoffset += grainsize * pitch;
#if defined DOES_INPUT_RESAMPLING
								auto inputPos = tempoffsetDaw;
								while (inputPos < 0.0)
									inputPos += inputSize;
								while (inputPos >= inputSize)
									inputPos -= inputSize;
#endif
								for (int i = 0; i < grainsize; i++) {
									auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
									int   i0 = static_cast<int>(inputPos);
									int   i1 = (i0 + 1) & inputMask;
									double frac = inputPos - static_cast<double>(i0);
									auto inputSample = (inBuf[i0] +
										frac *
										(inBuf[i1] -
											inBuf[i0])) * dawGrainInputGain;
#endif
									if (to + 1 >= off || to < 0) {
										grainbuffer[i] = 0.0;
#ifdef DOES_INPUT_RESAMPLING
										grainbuffer[i] += inputSample;
										tempoffset -= pitch;
										inputPos -= pitch;
										while (inputPos < 0)
											inputPos += inputSize;
										pitch += pitchinc;
										if (pitch > 8.)pitch = 8.;
										else if (pitch < .125)pitch = .125;
#endif
										continue;
									}
									int v1 = to;
									int v2 = v1 + 1;

									grainbuffer[i] = (MYFLOAT)(filebuffer1[v1] +
										(tempoffset - v1) *
										(filebuffer1[v2] -
											filebuffer1[v1])) *
										CONVMYFLT
#ifdef DOES_INPUT_RESAMPLING
										* grainGain + inputSample;
#endif
									;
									tempoffset -= pitch;
#ifdef DOES_INPUT_RESAMPLING
									inputPos -= pitch;
									while (inputPos < 0)
										inputPos += inputSize;
#endif
									pitch += pitchinc;
									if (pitch > 8.)pitch = 8.;
									else if (pitch < .125)pitch = .125;
								}
							}
						}
					}
					else {
						if (forward == 1) {
							auto to = static_cast<int>(tempoffset);
#if defined DOES_INPUT_RESAMPLING
							auto inputPos = (static_cast<int>(tempoffsetDaw - grainsize)) & inputMask;
#endif
							for (int32_t i = 0; i < grainsize; i++) {
								if (to >= off || to < 0) {
									grainbuffer[i] = 0.0;
#if defined DOES_INPUT_RESAMPLING
									grainbuffer[i] += inBuf[inputPos] * dawGrainInputGain;
									inputPos = (inputPos + 1) & inputMask;
#endif
								}
								else {
									grainbuffer[i] = (MYFLOAT)filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
										* grainGain + inBuf[inputPos] * dawGrainInputGain;
#endif                              
									;
									to++;
#if defined DOES_INPUT_RESAMPLING
									inputPos = (inputPos + 1) & inputMask;
#endif                              
								}
							}
						}
						else {
							auto to = static_cast<int>(tempoffset + grainsize);
#if defined DOES_INPUT_RESAMPLING
							auto inputPos = static_cast<int>(tempoffsetDaw) & inputMask;
#endif
							for (int i = 0; i < grainsize; i++) {
								if (to >= off || to < 0) {
									grainbuffer[i] = 0.0;
#if defined DOES_INPUT_RESAMPLING
									grainbuffer[i] += inBuf[inputPos] * dawGrainInputGain;
									inputPos = (inputPos - 1) & inputMask;
#endif                              
									continue;
								}
								grainbuffer[i] = (MYFLOAT)filebuffer1[to] * CONVMYFLT
#if defined DOES_INPUT_RESAMPLING
									* grainGain + inBuf[inputPos] * dawGrainInputGain;
#endif                              
								;
								to--;
#if defined DOES_INPUT_RESAMPLING
								inputPos = (inputPos - 1) & inputMask;
#endif                              
							}
						}
					}
				}
				else {
					for (int i = 0; i < grainsize; i++)grainbuffer[i] = 0.0;
				}

				MYFLOAT max = 0;
				for (int i = 0; i < grainsize; i++) {
					auto tmp = std::abs(grainbuffer[i]);
					max = max > tmp ? max : tmp;
				}

				track->maxTmp[channel] = max;

				auto* n = fx_queue_grain._first;
				while (n) {
					auto next = n->next;
					if (n->data->readyToDestroy)
						fx_queue_grain.del(n);
					else
						n->data->compute(grainbuffer, grainsize);
					n = next;
				}

				auto grainsize_int = static_cast<int>(grainsize);

				if (cross_power || pv_power) {
					MYFLOAT sl = (MYFLOAT)1. / grainsize_int;
					MYFLOAT sp = 0;
					for (int i = 0; i < grainsize_int; i++) {
						grainbuffer[i] *= hanningwin[PHS2INT(sp)];
						sp += sl;
					}

					if (pv_power)
						pvfunc(track, channel, grainsize_int, speed);

					if (cross_power) {
						// Level reference for the cross algorithms: the RMS of the
						// grain actually entering the cross function (windowed, and
						// post-PV if PV is also on). Each cross alg normalises its
						// output back to this, so they match loudness.
						MYFLOAT ms = 0;
						for (int i = 0; i < grainsize_int; i++)
							ms += grainbuffer[i] * grainbuffer[i];
						track->srcRmsTmp[channel] =
							grainsize_int > 0 ? (MYFLOAT)std::sqrt(ms / grainsize_int) : 0;

						crossfunc(track, channel, grainsize_int, isMultiThreaded);
						if (crossfunc == cross_convolve && !envonly)
							grainsize_int *= 2;
					}

				}


				int offset = (ringPosInternal + smpl + static_cast<int>(lfo_write_offset)) & ringMask;

				const MYFLOAT gain = panenv[
					(uint32_t)((pan_const +
						(dopanlfo ? pan_lfo->buf[smpl] * pan_range
							: 0.)) *
						TBLMASK2) & TBLMASK2] * linearGain;// 

				if (!cross_power && !pv_power) {
					uint64_t s = WINDOW_SIZE;
					uint64_t lobits = 49, mask = 562949953421311;
					auto pfrac = 1.776357e-15;
					uint64_t s2 = WINDOW_SIZE;
					auto lobits2 = lobits, mask2 = mask;
					auto pfrac2 = pfrac;

					auto& ftable1 = bandlimited_env ? bandlimitedEnvtable1.getGrainTable(grainsize,
						window_freq_anal, sr,
						s, lobits,
						pfrac)
						: window1;
					auto& ftable2 = bandlimited_env ? bandlimitedEnvtable2.getGrainTable(grainsize,
						window_freq_anal, sr, s, lobits, pfrac)
						: window2;
					auto& ftable3 = bandlimited_env ? bandlimitedEnvtable3.getGrainTable(grainsize,
						1, sr, s2,
						lobits2,
						pfrac2)
						: window3;

					const uint64_t MASK1 = s - 1;
					const uint64_t MASK2 = s2 - 1;
					
					long double step_length_env_anal = ((s - 1) * (long double)window_freq_anal +
						window_freq_anal - 0.94) /
						((long double)grainsize_int - 1);
					auto frq = (uint64_t)(step_length_env_anal * (1ULL << lobits));

					long double step_length_env3 = ((s2 - 1) * (long double)1 +
						1 - 0.94) /
						((long double)grainsize_int - 1);
					auto frq2 = (uint64_t)(step_length_env3 * (1ULL << lobits2));
					
					/*
					uint64_t frq = (uint64_t)(((long double)(window_freq_anal * s) * (1ULL << lobits) + (grainsize_int - 2)) / (grainsize_int - 1));
					uint64_t frq2 = (uint64_t)(((long double)s2 * (1ULL << lobits2) + (grainsize_int - 2)) / (grainsize_int - 1));
					*/
					
					
					const MYFLOAT envamp2 = dogpollfo ? gpolconst + gpol_range * gpol_lfo->buf[smpl]
						: gpolconst;
					const MYFLOAT envamp1 = 1. - envamp2;



					// 1. Set Initial Phases (assuming startPos1 and startPos2 are 0.0 - 1.0)
					uint64_t phase = static_cast<uint64_t>((phaseconst +
						(phase_lfo ? phase_range * phase_lfo->buf[smpl]
							: 0.)) * (MYFLOAT)OSCBNK_PHSMAX_64) & OSCBNK_PHSMSK_64;
					uint64_t phase2 = 0;

					if (!envonly) {
						for (int i = 0; i < grainsize_int; i++) {
							UDD(grainbuffer[i])
								// --- Table 1 & 2 (Sync'd phase) ---
								uint64_t idx1 = (phase >> lobits);
							auto i0 = static_cast<size_t>(idx1 & MASK1);
							auto i1 = static_cast<size_t>((i0 + 1) & MASK1);

							// Correct 64-bit fraction
							double f1 = static_cast<double>(phase & ((1ULL << lobits) - 1)) * pfrac;

							double v = envamp1 * (ftable1[i0] + f1 * (ftable1[i1] - ftable1[i0]))
								+ envamp2 * (ftable2[i0] + f1 * (ftable2[i1] - ftable2[i0]));

							// --- Table 3 (Independent phase) ---
							uint64_t idx2 = (phase2 >> lobits2);
							auto j0 = static_cast<size_t>(idx2 & MASK2);
							auto j1 = static_cast<size_t>((j0 + 1) & MASK2);

							double f2 = static_cast<double>(phase2 & ((1ULL << lobits2) - 1)) * pfrac2;

							double v2 = ftable3[j0] + f2 * (ftable3[j1] - ftable3[j0]);

							// --- Accumulate ---
							ringbuffer[offset] += grainbuffer[i] * (v * v2 * gain);

							phase += frq;
							phase2 += frq2;
							offset = (offset + 1) & ringMask;
						}
						//LOGE("%d %d", ((phase - frq)& OSCBNK_PHSMSK_64) >> lobits, OSCBNK_PHSMSK_64);
					}
					else {
						for (int32_t i = 0; i < grainsize_int; i++) {
							uint64_t idx1 = (phase >> lobits);
							auto i0 = static_cast<size_t>(idx1 & MASK1);
							auto i1 = static_cast<size_t>((i0 + 1) & MASK1);

							// Correct 64-bit fraction
							double f1 = static_cast<double>(phase & ((1ULL << lobits) - 1)) * pfrac;

							double v = envamp1 * (ftable1[i0] + f1 * (ftable1[i1] - ftable1[i0]))
								+ envamp2 * (ftable2[i0] + f1 * (ftable2[i1] - ftable2[i0]));

							// --- Table 3 (Independent phase) ---
							uint64_t idx2 = (phase2 >> lobits2);
							auto j0 = static_cast<size_t>(idx2 & MASK2);
							auto j1 = static_cast<size_t>((j0 + 1) & MASK2);

							double f2 = static_cast<double>(phase2 & ((1ULL << lobits2) - 1)) * pfrac2;

							double v2 = ftable3[j0] + f2 * (ftable3[j1] - ftable3[j0]);

							// --- Accumulate ---
							ringbuffer[offset] += (v * v2 * gain);

							phase += frq;
							phase2 += frq2;
							offset = (offset + 1) & ringMask;

						}
					}
					
				}
				else {
					MYFLOAT sl = 1. / (MYFLOAT)grainsize_int;
					MYFLOAT sp = 0;
					for (int32_t i = 0; i < grainsize_int; i++) {
						UDD(grainbuffer[i])
							ringbuffer[offset] +=
							grainbuffer[i] * hanningwin[PHS2INT(sp)] * gain;
						sp += sl;
						offset = (offset + 1) & ringMask;
					}
				}
				// LOGE("%d %d", ((phs - frq)  & OSCBNK_PHSMSK) >>lobits, OSCBNK_PHSMSK);
			}
		}
		/*
		  int32_t lookup = phs >> lobits;
								float v = ftable[lookup++];
								v += (ftable[lookup] - v) * (MYFLT) ((int32_t) (phs & mask)) * pfrac;
								UDF(v)
		 */
		if (!doloopposlfo)
			fileoffset += speed * playbackspeed_dir;
		if (fileoffset > off_stop && bounce_type != NO_BOUNCE) {
			if (isresizing)
				fileoffset = off_stop;
			else
				fileoffset = off_stop - DISTANCE(off_stop, fileoffset);
			if (fileoffset <= off_start)
				fileoffset = off_stop;
			playbackspeed_dir *= -1;
		}
		else if (fileoffset < off_start && bounce_type != NO_BOUNCE) {
			if (isresizing)
				fileoffset = off_start;
			else
				fileoffset = off_start + DISTANCE(off_start, fileoffset);
			if (fileoffset >= off_stop)
				fileoffset = off_start;
			playbackspeed_dir *= -1;
		}
		else if (fileoffset < off_start) {
			if (isresizing)
				fileoffset = off_stop;
			else {
				fileoffset = off_stop - DISTANCE(off_start, fileoffset);
				if (fileoffset < off_start)
					fileoffset = off_stop;
			}

		}
		else if (fileoffset > off_stop) {
			if (isresizing)
				fileoffset = off_start;
			else {
				fileoffset = off_start + DISTANCE(off_stop, fileoffset);
				if (fileoffset > off_stop)
					fileoffset = off_start;
			}
		}
	}
#ifdef IS_MULTITHREADED

	if (cross_power && isMultiThreaded)
		cross_barrier.loop_exit();
#endif

	auto outbuf = track->out_buf[channel];

	// if (envonly) {
	auto xdc = track->xdc[channel];
	auto ydc = track->ydc[channel];
	auto ringPos = _STATE->ringPos & ringMask;
#if defined DOES_INPUT_RESAMPLING
	const auto dawGain = dbToLinear60(_STATE->params[track->index][INPUT_GAIN_DAW].load());
	// Read the same window the grain path reads (_STATE->inputPos + smpl). This used to
	// step back by currentBufSize to reach the PREVIOUS block, but the previous block's
	// length is not currentBufSize whenever the engine block size moves by a frame, so
	// the dry path slipped a sample at those seams. Measured at 44.1k: -24.6 -> -78.2 dB.
	// Side effect: the dry path is now aligned with the wet path instead of one block late.
	auto inputPos = _STATE->inputPos & inputMask;
	for (int i = 0; i < currentBufSize; i++) {
		auto temp = ringbuffer[ringPos] + inBuf[inputPos] * dawGain;
		inputPos = (inputPos + 1) & inputMask;
#else
	for (int i = 0; i < currentBufSize; i++) {
		auto temp = ringbuffer[ringPos];
#endif
		ringbuffer[ringPos] = 0;
		UDD(temp)
			outbuf[i] = ydc = temp - xdc + .995 * ydc;
		xdc = temp;
		ringPos = (ringPos + 1) & ringMask;
	}
	UDD(ydc)

		track->ydc[channel] = ydc;
	track->xdc[channel] = xdc;

	auto n = track->fx_queue[channel]._first;
	while (n) {
		auto next = n->next;
		if (n->data->readyToDestroy) {
			track->fx_queue[channel].del(n);
		}
		else
			n->data->compute(outbuf, currentBufSize);
		n = next;
	}

	if (channel == 1) {
#ifdef IS_MULTITHREADED
		if (isMultiThreaded)
			track->stereosem.signal();
#endif
	}
	else {
		auto buffer1 = outbuf;
		auto buffer2 = track->out_buf[1];
		if (_STATE->channels == 1) {
			memcpy(buffer2, buffer1, sizeof(MYFLOAT) * currentBufSize);
		}
#ifdef IS_MULTITHREADED
		else if (_STATE->channels == 2 && isMultiThreaded) {
			track->stereosem.wait();
		}
#endif
		auto nn = track->fx_queue_stereo._first;
		while (nn) {
			auto next = nn->next;
			if (nn->data->readyToDestroy)
				track->fx_queue_stereo.del(nn);
			else
				nn->data->compute(buffer1, buffer2, buffer1,
					buffer2, currentBufSize);
			nn = next;
		}
		track->gainTask.compute(buffer1, buffer2, buffer1,
			buffer2, currentBufSize);
		_DATA->playBufQueue.try_push(track);

		if (_DATA->updateRenderThreadSpectrum[track->index].load() && !track->bypass[SPACE_SPECTRUM].load()) {
			for (int i = 0; i < currentBufSize; i++) {
				track->spectrumAnalyzer.tick((buffer1[i] + buffer2[i]) * .5);
			}
		}
		else
			track->spectrumAnalyzer.reset();


		if (!source->updated_this_cycle.exchange(true, std::memory_order_acq_rel)) {

			if (off > 0) {
				auto state = source->currentState;
				if (state) {
					if (state->playbackDir.exchange(playbackspeed_dir) != playbackspeed_dir)
						_DATA->snapShot.queue.try_push(tsl::parameters::Event::createEvent(source->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, playbackspeed_dir, tsl::parameters::EventSubtype::playbackDir));
					if (!filebuffer->ismoving.load()) {
						if (auto old = state->offset.exchange(fileoffset) != fileoffset)
							_DATA->snapShot.queue.try_push(tsl::parameters::Event::createEvent(source->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, fileoffset, tsl::parameters::EventSubtype::offset));
					}
				}

			}

		}
	}
}
