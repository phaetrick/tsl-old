//
// Created by pr on 22.08.21.

#include "ffttools.h"
#include <tools.h>
#include <skia.h>
#include <include/core/SkPath.h>
#include <SkPathEffect.h>
#include <include/effects/SkCornerPathEffect.h>
#include <SkSurface.h>
#include <SkBitmap.h>
#include <SkImageInfo.h>
#include <SkImage.h>
#include <iosfwd>
#include <fstream>
#include <SkMaskFilter.h>
#include <SkTextBlob.h>
#include <SkShadowUtils.h>
#include <SkImageFilter.h>
#include <SkImageFilters.h>
#include <vector>
#include "iconrender.h"


#include <SkGradientShader.h>
#include <lodepng.h>
#include <window.h>
#include <app.h>
#include <include/core/SkStream.h>

#define SIZE size
#define STROKEWIDTH (SIZE * 0.0425)
using namespace tsl::graphics;

float getBoxFilteredSaw(float phase, float kernelSize) {
    float a, b;

    // Check if kernel is longer that one cycle
    if (kernelSize >= 1.0f) {
        return 0.0f;
    }

    // Remap phase and kernelSize from [0.0, 1.0] to [-1.0, 1.0]
    kernelSize *= 2.0f;
    phase = phase * 2.0f - 1.0f;

    if (phase + kernelSize > 1.0f) {
        // Kernel wraps around edge of [-1.0, 1.0]
        a = phase;
        b = phase + kernelSize - 2.0f;
    } else {
        // Kernel fits nicely in [-1.0, 1.0]
        a = phase;
        b = phase + kernelSize;
    }

    // Integrate and divide with kernelSize
    return (b * b - a * a) / (2.0f * kernelSize);
}

float getSaw(float phaseChange) {
    static float phase = 0.0f;
    phase = fmod(phase + phaseChange, 1.0f);
    return getBoxFilteredSaw(phase, phaseChange);
}

float getPulse(float phaseChange, float pulseWidth) {
    static float phase = 0.0f;
    phase = fmod(phase + phaseChange, 1.0f);
    return getBoxFilteredSaw(phase, phaseChange) -
           getBoxFilteredSaw(fmod(phase + pulseWidth, 1.0f), phaseChange);
}


#define s2 (SIZE * .5f)
#define offs ((SIZE * .125f))
#define heightcenter offs
#define offs2 (heightcenter * .5f)
#define offsettop (offs * .25f)

#define offsetcentervertical (offsettop * 1.625f)
#define offsetcenterhorizontal offsettop
#define STARTX (2.5f * offs)
#define SX (offs + STROKEWIDTH)
#define SR (SIZE - 2 * SX)


class Bla {
public:
    Bla(int size) {
        int size1 = next_pow_2(size);
        std::vector<float>tabfft;

        tabfft.resize(size1 + 10, 0);
        for (int i = 0; i <  size1; i++) {
            tabfft[i] = i < size1 / 2 ? - 1.0 : 1.0;
        }
        FFT fft(size1);
        fft.forward(tabfft.data(), tabfft.data());
        tabfft[1] = 0.0f;
        for (int i = 14; i < size1; i++)
            tabfft[i] = 0;
        fft.backward(tabfft.data(), tabfft.data());
        table.resize(size, 0);
        float inc = size1 / (float) size;
        float sp = 0;
        for(int i=0;i<size;i++, sp +=inc){
            int v1 = (int) sp;
            while(v1 >= size1)
                v1 -= size1;
            int v2 = v1 + 1;
            while(v2 >= size1)
                v2 -= size1;
            table[i] = (tabfft[v1] + (sp - v1) * (tabfft[v2] - tabfft[v1]));
        }
        table[size-1] = 0;
    }

    std::vector<float> table;
};

void createLogo2(SkPath &path, SkPaint &paint, int imgsize, float x, float y) {
    Bla bla(imgsize);
    paint.setStyle(SkPaint::kStroke_Style);
    const float randfact = .5f;
    float sw = imgsize * 0.05;
    const float rand = imgsize * randfact;
    const float offset = (sw + rand) * .5f;
    const float inc = (imgsize - sw - rand) / imgsize;
    const float heightmulti = ((imgsize * (1.f - randfact)) * .5) - sw;
    paint.setStrokeWidth(sw);
    float i = offset;
    path.moveTo(x + i, y + imgsize * .5f);

    for (float i = offset, j = 0; i < imgsize - offset - inc; i += inc, j++) {
        if (j >= imgsize)
            j = imgsize - 1;
        path.lineTo(x + i, y + imgsize * .5f + bla.table[j] * heightmulti);
    }
    //path.lineTo(x + imgsize-offset, y + imgsize * .5f);


/*
    paint.setColor(SK_ColorBLACK);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawRect(SkRect::MakeXYWH(0, 0, SIZE, SIZE), paint);

    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(STROKEWIDTH);
    paint.setColor(SK_ColorWHITE);
    canvas->drawArc(SkRect::MakeXYWH(SX, SX, SR, SR), 0, 360, false, paint);

    SkPath path;
    paint.setColor(SK_ColorBLACK);
    canvas->drawLine(s2 + offsettop, offs, s2 - 5.5 * offsetcenterhorizontal,
                     s2 - offsetcentervertical, paint);
    canvas->drawLine(s2 + offsettop, offs, s2 - .5 * offsetcenterhorizontal,
                     s2 - offsetcentervertical, paint);
    canvas->drawLine(s2 - offsettop, SIZE - offs, s2 + .5 * offsetcenterhorizontal,
                     s2 + offsetcentervertical, paint);
    canvas->drawLine(s2 - offsettop, SIZE - offs, s2 + 5.5 * offsetcenterhorizontal,
                     s2 + offsetcentervertical, paint);


    paint.setStyle(SkPaint::kFill_Style);
    path.moveTo(s2 + offsettop, offs);
    path.lineTo(s2 - offsetcenterhorizontal, s2 - offsetcentervertical);
    path.lineTo(SIZE - STARTX, s2 - offsetcentervertical);
    path.lineTo(s2 - offsettop, SIZE - offs);
    path.lineTo(s2 + offsetcenterhorizontal, s2 + offsetcentervertical);
    path.lineTo(STARTX, s2 + offsetcentervertical);
    path.lineTo(s2 + offsettop, offs);
    path.lineTo(s2 - offsetcenterhorizontal, s2 - offsetcentervertical);
    paint.setColor(SK_ColorWHITE);
    canvas->drawPath(path, paint);
    */
}

