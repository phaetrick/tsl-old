//
// Created by pr on 12.07.25.
//
#include "SpectrumAnalyzerView.h"
#include <SkGradientShader.h>

using namespace tsl::graphics;

inline SkPoint addScaled(const SkPoint &a, const SkPoint &b, float scale) {
    return SkPoint::Make(a.x() + b.x() * scale, a.y() + b.y() * scale);
}

inline SkPoint sub(const SkPoint &a, const SkPoint &b) {
    return SkPoint::Make(a.x() - b.x(), a.y() - b.y());
}

inline SkPoint interpolate(const SkPoint &a, const SkPoint &b, float t) {
    return {a.x() + t * (b.x() - a.x()), a.y() + t * (b.y() - a.y())};
}

template<typename T>
static SkPath
buildSpectrumPath(const std::vector<T> &values, float width, float height, float min,
                  float max) {
    int N = (int) values.size();
    if (N < 2) return SkPath();

    SkPath path;

    // Optional: Use log spacing
    auto xAt = [=](int i) {
        float norm = (float) i / (N - 1);
        return std::log10(1.0f + 9.0f * norm) / std::log10(10.0f);  // Log base-10 scale
    };

    std::vector<SkPoint> points(N);
    for (int i = 0; i < N; ++i) {
        float x = (float) i / (N - 1) * width;
        float y = (1.0f - tsl::toNorm(values[i], min, max)) * height;
        points[i] = SkPoint::Make(x, y);
    }

    // Start path
    path.moveTo(points[0]);

    // Cubic Bézier using adjacent points as control (Catmull-Rom-like)
    for (int i = 1; i < N - 2; ++i) {
        const SkPoint &p0 = points[i - 1];
        const SkPoint &p1 = points[i];
        const SkPoint &p2 = points[i + 1];
        const SkPoint &p3 = points[i + 2];

        // Calculate Bézier control points
        SkPoint cp1 = p1 + (p2 - p0) * (1.0f / 6.0f);
        SkPoint cp2 = p2 - (p3 - p1) * (1.0f / 6.0f);

        path.cubicTo(cp1, cp2, p2);
    }

    // Optional: line to end if not covered
    path.lineTo(points.back());

    return path;
}

template<typename T>
static SkPath
buildSpectrumPathNormalized(const T values, float width, float height, int N) {
    if (N < 2) return SkPath();

    SkPath path;


    std::vector<SkPoint> points(N);
    for (int i = 0; i < N; ++i) {
        points[i] = SkPoint::Make((float) i / (N - 1) * width, (1.0 - values[i]) * height);
    }

    // Start path
    path.moveTo(points[0]);

    // Cubic Bézier using adjacent points as control (Catmull-Rom-like)
    for (int i = 1; i < N - 2; ++i) {
        const SkPoint &p0 = points[i - 1];
        const SkPoint &p1 = points[i];
        const SkPoint &p2 = points[i + 1];
        const SkPoint &p3 = points[i + 2];

        // Calculate Bézier control points
        SkPoint cp1 = p1 + (p2 - p0) * (1.0f / 6.0f);
        SkPoint cp2 = p2 - (p3 - p1) * (1.0f / 6.0f);

        path.cubicTo(cp1, cp2, p2);
    }

    // Optional: line to end if not covered
    path.lineTo(points.back());

    return path;
}

