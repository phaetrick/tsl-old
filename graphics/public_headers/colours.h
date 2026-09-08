#pragma once
#ifndef COLOURS_H
#define COLOURS_H

#include <include/core/SkColor.h>

#define Colour(X) (X)->b, (X)->g, (X)->r, (X)->a
#define Colour_RGB(X) (X)->b, (X)->g, (X)->r
#define SK_Colour(R, G, B, A) SkColorSetARGB((uint8_t) A, (uint8_t) R, (uint8_t) G, (uint8_t)  B)

#define IM_Colour(x) IM_COL32((x >> 16) & 0xFF, (x >> 8) & 0xFF, x & 0xFF, (x >> 24) & 0xFF)


#define COL2VAL(col) (col).r, (col).g, (col).b, (col).a
#define colour_t tsl::colour
#define skcol tsl::sk_colours
//#define fromIntRGBA(R, G, B, A) ((float) A / 255.f, (float) R / 255.f, (float) G / 255.f, (float)  B / 255.f)
//#define Col32toVec4(x) glm::vec4((((x) >> 16) & 0xFF) * 1.f/255.f, (((x) >> 8) & 0xFF) * 1.f/255.f, ((x) & 0xFF)  * 1.f/255.f, (((x) >> 24) & 0xFF) * 1.f/255.f)
//#define Col32toVec4WithAlpha(x,alpha) glm::vec4((((x) >> 16) & 0xFF) * 1.f/255.f, (((x) >> 8) & 0xFF) * 1.f/255.f, ((x) & 0xFF)  * 1.f/255.f, (alpha))
//#define SkColorWithAlpha(x, alpha) SkColorSetARGB(((x) >> 16) & 0xFF, ((x) >> 8) & 0xFF, (x) & 0xFF, (uint8_t) (alpha * 255.f))
namespace tsl {
	struct colour {
		constexpr colour() : r(0), g(0), b(0), a(0) {};

		constexpr colour(const colour& o) : r(o.r), g(o.g), b(o.b), a(o.a) {};

		constexpr colour(const colour& o, const float a_) : r(o.r), g(o.g), b(o.b), a(a_) {};

		constexpr colour(const float r_, const float g_, const float b_, const float a_) : r(r_),
			g(g_),
			b(b_),
			a(a_) {
		};

		constexpr colour(const int r_, const int g_, const int b_, const int a_) : r(r_ / 255.f),
			g(g_ / 255.f),
			b(b_ / 255.f),
			a(a_ / 255.f) {
		};

		constexpr colour(const uint32_t x)
			: r(((x >> 16) & 0xFF) / 255.f),
			g(((x >> 8) & 0xFF) / 255.f),
			b((x & 0xFF) / 255.f),
			a(((x >> 24) & 0xFF) / 255.f) {
		}

		float r, g, b, a;
	};


	namespace sk_colours {

		static constexpr
			uint32_t white{ SK_Colour(255, 255, 255, 255) },
			dg{ SK_Colour(30, 30, 30, 255) },
			gr{ SK_Colour(37, 37, 37, 255) },
			lg{ SK_Colour(51, 51, 51, 255) },
			lblue{ SK_Colour(0, 122, 204, 255) },
			greytabs{ SK_Colour(45, 45, 45, 255) },
			font{ SK_Colour(206, 206, 160, 255) },
			fontpanel{ SK_Colour(184, 218, 241, 255) },
			sel_fg{ SK_Colour(60, 60, 60, 255) },
			sel_text{ SK_Colour(184, 218, 241, 255) },
			fg{ SK_Colour(255, 255, 255, 255) },
			bg{ SK_Colour(0,0,0,255) },
			wbg{ SK_Colour(0, 0, 0, 255) },
			border{ SK_Colour(255, 255, 255, 255) },
			waveform{ fg },
			text{ fg },
			blue{ SK_Colour(33, 150, 243, 255) },
			pressed{ SK_Colour(255, 255, 255, 255) },
			frame{ SK_Colour(100, 100, 100, 255) },
			lightest_grey{ SK_Colour(200, 200, 200, 255) },
			lighter_grey{ SK_Colour(175, 175, 175, 204) },
			light_grey{ SK_Colour(150, 150, 150, 255) },
			grey{ SK_Colour(125, 125, 125, 125) },
			dark_grey{ SK_Colour(100, 100, 100, 255) },
			darker_grey{ SK_Colour(75, 75, 75, 255) },
			darkest_grey{ SK_Colour(50, 50, 50, 255) },
			light_blue{ SK_Colour(255, 0, 179, 255) },
			yellow{ SK_Colour(255, 235, 59, 204) },
			red{ SK_Colour(255, 55, 0, 255) },
			orange{ SK_Colour(255, 152, 0, 255) },
			custom_blue{ SK_Colour(0, 0, 153, 255) },
			custom_yellow{},
			custom_green{ SK_Colour(76, 175, 80, 255) },
			custom_red{ SK_Colour(244, 67, 54, 255) },
#ifdef __ANDROID__
			blue_transparent{ SK_Colour(33, 150, 243, 100) },
#else
			blue_transparent{ SK_Colour(33, 150, 243, 100) },
#endif
			transparent{ SK_Colour(0, 0, 0, 0) },
			blue_violet_transparent{ SK_Colour(138, 43, 226, 100) },
			blue_violet{ SK_Colour(138, 43, 226, 255) },
			// The stretch of a knob's ring that modulation can move the value across.
			// Orange against the blue value arc: the two are far enough apart in hue
			// that the split reads at a glance, even on a thin ring at phone scale.
			modarc{ orange },
			window_darktransp{ SkColorSetARGB(0xEE, 0x18, 0x18, 0x18) },
			midilearning{ orange },
			active{ custom_green },
			bghot{ blue_transparent },
			fghot{ fg };
		static constexpr uint32_t bounce_colours[]{ blue, orange, custom_green };

	};


	namespace colours {
		static constexpr colour_t fg{ 255, 255, 255, 255 },
			wbg{ 0, 0, 0, 255 },
			border{ 255, 255, 255, 255 },
			bg{ 0, 0, 0, 255 },
			waveform{ fg },
			text{ fg },
			blue{ 33, 150, 243, 255 },
			pressed{ 255, 255, 255, 255 },
			frame{ 100, 100, 100, 255 },
			lightest_grey{ 200, 200, 200, 255 },
			lighter_grey{ 175, 175, 175, 204 },
			light_grey{ fg },
			grey{ 125, 125, 125, 125 },
			dark_grey{ 100, 100, 100, 255 },
			darker_grey{ 75, 75, 75, 255 },
			darkest_grey{ 50, 50, 50, 255 },
			light_blue{ 255, 0, 179, 255 },
			yellow{ 255, 235, 59, 204 },
			red{ 255, 55, 0, 255 },
			orange{ 255, 152, 0, 255 },
			custom_blue{ 0, 0, 153, 255 },
			custom_yellow{},
			custom_green{ 76, 175, 80, 255 },
			custom_red{ 244, 67, 54, 255 },
#ifdef __ANDROID__
                blue_transparent{ 33, 150, 243, 100 },
#else
        blue_transparent{ 33, 150, 243, 100 },
#endif

			transparent{ 0, 0, 0, 0 },
			blue_violet_transparent{ 138, 43, 226, 100 },
			midilearning{ orange },
			bghot{ fg },
			fghot{ bg },
			marker{ 127, 127, 127, 127 };
		static constexpr colour_t bounce_colours[]{ blue, orange, custom_green };
	};
}
#endif