//#include <lodepng.h>
//#include <SkImageFilters.h>
//#include <SkGradientShader.h>


void drawIcon(){

    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(512, 512);
    auto canvas = surface->getCanvas();

    SkPaint paint;
    SkPath path;
    createLogo2(path, paint, 512, 0, 0);
    canvas->clear(SK_ColorBLACK);
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(SK_ColorWHITE);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    canvas->drawPath(path, paint);
    SkImageInfo info = SkImageInfo::MakeN32Premul(512, 512);
    auto pixels = malloc(info.width() * info.height() * info.bytesPerPixel());
    if (   canvas->readPixels(info, pixels, info.width() * info.bytesPerPixel(), 0, 0)) {
        LOGE("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
    }
    else{
        LOGE("error");
        return;
    }


    auto bytes = (uint8_t*) pixels;
    auto res = static_cast<uint8_t*>(malloc(info.width() * info.height() * 3));
    int off = 0;
    for(int i=0;i<info.width() * info.height() * info.bytesPerPixel(); i+= info.bytesPerPixel(), off+=3){
        auto tmp = &bytes[i];
        res[off] = tmp[0];
        res[off+1] = tmp[1];
        res[off+2] = tmp[2];
    }
    /*
    char buf[4000];
    get_storage_preset(getInstance(), buf);
    std::string str(buf);
    str.append("/icon.png");
    LOGE("%s", str.c_str());
    lodepng::encode(str.c_str(), res, 512, 512, LCT_RGB);

     */
}

void draw3(SkCanvas *canvas) {
canvas->clear(SK_ColorBLACK);

/*    static bool hit = false;
    if(hit)
        return;
    else
        hit = true;
*/
//drawIcon();
//return;
    //sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(1024, 500);
    //auto canvas = surface->getCanvas();
    View v;
    v.width = 1024;//getInstance()->width;
    v.height = 500;//getInstance()->height;
int i=0;
    LOGE("%d", i++);
    const char *text = "POCKET ANALOG";

    SkFont font(_STATE->font_normal);


    float startxtext, startytext, fontsize;
    v.height /= 2;
    v.measureText(&v, _STATE->font_normal, text, .5, &startxtext, &startytext, &fontsize);
    startytext += v.height - fontsize * .25f;

    font.setSize(fontsize);
    //font.setEmbolden(true);


    LOGE("%d", i++);


    const float imgsize = floorf(startytext / 2.5f);
    const float startyimg = floorf((startytext - imgsize) * .5f);
    const float stopyimg = startyimg + imgsize;
    const float startximg = (v.width - imgsize) * .5f;




    SkColor bg = SkColorSetARGB(255, 188, 188, 188);

    SkPaint bgpaint;
    bgpaint.setAntiAlias(true);
    SkPoint bgpoints[] = {{0, 0},
                      {0, (float) v.height*2}};
    const SkScalar bgpos[] = {0, startyimg / (float)v.height, 1.f};
//    const SkScalar bgpos[] = {0, .1f, .9f, 1.f};
  //  SkColor bgcol[] = {bg, SK_ColorWHITE, SK_ColorWHITE, bg};
    SkColor bgcol[] = {SK_ColorWHITE, SK_ColorWHITE, bg};
    bgpaint.setShader(SkGradientShader::MakeLinear(
            /*pts=*/      bgpoints,
            /*colors=*/   bgcol,
            /*pos=*/      bgpos,
            /*count=*/    ARRAY_LEN(bgpos),
            /*tileMode=*/ SkTileMode::kClamp));
    canvas->drawPaint(bgpaint);
//    canvas->clear(SK_ColorWHITE);

    LOGE("%d", i++);


    //const SkScalar sigma = imgsize *.01;
    //const uint8_t blurAlpha = 127;
    //SkPaint blur(paint);
    //blur.setAlpha(blurAlpha);
    //blur.setMaskFilter(SkMaskFilter::MakeBlur(kNormal_SkBlurStyle, sigma, 0));    canvas->drawRect(SkRect::MakeXYWH(x, y, imgsize, imgsize), paint);
    //canvas->drawRect(SkRect::MakeXYWH(x-sigma,y-sigma, imgsize + 2 * sigma, imgsize + sigma), blur);
    SkPath path;
    SkPaint logopaintunscaled;
    //logopaintunscaled.setAntiAlias(true);
    //logopaintunscaled.setAntiAlias(true);
    createLogo2(path, logopaintunscaled, imgsize, startximg,  startyimg);
    SkPaint logopaintscaled(logopaintunscaled);




    SkPoint pointrectshader[] = {{startximg, stopyimg},
                      {startximg, stopyimg - imgsize *.05f}};
    SkColor colorsrectshader[] = {SK_ColorLTGRAY, SK_ColorTRANSPARENT};

    SkPaint rectpaint;
    sk_sp<SkShader> rectgradient  = SkGradientShader::MakeLinear(
            /*pts=*/      pointrectshader,
            /*colors=*/   colorsrectshader,
            /*pos=*/      nullptr,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp);

    rectpaint.setShader(rectgradient);
    rectpaint.setImageFilter(SkImageFilters::Blur(imgsize/50.f, 0, nullptr, nullptr));

    //paint.setMaskFilter(mask);

    LOGE("%d", i++);

    SkColor colorslogoshader[] = {SK_ColorWHITE, SK_ColorTRANSPARENT};

    sk_sp<SkMaskFilter> logomask = SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
                                                          imgsize / 36. * .5f);
    SkPoint logoshader[] = {{startximg, stopyimg},
                                 {startximg, stopyimg - imgsize *.3f}};
    logopaintscaled.setShader(SkGradientShader::MakeLinear(
            /*pts=*/      logoshader,
            /*colors=*/   colorslogoshader,
            /*pos=*/      nullptr,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp));
    //logopaintscaled.setColor(SK_ColorWHITE);
    logopaintscaled.setImageFilter(SkImageFilters::Blur(imgsize/36.f, imgsize/36.f, nullptr, nullptr));

    logopaintscaled.setStyle(SkPaint::kStroke_Style);


    canvas->save();
    SkMatrix imgmatrix;
    imgmatrix.setScale(1, -1, 0, stopyimg);
    canvas->setMatrix(imgmatrix);
    canvas->drawRect(SkRect::MakeXYWH(startximg,startyimg, imgsize, imgsize), rectpaint);
//    canvas->drawPath(path, logopaintscaled);
    //logopaintscaled.setMaskFilter(logomask);
    canvas->restore();



    LOGE("%d", i++);





    SkPaint paint;
    paint.setAntiAlias(true);
    // Use those offsets to create a gradient for the reflected text
    SkPoint pts[] = {{startxtext, startytext},
                      {startxtext, startytext - fontsize*.25f}};
    SkColor colors[] = {SK_ColorGRAY, SK_ColorTRANSPARENT, SK_ColorTRANSPARENT};
    const SkScalar pos2[] = {0., 0.99,1.f};

    sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
            /*pts=*/      pts,
            /*colors=*/   colors,
            /*pos=*/      pos2,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp);

    paint.setShader(gradient);
    paint.setImageFilter(SkImageFilters::Blur(fontsize/36.f, 0/*fontsize/36.f*/, nullptr, nullptr));
    canvas->save();
    SkMatrix matrix;
    matrix.setScale(1, -1, 0, startytext);
    canvas->setMatrix(matrix);
    //canvas->scale(0, SIZE - offs);
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startxtext, startytext, font, paint);
    canvas->restore();
    LOGE("%d", i++);
    paint.reset();
    paint.setAntiAlias(true);
    SkColor color = SkColorSetARGB(255, 26,22,27);//SK_ColorBLACK;

    paint.setColor(color);
    sk_sp<SkMaskFilter> mask = SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
                                                      1.2);
    //paint.setMaskFilter(mask);
    //paint.setImageFilter(SkImageFilters::Blur(fontsize/100.f, 0/*fontsize/36.f*/, nullptr));

    // Draw unreflected text
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startxtext, startytext, font, paint);

    logopaintunscaled.setStrokeCap(SkPaint::kRound_Cap);
    SkPaint rectpaintnormal;

    rectpaintnormal.setColor(SK_ColorBLACK);
    rectpaintnormal.setAntiAlias(true);
    rectpaintnormal.setStyle(SkPaint::kFill_Style);
    // rectpaintnormal.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
    //                                                    imgsize / 100.));
    //rectpaintnormal.setImageFilter(SkImageFilters::Blur(imgsize/75.f, imgsize/75.f, nullptr));

    canvas->drawRect(SkRect::MakeXYWH(startximg,startyimg, imgsize, imgsize), rectpaintnormal);
    logopaintunscaled.setAntiAlias(true);
    logopaintunscaled.setColor(SK_ColorWHITE);
    //logopaintunscaled.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
      //                                                     1.2));
    logopaintunscaled.setStyle(SkPaint::kStroke_Style);

    canvas->drawPath(path, logopaintunscaled);