void SpectrumAnalyzerView::render(void *ctx) {
    auto c = (SkCanvas *) ctx;
    auto& _path = path[activeTrack_];
    if (auto buf =input_[activeTrack_].exchange(nullptr, std::memory_order_acq_rel)) {
        auto w = width.load(), h = height.load();
        for (int i = 0; i < tsl::displayChannelsSpectrum; i++) {
            auto norm = tsl::toNorm(buf[i], minDb_, maxDb_);
            smoothed[activeTrack_][i] = std::max(smoothed[activeTrack_][i] * 0.75, norm);
        }

        _path = buildSpectrumPathNormalized(smoothed[activeTrack_], w, h,
                                                        tsl::displayChannelsSpectrum);

//SkPath filledPath = genPoly2(*buf, -60., 15., w, h);
        _path.lineTo(w, h); // bottom right
        _path.lineTo(0, h);           // bottom left
        _path.close();
        _STATE->pool.release(buf);
    }
    flush(c);
    c->save();
    c->translate(startx, starty);

    SkPoint gradientPts[2] = {{0, 0},
                              {0, (float) height}};
    SkColor colors[2] = {skcol::orange, skcol::blue_transparent};

    auto shader = SkGradientShader::MakeLinear(
            gradientPts, colors, nullptr, 2, SkTileMode::kClamp
    );

    SkPaint fillPaint;
    fillPaint.setStyle(SkPaint::kFill_Style);
    fillPaint.setShader(shader);
    fillPaint.setAntiAlias(true);
    c->drawPath(_path, fillPaint);
    c->restore();
    drawRect(c);
};

template<typename T>
static std::vector<SkPoint>
drawSpectrumCurve(const std::vector<T> &dbValues, int canvasWidth, int canvasHeight,
                  float minDb, float maxDb) {
    const int numBins = dbValues.size();  // Should be 256

    // Step 1: Map bin index to X, and dB to Y
    std::vector<SkPoint> points(numBins);
    for (int i = 0; i < numBins; ++i) {
        float x = (i / float(numBins - 1)) * canvasWidth;
        float db = std::clamp(dbValues[i], minDb, maxDb);
        float normDb = (db - minDb) / (maxDb - minDb);  // 0 to 1
        float y = (1.0f - normDb) * canvasHeight;       // 0 dB at top, -60 dB at bottom
        points[i] = SkPoint::Make(x, y);
    }
    return points;
}



template<typename T>
static SkPath
buildCurvedPath(T values, T positions, float width, float height, float min,
                float max, int N) {
    if (N < 2) return SkPath();

    SkPath path;

    std::vector<SkPoint> points(N);
    for (int i = 0; i < N; ++i) {
        float x = positions[i] * width;
        float y = (1.0f - tsl::toNorm(values[i], min, max)) * height;
        points[i] = SkPoint::Make(x, y);
    }

    // Start path
    path.moveTo(points[0]);

    // Cubic Bézier using adjacent points as control (Catmull-Rom-like)
    for (int i = 1; i < N - 2; ++i) {
        const SkPoint &p0 = points[i - 1];
        const SkPoint &p1 = points[i];
        const SkPoint &p2 = points[i + 1];
        const SkPoint &p3 = points[i + 2];

        // Calculate Bézier control points
        SkPoint cp1 = p1 + (p2 - p0) * (1.0f / 6.0f);
        SkPoint cp2 = p2 - (p3 - p1) * (1.0f / 6.0f);

        path.cubicTo(cp1, cp2, p2);
    }

    // Optional: line to end if not covered
    path.lineTo(points.back());

    return path;
}

