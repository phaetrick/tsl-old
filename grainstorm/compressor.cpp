#include "compressor.h"
#include "logger.h"
#include "grainstorm.h"
#include <SkPath.h>
#include "track.h"
#include "app.h"

MonoCompressor::MonoCompressor(MYFLOAT sr) {
    setAttack(0);
    setRelease(0);
    Threshold = -10;
    SoftKnee = 0; // for first init
    setThreshold(-10);
    setSoftKnee(0);
    setRatio(1);
    env = 0.;
    rms.init(sr);
}


compfullstereo::compfullstereo(TRACK *track) : Effect(track, SPACE_STC, STEREOEFFECT), compL(_appState->sr), compR(_appState->sr) {
    currentfs = _STATE->sr;
    _bypass = &track->bypass[SPACE_STC];
    makeup = &_STATE->params[track->index][STCOMPMAKE];
    prv_makeup = *makeup;
    setMakeUp(*makeup);
    attack = &_STATE->params[track->index][STCOMPATT];
    prv_attack = *attack;
    setAttack(*attack);
    release = &_STATE->params[track->index][STCOMPDEC];
    prv_release = *release;
    setRelease(*release);
    threshold = &_STATE->params[track->index][STCOMPTHR];
    prv_threshold = *threshold;
    setThreshold(*threshold);
    ratio = &_STATE->params[track->index][STCOMPRATIO];
    prv_ratio = *ratio;
    setRatio(*ratio);
    rmssize = &_STATE->params[track->index][STCOMPRMS];
    prv_rmssize = *rmssize;
    setRMS(*rmssize);
    lookahead = &_STATE->params[track->index][STCOMPLOOKA];
    prv_lookahead = *lookahead;
    lookaL.init(_STATE->sr, prv_lookahead * _STATE->sr * 0.001);
    lookaR.init(_STATE->sr, prv_lookahead * _STATE->sr * 0.001);
    knee = &_STATE->params[track->index][STCOMPKNEE];
    prv_knee = *knee;
    setSoftKnee(*knee);
    currentgain = &_STATE->params[track->index][STC_CURRENTGAIN];
    compL.setEnv(*currentgain);
    compR.setEnv(*currentgain);
}

MYFLOAT compfullstereo::getRMS() const {
    return RMS;
}

void compfullstereo::setRMS(MYFLOAT msec) {
    RMS = msec;
    compL.setRMS(_STATE->sr * RMS * 0.001);
    compR.setRMS(_STATE->sr * RMS * 0.001);
}

MYFLOAT compfullstereo::getLookahead() const {
    return Lookahead;
}

void compfullstereo::setLookahead(MYFLOAT msec) {
    Lookahead = msec;
    lookaL.setDelayMS(Lookahead, currentfs);
    lookaR.setDelayMS(Lookahead, currentfs);
}

MYFLOAT compfullstereo::getAttack() const {
    return Attack;
}

void compfullstereo::setAttack(MYFLOAT att_log) {
    Attack = att_log; // stored Log10 value; getAttack()/setSampleRate round-trip on it
    const MYFLOAT msec = LOG2NORMAL(att_log);
    compL.setAttack(MS2SMPL(msec, currentfs));
    compR.setAttack(MS2SMPL(msec, currentfs));
}

MYFLOAT compfullstereo::getRelease() const {
    return Release;
}

void compfullstereo::setRelease(MYFLOAT rel_log) {
    Release = rel_log;
    const MYFLOAT msec = LOG2NORMAL(rel_log);
    compL.setRelease(MS2SMPL(msec, currentfs));
    compR.setRelease(MS2SMPL(msec, currentfs));
}

MYFLOAT compfullstereo::getThreshold() const {
    return Threshold;
}

void compfullstereo::setThreshold(MYFLOAT dB) {
    Threshold = dB;
    compL.setThreshold(LOG2NORMALF(Threshold));
    compR.setThreshold(LOG2NORMALF(Threshold));
    compL.setThresholddB(Threshold);
    compR.setThresholddB(Threshold);
}