/*
    SkImageInfo info = SkImageInfo::MakeN32Premul(1024, 500);
    auto pixels = malloc(info.width() * info.height() * info.bytesPerPixel());
    if (   canvas->readPixels(info, pixels, info.width() * info.bytesPerPixel(), 0, 0)) {
        LOGE("OK");
    }
    else return;


    auto bytes = (uint8_t*) pixels;
    auto res = static_cast<uint8_t*>(malloc(info.width() * info.height() * 3));
    int off = 0;
    for(int i=0;i<info.width() * info.height() * info.bytesPerPixel(); i+= info.bytesPerPixel(), off+=3){
        auto tmp = &bytes[i];
        res[off] = tmp[0];
        res[off+1] = tmp[1];
        res[off+2] = tmp[2];
    }
char buf[4000]{};
    //get_storage_preset(getInstance(), buf);
    std::string str(buf);
    str.append("/banner.png");
    LOGE("%s", str.c_str());
    lodepng::encode(str.c_str(), res, 1024, 500, LCT_RGB);

    drawIcon();
  */ // unsigned error = lodepng::encode(str.c_str(), data, pixmap.width(), pixmap.height());
    // SkFILEWStream out(str.c_str());
    /*
    std::ofstream file (str);
    if (file.is_open())
    {
       file.write(static_cast<const char *>(pixmap.addr()), pixmap.computeByteSize());
        file.close();
    }
    else LOGE("Unable to open file");
*/
}