template<typename T>
SkPath genPoly2(std::vector<T> values,
                T minDb, T maxDb, // Pass minDb and maxDb as arguments for clarity
                int width, int height) {

    // 1. Normalize values and pad
    // Normalize first, then pad with normalized values
    for (auto &val: values) {
        val = (val - minDb) / (maxDb - minDb);
        // Clamp to 0-1 range after normalization in case values are outside min/maxDb
        val = std::clamp(val, (T) 0.0, (T) 1.0);
    }

    // Add baseline points at the beginning and end.
    // The '0.' corresponds to minDb after normalization.
    values.insert(values.begin(), (T) 0.0);
    values.push_back((T) 0.0);

    // Now, values.size() includes the two added padding points.
    // nsegs should be values.size() - 1, as there's one segment between N points.
    int nsegs = values.size() -
                1; // Number of segments between the points (including padded ones)

    SkPath path;

    // Start at the bottom-left corner (first padded point's x-coord is 0, y is height)
    path.moveTo(0, height -
                   height * values.at(0)); // Start at the first padded point's Y value

    // Calculate segment width for drawing. Each point in 'values' corresponds to an equal X-interval.
    // Each segment has a width equal to (total_width / number_of_segments).
    // Or, if values.size() is displayChannels + 2, then each of displayChannels original points
    // corresponds to `width / (displayChannels)` pixels, plus the padding.
    // Let's assume each of the `nsegs` covers `width / nsegs` pixels.
    T segmentPixelWidth = (T) width / (T) nsegs; // Width of each segment in pixels

    // Loop through each segment (from point i to point i+1)
    for (int seg_idx = 0; seg_idx < nsegs; ++seg_idx) {
        T current_x_start = seg_idx * segmentPixelWidth;
        T next_x_end = (seg_idx + 1) * segmentPixelWidth;

        T val_start = values.at(
                seg_idx);     // Y value at the start of this segment (normalized 0-1)
        T val_end = values.at(
                seg_idx + 1);   // Y value at the end of this segment (normalized 0-1)

        // Number of steps (pixels) to draw for this segment
        int num_steps = static_cast<int>(std::round(segmentPixelWidth));
        if (num_steps < 1) num_steps = 1; // Ensure at least one step per segment

        // Option 1: Simple Linear Interpolation (if you don't need cubic smoothing between points)
        // This would just connect the dots of your 'values' array
        // path.lineTo(next_x_end, height - height * val_end);

        // Option 2: Cubic-like Interpolation (closer to your original attempt's intent)
        // This is a common way to blend between two points (a, b) over a normalized range (t from 0 to 1)
        // The coefficients (3.0 - y) * y * y suggest a cubic easing function or a specific spline.
        // Let's adapt your interpolation logic to be per-segment and continuous.
        // This function y_interp(t, val_start, val_end) could be a cubic Bezier or Hermite curve.

        // For simplicity, let's use a basic cubic interpolation that smooths between the 'val_start' and 'val_end'.
        // This is a basic cubic bezier, implicitly assuming control points.
        // Your (3.0 - y) * y * y * diffd2 + extreme suggests an S-curve, often for easing.
        // Let's re-interpret the logic: "extreme" is the starting point, "inflect" is a target for the mid-point.
        // It looks like you're trying to use two halves of a cubic ease function.
        // A more standard approach for connecting two points (P0, P1) with two control points (C0, C1) is:
        // B(t) = (1-t)^3*P0 + 3(1-t)^2*t*C0 + 3(1-t)*t^2*C1 + t^3*P1

        // Let's try to make your original interpolation logic continuous and correct.
        // It seems to be trying to draw a curve from 'a' to 'b' by drawing a curve from 'a' to 'inflect',
        // and then another curve from 'inflect' to 'b'. But the second curve's 'pntno' is reversed.

        // A better cubic interpretation of your intent (from a to b) might be:
        // Use a 3rd order polynomial: y = A*t^3 + B*t^2 + C*t + D where t goes from 0 to 1
        // D = val_start (at t=0)
        // A + B + C + D = val_end (at t=1)
        // And some conditions on derivatives for smoothness, or inferred control points.

        // Given your original code's structure with `extreme` and `inflect`:
        // It implies a curve from 'extreme' to 'inflect' over the first half of the segment,
        // and then from 'inflect' to 'extreme' over the second half, with `extreme` being `b` for the second half.
        // Let's re-implement this, ensuring `lineTo` continuity.

        // First half of the segment
        T current_pixel_x = current_x_start;
        T diff_first_half = ((val_start + val_end) * 0.5 - val_start) *
                            0.5; // (inflect - extreme) * 0.5
        for (int pntno = 0;
             pntno < num_steps / 2; ++pntno) { // Iterate for first half of pixels
            T t = (T) pntno / (num_steps / 2.0); // t from 0 to <1
            T interpolated_val_y = (3.0 - t) * t * t * diff_first_half + val_start;
            path.lineTo(current_pixel_x, height - height * interpolated_val_y);
            current_pixel_x += 1.0; // Move to next pixel
        }

        // Second half of the segment
        T diff_second_half = ((val_start + val_end) * 0.5 - val_end) *
                             0.5; // (inflect - new extreme (b)) * 0.5
        for (int pntno = num_steps / 2;
             pntno < num_steps; ++pntno) { // Iterate for second half of pixels
            T t = (T) (pntno - num_steps / 2) /
                  (num_steps / 2.0); // t from 0 to <1 for the second half
            T interpolated_val_y = (3.0 - t) * t * t * diff_second_half +
                                   val_end; // Interpolate towards 'end' from 'inflect'
            path.lineTo(current_pixel_x, height - height * interpolated_val_y);
            current_pixel_x += 1.0; // Move to next pixel
        }

        // Ensure the path ends exactly at the next point's coordinates
        path.lineTo(next_x_end, height - height * val_end);

    }

    // Final segment to reach the far right edge, ensuring it touches the last padded '0' value.
    // The previous loop should have already drawn up to the last point's X.
    // This while loop is probably for ensuring the line goes all the way to 'width'.
    // Given the segmentPixelWidth and `next_x_end`, `fp` (now `current_pixel_x` after the loops) should be near `width`.
    // It's better to explicitly add the last point:
    // path.lineTo(width, height - height * values.back()); // Which is the last padded 0.0

    // Adjust the ending of the path to explicitly go to the last padded point and then straight down to the base line
    // The last point drawn by the loop `seg_idx == nsegs - 1` is `next_x_end` for that segment, which should be `width`.
    // So if the path already goes to `width`, you might just need to drop to `height`.

    // The provided original code implicitly draws the final segment to `width` using `while (fp < width)`.
    // Let's refine the loop to make sure it covers the full width cleanly.
    // The loop `for (int seg_idx = 0; seg_idx < nsegs; ++seg_idx)` already means we have `nsegs+1` points being connected.
    // The last point is `values.back()`, which is at `width`.

    // After the main loop, if there's any remaining width, ensure path extends.
    // This handles potential rounding errors or if the last point doesn't exactly align with 'width'
    // It should draw to the final padded 0 at 'width'.
    // The final value `values.back()` is at `width` pixels, mapped to `height - height * values.back()`.
    path.lineTo(width, height - height * values.back());


    return path;
}