MYFLOAT compfullstereo::getSoftKnee() const {
    return SoftKnee;
}

void compfullstereo::setSoftKnee(MYFLOAT dB) {
    SoftKnee = dB;
    compL.setSoftKnee(SoftKnee);
    compR.setSoftKnee(SoftKnee);
}

void compfullstereo::setRatio(MYFLOAT value) {
    Ratio = value;
    compL.setRatio(Ratio);
    compR.setRatio(Ratio);
}


void compfullstereo::setMakeUp(MYFLOAT dB) {
    compL.setMakeUp(dB);
    compR.setMakeUp(dB);
}

//! main process
/*!
  output is valid even inputL/R == outputL/R.
 */

compfullmono::compfullmono(TRACK *track, int32_t chan) : Effect(track, chan, SPACE_COMPRESSION, MONOEFFECT), comp(_appState->sr) {
    _bypass = &track->bypass[SPACE_COMPRESSION];
    makeup = &_STATE->params[track->index][COMPMAKE];
    prv_makeup = *makeup;
    setMakeUp(*makeup);
    attack = &_STATE->params[track->index][COMPATT];
    prv_attack = *attack;
    setAttack(*attack);
    release = &_STATE->params[track->index][COMPREL];
    prv_release = *release;
    setRelease(*release);
    threshold = &_STATE->params[track->index][COMPTHR];
    prv_threshold = *threshold;
    setThreshold(*threshold);
    ratio = &_STATE->params[track->index][COMPRATIO];
    prv_ratio = *ratio;
//com_DATA->setRatio((.1 + .9 * *ratio) * 10.);
    setRatio(*ratio);
    rmssize = &_STATE->params[track->index][COMPRMS];
    prv_rmssize = *rmssize;
    setRMS(*rmssize);
    lookahead = &_STATE->params[track->index][COMPLOOKA];
    prv_lookahead = *lookahead;
    looka.init(_STATE->sr, _STATE->sr * prv_lookahead * 0.001);
    knee = &_STATE->params[track->index][COMPKNEE];
    prv_knee = *knee;
    setSoftKnee(*knee);
    currentgain = &_STATE->params[track->index][COMPGAIN0 + _chan];
    comp.setEnv(*currentgain);
};

void compfullmono::compute(MYFLOAT *in, int32_t size) {
    if (*attack != prv_attack) {
        prv_attack = *attack;
        setAttack(*attack);
    }
    if (*release != prv_release) {
        prv_release = *release;
        setRelease(*release);
    }
    if (*ratio != prv_ratio) {
        prv_ratio = *ratio;
        setRatio(*ratio);
    }
    if (*threshold != prv_threshold) {
        prv_threshold = *threshold;
        setThreshold(*threshold);
    }
    if (*lookahead != prv_lookahead) {
        prv_lookahead = *lookahead;
        setLookahead(*lookahead);
    }
    if (*rmssize != prv_rmssize) {
        prv_rmssize = *rmssize;
        setRMS(*rmssize);
    }
    if (*makeup != prv_makeup) {
        prv_makeup = *makeup;
        setMakeUp(*makeup);
    }
    if (*knee != prv_knee) {
        prv_knee = *knee;
        setSoftKnee(*knee);
    }
    int32_t index = (int) _STATE->params[_track->index][MONOCOMPSRC].load();
        if (index == _track->index)
            process(in, in, size);
        else
            process(_DATA->tracks[index]->envf_buffer[_chan], in,
                    size);
    *currentgain = getEnv();
}

void compfullmono::setSampleRate(MYFLOAT fs) {
    if (fs <= 0) return;
    setRMS(getRMS());
    setLookahead(getLookahead());
    setAttack(getAttack());
    setRelease(getRelease());
}

MYFLOAT compfullmono::getRMS() const {
    return RMS;
}

void compfullmono::setRMS(MYFLOAT msec) {
    RMS = msec;
    comp.setRMS(_STATE->sr * msec * 0.001);
}

MYFLOAT compfullmono::getLookahead() const {
    return Lookahead;
}

