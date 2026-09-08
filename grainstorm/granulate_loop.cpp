#include "granulate_loop.h"
#include "track.h"
#include "grainstorm.h"

void loop(TRACK* track, int32_t channel, bool isMultiThreaded) {
	auto _appState = track->_appState;
	const auto source = track->source;
	int32_t index = track->index;
	track->xdc[channel] = track->ydc[channel] = 0;
	auto ringMask = _STATE->ringMask;
	int32_t ringPosInternal = _STATE->ringPos & ringMask;
	const int32_t bufsize_init = _STATE->currentBufSize;
	for (int32_t i = 0; i < bufsize_init; i++) {
		track->ringbuffer[channel][ringPosInternal] = 0;
		ringPosInternal = (ringPosInternal + 1) & ringMask;
	}
	auto filebuffer = source->filebuffer.load();
	const auto off = filebuffer == nullptr ? 0 : filebuffer->off;
	auto filebuffer1 = off > 0 ? filebuffer->buffer[channel].data() : nullptr;

	const auto off_start = track->off_start_tmp;
	const auto off_stop = track->off_stop_tmp;
	MYFLOAT playbackspeed_dir = track->playbackDirTmp;
	MYFLOAT fileoffset = track->offset_tmp;
	const int32_t bounce_type = track->bounce_type_tmp;

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

	auto speed =
		stopped ? 0.0f :
		sync_to_host
		? (MYFLOAT)hostSpeed
		: track->speed_tmp;
#else
	const bool stopped = track->stopped_tmp;
	auto speed = stopped ? 0 :
		track->speed_tmp;
#endif
	if (speed < .1)
		speed = .1;
	else if (speed > 10.)
		speed = 10.;


	const auto pregain_const = dbToLinear60(_STATE->params[track->index][PREGAIN].load());
	auto pregain = track->pregrainSmoothed;
	auto coeff = _STATE->smoothCoeff;


	bool cross_power = track->cross_power_tmp;



	int32_t fadesamples = 1 + (int)(_STATE->sr / 1000. *
		_STATE->params[track->index][LOOPERFADE].load());
	int32_t distance = DISTANCE(off_start, off_stop);
	if (distance < 1)
		distance = 1;
	if (fadesamples > distance)
		fadesamples = distance;
	const double onedfadesamples = 1. / (double)fadesamples;

	

	auto outbuf = track->out_buf[channel];
	if (track->rst[channel] == nullptr)
		track->rst[channel] = std::make_unique<PolyPhaseResampler<MYFLOAT, 32, 256 >>();

	auto rs = track->rst[channel].get();
	rs->init(speed);
	int32_t frameswritten = 0;
	while (frameswritten < bufsize_init) {
		if (bounce_type == BOUNCE_NORMAL) {
			while (rs->isWriteNeeded()) {
				MYFLOAT smpl;
				if (playbackspeed_dir > 0) {
					int32_t dist = DISTANCE(fileoffset, off_stop);
					if (dist <= fadesamples) {
						double vala = (dist * onedfadesamples) *
							((fileoffset >= off || fileoffset < 0) ? 0.0 :
								filebuffer1[(int)fileoffset] *
								CONVMYFLT);
						int32_t fob = off_stop + dist;
						double valb = (1.0 - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[fob] *
								CONVMYFLT);
						smpl = vala + valb;
					}
					else
						smpl = ((fileoffset >= off || fileoffset < 0) ? 0.0 :
							filebuffer1[(int)fileoffset] *
							CONVMYFLT);

				}
				else {
					int32_t dist = DISTANCE(fileoffset, off_start);
					if (dist <= fadesamples) {
						double vala = (dist * onedfadesamples) *
							((fileoffset >= off || fileoffset < 0) ? 0.0 :
								filebuffer1[(int)fileoffset] *
								CONVMYFLT);
						int32_t fob = off_start - dist;
						double valb = (1.0 - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[fob] *
								CONVMYFLT);
						smpl = vala + valb;
					}
					else
						smpl = ((fileoffset >= off || fileoffset < 0) ? 0.0 :
							filebuffer1[(int)fileoffset] *
							CONVMYFLT);
				}
				rs->writeNextFrame(smpl);
				fileoffset += playbackspeed_dir;
				if (fileoffset >= off_stop) {
					fileoffset = off_stop - DISTANCE(off_stop, fileoffset);
					if (fileoffset < off_start)
						fileoffset = off_start;
					playbackspeed_dir *= -1;
				}
				else if (fileoffset < off_start) {
					fileoffset = off_start + DISTANCE(off_start, fileoffset);
					if (fileoffset >= off_stop)
						fileoffset = off_start;
					playbackspeed_dir *= -1;
				}
			}
			outbuf[frameswritten++] = rs->readNextFrame() * pregain;

		}
		else if (bounce_type == BOUNCE_NO_REVERSE) {

			while (rs->isWriteNeeded()) {
				MYFLOAT smpl1, smpl2;
				if (playbackspeed_dir > 0) {
					int32_t dist = DISTANCE(fileoffset, off_stop);
					int32_t fo2 = off_start + dist;
					if (dist <= fadesamples) {
						double vala = (dist * onedfadesamples) *
							((fileoffset >= off || fileoffset < 0) ? 0.0 :
								filebuffer1[(int)fileoffset] *
								CONVMYFLT);
						int32_t fob = off_stop + dist;
						double valb = (1.0f - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[fob] *
								CONVMYFLT);
						smpl1 = vala + valb;
						vala = (dist * onedfadesamples) *
							((fo2 >= off || fo2 < 0) ? 0.0 : filebuffer1[fo2] *
								CONVMYFLT);
						fob = off_start - dist;
						valb = (1.0f - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[fob] *
								CONVMYFLT);
						smpl2 = vala + valb;
					}
					else {
						smpl1 = ((fileoffset >= off || fileoffset < 0) ? 0.0 :
							filebuffer1[(int)fileoffset] *
							CONVMYFLT);
						smpl2 = ((fo2 >= off || fo2 < 0) ? 0.0 :
							filebuffer1[fo2] *
							CONVMYFLT);
					}


				}
				else {
					int32_t dist = DISTANCE(fileoffset, off_start);
					int32_t fo2 = off_stop - dist;

					if (dist <= fadesamples) {
						double vala = (dist * onedfadesamples) *
							((fileoffset >= off || fileoffset < 0) ? 0.0 :
								filebuffer1[(int)fileoffset] *
								CONVMYFLT);
						int32_t fob = off_start - dist;
						double valb = (1.0 - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[fob] *
								CONVMYFLT);
						smpl1 = vala + valb;

						vala = (dist * onedfadesamples) *
							((fo2 >= off || fo2 < 0) ? 0.0 : filebuffer1[(int)fo2] *
								CONVMYFLT);
						fob = off_stop + dist;
						valb = (1.0 - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[fob] *
								CONVMYFLT);
						smpl2 = vala + valb;
					}
					else {
						smpl1 = ((fileoffset >= off || fileoffset < 0) ? 0.0 :
							filebuffer1[(int)fileoffset] *
							CONVMYFLT);
						smpl2 = ((fo2 >= off || fo2 < 0) ? 0.0 :
							filebuffer1[fo2] *
							CONVMYFLT);
					}
				}

				rs->writeNextFrame(smpl1 + smpl2);

				fileoffset += playbackspeed_dir;
				if (fileoffset >= off_stop) {
					fileoffset = off_stop - DISTANCE(off_stop, fileoffset);
					if (fileoffset < off_start)
						fileoffset = off_start;
					playbackspeed_dir *= -1;
				}
				else if (fileoffset < off_start) {
					fileoffset = off_start + DISTANCE(off_start, fileoffset);
					if (fileoffset >= off_stop)
						fileoffset = off_start;
					playbackspeed_dir *= -1;
				}
			}
			outbuf[frameswritten++] = rs->readNextFrame() * 0.5 * pregain;
		}
		else if (bounce_type == NO_BOUNCE) {
			while (rs->isWriteNeeded()) {
				MYFLOAT smpl;
				if (playbackspeed_dir > 0) {
					double dist = DISTANCE(fileoffset, off_stop);
					if (dist <= fadesamples) {
						double vala = (dist * onedfadesamples) *
							((fileoffset >= off || fileoffset < 0) ? 0.0 :
								filebuffer1[(int)fileoffset] *
								CONVMYFLT);
						double fob = off_start - dist;
						double valb = (1.0f - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[(int)fob] *
								CONVMYFLT);
						smpl = vala + valb;
					}
					else
						smpl = ((fileoffset >= off || fileoffset < 0) ? 0.0 :
							filebuffer1[(int)fileoffset] *
							CONVMYFLT);

				}
				else {
					double dist = DISTANCE(fileoffset, off_start);
					if (dist <= fadesamples) {
						double vala = (dist * onedfadesamples) *
							((fileoffset >= off || fileoffset < 0) ? 0.0 :
								filebuffer1[(int)fileoffset] *
								CONVMYFLT);
						double fob = off_stop + dist;
						double valb = (1.0 - (dist * onedfadesamples)) *
							((fob >= off || fob < 0) ? 0.0 : filebuffer1[(int)fob] *
								CONVMYFLT);
						smpl = vala + valb;
					}
					else
						smpl = ((fileoffset >= off || fileoffset < 0) ? 0.0 :
							filebuffer1[(int)fileoffset] *
							CONVMYFLT);
				}

				rs->writeNextFrame(smpl);
				fileoffset += playbackspeed_dir;
				if (fileoffset < off_start) {
					fileoffset = off_stop - DISTANCE(off_start, fileoffset);
					if (fileoffset < off_start)
						fileoffset = off_start;
				}
				else if (fileoffset >= off_stop) {
					fileoffset = off_start + DISTANCE(off_stop, fileoffset);
					if (fileoffset >= off_stop)
						fileoffset = off_start;
				}
			}
			outbuf[frameswritten++] = rs->readNextFrame() * pregain;
		}
		pregain = coeff * (pregain_const - pregain) + pregain;
	}
#ifdef IS_MULTITHREADED
	if (cross_power && isMultiThreaded) {
		track->cross_barrier[channel].loop_exit();
	}
#endif
	auto n = track->fx_queue[channel]._first;
	while (n) {
		auto next = n->next;
		if (n->data->readyToDestroy)
			track->fx_queue[channel].del(n);
		else
			n->data->compute(outbuf, bufsize_init);
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
			std::memcpy(buffer2, buffer1, sizeof(MYFLOAT) * bufsize_init);
		}
#ifdef IS_MULTITHREADED
		else if (_STATE->channels == 2 && isMultiThreaded) {
			track->stereosem.wait();
		}
#endif
		track->pregrainSmoothed = pregain;
		auto nn = track->fx_queue_stereo._first;
		while (nn) {
			auto next = nn->next;
			if (nn->data->readyToDestroy)
				track->fx_queue_stereo.del(nn);
			else
				nn->data->compute(buffer1, buffer2, buffer1,
					buffer2, bufsize_init);
			nn = next;
		}
			track->gainTask.compute(buffer1, buffer2, buffer1,
				buffer2, bufsize_init);
		_DATA->playBufQueue.
			try_push(track);

		if (_DATA->updateRenderThreadSpectrum[track->index].load() && !track->bypass[SPACE_SPECTRUM].load()) {
			for (int i = 0; i < bufsize_init; i++) {
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
						if (auto old = state->offset.exchange(fileoffset) != fileoffset)
							_DATA->snapShot.queue.try_push(tsl::parameters::Event::createEvent(source->index, tsl::parameters::Eventtype::Recording, filebuffer->poolHandle, fileoffset, tsl::parameters::EventSubtype::offset));
				}
			}
		}
	}
}