void draw13(SkCanvas *canvas) {
    canvas->clear(SK_ColorWHITE);
    SkPath path;
    float size = 512;
    path.moveTo(s2 + offsettop, offs);
    path.lineTo(s2 - offsetcenterhorizontal, s2 - offsetcentervertical);
    path.lineTo(SIZE - STARTX, s2 - offsetcentervertical);
    path.lineTo(s2 - offsettop, SIZE - offs);
    path.lineTo(s2 + offsetcenterhorizontal, s2 + offsetcentervertical);
    path.lineTo(STARTX, s2 + offsetcentervertical);
    path.lineTo(s2 + offsettop, offs);
    path.lineTo(s2 - offsetcenterhorizontal, s2 - offsetcentervertical);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLACK);
    canvas->drawPath(path, paint);

    // Use those offsets to create a gradient for the reflected text
    SkPoint pts[2] = {{0, offs},
                      {0, SIZE - offs}};
    SkColor colors[2] = {SkColorSetARGB(0, 0, 0, 0), SkColorSetARGB(255, 0, 0, 0)};
    sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
            /*pts=*/      pts,
            /*colors=*/   colors,
            /*pos=*/      nullptr,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp);

    paint.setShader(gradient);
    sk_sp<SkMaskFilter> mask = SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
                                                      SIZE * .75 / 36.);
    paint.setMaskFilter(mask);
    canvas->save();
    SkMatrix matrix;
    matrix.setScale(1, -1, 0, SIZE - offs);
    canvas->setMatrix(matrix);
    //canvas->scale(0, SIZE - offs);
    canvas->drawPath(path, paint);
    canvas->restore();
    /*
    paint.Shader = SKShader.CreateLinearGradient(
            new SKPoint(0, textBounds.Top),
            new SKPoint(0, textBounds.Bottom),
            new SKColor[] { paint.Color.WithAlpha(0),
                            paint.Color.WithAlpha(0x80) },
            null,
            SKShaderTileMode.Clamp);

    // Create a blur mask filter
    paint.MaskFilter = SKMaskFilter.CreateBlur(SKBlurStyle.Normal, paint.TextSize / 36);

    // Scale the canvas to flip upside-down around the vertical center
    canvas.Scale(1, -1, 0, yText);

    // Draw reflected text
    canvas.DrawText(TEXT, xText, yText, paint);
*/


}

void draw(void *ctx) {
    auto canvas = static_cast<SkCanvas *>(ctx);
   // draw3(canvas);
   // return;
    float size = 512;
    canvas->clear(SK_ColorYELLOW);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLACK);
    paint.setStyle(SkPaint::kFill_Style);
    canvas->drawRect(SkRect::MakeXYWH(0, 0, SIZE, SIZE), paint);

    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(STROKEWIDTH);
    paint.setColor(SK_ColorWHITE);
    canvas->drawArc(SkRect::MakeXYWH(SX, SX, SR, SR), 0, 360, false, paint);

    SkPath path;
    paint.setColor(SK_ColorBLACK);
    canvas->drawLine(s2 + offsettop, offs, s2 - 5.5 * offsetcenterhorizontal,
                     s2 - offsetcentervertical, paint);
    canvas->drawLine(s2 + offsettop, offs, s2 - .5 * offsetcenterhorizontal,
                     s2 - offsetcentervertical, paint);
    canvas->drawLine(s2 - offsettop, SIZE - offs, s2 + .5 * offsetcenterhorizontal,
                     s2 + offsetcentervertical, paint);
    canvas->drawLine(s2 - offsettop, SIZE - offs, s2 + 5.5 * offsetcenterhorizontal,
                     s2 + offsetcentervertical, paint);


    paint.setStyle(SkPaint::kFill_Style);
    path.moveTo(s2 + offsettop, offs);
    path.lineTo(s2 - offsetcenterhorizontal, s2 - offsetcentervertical);
    path.lineTo(SIZE - STARTX, s2 - offsetcentervertical);
    path.lineTo(s2 - offsettop, SIZE - offs);
    path.lineTo(s2 + offsetcenterhorizontal, s2 + offsetcentervertical);
    path.lineTo(STARTX, s2 + offsetcentervertical);
    path.lineTo(s2 + offsettop, offs);
    path.lineTo(s2 - offsetcenterhorizontal, s2 - offsetcentervertical);
    paint.setColor(SK_ColorWHITE);
    sk_sp<SkPathEffect> skSp = SkCornerPathEffect::Make((STROKEWIDTH));
    paint.setPathEffect(skSp);
    canvas->drawPath(path, paint);
    //canvas->drawArc(SkRect::MakeXYWH(SX, SX, SR, SR), 0, 360, false, paint);




    // canvas->drawArc(SkRect::MakeIWH(SIZE, SIZE), 0, 360, false, paint);
}