template<typename T>
SkPath genPoly(std::vector<T> values,
               int width, int height, T minDb, T maxDb) {
    for (auto &val: values)val = (val - minDb) / (maxDb - minDb);

    values.insert(values.begin(), 0.); // Insert '5' at the very beginning
    values.push_back(0.); // Insert '5' at the very beginning
    int fp = 0;
    T vala = 0;
    SkPath path;
    path.moveTo(fp, height);
    int nsegs = values.size() - 1;
    for (int seg = 0; seg < nsegs; seg++) {
        int npts;
        T length = width / (T) nsegs * .5;
        if ((npts = (int) length) < 0) {
            return path;
        }
        T a = values.at(seg);
        T b = values.at(seg + 1);
        T extreme = a;
        T inflect = (a + b) * .5;
        int pntno = 0;
        T diffd2 = (inflect - extreme) * (0.5);
        for (; npts > 0 && fp < width; pntno++, npts--) {
            T y = (T) pntno / length;
            vala = (3.0 - y) * y * y * diffd2 + extreme;
            path.lineTo(fp++, height - height * vala);
        }
        extreme = b;
        npts = (int) length;
        pntno = npts;
        diffd2 = (inflect - extreme) * (0.5);
        for (; npts > 0 && fp < width; pntno--, npts--) {
            T y = (T) pntno / length;
            vala = ((3.0) - y) * y * y * diffd2 + extreme;
            path.lineTo(fp++, height - height * vala);
        }
    }
    while (fp < width)                 /* if 2**n pnts, add guardpt */
        path.lineTo(fp++, height - height * vala);
    return path;
}


