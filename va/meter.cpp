#include <logger.h>
#include "defines.h"
#include "meter.h"
#include <app.h>
#include <SkCanvas.h>

#if DEBUG_ENABLED == 1
#define DEBUG 1
#else
#define DEBUG 0
#endif
using namespace tsl::graphics;
#define SAMPLETOPERCENTAGE(x) (DBTOPERCENTAGE(LOG10D20(ABS(x))))

const float values[] = {-120, -60, -40, -30, -25, -20, -15, -10, -6, -3, 0};


#define TOP_DRAW(x, y) ((x) > (y) ? (y) : (y) - DISTANCEF((x), (y)))


void Meter::rendervert(void *context) {

    
    float left = _STATE->peak[0];

    float right = _STATE->peak[1];

    _STATE->peak[1] = _STATE->peak[0] = 0.00001f;
    if (left > 2.0f)
        left = 2.0f;
    if (right > 2.0f)
        right = 2.0f;


    float leftdb = LOG10D20F(left);
    float decreaser = 20.f / _STATE->actual_framerate.load();

    static float leftsaved = -135;
    static float peakleft = -135;

    static int dir = 1;

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

    static float rightsaved = -135;
    static float peakright = -135;

    static int dirright = 1;

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


    auto canvas = (SkCanvas *) context;
    // FLUSH(view);

    canvas->save();
    canvas->translate(startx, starty);
    SkPaint paint;
    paint.setStrokeWidth(lw);
    paint.setColor(skcol::bg);
    canvas->drawRect(SkRect::MakeIWH((int) width, (int) (height)), paint);
//#define TOP_DRAW(x, y) ((x) > (y) ? (y) : (y) - DISTANCE((x), (y)))

//	LOGE("%g", DBTOPERCENTAGE(LOG10D20(1.1)));
//	LOGE("%g", LOG10D20(1.1));
    float height_pos = height * .01f * 5.908f;
    float height_neg = height - height_pos;

    float yellowheight = height_neg * DBTOPERCENTAGE(-6);

    float drawheightleft = height_neg * DBTOPERCENTAGE(leftsaved);

    float heightgreenleft = std::min(drawheightleft, yellowheight);
    float heightyellowleft =
            drawheightleft > yellowheight ? DISTANCE(drawheightleft, yellowheight) : 0;
    float heightredleft = drawheightleft > height_neg ? DISTANCE(drawheightleft, height_neg) : 0;
    //double heightred = (leftsaved >= 1.0) ? ((DBTOPERCENTAGE(leftsaved) - 1.0) * 100. / 5.908) *
    //                                        width_pos : 0;

    const float w2 = width * .5f;

    paint.setColor(skcol::custom_green);
    canvas->drawRect(SkRect::MakeXYWH(0, height - heightgreenleft, w2, heightgreenleft), paint);


    paint.setColor(skcol::yellow);
    canvas->drawRect(
            SkRect::MakeXYWH(0, height - yellowheight - heightyellowleft, w2, heightyellowleft),
            paint);

    paint.setColor(skcol::custom_red);
    canvas->drawRect(SkRect::MakeXYWH(0, height - height_neg - heightredleft, w2, heightredleft),
                     paint);


    if (peakleft >= -135) {
        if (peakleft >= 0)
            paint.setColor(skcol::custom_red);
        else if (peakleft >= -6)
            paint.setColor(skcol::yellow);
        else
            paint.setColor(skcol::custom_green);

        canvas->drawLine(0, height - height_neg * DBTOPERCENTAGE(peakleft), w2,
                         height - height_neg * DBTOPERCENTAGE(peakleft), paint);
    }


    float drawheightright = height_neg * DBTOPERCENTAGE(rightsaved);

    float heightgreenright = std::min(drawheightright, yellowheight);
    float heightyellowright =
            drawheightright > yellowheight ? DISTANCE(drawheightright, yellowheight) : 0;
    float heightredright = drawheightright > height_neg ? DISTANCE(drawheightright, height_neg) : 0;

    paint.setColor(skcol::custom_green);
    canvas->drawRect(SkRect::MakeXYWH(w2, height - heightgreenright, w2, heightgreenright), paint);

    paint.setColor(skcol::yellow);
    canvas->drawRect(
            SkRect::MakeXYWH(w2, height - yellowheight - heightyellowright, w2, heightyellowright),
            paint);

    paint.setColor(skcol::custom_red);
    canvas->drawRect(
            SkRect::MakeXYWH(w2, height - height_neg - heightredright, w2, heightredright),
            paint);


    if (peakright >= -135) {
        if (peakright >= 0)
            paint.setColor(skcol::custom_red);
        else if (peakright >= -6)
            paint.setColor(skcol::yellow);
        else
            paint.setColor(skcol::custom_green);

        canvas->drawLine(w2, height - height_neg * DBTOPERCENTAGE(peakright), width,
                         height - height_neg * DBTOPERCENTAGE(peakright), paint);
    }

    paint.setColor(skcol::bg);
    canvas->drawLine(w2, 0, w2, height, paint);

    canvas->restore();
}

void Meter::render(void *context) {


    float left = _STATE->peak[0];

    float right = _STATE->peak[1];

    _STATE->peak[1] = _STATE->peak[0] = 0.00001f;
    if (left > 2.0f)
        left = 2.0f;
    if (right > 2.0f)
        right = 2.0f;


    float leftdb = LOG10D20F(left);
    float decreaser = 20.f / _STATE->actual_framerate.load();

    static float leftsaved = -135;
    static float peakleft = -135;

    static int dir = 1;

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

    static float rightsaved = -135;
    static float peakright = -135;

    static int dirright = 1;

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
    float heightredleft = TOP_DRAW(drawwidthleft, (float)width);
    //double heightred = (leftsaved >= 1.0) ? ((DBTOPERCENTAGE(leftsaved) - 1.0) * 100. / 5.908) *
    //                                        width_pos : 0;


    if (heightredleft >0) {
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
    float heightredright = TOP_DRAW(drawwidthright, (float)width);
    //double heightred = (leftsaved >= 1.0) ? ((DBTOPERCENTAGE(leftsaved) - 1.0) * 100. / 5.908) *
    //                                        width_pos : 0;


    if (heightredright >0) {
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

    paint.setColor(skcol::bg);
    canvas->drawLine(0,  height * .5f, width,  height * .5f, paint);

    canvas->restore();
}