void draw2(void *ctx) {
    float size = 512;
    auto canvas = static_cast<SkCanvas *>(ctx);
    canvas->clear(SK_ColorBLACK);
    float width = SIZE, /*= canvas->getSurface()->width(),*/ height = SIZE;//canvas->getSurface()->height();
    SkPaint paint;
    paint.setStrokeWidth(width * .05);
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorWHITE);
    paint.setStyle(SkPaint::kStroke_Style);
    canvas->drawCircle(width * .5f, height * .5f, width * .375 - width * .085, paint);
    SkPath path;
    /*
     path.moveTo(s2 - offs, s2 + offs);
     path.lineTo(offs2, s2 +offs);
    path.lineTo(s2+offs, offs2);
     path.lineTo(s2 - offs, s2 + offs);
     */
    paint.setStyle(SkPaint::kFill_Style);
    path.moveTo(s2 + offs, offs2);
    path.lineTo(s2 - offs, s2 - offs2 * 3.f);
    path.lineTo(width - width * .15, s2 - offs2 * 3.f);
    path.lineTo(s2 - offs, height - offs2);
    path.lineTo(s2 + offs, s2 + offs2 * 3.f);
    path.lineTo(width * 0.15, s2 + offs2 * 3.f);
    path.lineTo(s2 + offs, offs2);
    path.lineTo(s2 - offs, s2 - offs2 * 3.f);
    canvas->save();
    // canvas->translate(-width * .05, 05);
    float ooo = width * .035;
    paint.setColor(SK_ColorBLACK);
    canvas->translate(-ooo, 0);
    canvas->drawPath(path, paint);
    canvas->restore();
    canvas->save();
    canvas->translate(ooo, 0);
    canvas->drawPath(path, paint);
    canvas->restore();
    canvas->save();
    canvas->translate(0, -ooo);
    canvas->drawPath(path, paint);
    canvas->restore();
    canvas->save();
    canvas->translate(0, ooo);
    canvas->drawPath(path, paint);
    canvas->restore();
    /*
    canvas->save();
    canvas->translate(-ooo, ooo);
    canvas->drawPath(path, paint);
    canvas->restore();canvas->save();
    canvas->translate(ooo, -ooo);
    canvas->drawPath(path, paint);
    canvas->restore();
     */

    paint.setColor(skcol::waveform);
    sk_sp<SkPathEffect> skSp = SkCornerPathEffect::Make((offs));
    paint.setPathEffect(skSp);
    canvas->drawPath(path, paint);

    // canvas->drawArc(SkRect::MakeIWH(SIZE, SIZE), 0, 360, false, paint);
}


sk_sp<SkSurface> intopng() {
    float size = 512;
    sk_sp<SkSurface> rasterSurface = SkSurface::MakeRasterN32Premul(SIZE, SIZE);
    draw(rasterSurface->getCanvas());
    sk_sp<SkImage> img(rasterSurface->makeImageSnapshot());
    if (!img) { return rasterSurface; }
    LOGE("XXXXXXXXXXXXXX %d %d", img->width(), img->height());
    SkPixmap pixmap;
    img->peekPixels(&pixmap);

    ///if (!png) { return rasterSurface; }
    char buf[4000];
  //  get_storage_preset(getInstance(), buf);
    std::string str(buf);
    str.append("/test.png");
    LOGE("XXXXXXXXXXXXXXXXXXX   %s %ld", str.c_str(), pixmap.computeByteSize());
    std::vector<unsigned char> data;
    data.resize(pixmap.computeByteSize());
    memcpy(data.data(), pixmap.addr(), pixmap.computeByteSize());
    unsigned error = lodepng::encode(str.c_str(), data, pixmap.width(), pixmap.height());
    // SkFILEWStream out(str.c_str());
    /*
    std::ofstream file (str);
    if (file.is_open())
    {
       file.write(static_cast<const char *>(pixmap.addr()), pixmap.computeByteSize());
        file.close();
    }
    else LOGE("Unable to open file");
    */return rasterSurface;
}

