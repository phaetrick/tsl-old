#include "defines.h"
#include "logger.h"
#include <SkCanvas.h>
#include "grainstorm.h"
#include "view.h"
#include "meter.h"
#include "track.h"
#include "colours.h"
#include "compressor.h"
#include "app.h"

#if DEBUG_ENABLED == 1
#define DEBUG 1
#else
#define DEBUG 0
#endif

#define SAMPLETOPERCENTAGE(x) (DBTOPERCENTAGE(LOG10D20(ABS(x))))

const float values[] = {-120, -60, -40, -30, -25, -20, -15, -10, -6, -3, 0};


#define TOP_DRAW(x, y) ((x) > (y) ? (y) : (y) - DISTANCEF((x), (y)))


using namespace tsl::graphics;

void Meter::render(void *context) {

    auto tindex = _STATE->active_track.load();

    float left = _STATE->peak[0];

    float right = _STATE->peak[1];

    _STATE->peak[1] = _STATE->peak[0] = 0.00001f;
    left = left > 2.f ? 2.f : (left < - 2.f ? -2.f : left);
    right = right > 2.f ? 2.f : (right < - 2.f ? -2.f : right);

    float leftdb = LOG10D20F(left);
    float decreaser = 20.f / _STATE->actual_framerate.load();

    
    if (leftdb > leftsaved) {
        leftsaved = leftdb;
        if (dir == -1) {
            dir = 1;
        }
    } else {
        if (dir == 1) {
            peakleft = leftsaved;
            dir = -1;
        }
        leftsaved -= decreaser;
    }
    if (leftsaved < -130)
        leftsaved = -130;



    //left = ABS(left) > 1 ? 1 : left;
    //right = ABS(right) > 1 ? 1 : right;


    float rightdb = LOG10D20F(right);

    if (rightdb > rightsaved) {
        rightsaved = rightdb;
        if (dirright == -1) {
            dirright = 1;
        }
    } else {
        if (dirright == 1) {
            peakright = rightsaved;
            dirright = -1;
        }
        rightsaved -= decreaser;
    }
    if (rightsaved < -130)
        rightsaved = -130;


    auto canvas = (SkCanvas*) context;
   // FLUSH(view);

    canvas->save();
    canvas->translate(startx, starty);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStrokeWidth(lw);
    paint.setColor(skcol::bg);
    canvas->drawRect(SkRect::MakeIWH((int) width, (int) (height)), paint);
//#define TOP_DRAW(x, y) ((x) > (y) ? (y) : (y) - DISTANCE((x), (y)))

//	LOGE("%g", DBTOPERCENTAGE(LOG10D20(1.1)));
//	LOGE("%g", LOG10D20(1.1));
    float width_neg = width - width * .01f * 5.908f;

    float yellowstart = width_neg * DBTOPERCENTAGE(-6);
    float drawwidthleft = width_neg * DBTOPERCENTAGE(leftsaved);

    float heightgreenleft = TOP_DRAW(drawwidthleft, yellowstart);
    float heightyellowleft = TOP_DRAW(drawwidthleft, width_neg);
    float heightredleft = TOP_DRAW(drawwidthleft, width.load());
    //double heightred = (leftsaved >= 1.0) ? ((DBTOPERCENTAGE(leftsaved) - 1.0) * 100. / 5.908) *
    //                                        width_pos : 0;


    if (heightredleft) {
        paint.setColor(skcol::custom_red);
        canvas->drawRect(SkRect::MakeIWH((int) heightredleft, (int) (height * .5)), paint);
    }

    paint.setColor(skcol::yellow);
    canvas->drawRect(SkRect::MakeIWH((int) (heightyellowleft), (int) (height * .5)), paint);

    paint.setColor(skcol::custom_green);
    canvas->drawRect(SkRect::MakeIWH((int) heightgreenleft, (int) (height * .5)), paint);

    if (peakleft >= -135) {
        if (peakleft >= 0)
            paint.setColor(skcol::custom_red);
        else if (peakleft >= -6)
            paint.setColor(skcol::yellow);
        else
            paint.setColor(skcol::custom_green);

        canvas->drawLine(width_neg * DBTOPERCENTAGE(peakleft), 0, width_neg * DBTOPERCENTAGE(peakleft), height * .5f,  paint);
    }


    float drawwidthright = width_neg * DBTOPERCENTAGE(rightsaved);

    float heightgreenright = TOP_DRAW(drawwidthright, yellowstart);
    float heightyellowright = TOP_DRAW(drawwidthright, width_neg);
    float heightredright = TOP_DRAW(drawwidthright, width.load());
    //double heightred = (leftsaved >= 1.0) ? ((DBTOPERCENTAGE(leftsaved) - 1.0) * 100. / 5.908) *
    //                                        width_pos : 0;


    if (heightredright) {
        paint.setColor(skcol::custom_red);
        canvas->drawRect(SkRect::MakeXYWH(0, height * .5f, heightredright, height * .5f), paint);
    }

    paint.setColor(skcol::yellow);
    canvas->drawRect(SkRect::MakeXYWH(0, height * .5f, heightyellowright, height * .5f), paint);

    paint.setColor(skcol::custom_green);
    canvas->drawRect(SkRect::MakeXYWH(0, height * .5f, heightgreenright, height * .5f), paint);

    if (peakright >= -135) {
        if (peakright >= 0)
            paint.setColor(skcol::custom_red);
        else if (peakright >= -6)
            paint.setColor(skcol::yellow);
        else
            paint.setColor(skcol::custom_green);

        canvas->drawLine(width_neg * DBTOPERCENTAGE(peakright), height * .5f, width_neg * DBTOPERCENTAGE(peakright), height,  paint);
    }


    if (_DATA->tracks[tindex]->fxpower[SPACE_COMPRESSION]) {
        float draw_offset = height / (float) _STATE->channels;
        for (int32_t i = 0; i < _STATE->channels; i++) {
                paint.setColor(skcol::orange);
                float width_coeff = DBTOPERCENTAGE(-_STATE->params[tindex][COMPGAIN0+i].load());
                canvas->drawLine(width_neg * width_coeff, i * draw_offset, width_neg * width_coeff, (i + 1) * draw_offset,  paint);
            }
        paint.setColor(skcol::orange);
        float width_coeff = DBTOPERCENTAGE(_STATE->params[tindex][COMPTHR].load());
        canvas->drawLine(width_neg * width_coeff, 0, width_neg * width_coeff,  height, paint);
    }

    if (_DATA->tracks[tindex]->fxpower[SPACE_STC].load()) {
        float draw_offset = height / (float) _STATE->channels;
        paint.setColor(skcol::blue);
        //double width_coeff = DBTOPERCENTAGE(
        //        LOG10D20(
        //                gain < 1. ? (float) (1. - gain) : (float) ABS(
        //                        1. - gain)));
        float width_coeff = DBTOPERCENTAGE(- _STATE->params[tindex][STC_CURRENTGAIN].load());

        for (int32_t i = 0; i < _STATE->channels; i++) {
            canvas->drawLine(width_neg * width_coeff, i * draw_offset, width_neg * width_coeff,  (i + 1) * draw_offset, paint);
        }
        width_coeff = DBTOPERCENTAGE(_STATE->params[tindex][STCOMPTHR].load());
        canvas->drawLine(width_neg * width_coeff,0, width_neg * width_coeff,  height, paint);
    }
    paint.setColor(skcol::bg);
    canvas->drawLine(0,  height * .5f, width,  height * .5f, paint);

    canvas->restore();
}