void compfullmono::setLookahead(MYFLOAT msec) {
    Lookahead = msec;
    looka.setDelayMS(Lookahead, _STATE->sr);
}

MYFLOAT compfullmono::getAttack() const {
    return Attack;
}

void compfullmono::setAttack(MYFLOAT att_log) {
    Attack = att_log; // stored Log10 value; getAttack()/setSampleRate round-trip on it
    comp.setAttack(MS2SMPL(LOG2NORMAL(att_log), _STATE->sr));
}

MYFLOAT compfullmono::getRelease() const {
    return Release;
}

void compfullmono::setRelease(MYFLOAT rel_log) {
    Release = rel_log;
    comp.setRelease(MS2SMPL(LOG2NORMAL(rel_log), _STATE->sr));
}

void compfullmono::setThreshold(MYFLOAT dB) {
    Threshold = dB;
    comp.setThreshold(LOG2NORMALF(Threshold));
    comp.setThresholddB(Threshold);
}

void compfullmono::setSoftKnee(MYFLOAT dB) {
    SoftKnee = dB;
    comp.setSoftKnee(SoftKnee);
}

void compfullmono::setRatio(MYFLOAT value) {
    Ratio = value;
    comp.setRatio(Ratio);
}

void compfullmono::setMakeUp(MYFLOAT dB) {
    comp.setMakeUp(dB);
}

//! main process
/*!
  output is valid even inputL/R == outputL/R.
 */




#include "view.h"

#include "view.h"
#include "colours.h"
#include "tools.h"

using namespace tsl::graphics;
void CompressorView::render(void *context) {
    auto tindex = _STATE->active_track.load();
    float threshold, ratio, knee, makeup;
    if (GASMAIN == SPACE_FX && GASFX == SPACE_COMPRESSION) {
        threshold = (float) _STATE->params[tindex][COMPTHR];
        ratio = (float) _STATE->params[tindex][COMPRATIO];
        knee = (float) _STATE->params[tindex][COMPKNEE];
        makeup = (float) _STATE->params[tindex][COMPMAKE];
    } else if (GASMAIN == SPACE_REVERB && GASSTFX == SPACE_STC) {
        threshold = (float) _STATE->params[tindex][STCOMPTHR];
        ratio = (float) _STATE->params[tindex][STCOMPRATIO];
        knee = (float) _STATE->params[tindex][STCOMPKNEE];
        makeup = (float) _STATE->params[tindex][STCOMPMAKE];
    } else
        return;
    makeup = 0;
    float w = width - lw, h = height - lw;

    auto canvas = (SkCanvas *) context;
    flush(canvas);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setAntiAlias(true);

    SkPath path;
    path.addRect(SkRect::MakeXYWH(startx, starty, width, height),
                 SkPathDirection::kCW);
    //path.addRect(SkRect::MakeXYWH(view->startx, view->starty, view->width, view->height), SkPath::kCW_Direction);
    canvas->save();
    canvas->clipPath(path);
    paint.setColor(skcol::orange);

    float x = -60;
    canvas->translate(startx, starty);

    while (x < 0) {
        canvas->drawLine(w / 60.f * (60 - ABS(x)), h - (h / 60 * (60 - ABS(x) +
                                                                                MonoCompressor::IR(
                                                                                        x,
                                                                                        threshold,
                                                                                        ratio,
                                                                                        knee,
                                                                                        makeup))),
                         w / 60.f * (60 - ABS(x + .5f)), h - (h / 60 *
                                                                       (60 - ABS(x + .5f) +
                                                                        MonoCompressor::IR(x + .5f,
                                                                                           threshold,
                                                                                           ratio,
                                                                                           knee,
                                                                                           makeup))),
                         paint);
        x += .5f;
    }
    canvas->restore();
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(2);
    paint.setColor(skcol::grey);
    canvas->drawRect(
            SkRect::MakeXYWH(startx + lw2, starty + lw2, width - lw, height - lw),
            paint);//draw_frame2(canvas, view, paint);
}

CompressorView::CompressorView(tsl::AppState *appState, float sc, int32_t as, int al) : View(appState, sc, as, al, 10, false)
{
}