void draw5(SkCanvas *canvas) {
    canvas->clear(SK_ColorBLACK);

/*    static bool hit = false;
    if(hit)
        return;
    else
        hit = true;
*/
//drawIcon();
//return;
/*
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(1024, 500);
    auto canvas = surface->getCanvas();
  */  View v;
    v.width = canvas->getSurface()->width();//1024getInstance()->width;
    v.height = canvas->getSurface()->height();//500;//getInstance()->height;
    int i=0;
    LOGE("%d", i++);
    const char *text = "POCKET ANALOG";

    SkFont font(_STATE->font_normal);


    float startxtext, startytext, fontsize;
    v.height /= 2;
    v.measureText(&v, _STATE->font_normal, text, .5, &startxtext, &startytext, &fontsize);
    startytext += v.height - fontsize * .25f;

    font.setSize(fontsize);
    //font.setEmbolden(true);


    LOGE("%d", i++);


    const float imgsize = floorf(startytext / 2.5f);
    const float startyimg = floorf((startytext - imgsize) * .5f);
    const float stopyimg = startyimg + imgsize;
    const float startximg = (v.width - imgsize) * .5f;




    //SkColor bg = SkColorSetARGB(255, 255-188, 255-188, 255-188);

    SkPaint bgpaint;
    bgpaint.setAntiAlias(true);
    SkPoint bgpoints[] = {{0, 0},
                          {0, (float) v.height*2}};
    const SkScalar bgpos[] = {0, startyimg / (float)v.height, 1.f};
//    const SkScalar bgpos[] = {0, .1f, .9f, 1.f};
    //  SkColor bgcol[] = {bg, SK_ColorWHITE, SK_ColorWHITE, bg};
    SkColor bgcol[] = {tsl::sk_colours::orange, SK_ColorBLACK, SK_ColorBLACK};
    bgpaint.setShader(SkGradientShader::MakeLinear(
            /*pts=*/      bgpoints,
            /*colors=*/   bgcol,
            /*pos=*/      bgpos,
            /*count=*/    ARRAY_LEN(bgpos),
            /*tileMode=*/ SkTileMode::kClamp));
    canvas->drawPaint(bgpaint);
//    canvas->clear(SK_ColorWHITE);

    LOGE("%d", i++);


    SkPath path;
    SkPaint logopaintunscaled;
    //logopaintunscaled.setAntiAlias(true);
    //logopaintunscaled.setAntiAlias(true);
    createLogo2(path, logopaintunscaled, imgsize, startximg,  startyimg);
    SkPaint logopaintscaled(logopaintunscaled);




    SkPoint pointrectshader[] = {{startximg, stopyimg},
                                 {startximg, stopyimg - imgsize *.05f}};
    SkColor colorsrectshader[] = {SK_ColorLTGRAY, SK_ColorTRANSPARENT};

    SkPaint rectpaint;
    sk_sp<SkShader> rectgradient  = SkGradientShader::MakeLinear(
            /*pts=*/      pointrectshader,
            /*colors=*/   colorsrectshader,
            /*pos=*/      nullptr,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp);

    rectpaint.setShader(rectgradient);
    rectpaint.setImageFilter(SkImageFilters::Blur(imgsize/50.f, 0, nullptr, nullptr));

    //paint.setMaskFilter(mask);

    LOGE("%d", i++);

    SkColor colorslogoshader[] = {SK_ColorGRAY, SK_ColorRED};

    SkPoint logoshader[] = {{startximg, stopyimg},
                            {startximg, stopyimg - imgsize * .33f}};
    const SkScalar pos4[] = {0., 1.0,1.f};
    logopaintscaled.setShader(SkGradientShader::MakeLinear(
            /*pts=*/      logoshader,
            /*colors=*/   colorslogoshader,
            /*pos=*/      pos4,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp));
    //logopaintscaled.setColor(SK_ColorWHITE);
    logopaintscaled.setImageFilter(SkImageFilters::Blur(imgsize/36.f, 0, nullptr, nullptr));

    logopaintscaled.setStyle(SkPaint::kStroke_Style);


    canvas->save();
    SkMatrix imgmatrix;
    imgmatrix.setScale(1, -1, 0, stopyimg);
    canvas->setMatrix(imgmatrix);
    auto icon = __STATE->LoadPNG();
    // imgsize, startximg,  startyimg
   // canvas->drawImageRect(icon, SkRect::MakeXYWH(startximg, startyimg, imgsize, imgsize),
     //                     SkSamplingOptions(), &logopaintscaled);
    canvas->drawRect(SkRect::MakeXYWH(startximg,startyimg, imgsize, imgsize), logopaintscaled);
//    canvas->drawPath(path, logopaintscaled);
    //logopaintscaled.setMaskFilter(logomask);
    canvas->restore();



    LOGE("%d", i++);





    SkPaint paint;
    paint.setAntiAlias(true);
    // Use those offsets to create a gradient for the reflected text
    SkPoint pts[] = {{startxtext, startytext},
                     {startxtext, startytext - fontsize*.5f}};
    SkColor colors[] = {SK_ColorGRAY, SK_ColorTRANSPARENT, SK_ColorTRANSPARENT};
    const SkScalar pos2[] = {0., 0.99,1.f};

    sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
            /*pts=*/      pts,
            /*colors=*/   colors,
            /*pos=*/      pos2,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp);

    paint.setShader(gradient);
    paint.setImageFilter(SkImageFilters::Blur(fontsize/36.f, 0/*fontsize/36.f*/, nullptr, nullptr));
    canvas->save();
    SkMatrix matrix;
    matrix.setScale(1, -1, 0, startytext);
    canvas->setMatrix(matrix);
    //canvas->scale(0, SIZE - offs);
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startxtext, startytext, font, paint);
    canvas->restore();
    LOGE("%d", i++);
    paint.reset();
    paint.setAntiAlias(true);
    SkColor color = SkColorSetARGB(255, 26,22,27);//SK_ColorBLACK;

    paint.setColor(tsl::sk_colours::fg);
    sk_sp<SkMaskFilter> mask = SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
                                                      1.2);
    //paint.setMaskFilter(mask);
    //paint.setImageFilter(SkImageFilters::Blur(fontsize/100.f, 0/*fontsize/36.f*/, nullptr));

    // Draw unreflected text
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startxtext, startytext, font, paint);

    logopaintunscaled.setStrokeCap(SkPaint::kRound_Cap);
    SkPaint rectpaintnormal;

    rectpaintnormal.setColor(SK_ColorBLACK);
    rectpaintnormal.setAntiAlias(true);
    rectpaintnormal.setStyle(SkPaint::kFill_Style);
    // rectpaintnormal.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
    //                                                    imgsize / 100.));
    //rectpaintnormal.setImageFilter(SkImageFilters::Blur(imgsize/75.f, imgsize/75.f, nullptr));

    canvas->drawRect(SkRect::MakeXYWH(startximg,startyimg, imgsize, imgsize), rectpaintnormal);
    logopaintunscaled.setAntiAlias(true);
    logopaintunscaled.setColor(SK_ColorWHITE);
    //logopaintunscaled.setMaskFilter(SkMaskFilter::MakeBlur(SkBlurStyle::kNormal_SkBlurStyle,
    //                                                     1.2));
    logopaintunscaled.setStyle(SkPaint::kStroke_Style);
    //auto icon = __STATE->LoadPNG();
   // imgsize, startximg,  startyimg
    canvas->drawImageRect(icon, SkRect::MakeXYWH(startximg, startyimg, imgsize, imgsize),
                     SkSamplingOptions(), &paint);
    //canvas->drawPath(path, logopaintunscaled);
    //_canvas->getSurface()->draw(canvas,0,0);

/*
    SkImageInfo info = SkImageInfo::MakeN32Premul(1024, 500);
    auto pixels = malloc(info.width() * info.height() * info.bytesPerPixel());
    if (   canvas->readPixels(info, pixels, info.width() * info.bytesPerPixel(), 0, 0)) {
        LOGE("OK");
    }
    else return;


    auto bytes = (uint8_t*) pixels;
    auto res = static_cast<uint8_t*>(malloc(info.width() * info.height() * 3));
    int off = 0;
    for(int i=0;i<info.width() * info.height() * info.bytesPerPixel(); i+= info.bytesPerPixel(), off+=3){
        auto tmp = &bytes[i];
        res[off] = tmp[0];
        res[off+1] = tmp[1];
        res[off+2] = tmp[2];
    }
char buf[4000]{};
    //get_storage_preset(getInstance(), buf);
    std::string str(buf);
    str.append("/banner.png");
    LOGE("%s", str.c_str());
    lodepng::encode(str.c_str(), res, 1024, 500, LCT_RGB);

    drawIcon();
  */ // unsigned error = lodepng::encode(str.c_str(), data, pixmap.width(), pixmap.height());
    // SkFILEWStream out(str.c_str());
    /*
    std::ofstream file (str);
    if (file.is_open())
    {
       file.write(static_cast<const char *>(pixmap.addr()), pixmap.computeByteSize());
        file.close();
    }
    else LOGE("Unable to open file");
*/
}
#include <fonts/futura_extra_bold.h>
void draw4(SkCanvas *_canvass) {
    _canvass->clear(SK_ColorWHITE);
    sk_sp<SkTypeface> tf{SkTypeface::MakeFromStream(
            SkMemoryStream::MakeDirect(futura_extra_bold_data,
                                       futura_extra_bold_size), 0)};
    SkFont font{tf};
/*    static bool hit = false;
    if(hit)
        return;
    else
        hit = true;
*/
//drawIcon();
//return;
/*
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(1024, 500);
    auto canvas = surface->getCanvas();
  */

auto s = SkSurface::MakeRasterN32Premul(1024,500);
auto canvas = s->getCanvas();
canvas->clear(SK_ColorBLACK);
View v;
    v.width = 1024;//canvas->getSurface()->width();//1024getInstance()->width;
    v.height = 500;//canvas->getSurface()->height();//500;//getInstance()->height;
    const char *text = "POCKET ANALOG";

   // font.setEmbolden(true);
    float startxtext, startytext, fontsize;
    v.measureText(&v, font, text, .5, &startxtext, &startytext, &fontsize);
    SkRect bounds;
    font.setSize(fontsize);
    font.measureText(text, strlen(text), SkTextEncoding::kUTF8, &bounds);

    startytext = v.height *.61 + bounds.height();
    const float imgsize = v.height * .61 * .5;
    const float startyimg = v.height* .39 -   imgsize * .5;
    const float stopyimg = startyimg + imgsize;
    const float startximg = (v.width - imgsize) * .5f;





    //font.setEmbolden(true);






    //SkColor bg = SkColorSetARGB(255, 255-188, 255-188, 255-188);

    SkPaint bgpaint;
    bgpaint.setAntiAlias(true);
    SkPoint bgpoints[] = {{0, 0},
                          {0, (float) v.height}};
    const SkScalar bgpos[] = {0, .61, 1.f};
//    const SkScalar bgpos[] = {0, .1f, .9f, 1.f};
    //  SkColor bgcol[] = {bg, SK_ColorWHITE, SK_ColorWHITE, bg};
    SkColor bgcol[] = {tsl::sk_colours::orange, SK_ColorBLACK, SK_ColorBLACK};
    bgpaint.setShader(SkGradientShader::MakeLinear(
            /*pts=*/      bgpoints,
            /*colors=*/   bgcol,
            /*pos=*/      bgpos,
            /*count=*/    ARRAY_LEN(bgpos),
            /*tileMode=*/ SkTileMode::kClamp));
    canvas->drawPaint(bgpaint);
//    canvas->clear(SK_ColorWHITE);


    SkPoint pointrectshader[] = {{startximg, stopyimg},
                                 {startximg, stopyimg - imgsize *.05f}};
    SkColor colorsrectshader[] = {SK_ColorLTGRAY, SK_ColorTRANSPARENT};

    SkPaint rectpaint;
    sk_sp<SkShader> rectgradient  = SkGradientShader::MakeLinear(
            /*pts=*/      pointrectshader,
            /*colors=*/   colorsrectshader,
            /*pos=*/      nullptr,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp);

    rectpaint.setShader(rectgradient);
    rectpaint.setImageFilter(SkImageFilters::Blur(imgsize/50.f, 0, nullptr, nullptr));

    //paint.setMaskFilter(mask);

    SkColor colorslogoshader[] = {SK_ColorBLACK, SK_ColorTRANSPARENT};

    SkPoint logoshader[] = {{startximg, stopyimg},
                            {startximg, stopyimg - imgsize * .1f}};
    const SkScalar pos4[] = {0., 1.0,1.f};

    SkPaint paintlogo;
    paintlogo.setAntiAlias(true);
    paintlogo.setStyle(SkPaint::kFill_Style);
    paintlogo.setShader(SkGradientShader::MakeLinear(
            /*pts=*/      logoshader,
            /*colors=*/   colorslogoshader,
            /*pos=*/      pos4,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp));
    //logopaintscaled.setColor(SK_ColorWHITE);
    paintlogo.setImageFilter(SkImageFilters::Blur(imgsize/36.f, 0, nullptr, nullptr));

    canvas->save();
    SkMatrix imgmatrix;
    imgmatrix.setScale(1, -1, 0, stopyimg +1);
    canvas->setMatrix(imgmatrix);
    auto icon = __STATE->LoadPNG();
    // imgsize, startximg,  startyimg
     //canvas->drawImageRect(icon, SkRect::MakeXYWH(startximg, startyimg, imgsize, imgsize),
       //                  SkSamplingOptions(), &paintlogo);
    canvas->drawRect(SkRect::MakeXYWH(startximg,startyimg, imgsize, imgsize), paintlogo);
//    canvas->drawPath(path, logopaintscaled);
    //logopaintscaled.setMaskFilter(logomask);
    canvas->restore();




    SkPaint paint;
    paint.setAntiAlias(true);
    // Use those offsets to create a gradient for the reflected text
    SkPoint pts[] = {{startxtext, startytext},
                     {startxtext, startytext - fontsize*.4f}};
    SkColor colors[] = {SK_ColorGRAY, SK_ColorTRANSPARENT, SK_ColorTRANSPARENT};
    const SkScalar pos2[] = {0., 0.99,1.f};

    sk_sp<SkShader> gradient = SkGradientShader::MakeLinear(
            /*pts=*/      pts,
            /*colors=*/   colors,
            /*pos=*/      pos2,
            /*count=*/    2,
            /*tileMode=*/ SkTileMode::kClamp);

    paint.setShader(gradient);
    paint.setImageFilter(SkImageFilters::Blur(fontsize/36.f, 0/*fontsize/36.f*/, nullptr, nullptr));
    canvas->save();
    SkMatrix matrix;
    matrix.setScale(1, -1, 0, startytext + 1);
    canvas->setMatrix(matrix);
    //canvas->scale(0, SIZE - offs);
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startxtext, startytext, font, paint);
    canvas->restore();
    paint.reset();
    paint.setAntiAlias(true);
    SkColor color = SkColorSetARGB(255, 26,22,27);//SK_ColorBLACK;

    paint.setColor(tsl::sk_colours::fg);

    // Draw unreflected text
    canvas->drawSimpleText(text, strlen(text), SkTextEncoding::kUTF8, startxtext, startytext, font, paint);

    SkPaint rectpaintnormal;
    rectpaintnormal.setAntiAlias(true);
    rectpaintnormal.setStyle(SkPaint::kStroke_Style);
    const SkCubicResampler cubic = {0, 0};
    SkSamplingOptions skSamplingOptions(cubic);
    canvas->drawImageRect(icon, SkRect::MakeXYWH(startximg, startyimg, imgsize, imgsize),
                          SkSamplingOptions(SkFilterMode::kLinear, SkMipmapMode::kNearest), &rectpaintnormal);
    static bool done = false;
    if(done)
        return;
    else done = true;
    ATTACH
    if (env && tsl::android::activityclass) {
        jmethodID mid = env->GetStaticMethodID(tsl::android::activityclass, "getff", "()I");
        if (mid) {
            auto fdd = (int ) env->CallStaticIntMethod(tsl::android::activityclass, mid, nullptr);
        } else {
            env->ExceptionClear();
        }
    }
    DETACH
    //canvas->drawPath(path, logopaintunscaled);
    //_canvas->getSurface()->draw(canvas,0,0);
    SkImageInfo info = SkImageInfo::MakeN32Premul(1024, 500);
    auto pixels = malloc(info.width() * info.height() * info.bytesPerPixel());
    if (canvas->readPixels(info, pixels, info.width() * info.bytesPerPixel(), 0, 0)) {
        LOGE("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
    }
    else{
        LOGE("error");
        return;
    }


    auto bytes = (uint8_t*) pixels;
    std::vector<unsigned char> out;
    unsigned error = lodepng::encode(out, (const unsigned char*)pixels, 1024, 500);
    //unsigned error = lodepng::encode(ret, (const unsigned char*)pixels, canvas->getSurface()->width(), canvas->getSurface()->height());
LOGE("%d %s", fdd, lodepng_error_text(error));
auto fd = fdopen(fdd, "wb");
if(fwrite(out.data(), out.size(), 1,fd) !=1)LOGE("dskfhsdf");
fclose(fd);

    /*
    char buf[4000];
    get_storage_preset(getInstance(), buf);
    std::string str(buf);
    str.append("/icon.png");
    LOGE("%s", str.c_str());
    lodepng::encode(str.c_str(), res, 512, 512, LCT_RGB);
    // SkFILEWStream out(str.c_str());
    /*
    std::ofstream file (str);
    if (file.is_open())
    {
       file.write(static_cast<const char *>(pixmap.addr()), pixmap.computeByteSize());
        file.close();
    }
    else LOGE("Unable to open file");
*/
    _canvass->getSurface()->draw(canvas, 0, 0);
}

void IconRender2::render(void *ctx) {
    startx = starty = 0;
    width = height = stopx = stopy = 512.f;
    width = 1024;
    height = 500;
    stopx = 1024;
    stopy = 500;
    perm = true;
    draw4(static_cast<SkCanvas*>(ctx));
}