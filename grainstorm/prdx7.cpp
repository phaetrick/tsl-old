//
// Created by pr on 06.10.20.
//

#include "prdx7.h"
//
// Created by pr on 30.06.20.
//

#include <cstdlib>
#include <pthread.h>
#include <cstring>
#include "tools.h"
#include "prdx7.h"
#include "logger.h"


/* dx7_voice_data.c */

static dx7_patch_t dx7_voice_init_voice = {
        {0x62, 0x63, 0x63, 0x5A, 0x63, 0x63, 0x63, 0x00,
                0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x02,
                0x00, 0x62, 0x63, 0x63, 0x5A, 0x63, 0x63, 0x63,
                0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00, 0x00,
                0x02, 0x00, 0x62, 0x63, 0x63, 0x5A, 0x63, 0x63,
                0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38, 0x00,
                0x00, 0x02, 0x00, 0x62, 0x63, 0x63, 0x5A, 0x63,
                0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x38,
                0x00, 0x00, 0x02, 0x00, 0x62, 0x63, 0x63, 0x5A,
                0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00,
                0x38, 0x00, 0x00, 0x02, 0x00, 0x62, 0x63, 0x63,
                0x5A, 0x63, 0x63, 0x63, 0x00, 0x00, 0x00, 0x00,
                0x00, 0x38, 0x00, 0x63, 0x02, 0x00, 0x63, 0x63,
                0x63, 0x63, 0x32, 0x32, 0x32, 0x32, 0x00, 0x08,
                0x23, 0x00, 0x00, 0x00, 0x31, 0x18, 0x20, 0x20,
                0x20, 0x7F, 0x2D, 0x2D, 0x7E, 0x20, 0x20, 0x20}
};

static uint8_t dx7_init_performance[DX7_PERFORMANCE_SIZE] = {
        0, 0, 0, 2, 0, 0, 0, 0,
        0, 15, 1, 0, 4, 15, 2, 15,
        2, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
};

static float dx7_voice_eg_rate_rise_duration[128] = {  /* generated from my f04new */

        39.638000, 37.013000, 34.388000, 31.763000, 27.210500,
        22.658000, 20.408000, 18.158000, 15.908000, 14.557000,
        13.206000, 12.108333, 11.010667, 9.913000, 8.921000,
        7.929000, 7.171333, 6.413667, 5.656000, 5.307000,
        4.958000, 4.405667, 3.853333, 3.301000, 2.889000,
        2.477000, 2.313000, 2.149000, 1.985000, 1.700500,
        1.416000, 1.274333, 1.132667, 0.991000, 0.909000,
        0.827000, 0.758000, 0.689000, 0.620000, 0.558000,
        0.496000, 0.448667, 0.401333, 0.354000, 0.332000,
        0.310000, 0.275667, 0.241333, 0.207000, 0.180950,
        0.154900, 0.144567, 0.134233, 0.123900, 0.106200,
        0.088500, 0.079667, 0.070833, 0.062000, 0.056800,
        0.051600, 0.047300, 0.043000, 0.038700, 0.034800,
        0.030900, 0.028000, 0.025100, 0.022200, 0.020815,
        0.019430, 0.017237, 0.015043, 0.012850, 0.011230,
        0.009610, 0.009077, 0.008543, 0.008010, 0.006960,
        0.005910, 0.005357, 0.004803, 0.004250, 0.003960,
        0.003670, 0.003310, 0.002950, 0.002590, 0.002420,
        0.002250, 0.002000, 0.001749, 0.001499, 0.001443,
        0.001387, 0.001242, 0.001096, 0.000951, 0.000815,
        0.000815, 0.000815, 0.000815, 0.000815, 0.000815,
        0.000815, 0.000815, 0.000815, 0.000815, 0.000815,
        0.000815, 0.000815, 0.000815, 0.000815, 0.000815,
        0.000815, 0.000815, 0.000815, 0.000815, 0.000815,
        0.000815, 0.000815, 0.000815, 0.000815, 0.000815,
        0.000815, 0.000815, 0.000815

};

static float dx7_voice_eg_rate_decay_duration[128] = {  /* generated from my f06new */

        317.487000, 285.764500, 254.042000, 229.857000, 205.672000,
        181.487000, 170.154000, 158.821000, 141.150667, 123.480333,
        105.810000, 98.382500, 90.955000, 81.804667, 72.654333,
        63.504000, 58.217000, 52.930000, 48.512333, 44.094667,
        39.677000, 33.089000, 26.501000, 24.283333, 22.065667,
        19.848000, 17.881500, 15.915000, 14.389667, 12.864333,
        11.339000, 10.641000, 9.943000, 8.833333, 7.723667,
        6.614000, 6.149500, 5.685000, 5.112667, 4.540333,
        3.968000, 3.639000, 3.310000, 3.033667, 2.757333,
        2.481000, 2.069500, 1.658000, 1.518667, 1.379333,
        1.240000, 1.116500, 0.993000, 0.898333, 0.803667,
        0.709000, 0.665500, 0.622000, 0.552667, 0.483333,
        0.414000, 0.384500, 0.355000, 0.319333, 0.283667,
        0.248000, 0.228000, 0.208000, 0.190600, 0.173200,
        0.155800, 0.129900, 0.104000, 0.095400, 0.086800,
        0.078200, 0.070350, 0.062500, 0.056600, 0.050700,
        0.044800, 0.042000, 0.039200, 0.034833, 0.030467,
        0.026100, 0.024250, 0.022400, 0.020147, 0.017893,
        0.015640, 0.014305, 0.012970, 0.011973, 0.010977,
        0.009980, 0.008310, 0.006640, 0.006190, 0.005740,
        0.005740, 0.005740, 0.005740, 0.005740, 0.005740,
        0.005740, 0.005740, 0.005740, 0.005740, 0.005740,
        0.005740, 0.005740, 0.005740, 0.005740, 0.005740,
        0.005740, 0.005740, 0.005740, 0.005740, 0.005740,
        0.005740, 0.005740, 0.005740, 0.005740, 0.005740,
        0.005740, 0.005740, 0.005740

};

static float dx7_voice_eg_rate_decay_percent[128] = {  /* generated from P/H/Op f07 */

        0.000010, 0.025009, 0.050008, 0.075007, 0.100006,
        0.125005, 0.150004, 0.175003, 0.200002, 0.225001,
        0.250000, 0.260000, 0.270000, 0.280000, 0.290000,
        0.300000, 0.310000, 0.320000, 0.330000, 0.340000,
        0.350000, 0.358000, 0.366000, 0.374000, 0.382000,
        0.390000, 0.398000, 0.406000, 0.414000, 0.422000,
        0.430000, 0.439000, 0.448000, 0.457000, 0.466000,
        0.475000, 0.484000, 0.493000, 0.502000, 0.511000,
        0.520000, 0.527000, 0.534000, 0.541000, 0.548000,
        0.555000, 0.562000, 0.569000, 0.576000, 0.583000,
        0.590000, 0.601000, 0.612000, 0.623000, 0.634000,
        0.645000, 0.656000, 0.667000, 0.678000, 0.689000,
        0.700000, 0.707000, 0.714000, 0.721000, 0.728000,
        0.735000, 0.742000, 0.749000, 0.756000, 0.763000,
        0.770000, 0.777000, 0.784000, 0.791000, 0.798000,
        0.805000, 0.812000, 0.819000, 0.826000, 0.833000,
        0.840000, 0.848000, 0.856000, 0.864000, 0.872000,
        0.880000, 0.888000, 0.896000, 0.904000, 0.912000,
        0.920000, 0.928889, 0.937778, 0.946667, 0.955556,
        0.964444, 0.973333, 0.982222, 0.991111, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000

};

static float dx7_voice_eg_rate_rise_percent[128] = {  /* checked, matches P/H/Op f05 */

        0.000010, 0.000010, 0.000010, 0.000010, 0.000010,
        0.000010, 0.000010, 0.000010, 0.000010, 0.000010,
        0.000010, 0.000010, 0.000010, 0.000010, 0.000010,
        0.000010, 0.000010, 0.000010, 0.000010, 0.000010,
        0.000010, 0.000010, 0.000010, 0.000010, 0.000010,
        0.000010, 0.000010, 0.000010, 0.000010, 0.000010,
        0.000010, 0.000010, 0.005007, 0.010005, 0.015003,
        0.020000, 0.028000, 0.036000, 0.044000, 0.052000,
        0.060000, 0.068000, 0.076000, 0.084000, 0.092000,
        0.100000, 0.108000, 0.116000, 0.124000, 0.132000,
        0.140000, 0.150000, 0.160000, 0.170000, 0.180000,
        0.190000, 0.200000, 0.210000, 0.220000, 0.230000,
        0.240000, 0.251000, 0.262000, 0.273000, 0.284000,
        0.295000, 0.306000, 0.317000, 0.328000, 0.339000,
        0.350000, 0.365000, 0.380000, 0.395000, 0.410000,
        0.425000, 0.440000, 0.455000, 0.470000, 0.485000,
        0.500000, 0.520000, 0.540000, 0.560000, 0.580000,
        0.600000, 0.620000, 0.640000, 0.660000, 0.680000,
        0.700000, 0.732000, 0.764000, 0.796000, 0.828000,
        0.860000, 0.895000, 0.930000, 0.965000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000, 1.000000, 1.000000,
        1.000000, 1.000000, 1.000000

};

/* This table converts pitch envelope level parameters into the
 * actual pitch shift in semitones.  For levels [17,85], this is
 * just ((level - 50) / 32 * 12), but at the outer edges the shift
 * is exagerated to 0 = -48 and 99 => 47.624.  This is based on
 * measurements I took from my TX7. */
static double dx7_voice_pitch_level_to_shift[128] = {

        -48.000000, -43.497081, -38.995993, -35.626132, -31.873615,
        -28.495880, -25.500672, -22.872620, -20.998167, -19.496961,
        -18.373238, -17.251065, -16.122139, -15.375956, -14.624487,
        -13.876516, -13.126351, -12.375000, -12.000000, -11.625000,
        -11.250000, -10.875000, -10.500000, -10.125000, -9.750000,
        -9.375000, -9.000000, -8.625000, -8.250000, -7.875000,
        -7.500000, -7.125000, -6.750000, -6.375000, -6.000000,
        -5.625000, -5.250000, -4.875000, -4.500000, -4.125000,
        -3.750000, -3.375000, -3.000000, -2.625000, -2.250000,
        -1.875000, -1.500000, -1.125000, -0.750000, -0.375000, 0.000000,
        0.375000, 0.750000, 1.125000, 1.500000, 1.875000, 2.250000,
        2.625000, 3.000000, 3.375000, 3.750000, 4.125000, 4.500000,
        4.875000, 5.250000, 5.625000, 6.000000, 6.375000, 6.750000,
        7.125000, 7.500000, 7.875000, 8.250000, 8.625000, 9.000000,
        9.375000, 9.750000, 10.125000, 10.500000, 10.875000, 11.250000,
        11.625000, 12.000000, 12.375000, 12.750000, 13.125000,
        14.251187, 15.001922, 16.126327, 17.250917, 18.375718,
        19.877643, 21.753528, 24.373913, 27.378021, 30.748956,
        34.499234, 38.627888, 43.122335, 47.624065, 48.0, 48.0, 48.0,
        48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0,
        48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0, 48.0,
        48.0, 48.0, 48.0, 48.0, 48.0

};

static char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/* This table lists which operators of an algorithm are carriers.  Bit 0 (LSB)
 * is set if operator 1 is a carrier, and so on through bit 5 for operator 6.
 */
static uint8_t dx7_voice_carriers[32] = {
        0x05, /* algorithm 1, operators 1 and 3 */
        0x05,
        0x09, /* algorithm 3, operators 1 and 4 */
        0x09,
        0x15, /* algorithm 5, operators 1, 3, and 5 */
        0x15,
        0x05,
        0x05,
        0x05,
        0x09,
        0x09,
        0x05,
        0x05,
        0x05,
        0x05,
        0x01, /* algorithm 16, operator 1 */
        0x01,
        0x01,
        0x19, /* algorithm 19, operators 1, 4, and 5 */
        0x0b, /* algorithm 20, operators 1, 2, and 4 */
        0x1b, /* algorithm 21, operators 1, 2, 4, and 5 */
        0x1d, /* algorithm 22, operators 1, 3, 4, and 5 */
        0x1b,
        0x1f, /* algorithm 24, operators 1 through 5 */
        0x1f,
        0x0b,
        0x0b,
        0x25, /* algorithm 28, operators 1, 3, and 6 */
        0x17, /* algorithm 29, operators 1, 2, 3, and 5 */
        0x27, /* algorithm 30, operators 1, 2, 3, and 6 */
        0x1f,
        0x2f, /* algorithm 32, all operators */
};

static float dx7_voice_carrier_count[32] = {
        2.0f, 2.0f, 2.0f, 2.0f, 3.0f, 3.0f, 2.0f, 2.0f,
        2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 1.0f,
        1.0f, 1.0f, 3.0f, 3.0f, 4.0f, 4.0f, 4.0f, 5.0f,
        5.0f, 3.0f, 3.0f, 3.0f, 4.0f, 4.0f, 5.0f, 6.0f
};

/* This table converts an output level of 0 to 99 into a phase
 * modulation index of 0 to ~2.089 periods.  It actually extends
 * below 0 and beyond 99, since amplitude modulation can produce
 * 'negative' output levels, and velocities above 100 can produce
 * output levels above 99, plus it includes a 257th 'guard' point.
 * Table index 128 corresponds to output level 0, and index 227 to OL 99.
 * I believe this is based on information from the Chowning/Bristow
 * book (see the CREDITS file), filtered down to me through the work of
 * Pinkston, Harrington, and Abdullah as I found it on the Internet.  The
 * code used to calculate it looks something like this:
 *
 *    // DX7 output level to TL translation table
 *    int32_t tl_table[128] = {
 *        127, 122, 118, 114, 110, 107, 104, 102, 100, 98, 96, 94, 92, 90,
 *        88, 86, 85, 84, 82, 81, 79, 78, 77, 76, 75, 74, 73, 72, 71,
 *        70, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55,
 *        54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39,
 *        38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23,
 *        22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6,
 *        5, 4, 3, 2, 1, 0, -1, -2, -3, -4, -5, -6, -7, -8, -9, -10, -11,
 *        -12, -13, -14, -15, -16, -17, -18, -19, -20, -21, -22, -23, -24,
 *        -25, -26, -27, -28
 *    };
 *
 *    int32_t ol;
 *    double mi;
 *
 *    for (ol = 0; ol < 128; ol++) {
 *        if (ol < 5) {    // smoothly ramp from 0.0 at 0 to the proper value at 5
 *            mi = pow(2.0, ( (33.0/16.0) - ((double)tl_table[5]/8.0) - 1.0));
 *            mi = mi * ((double)ol / 5.0);
 *        } else {
 *            mi = pow(2.0, ( (33.0/16.0) - ((double)tl_table[ol]/8.0) - 1.0));
 *        }
 *    #ifndef PRDX7_USE_FLOATING_POINT
 *        printf(" %6d,", DOUBLE_TO_FP(mi));
 *    #else
 *        printf(" %g,", mi);
 *    #endif
 *    }
 */
static dx7_sample_t dx7_voice_eg_ol_to_mod_index_table[257] = {
#ifndef PRDX7_USE_FLOATING_POINT
/* phase modulation index, expressed in s7.24 fixed point */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 659, 1319, 1978, 2638, 3298, 4277, 5086, 6049, 7193, 8554,
        10173, 12098, 14387, 17109, 20346, 22188, 24196, 28774, 31378,
        37315, 40693, 44376, 48392, 52772, 57548, 62757, 68437, 74631,
        81386, 88752, 96785, 105545, 115097, 125514, 136875, 149263,
        162772, 177504, 193570, 211090, 230195, 251029, 273750, 298526,
        325545, 355009, 387141, 422180, 460390, 502059, 547500, 597053,
        651091, 710019, 774282, 844360, 920781, 1004119, 1095000,
        1194106, 1302182, 1420039, 1548564, 1688721, 1841563, 2008239,
        2190000, 2388212, 2604364, 2840079, 3097128, 3377443, 3683127,
        4016479, 4380001, 4776425, 5208729, 5680159, 6194257, 6754886,
        7366255, 8032958, 8760003, 9552851, 10417458, 11360318,
        12388515, 13509772, 14732510, 16065917, 17520006, 19105702,
        20834916, 22720637, 24777031, 27019544, 29465021, 32131834,
        35040013, 38211405, 41669833, 45441275, 49554062, 54039088,
        58930043, 64263668, 70080027, 76422811, 83339667, 90882551,
        99108124, 108078176, 117860087, 128527336, 140160054, 152845623,
        166679334, 181765102, 198216249, 216156353, 235720174,
        257054673, 280320108, 305691246, 333358668, 363530205,
        396432499, 396432499
#else /* PRDX7_USE_FLOATING_POINT */
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 3.93186e-05f, 7.86372e-05f, 0.000117956f,
        0.000157274f, 0.000196593f, 0.00025495f, 0.000303188f,
        0.000360553f, 0.000428773f, 0.000509899f, 0.000606376f,
        0.000721107f, 0.000857545f, 0.0010198f, 0.00121275f,
        0.00132252f, 0.00144221f, 0.00171509f, 0.00187032f,
        0.0022242f, 0.0024255f, 0.00264503f, 0.00288443f,
        0.00314549f, 0.00343018f, 0.00374064f, 0.00407919f,
        0.00444839f, 0.00485101f, 0.00529006f, 0.00576885f,
        0.00629098f, 0.00686036f, 0.00748128f, 0.00815839f,
        0.00889679f, 0.00970201f, 0.0105801f, 0.0115377f,
        0.012582f, 0.0137207f, 0.0149626f, 0.0163168f,
        0.0177936f, 0.019404f, 0.0211602f, 0.0230754f,
        0.0251639f, 0.0274414f, 0.0299251f, 0.0326336f,
        0.0355871f, 0.0388081f, 0.0423205f, 0.0461508f,
        0.0503278f, 0.0548829f, 0.0598502f, 0.0652671f,
        0.0711743f, 0.0776161f, 0.084641f, 0.0923016f,
        0.100656f, 0.109766f, 0.1197f, 0.130534f,
        0.142349f, 0.155232f, 0.169282f, 0.184603f,
        0.201311f, 0.219532f, 0.239401f, 0.261068f,
        0.284697f, 0.310464f, 0.338564f, 0.369207f,
        0.402623f, 0.439063f, 0.478802f, 0.522137f,
        0.569394f, 0.620929f, 0.677128f, 0.738413f,
        0.805245f, 0.878126f, 0.957603f, 1.04427f,
        1.13879f, 1.24186f, 1.35426f, 1.47683f,
        1.61049f, 1.75625f, 1.91521f, 2.08855f,
        2.27758f, 2.48372f, 2.70851f, 2.95365f,
        3.22098f, 3.5125f, 3.83041f, 4.1771f,
        4.55515f, 4.96743f, 5.41702f, 5.9073f,
        6.44196f, 7.02501f, 7.66083f, 8.35419f,
        9.11031f, 9.93486f, 10.834f, 11.8146f,
        12.8839f, 14.05f, 15.3217f, 16.7084f,
        18.2206f, 19.8697f, 21.6681f, 23.6292f,
        23.6292f
#endif /* PRDX7_USE_FLOATING_POINT */
};

static dx7_sample_t *dx7_voice_eg_ol_to_mod_index = &dx7_voice_eg_ol_to_mod_index_table[128];


/* This table lists the output level adjustment needed for a certain
 * velocity, expressed in output level units per unit of velocity
 * sensitivity. It is based on measurements I took from my TX7. */
static float dx7_voice_velocity_ol_adjustment[128] = {

        -99.0, -10.295511, -9.709229, -9.372207,
        -9.121093, -8.629703, -8.441805, -8.205647,
        -7.810857, -7.653259, -7.299901, -7.242308,
        -6.934396, -6.727051, -6.594723, -6.427755,
        -6.275133, -6.015212, -5.843023, -5.828787,
        -5.725659, -5.443202, -5.421110, -5.222133,
        -5.160615, -5.038265, -4.948225, -4.812105,
        -4.632120, -4.511531, -4.488645, -4.370043,
        -4.370610, -4.058591, -4.066902, -3.952988,
        -3.909686, -3.810096, -3.691883, -3.621306,
        -3.527286, -3.437519, -3.373512, -3.339195,
        -3.195983, -3.167622, -3.094788, -2.984045,
        -2.937463, -2.890713, -2.890660, -2.691874,
        -2.649229, -2.544696, -2.498147, -2.462573,
        -2.396637, -2.399795, -2.236338, -2.217625,
        -2.158336, -2.135569, -1.978521, -1.913965,
        -1.937082, -1.752275, -1.704013, -1.640514,
        -1.598791, -1.553859, -1.512187, -1.448088,
        -1.450443, -1.220567, -1.182340, -1.123139,
        -1.098469, -1.020642, -0.973039, -0.933279,
        -0.938035, -0.757380, -0.740860, -0.669721,
        -0.681526, -0.555390, -0.519321, -0.509318,
        -0.456936, -0.460622, -0.290578, -0.264393,
        -0.252716, -0.194141, -0.153566, -0.067842,
        -0.033402, -0.054947, 0.012860, 0.000000,
        -0.009715, 0.236054, 0.273956, 0.271968,
        0.330177, 0.345427, 0.352333, 0.433861,
        0.442952, 0.476411, 0.539632, 0.525355,
        0.526115, 0.707022, 0.701551, 0.734875,
        0.739149, 0.794320, 0.801578, 0.814225,
        0.818939, 0.897102, 0.895082, 0.927998,
        0.929797, 0.956112, 0.956789, 0.958121

};

/* This table converts LFO speed to frequency in Hz. It is based on
 * interpolation of Jamie Bullock's measurements. */
static float dx7_voice_lfo_frequency[128] = {
        0.062506, 0.124815, 0.311474, 0.435381, 0.619784,
        0.744396, 0.930495, 1.116390, 1.284220, 1.496880,
        1.567830, 1.738994, 1.910158, 2.081322, 2.252486,
        2.423650, 2.580668, 2.737686, 2.894704, 3.051722,
        3.208740, 3.366820, 3.524900, 3.682980, 3.841060,
        3.999140, 4.159420, 4.319700, 4.479980, 4.640260,
        4.800540, 4.953584, 5.106628, 5.259672, 5.412716,
        5.565760, 5.724918, 5.884076, 6.043234, 6.202392,
        6.361550, 6.520044, 6.678538, 6.837032, 6.995526,
        7.154020, 7.300500, 7.446980, 7.593460, 7.739940,
        7.886420, 8.020588, 8.154756, 8.288924, 8.423092,
        8.557260, 8.712624, 8.867988, 9.023352, 9.178716,
        9.334080, 9.669644, 10.005208, 10.340772, 10.676336,
        11.011900, 11.963680, 12.915460, 13.867240, 14.819020,
        15.770800, 16.640240, 17.509680, 18.379120, 19.248560,
        20.118000, 21.040700, 21.963400, 22.886100, 23.808800,
        24.731500, 25.759740, 26.787980, 27.816220, 28.844460,
        29.872700, 31.228200, 32.583700, 33.939200, 35.294700,
        36.650200, 37.812480, 38.974760, 40.137040, 41.299320,
        42.461600, 43.639800, 44.818000, 45.996200, 47.174400,
        47.174400, 47.174400, 47.174400, 47.174400, 47.174400,
        47.174400, 47.174400, 47.174400, 47.174400, 47.174400,
        47.174400, 47.174400, 47.174400, 47.174400, 47.174400,
        47.174400, 47.174400, 47.174400, 47.174400, 47.174400,
        47.174400, 47.174400, 47.174400, 47.174400, 47.174400,
        47.174400, 47.174400, 47.174400
};

/* This table converts pitch modulation sensitivity to semitones at full
 * modulation (assuming a perfectly linear pitch mod depth to pitch
 * relationship).  It is from a simple averaging of Jamie Bullock's
 * TX-data-1/PMD and TX-data-2/ENV data, and ignores the apparent ~0.1
 * semitone positive bias that Jamie observed. [-FIX- smbolton: my
 * inclination would be to call this bias, if it's reproducible, a
 * non-desirable 'bug', and _not_ implement it in prdx7. And, at
 * least for my own personal build, I'd change that PMS=7 value to a
 * full octave, since that's one thing that's always bugged me about
 * my TX7.  Thoughts? ] */
static float dx7_voice_pms_to_semitones[8] = {
        0.0, 0.450584, 0.900392, 1.474744,
        2.587385, 4.232292, 6.982097, /* 11.722111 */ 12.0
};

/* This table converts amplitude modulation depth to output level
 * reduction at full modulation with an amplitude modulation sensitivity
 * of 3.  It was constructed from regression of a very few data points,
 * using this code:
 *   perl -e 'for ($i = 0; $i <= 99; $i++) { printf " %f,\n", exp($i * 0.0428993 - 0.285189); }' >x.c
 * and is probably rather rough in its accuracy. -FIX- */
static float dx7_voice_amd_to_ol_adjustment[100] = {
        0.0, 0.784829, 0.819230, 0.855139, 0.892622, 0.931748,
        0.972589, 1.015221, 1.059721, 1.106171, 1.154658, 1.205270,
        1.258100, 1.313246, 1.370809, 1.430896, 1.493616, 1.559085,
        1.627424, 1.698759, 1.773220, 1.850945, 1.932077, 2.016765,
        2.105166, 2.197441, 2.293761, 2.394303, 2.499252, 2.608801,
        2.723152, 2.842515, 2.967111, 3.097167, 3.232925, 3.374633,
        3.522552, 3.676956, 3.838127, 4.006362, 4.181972, 4.365280,
        4.556622, 4.756352, 4.964836, 5.182458, 5.409620, 5.646738,
        5.894251, 6.152612, 6.422298, 6.703805, 6.997652, 7.304378,
        7.624549, 7.958754, 8.307609, 8.671754, 9.051861, 9.448629,
        9.862789, 10.295103, 10.746365, 11.217408, 11.709099,
        12.222341, 12.758080, 13.317302, 13.901036, 14.510357,
        15.146387, 15.810295, 16.503304, 17.226690, 17.981783,
        18.769975, 19.592715, 20.451518, 21.347965, 22.283705,
        23.260462, 24.280032, 25.344294, 26.455204, 27.614809,
        28.825243, 30.088734, 31.407606, 32.784289, 34.221315,
        35.721330, 37.287095, 38.921492, 40.627529, 42.408347,
        44.267222, 46.207578, 48.232984, 50.347169, 52.75
};

/* This table converts modulation source sensitivity (e.g. 'foot
 * controller sensitivity') into output level reduction at full modulation
 * with amplitude modulation sensitivity 3.  It's basically just the above
 * table scaled for 0 to 15 instead of 0 to 99. */
static float dx7_voice_mss_to_ol_adjustment[16] = {
        0.0, 0.997948, 1.324562, 1.758071, 2.333461, 3.097167, 4.110823,
        5.456233, 7.241976, 9.612164, 12.758080, 16.933606, 22.475719,
        29.831681, 39.595137, 52.75
};

/* these come right out of alsa/asoundef.h */
#define MIDI_CTL_MSB_MODWHEEL           0x01    /**< Modulation */
#define MIDI_CTL_MSB_BREATH             0x02    /**< Breath */
#define MIDI_CTL_MSB_FOOT               0x04    /**< Foot */
/* -FIX- support 5 portamento time */
#define MIDI_CTL_MSB_DATA_ENTRY         0x06    /**< Data entry */
#define MIDI_CTL_MSB_MAIN_VOLUME        0x07    /**< Main volume */
#define MIDI_CTL_MSB_PAN                0x0a    /**< Panpot */
#define MIDI_CTL_MSB_EXPRESSION         0x0b    /**< Expression */
#define MIDI_CTL_MSB_GENERAL_PURPOSE1   0x10    /**< General purpose 1 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE2   0x11    /**< General purpose 2 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE3   0x12    /**< General purpose 3 */
#define MIDI_CTL_MSB_GENERAL_PURPOSE4   0x13    /**< General purpose 4 */
#define MIDI_CTL_LSB_MODWHEEL           0x21    /**< Modulation */
#define MIDI_CTL_LSB_BREATH             0x22    /**< Breath */
#define MIDI_CTL_LSB_FOOT               0x24    /**< Foot */
#define MIDI_CTL_LSB_DATA_ENTRY         0x26    /**< Data entry */
#define MIDI_CTL_LSB_MAIN_VOLUME        0x27    /**< Main volume */
#define MIDI_CTL_SUSTAIN                0x40    /**< Sustain pedal */
/* -FIX- support 65(?) portamento switch */
#define MIDI_CTL_GENERAL_PURPOSE5       0x50    /**< General purpose 5 */
#define MIDI_CTL_GENERAL_PURPOSE6       0x51    /**< General purpose 6 */
#define MIDI_CTL_NONREG_PARM_NUM_LSB    0x62    /**< Non-registered parameter number */
#define MIDI_CTL_NONREG_PARM_NUM_MSB    0x63    /**< Non-registered parameter number */
#define MIDI_CTL_REGIST_PARM_NUM_LSB    0x64    /**< Registered parameter number */
#define MIDI_CTL_REGIST_PARM_NUM_MSB    0x65    /**< Registered parameter number */
#define MIDI_CTL_ALL_SOUNDS_OFF         0x78    /**< All sounds off */
#define MIDI_CTL_RESET_CONTROLLERS      0x79    /**< Reset Controllers */
#define MIDI_CTL_ALL_NOTES_OFF          0x7b    /**< All notes off */

#define PRDX7_INSTANCE_SUSTAINED(_s)  ((_s)->cc[MIDI_CTL_SUSTAIN] >= 64)

/* ==== debugging ==== */

/* DSSP_DEBUG bits */
#define DB_DSSI    1   /* DSSI interface */
#define DB_AUDIO   2   /* audio output */
#define DB_NOTE    4   /* note failed to allocate a voiceon/off, voice allocation */
#define DB_DATA    8   /* plugin patchbank handling */
#define DB_MAIN   16   /* GUI main program flow */
#define DB_OSC    32   /* GUI OSC handling */
#define DB_IO     64   /* GUI patch file input/output */
#define DB_GUI   128   /* GUI GUI callbacks, updating, etc. */


#define DEBUG_MESSAGE(type, fmt...) /* LOGE(fmt);*/
#define GUIDB_MESSAGE(type, fmt...)
#define TUIDB_MESSAGE(type, fmt...)
#define DSSP_DEBUG_INIT(x)


#ifndef PRDX7_USE_FLOATING_POINT

#define FP_SHIFT         24
#define FP_SIZE          (1<<FP_SHIFT)
#define FP_MASK          (FP_SIZE-1)
#define FP_TO_SINE_SHIFT (FP_SHIFT-SINE_SHIFT)
#define FP_TO_SINE_SIZE  (1<<FP_TO_SINE_SHIFT)
#define FP_TO_SINE_MASK  (FP_TO_SINE_SIZE-1)

#define FP_TO_INT(x)    ((x) >> FP_SHIFT)
#define FP_TO_FLOAT(x)  ((float)(x) * (1.0f / (float)FP_SIZE))
#define FP_TO_DOUBLE(x) ((double)(x) * (1.0 / (double)FP_SIZE))
#define INT_TO_FP(x)    ((x) << FP_SHIFT)
/* beware of using the next two with constants, they probably won't be optimized */
#define FLOAT_TO_FP(x)  lrintf((x) * (float)FP_SIZE)
#define DOUBLE_TO_FP(x) lrint((x) * (double)FP_SIZE)

#define FP_MULTIPLY(a, b)     ((int32_t)(((int64_t)(a) * (int64_t)(b)) >> FP_SHIFT))
#define FP_DIVIDE_CEIL(n, d)  (((n) + (d) - 1) / (d))
#define FP_ABS(x)             (abs(x))
#define FP_RAND()             (rand() & FP_MASK)

#else /* PRDX7_USE_FLOATING_POINT */

#define FP_TO_INT(x)        (lrintf(x))
#define FP_TO_FLOAT(x)      (x)
#define FP_TO_DOUBLE(x)     ((double)(x))
#define INT_TO_FP(x)        ((float)(x))
#define FLOAT_TO_FP(x)      (x)
#define DOUBLE_TO_FP(x)     ((float)(x))

#define FP_MULTIPLY(x, y)     ((x) * (y))
#define FP_DIVIDE_CEIL(n, d)  (lrintf((n) / (d) + 0.5f));
#define FP_ABS(x)             (fabsf(x))
#define FP_RAND()             ((float)rand() / (float)RAND_MAX)

#endif /* ! PRDX7_USE_FLOATING_POINT */

#define SINE_SHIFT       12
#define SINE_SIZE        (1<<SINE_SHIFT)
#define SINE_MASK        (SINE_SIZE-1)

#define _PLAYING(voice)    ((voice).status != DX7_VOICE_OFF)
#define _ON(voice)         ((voice).status == DX7_VOICE_ON)
#define _SUSTAINED(voice)  ((voice).status == DX7_VOICE_SUSTAINED)
#define _RELEASED(voice)   ((voice).status == DX7_VOICE_RELEASED)
#define _AVAILABLE(voice)  ((voice).status == DX7_VOICE_OFF)

/* dx7_voice.c */
static void dx7_voice_note_on(prdx7_instance_t *instance, dx7_voice_t &voice,
                              unsigned char key, unsigned char velocity);

static void dx7_voice_note_off(prdx7_instance_t *instance, dx7_voice_t &voice,
                               unsigned char key, unsigned char rvelocity);

static void dx7_voice_release_note(prdx7_instance_t *instance, dx7_voice_t &voice);

static void dx7_op_eg_set_increment(prdx7_instance_t *instance, dx7_op_eg_t &eg,
                                    int32_t new_rate, int new_level);

static void dx7_op_eg_set_next_phase(prdx7_instance_t *instance, dx7_op_eg_t &eg);

static void dx7_op_eg_set_phase(prdx7_instance_t *instance, dx7_op_eg_t &eg,
                                int32_t phase);

static void dx7_op_envelope_prepare(prdx7_instance_t *instance, dx7_op_t &op,
                                    int32_t transposed_note, int velocity);

static void dx7_eg_init_constants(prdx7_instance_t *instance);

static void dx7_pitch_eg_set_increment(prdx7_instance_t *instance,
                                       dx7_pitch_eg_t &eg, int32_t new_rate,
                                       int32_t new_level);

static void dx7_pitch_eg_set_next_phase(prdx7_instance_t *instance,
                                        dx7_pitch_eg_t &eg);

static void dx7_pitch_eg_set_phase(prdx7_instance_t *instance, dx7_pitch_eg_t &eg,
                                   int32_t phase);

static void dx7_pitch_envelope_prepare(prdx7_instance_t *instance,
                                       dx7_voice_t &voice);

static void dx7_portamento_set_segment(prdx7_instance_t *instance,
                                       dx7_portamento_t &port);

static void dx7_portamento_prepare(prdx7_instance_t *instance,
                                   dx7_voice_t &voice);

static void dx7_op_recalculate_increment(prdx7_instance_t *instance, dx7_op_t &op);

static double dx7_voice_recalculate_frequency(prdx7_instance_t *instance,
                                              dx7_voice_t &voice);

static void dx7_voice_recalculate_freq_and_inc(prdx7_instance_t *instance,
                                               dx7_voice_t &voice);

static void dx7_voice_recalculate_volume(prdx7_instance_t *instance,
                                         dx7_voice_t &voice);

static void dx7_lfo_reset(prdx7_instance_t *instance);

static void dx7_lfo_set(prdx7_instance_t *instance, dx7_voice_t &voice);

static void dx7_lfo_update(prdx7_instance_t *instance,
                           unsigned long sample_count);

static void dx7_voice_update_mod_depths(prdx7_instance_t *instance,
                                        dx7_voice_t &voice);

static void dx7_voice_calculate_runtime_parameters(prdx7_instance_t *instance,
                                                   dx7_voice_t &voice);

static void dx7_voice_setup_note(prdx7_instance_t *instance, dx7_voice_t &voice);

static void dx7_voice_set_data(prdx7_instance_t *instance, dx7_voice_t &voice);

/* dx7_voice_render.c */
static void dx7_voice_render(prdx7_instance_t *instance, dx7_voice_t &voice,
                             float *out, unsigned long sample_count,
                             int32_t do_control_update);

/* dx7_voice_tables.c */
static void dx7_voice_init_tables(void);


static char *dssp_error_message(const char *fmt, ...);

static int32_t decode_7in6(const char *string, int expected_length, uint8_t *data);

static void dx7_voice_copy_name(char *name, dx7_patch_t *patch);

static void dx7_patch_unpack(dx7_patch_t *packed_patch, uint8_t number,
                             uint8_t *unpacked_patch);

static void dx7_patch_pack(uint8_t *unpacked_patch, dx7_patch_t *packed_patch,
                           uint8_t number);

static void prdx7_data_patches_init(dx7_patch_t *patches);

static void prdx7_data_performance_init(uint8_t *performance);

static void prdx7_instance_init_controls(prdx7_instance_t *instance);

static void prdx7_instance_set_performance_data(prdx7_instance_t *instance);

#include "fm.h"

static dx7_sample_t dx7_voice_sin_table[SINE_SIZE + 1];

prdx7_synth_t::prdx7_synth_t() {
    instance_count = 0;
    instances = NULL;
    nugget_remains = 0;
    note_id = 0;
    global_polyphony = PRDX7_DEFAULT_POLYPHONY;
    for (int32_t i = 0; i <= SINE_SIZE; i++) {

        /* observation of my TX7's output with oscillator sync on suggests
         * it uses cosine */
        double f = cos(
                (double) (i) / SINE_SIZE * (2 * M_PI));  /* index / index max * radian cycle */
        dx7_voice_sin_table[i] = DOUBLE_TO_FP(f);
    }

#ifndef PRDX7_USE_FLOATING_POINT
    #if FP_SHIFT != 24
    /* Any fixed-point tables below are in s7.24 format.  Shift
     * them to match FP_SHIFT. */
    for (i = 0; i <= 256; i++) {
        dx7_voice_eg_ol_to_mod_index_table[i] >>= (24 - FP_SHIFT);
    }
#endif
#endif /* ! PRDX7_USE_FLOATING_POINT */
}

static prdx7_synth_t &prdx7_synth() {
    static prdx7_synth_t *synth = new prdx7_synth_t();
    return *synth;
}


static void
prdx7_cleanup(prdx7_instance_t *instance);

/* ---- mutual exclusion ---- */

static inline int
dssp_voicelist_mutex_trylock(void) {
    int32_t rc;

    /* Attempt the mutex lock */
    rc = pthread_mutex_trylock(&prdx7_synth().mutex);
    if (rc) {
        prdx7_synth().mutex_grab_failed = 1;
        return rc;
    }
    /* Clean up if a previous mutex grab failed */
    if (prdx7_synth().mutex_grab_failed) {
        prdx7_synth_all_voices_off();
        prdx7_synth().mutex_grab_failed = 0;
    }
    return 0;
}

static inline int
dssp_voicelist_mutex_lock(void) {
    return pthread_mutex_lock(&prdx7_synth().mutex);
}

static inline int
dssp_voicelist_mutex_unlock(void) {
    return pthread_mutex_unlock(&prdx7_synth().mutex);
}

prdx7_instance_t *
prdx7_instantiate(
        unsigned long sample_rate) {
    prdx7_instance_t *instance = new prdx7_instance_t;
    if (!instance) {
        prdx7_cleanup(NULL);
        return NULL;
    }
    instance->tuning = 440;
    instance->volume = -6;
    instance->next = prdx7_synth().instances;
    prdx7_synth().instances = instance;
    prdx7_synth().instance_count++;

    /* do any per-instance one-time initialization here */
    pthread_mutex_init(&instance->patches_mutex, NULL);

    instance->sample_rate = (float) sample_rate;
    dx7_eg_init_constants(instance);  /* depends on sample rate */

    instance->polyphony = PRDX7_DEFAULT_POLYPHONY;
    instance->monophonic = DSSP_MONO_MODE_OFF;
    instance->max_voices = instance->polyphony;
    instance->current_voices = 0;
    instance->last_key = 0;
    instance->pending_program_change = -1;
    instance->current_program = 0;
    instance->overlay_program = -1;
    prdx7_data_performance_init(instance->performance_buffer);
    prdx7_data_patches_init(instance->patches);
    prdx7_instance_select_program(instance, 0);
    prdx7_instance_init_controls(instance);

    return instance;
}


/*
 * prdx7_activate
 *
 * implements LADSPA (*activate)()
 */
void
prdx7_activate(prdx7_instance_t *handle) {
    prdx7_instance_t *instance = (prdx7_instance_t *) handle;

    prdx7_instance_all_voices_off(instance);  /* stop all sounds immediately */
    instance->current_voices = 0;
    dx7_lfo_reset(instance);
}

// optional:
//  void (*run_adding)(LADSPA_Handle Instance,
//                     unsigned long SampleCount);
//  void (*set_run_adding_gain)(LADSPA_Handle Instance,
//                              LADSPA_Data   Gain);

/*
 * prdx7_deactivate
 *
 * implements LADSPA (*deactivate)()
 */
void
prdx7_deactivate(prdx7_instance_t *handle) {
    prdx7_instance_t *instance = (prdx7_instance_t *) handle;

    prdx7_instance_all_voices_off(instance);  /* stop all sounds immediately */
}

/*
 * prdx7_cleanup
 *
 * implements LADSPA (*cleanup)()
 */
static void
prdx7_cleanup(prdx7_instance_t *handle) {
    prdx7_instance_t *instance = (prdx7_instance_t *) handle;
    int32_t i;

    if (instance) {
        prdx7_instance_t *inst, *prev;

        prdx7_deactivate(instance);

        prev = NULL;
        for (inst = prdx7_synth().instances; inst; inst = inst->next) {
            if (inst == instance) {
                if (prev)
                    prev->next = inst->next;
                else
                    prdx7_synth().instances = inst->next;
                break;
            }
            prev = inst;
        }
        prdx7_synth().instance_count--;

        delete (instance);
    }
}

/* ---- DSSI interface ---- */

/*
 * prdx7_configure
 *
 * implements DSSI (*configure)()
 */
char *
prdx7_configure(prdx7_instance_t *handle, const char *key, const char *value) {
    prdx7_instance_t *instance = (prdx7_instance_t *) handle;

    DEBUG_MESSAGE(DB_DSSI, " prdx7_configure called with '%s' and '%s'\n", key, value);

    if (strlen(key) == 8 && !strncmp(key, "patches", 7)) {

        return prdx7_instance_handle_patches(instance, key, value);

    } else if (!strcmp(key, "edit_buffer")) {

        return prdx7_instance_handle_edit_buffer(instance, value);

    } else if (!strcmp(key, "performance")) {  /* global performance parameters */

        return prdx7_instance_handle_performance(instance, value);

    } else if (!strcmp(key, "monophonic")) {

        return prdx7_instance_handle_monophonic(instance, value);

    } else if (!strcmp(key, "polyphony")) {

        return prdx7_instance_handle_polyphony(instance, value);

#ifdef DSSI_GLOBAL_CONFIGURE_PREFIX
        } else if (!strcmp(key, DSSI_GLOBAL_CONFIGURE_PREFIX "polyphony")) {
#else
    } else if (!strcmp(key, "global_polyphony")) {
#endif

        return prdx7_synth_handle_global_polyphony(value);

#ifdef DSSI_PROJECT_DIRECTORY_KEY
        } else if (!strcmp(key, DSSI_PROJECT_DIRECTORY_KEY)) {

        return NULL; /* plugin has no use for project directory key, ignore it */

#endif
    }
    return strdup("error: unrecognized configure key");
}

/*
 * prdx7_select_program
 *
 * implements DSSI (*select_program)()
 */
void
prdx7_select_program(prdx7_instance_t *handle, unsigned long program) {
    prdx7_instance_t *instance = (prdx7_instance_t *) handle;

    DEBUG_MESSAGE(DB_DSSI, " prdx7_select_program called with %lu\n", program);

    /* ignore invalid program requests */
    if (program >= DX_PROGRAM_BUFFER_SIZE)
        return;

    /* Attempt the patch mutex, return if lock fails. */
    if (pthread_mutex_trylock(&instance->patches_mutex)) {
        instance->pending_program_change = program;
        return;
    }

    prdx7_instance_select_program((prdx7_instance_t *) instance, program);

    pthread_mutex_unlock(&instance->patches_mutex);
}

/*
 * prdx7_handle_pending_program_change
 */
static inline void
prdx7_handle_pending_program_change(prdx7_instance_t *instance) {
    /* Attempt the patch mutex, return if lock fails. */
    if (pthread_mutex_trylock(&instance->patches_mutex))
        return;

    prdx7_instance_select_program((prdx7_instance_t *) instance,
                                  instance->pending_program_change);
    instance->pending_program_change = -1;

    pthread_mutex_unlock(&instance->patches_mutex);
}


/*
 * prdx7_handle_event
 */
void
prdx7_handle_event(prdx7_instance_t *instance, const smf::MidiEvent &event) {
    switch (event->type) {
        case DX7_NOTE_OFF:
            prdx7_instance_note_off(instance, event->note, event->velocity);
            break;
        case DX7_NOTE_ON:
            prdx7_instance_note_on(instance, event[1], event[2]);

            break;
        case DX7_KEY_PRESS:
            prdx7_instance_key_pressure(instance, event->note, event->velocity);
            break;
        case DX7_CONTROLLER:
            prdx7_instance_control_change(instance, event->param, event->value);
            break;
        case DX7_CHANPRESS:
            prdx7_instance_channel_pressure(instance, event->value);
            break;
        case DX7_PITCHBEND:
            prdx7_instance_pitch_bend(instance, event->value);
            break;
        default:
            break;
}


/*
 * dx7_voice_off
 *
 * turn off a voice immediately
 */
static inline void
dx7_voice_off(dx7_voice_t &voice) {
    voice.status = DX7_VOICE_OFF;
    if (voice.instance->monophonic)
        voice.instance->mono_voice = NULL;
    voice.instance->current_voices--;
}

/*
 * dx7_voice_start_voice
 */
static inline void
dx7_voice_start_voice(dx7_voice_t &voice) {
    voice.status = DX7_VOICE_ON;
    voice.instance->current_voices++;
}

/*
 * prdx7_instance_clear_held_keys
 */
static inline void
prdx7_instance_clear_held_keys(prdx7_instance_t *instance) {
    int32_t i;

    for (i = 0; i < 8; i++)
        instance->held_keys[i] = -1;
}

/*
 * prdx7_instance_remove_held_key
 */
static inline void
prdx7_instance_remove_held_key(prdx7_instance_t *instance, unsigned char key) {
    int32_t i;

    /* check if this key is in list of held keys; if so, remove it and
     * shift the other keys up */
    /* DEBUG_MESSAGE(DB_NOTE, " note-off key list before: %d %d %d %d %d %d %d %d\n", instance->held_keys[0], instance->held_keys[1], instance->held_keys[2], instance->held_keys[3], instance->held_keys[4], instance->held_keys[5], instance->held_keys[6], instance->held_keys[7]); */
    for (i = 7; i >= 0; i--) {
        if (instance->held_keys[i] == key)
            break;
    }
    if (i >= 0) {
        for (; i < 7; i++) {
            instance->held_keys[i] = instance->held_keys[i + 1];
        }
        instance->held_keys[7] = -1;
    }
    /* DEBUG_MESSAGE(DB_NOTE, " note-off key list after: %d %d %d %d %d %d %d %d\n", instance->held_keys[0], instance->held_keys[1], instance->held_keys[2], instance->held_keys[3], instance->held_keys[4], instance->held_keys[5], instance->held_keys[6], instance->held_keys[7]); */
}

/*
 * prdx7_synth_all_voices_off
 *
 * stop processing all notes of all instances immediately
 */
void
prdx7_synth_all_voices_off(void) {
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (_PLAYING(voice)) {
            if (voice.instance->held_keys[0] != -1)
                prdx7_instance_clear_held_keys(voice.instance);
            dx7_voice_off(voice);
        }
    }
}

/*
 * prdx7_instance_all_voices_off
 *
 * stop processing all notes within instance immediately
 */
void
prdx7_instance_all_voices_off(prdx7_instance_t *instance) {
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (voice.instance == instance && _PLAYING(voice)) {
            dx7_voice_off(voice);
        }
    }
    prdx7_instance_clear_held_keys(instance);
}

/*
 * prdx7_instance_note_off
 *
 * handle a note off message
 */
void
prdx7_instance_note_off(prdx7_instance_t *instance, unsigned char key,
                        unsigned char rvelocity) {
    prdx7_instance_remove_held_key(instance, key);

    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (voice.instance == instance &&
            (instance->monophonic ? (_PLAYING(voice)) :
             (_ON(voice) && (voice.key == key)))) {
            DEBUG_MESSAGE(DB_NOTE, " prdx7_instance_note_off: key %d rvel %d voice %d note id %d\n",
                          key, rvelocity, i, voice.note_id);
            dx7_voice_note_off(instance, voice, key, rvelocity);
        } /* if voice on */
    } /* for all voices */
}

/*
 * prdx7_instance_all_notes_off
 *
 * put all notes into the released state
 */
void
prdx7_instance_all_notes_off(prdx7_instance_t *instance) {

    /* reset the sustain controller */
    instance->cc[MIDI_CTL_SUSTAIN] = 0;
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (voice.instance == instance &&
            (_ON(voice) || _SUSTAINED(voice))) {
            dx7_voice_release_note(instance, voice);
        }
    }
}

/*
 * prdx7_synth_free_voice_by_kill
 *
 * selects a voice for killing. the selection algorithm is a refinement
 * of the algorithm previously in fluid_synth_alloc_voice.
 */
static dx7_voice_t *
prdx7_synth_free_voice_by_kill(prdx7_instance_t *instance) {
    int32_t best_prio = 10001;
    int32_t this_voice_prio;
    int32_t best_voice_index = -1;

    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];

        if (instance) {
            /* only look at playing voices of this instance */
            if (_AVAILABLE(voice) || voice.instance != instance)
                continue;
        } else {
            /* safeguard against an available voice. */
            if (_AVAILABLE(voice))
                return &voice;
        }

        /* Determine, how 'important' a voice is.
         * Start with an arbitrary number */
        this_voice_prio = 10000;

        if (_RELEASED(voice)) {
            /* This voice is in the release phase. Consider it much less
             * important than a voice which is still held. */
            this_voice_prio -= 2000;
        } else if (_SUSTAINED(voice)) {
            /* The sustain pedal is held down, and this voice is still "on"
             * because of this even though it has received a note off.
             * Consider it less important than voices which have not yet
             * received a note off. This decision is somewhat subjective, but
             * usually the sustain pedal is used to play 'more-voices-than-
             * fingers', and if so, it won't hurt as much to kill one of those
             * voices. */
            this_voice_prio -= 1000;
        };

        /* We are not enthusiastic about releasing voices, which have just been
         * started.  Otherwise hitting a chord may result in killing notes
         * belonging to that very same chord.  So subtract the age of the voice
         * from the priority - an older voice is just a little bit less
         * important than a younger voice. */
        this_voice_prio -= (prdx7_synth().note_id - voice.note_id);

        /* -FIX- not yet implemented:
         * /= take a rough estimate of loudness into account. Louder voices are more important. =/
         * if (voice->volenv_section != FLUID_VOICE_ENVATTACK){
         *     this_voice_prio += voice->volenv_val*1000.;
         * };
         */

        /* check if this voice has less priority than the previous candidate. */
        if (this_voice_prio < best_prio)
            best_voice_index = i,
                    best_prio = this_voice_prio;
    }

    if (best_voice_index < 0)
        return NULL;

    dx7_voice_t &voice = prdx7_synth().voice[best_voice_index];
    DEBUG_MESSAGE(DB_NOTE,
                  " prdx7_synth_free_voice_by_kill: no available voices, killing voice %d note id %d\n",
                  best_voice_index, voice.note_id);
    dx7_voice_off(voice);
    return &voice;
}

/*
 * prdx7_synth_alloc_voice
 */
static dx7_voice_t *
prdx7_synth_alloc_voice(prdx7_instance_t *instance, unsigned char key) {
    /* If there is another voice on the same key, advance it
     * to the release phase. Note that a DX7 doesn't do this,
     * but we do it here to keep our CPU usage low. */
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];

        if (voice.instance == instance && voice.key == key &&
            (_ON(voice) || _SUSTAINED(voice))) {
            dx7_voice_release_note(instance, voice);
        }
    }

    dx7_voice_t *voice = NULL;

    if (instance->current_voices < instance->max_voices) {
        /* check if there's an available voice */
        for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
            if (_AVAILABLE(prdx7_synth().voice[i])) {
                voice = &prdx7_synth().voice[i];
                break;
            }
        }

        /* if not, then stop a running voice. */
        if (voice == NULL) {
            voice = prdx7_synth_free_voice_by_kill(NULL);
        }
    } else {  /* at instance polyphony limit */
        voice = prdx7_synth_free_voice_by_kill(instance);
    }

    if (voice == NULL) {
        DEBUG_MESSAGE(DB_NOTE, " prdx7_synth_alloc_voice: failed to allocate a voice (key=%d)\n",
                      key);
        return NULL;
    }

    DEBUG_MESSAGE(DB_NOTE, " prdx7_synth_alloc_voice: key %d voice %p\n", key, voice);
    return voice;
}

/*
 * prdx7_instance_note_on
 */
void
prdx7_instance_note_on(prdx7_instance_t *instance, unsigned char key,
                       unsigned char velocity) {
    dx7_voice_t *voice;

    if (key > 127 || velocity > 127)
        return;  /* MidiKeys 1.6b3 sends bad notes.... */

    if (instance->monophonic) {

        if (instance->mono_voice) {
            voice = instance->mono_voice;
            DEBUG_MESSAGE(DB_NOTE,
                          " prdx7_instance_note_on: retriggering mono voice on new key %d\n", key);
        } else {
            voice = prdx7_synth_alloc_voice(instance, key);
            if (voice == NULL)
                return;
            instance->mono_voice = voice;
        }

    } else { /* polyphonic mode */

        voice = prdx7_synth_alloc_voice(instance, key);
        if (voice == NULL)
            return;

    }

    voice->instance = instance;
    voice->note_id = prdx7_synth().note_id++;

    dx7_voice_note_on(instance, *voice, key, velocity);
}

/*
 * prdx7_instance_key_pressure
 */
inline void
prdx7_instance_key_pressure(prdx7_instance_t *instance, unsigned char key,
                            unsigned char pressure) {
    if (instance->key_pressure[key] == pressure)
        return;

    /* save it for future voices */
    instance->key_pressure[key] = pressure;

    /* flag any playing voices as needing updating */
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (voice.instance == instance && _PLAYING(voice) && voice.key == key) {
            voice.mods_serial--;
        }
    }
}

/*
 * prdx7_instance_damp_voices
 *
 * advance all sustained voices to the release phase (note that this does not
 * clear the sustain controller.)
 */
void
prdx7_instance_damp_voices(prdx7_instance_t *instance) {
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (voice.instance == instance && _SUSTAINED(voice)) {
            /* this assumes the caller has cleared the sustain controller */
            dx7_voice_release_note(instance, voice);
        }
    }
}

/*
 * prdx7_instance_update_mod_wheel
 */
static inline void
prdx7_instance_update_mod_wheel(prdx7_instance_t *instance) {
    int32_t mod = instance->cc[MIDI_CTL_MSB_MODWHEEL] * 128 +
              instance->cc[MIDI_CTL_LSB_MODWHEEL];

    if (mod > 16256) mod = 16256;
    instance->mod_wheel = (float) mod / 16256.0f;
    instance->mods_serial++;

#ifdef PRDX7_DEBUG_CONTROL
    instance->feedback_mod = instance->cc[MIDI_CTL_MSB_MODWHEEL];
    printf("new mod wheel value %d\n", instance->feedback_mod);
#endif
}

/*
 * prdx7_instance_update_breath
 */
static inline void
prdx7_instance_update_breath(prdx7_instance_t *instance) {
    int32_t mod = instance->cc[MIDI_CTL_MSB_BREATH] * 128 +
              instance->cc[MIDI_CTL_LSB_BREATH];

    if (mod > 16256) mod = 16256;
    instance->breath = (float) mod / 16256.0f;
    instance->mods_serial++;
}

/*
 * prdx7_instance_update_foot
 */
static inline void
prdx7_instance_update_foot(prdx7_instance_t *instance) {
    int32_t mod = instance->cc[MIDI_CTL_MSB_FOOT] * 128 +
              instance->cc[MIDI_CTL_LSB_FOOT];

    if (mod > 16256) mod = 16256;
    instance->foot = (float) mod / 16256.0f;
    instance->mods_serial++;
}

/*
 * prdx7_instance_update_volume
 */
static inline void
prdx7_instance_update_volume(prdx7_instance_t *instance) {
    instance->cc_volume = instance->cc[MIDI_CTL_MSB_MAIN_VOLUME] * 128 +
                          instance->cc[MIDI_CTL_LSB_MAIN_VOLUME];
    if (instance->cc_volume > 16256)
        instance->cc_volume = 16256;
}

/*
 * prdx7_instance_update_op_param
 *
 * Generic function to update operator parameters
 *
 */
static void
prdx7_instance_update_op_param(prdx7_instance_t *instance, int32_t opnum,
                               int32_t param, signed int value) {
    /* scale the value */
    switch (param) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 16:
        case 19:
            value = value * 100 / 16384;  /* 0 to 99 */
            break;
        case 11:
        case 12:
        case 14:
            value = value * 4 / 16384;  /* 0 to 3 */
            break;
        case 13:
        case 15:
            value = value * 8 / 16384;  /* 0 to 7 */
            break;
        case 17:
            value = value * 2 / 16384;  /* 0 or 1 */
            break;
        case 18:
            value = value * 32 / 16384;  /* 0 to 31 */
            break;
        case 20:
            value = value * 15 / 16384;  /* 0 to 14 */
            break;
    }

    /* update edit buffer */
    if (!pthread_mutex_trylock(&instance->patches_mutex)) {

        instance->current_patch_buffer[((5 - opnum) * 21) + param] =
                value;

        pthread_mutex_unlock(&instance->patches_mutex);
    } else {
        /* In the unlikely event that we get here, it means another thread is
         * currently updating the current patch buffer. We could do something
         * like the 'pending_program_change' mechanism to cache this change
         * until we can lock the mutex, if it's really important. */
    }

    /* check if any playing voices need updating */
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (voice.instance == instance && _PLAYING(voice)) {
            dx7_op_t &op = voice.op[opnum];

            /* set values */
            switch (param) {
                case 0:
                    op.eg.base_rate[0] = value;
                    break;
                case 1:
                    op.eg.base_rate[1] = value;
                    break;
                case 2:
                    op.eg.base_rate[2] = value;
                    break;
                case 3:
                    op.eg.base_rate[3] = value;
                    break;
                case 4:
                    op.eg.base_level[0] = value;
                    break;
                case 5:
                    op.eg.base_level[1] = value;
                    break;
                case 6:
                    op.eg.base_level[2] = value;
                    break;
                case 7:
                    op.eg.base_level[3] = value;
                    break;
                case 8:
                    op.level_scaling_bkpoint = value;
                    break;
                case 9:
                    op.level_scaling_l_depth = value;
                    break;
                case 10:
                    op.level_scaling_r_depth = value;
                    break;
                case 11:
                    op.level_scaling_l_curve = value;
                    break;
                case 12:
                    op.level_scaling_r_curve = value;
                    break;
                case 13:
                    op.rate_scaling = value;
                    break;
                case 14:
                    op.amp_mod_sens = value;
                    break;
                case 15:
                    op.velocity_sens = value;
                    break;
                case 16:
                    op.output_level = value;
                    break;
                case 17:
                    op.osc_mode = value;
                    break;
                case 18:
                    op.coarse = value;
                    break;
                case 19:
                    op.fine = value;
                    break;
                case 20:
                    op.detune = value;
                    break;
            }

            /* do recalculations */
            switch (param) {
                case 17:    /* osc mode */
                case 18:    /* coarse */
                case 19:    /* fine */
                case 20:    /* detune */
                    dx7_op_recalculate_increment(instance, op);
                    break;
                    /* which other operator params need a recalc ?? */
            }
        }
    }
}

/*
 * prdx7_instance_update_fc
 */
static inline void
prdx7_instance_update_fc(prdx7_instance_t *instance, int32_t opnum,
                         signed int32_t value) {
    prdx7_instance_update_op_param(instance, opnum, 18, value * 128);
}

/*
 * prdx7_instance_handle_nrpn
 *
 * Update operator parameters via NRPN 0-125.
 */
static void
prdx7_instance_handle_nrpn(prdx7_instance_t *instance) {
    int32_t nrpn = instance->cc[MIDI_CTL_NONREG_PARM_NUM_MSB] * 128 +
               instance->cc[MIDI_CTL_NONREG_PARM_NUM_LSB];
    int32_t value = instance->cc[MIDI_CTL_MSB_DATA_ENTRY] * 128 +
                instance->cc[MIDI_CTL_LSB_DATA_ENTRY];

    int32_t opnum;
    int32_t op_param;

    if (nrpn >= 126) return; /* for now we only support operator params */

    opnum = nrpn / 21;   /* 0 = OP6, 5 = OP1 */
    op_param = nrpn - (21 * opnum);

    prdx7_instance_update_op_param(instance, 5 - opnum, op_param, value);
}

#ifdef PRDX7_DEBUG_CONTROL
void dx7_lfo_set_speed_x(prdx7_instance_t *instance);  /* prototype for test code below */
#endif

/*
 * prdx7_instance_control_change
 */
void
prdx7_instance_control_change(prdx7_instance_t *instance, uint32_t param,
                              signed int32_t value) {
    switch (param) {  /* these controls we act on always */

        case MIDI_CTL_SUSTAIN:
            DEBUG_MESSAGE(DB_NOTE, " prdx7_instance_control_change: got sustain control of %d\n",
                          value);
            instance->cc[param] = value;
            if (value < 64)
                prdx7_instance_damp_voices(instance);
            return;

        case MIDI_CTL_ALL_SOUNDS_OFF:
            instance->cc[param] = value;
            prdx7_instance_all_voices_off(instance);
            return;

        case MIDI_CTL_RESET_CONTROLLERS:
            instance->cc[param] = value;
            prdx7_instance_init_controls(instance);
            return;

        case MIDI_CTL_ALL_NOTES_OFF:
            instance->cc[param] = value;
            prdx7_instance_all_notes_off(instance);
            return;
    }

    if (param == MIDI_CTL_REGIST_PARM_NUM_LSB ||
        param == MIDI_CTL_REGIST_PARM_NUM_MSB) {

        /* reset NRPN numbers on receipt of RPN */
        instance->cc[MIDI_CTL_NONREG_PARM_NUM_LSB] = 127;
        instance->cc[MIDI_CTL_NONREG_PARM_NUM_MSB] = 127;
    }

    if (instance->cc[param] == value)  /* do nothing if control value has not changed */
        return;

    instance->cc[param] = value;

    switch (param) {

#ifdef PRDX7_DEBUG_CONTROL
        case MIDI_CTL_MSB_PAN: /* panning */
        // prdx7_instance_channel_pressure(instance, value);
        // { float f;
        //     f = 52.75f / (instance->sample_rate * 0.001f * (float)value);
        //     instance->amp_mod_max_slew = FLOAT_TO_FP(f);
        //     printf("new amp_mod_max_slew, %dms => %f = %d\n", value, f, instance->amp_mod_max_slew);
        // }
        {
            if (value == 0)
                instance->ramp_duration = 1;
            else
                instance->ramp_duration = (int)(instance->sample_rate * 0.001f * (float)value);  /* value ms ramp */
            printf("new ramp_duration, %dms => %d frames\n", value, instance->ramp_duration);
            dx7_lfo_set_speed_x(instance);
        }
        break;

      case MIDI_CTL_MSB_EXPRESSION: /* 'expression' */
        prdx7_instance_key_pressure(instance, 60, value);
        break;
#endif /* PRDX7_DEBUG_CONTROL */

        case MIDI_CTL_MSB_MODWHEEL:
        case MIDI_CTL_LSB_MODWHEEL:
            prdx7_instance_update_mod_wheel(instance);
            break;

        case MIDI_CTL_MSB_BREATH:
        case MIDI_CTL_LSB_BREATH:
            prdx7_instance_update_breath(instance);
            break;

        case MIDI_CTL_MSB_FOOT:
        case MIDI_CTL_LSB_FOOT:
            prdx7_instance_update_foot(instance);
            break;

        case MIDI_CTL_MSB_MAIN_VOLUME:
        case MIDI_CTL_LSB_MAIN_VOLUME:
            prdx7_instance_update_volume(instance);
            break;

        case MIDI_CTL_MSB_GENERAL_PURPOSE1:
        case MIDI_CTL_MSB_GENERAL_PURPOSE2:
        case MIDI_CTL_MSB_GENERAL_PURPOSE3:
        case MIDI_CTL_MSB_GENERAL_PURPOSE4:
            prdx7_instance_update_fc(instance, param - MIDI_CTL_MSB_GENERAL_PURPOSE1,
                                     value);
            break;

        case MIDI_CTL_GENERAL_PURPOSE5:
        case MIDI_CTL_GENERAL_PURPOSE6:
            prdx7_instance_update_fc(instance, param - MIDI_CTL_GENERAL_PURPOSE5 + 4,
                                     value);
            break;

            /* handle NRPN as real-time parameter change */
        case MIDI_CTL_MSB_DATA_ENTRY:
        case MIDI_CTL_LSB_DATA_ENTRY:
            if (instance->cc[MIDI_CTL_NONREG_PARM_NUM_MSB] != 127 &&
                instance->cc[MIDI_CTL_NONREG_PARM_NUM_LSB] != 127) {
                prdx7_instance_handle_nrpn(instance);
            }
            break;

            /* what others should we respond to? */

            /* these we ignore (let the host handle):
             *  BANK_SELECT_MSB
             *  BANK_SELECT_LSB
             *  RPN_MSB
             *  RPN_LSB
             * (may want to eventually implement RPN (0, 0) Pitch Bend Sensitivity)
             */
    }
}

/*
 * prdx7_instance_channel_pressure
 */
void
prdx7_instance_channel_pressure(prdx7_instance_t *instance,
                                signed int32_t pressure) {
    if (instance->channel_pressure == pressure)
        return;

    instance->channel_pressure = pressure;
    instance->mods_serial++;
}

/*
 * prdx7_instance_pitch_bend
 */
void
prdx7_instance_pitch_bend(prdx7_instance_t *instance, signed int32_t value) {
    instance->pitch_wheel = value; /* ALSA pitch bend is already -8192 - 8191 */
    instance->pitch_bend = (double) (value * instance->pitch_bend_range)
                           / 8192.0;
}

/*
 * prdx7_instance_init_controls
 */
static void
prdx7_instance_init_controls(prdx7_instance_t *instance) {
    int32_t i;

    /* if sustain was on, we need to damp any sustained voices */
    if (PRDX7_INSTANCE_SUSTAINED(instance)) {
        instance->cc[MIDI_CTL_SUSTAIN] = 0;
        prdx7_instance_damp_voices(instance);
    }

    for (i = 0; i < 128; i++) {
        instance->key_pressure[i] = 0;
        instance->cc[i] = 0;
    }
    instance->channel_pressure = 0;
    instance->pitch_wheel = 0;
    instance->pitch_bend = 0.0;
    instance->cc[MIDI_CTL_MSB_MAIN_VOLUME] = 127; /* full volume */
    instance->cc[MIDI_CTL_NONREG_PARM_NUM_LSB] = 127; /* 'null' */
    instance->cc[MIDI_CTL_NONREG_PARM_NUM_MSB] = 127; /* 'null' */

    prdx7_instance_update_mod_wheel(instance);
    prdx7_instance_update_breath(instance);
    prdx7_instance_update_foot(instance);
    prdx7_instance_update_volume(instance);

    instance->mods_serial++;
}

static inline int
limit(int32_t x, int min, int max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

/*
 * prdx7_instance_set_performance_data
 */
static void
prdx7_instance_set_performance_data(prdx7_instance_t *instance) {
    uint8_t *perf_buffer = instance->performance_buffer;

    /* set instance performance parameters */
    /* -FIX- later these will optionally come from patch */
    instance->pitch_bend_range = limit(perf_buffer[3], 0, 12);
    instance->portamento_time = limit(perf_buffer[5], 0, 99);
    instance->mod_wheel_sensitivity = limit(perf_buffer[9], 0, 15);
    instance->mod_wheel_assign = limit(perf_buffer[10], 0, 7);
    instance->foot_sensitivity = limit(perf_buffer[11], 0, 15);
    instance->foot_assign = limit(perf_buffer[12], 0, 7);
    instance->pressure_sensitivity = limit(perf_buffer[13], 0, 15);
    instance->pressure_assign = limit(perf_buffer[14], 0, 7);
    instance->breath_sensitivity = limit(perf_buffer[15], 0, 15);
    instance->breath_assign = limit(perf_buffer[16], 0, 7);
    if (perf_buffer[0] & 0x01) { /* 0.5.9 compatibility */
        instance->pitch_bend_range = 2;
        instance->portamento_time = 0;
        instance->mod_wheel_sensitivity = 0;
        instance->foot_sensitivity = 0;
        instance->pressure_sensitivity = 0;
        instance->breath_sensitivity = 0;
    }
}

/*
 * prdx7_instance_select_program
 */
void
prdx7_instance_select_program(prdx7_instance_t *instance, unsigned long program) {
    /* no support for banks, so we just ignore the bank number */
    if (program >= DX_PROGRAM_BUFFER_SIZE) return;
    instance->current_program = program;
    if (instance->overlay_program == program) { /* edit buffer applies */
        memcpy(instance->current_patch_buffer, instance->overlay_patch_buffer,
               DX7_VOICE_SIZE_UNPACKED);
    } else {
        dx7_patch_unpack(instance->patches, program, instance->current_patch_buffer);
    }
}

/*
 * prdx7_instance_set_program_descriptor
 */
char *prdx7_instance_get_program_descriptor(prdx7_instance_t *instance,
                                            unsigned long program) {
    static char name[11];

    /* no support for banks, so we just ignore the bank number */
    if (program >= DX_PROGRAM_BUFFER_SIZE) {
        return 0;
    }
    /* -FIX- some character set conversion would be appropriate here, but to what? */
    dx7_voice_copy_name(name, &instance->patches[program]);
    return name;
}

/*
 * prdx7_instance_handle_patches
 */
char *
prdx7_instance_handle_patches(prdx7_instance_t *instance, const char *key,
                              const char *value) {
    int32_t section;

    DEBUG_MESSAGE(DB_DATA, " prdx7_instance_handle_patches: received new '%s'\n", key);

    section = key[7] - '0';
    if (section < 0 || section > 3)
        return dssp_error_message("patch configuration failed: invalid section '%c'", key[7]);

    pthread_mutex_lock(&instance->patches_mutex);

    if (!decode_7in6(value, 32 * sizeof(dx7_patch_t),
                     (uint8_t *) &instance->patches[section * 32])) {
        pthread_mutex_unlock(&instance->patches_mutex);
        return dssp_error_message("patch configuration failed: corrupt data");
    }

    if ((instance->current_program / 32) == section &&
        instance->current_program != instance->overlay_program)
        dx7_patch_unpack(instance->patches, instance->current_program,
                         instance->current_patch_buffer);

    pthread_mutex_unlock(&instance->patches_mutex);

    return NULL; /* success */
}

/*
 * prdx7_instance_handle_edit_buffer
 */
char *
prdx7_instance_handle_edit_buffer(prdx7_instance_t *instance,
                                  const char *value) {
    struct {
        int32_t program;
        uint8_t buffer[DX7_VOICE_SIZE_UNPACKED];
    } edit_buffer;

    pthread_mutex_lock(&instance->patches_mutex);

    if (!strcmp(value, "off")) {

        DEBUG_MESSAGE(DB_DATA, " prdx7_instance_handle_edit_buffer: cancelled\n");
        if (instance->current_program == instance->overlay_program) {
            dx7_patch_unpack(instance->patches, instance->current_program,
                             instance->current_patch_buffer);
        }
        instance->overlay_program = -1;

    } else {

        DEBUG_MESSAGE(DB_DATA, " prdx7_instance_handle_edit_buffer: received new overlay\n");

        if (!decode_7in6(value, sizeof(edit_buffer), (uint8_t *) &edit_buffer)) {
            pthread_mutex_unlock(&instance->patches_mutex);
            return dssp_error_message("patch edit failed: corrupt data");
        }

        instance->overlay_program = edit_buffer.program;
        memcpy(instance->overlay_patch_buffer, edit_buffer.buffer, DX7_VOICE_SIZE_UNPACKED);
        if (instance->current_program ==
            instance->overlay_program) { /* applies to current patch also */
            memcpy(instance->current_patch_buffer, instance->overlay_patch_buffer,
                   DX7_VOICE_SIZE_UNPACKED);
        }
    }

    pthread_mutex_unlock(&instance->patches_mutex);

    return NULL; /* success */
}

char *
prdx7_instance_handle_performance(prdx7_instance_t *instance,
                                  const char *value) {
    pthread_mutex_lock(&instance->patches_mutex);

    DEBUG_MESSAGE(DB_DATA,
                  " prdx7_instance_handle_performance: received new global performance parameters\n");

    if (!decode_7in6(value, DX7_PERFORMANCE_SIZE, instance->performance_buffer)) {
        pthread_mutex_unlock(&instance->patches_mutex);
        return dssp_error_message("performance edit failed: corrupt data");
    }

    prdx7_instance_set_performance_data(instance);

    pthread_mutex_unlock(&instance->patches_mutex);

    /* we eventually may want to update playing voices here */

    return NULL; /* success */
}

/*
 * prdx7_instance_handle_monophonic
 */
char *
prdx7_instance_handle_monophonic(prdx7_instance_t *instance, const char *value) {
    int32_t mode = -1;

    if (!strcmp(value, "on")) mode = DSSP_MONO_MODE_ON;
    else if (!strcmp(value, "once")) mode = DSSP_MONO_MODE_ONCE;
    else if (!strcmp(value, "both")) mode = DSSP_MONO_MODE_BOTH;
    else if (!strcmp(value, "off")) mode = DSSP_MONO_MODE_OFF;

    if (mode == -1) {
        return dssp_error_message("error: monophonic value not recognized");
    }

    if (mode == DSSP_MONO_MODE_OFF) {  /* polyphonic mode */

        instance->monophonic = 0;
        instance->max_voices = instance->polyphony;

    } else {  /* one of the monophonic modes */

        if (!instance->monophonic) {

            dssp_voicelist_mutex_lock();

            prdx7_instance_all_voices_off(instance);
            instance->max_voices = 1;
            instance->mono_voice = NULL;
            prdx7_instance_clear_held_keys(instance);
            dssp_voicelist_mutex_unlock();
        }
        instance->monophonic = mode;
    }

    return NULL; /* success */
}

/*
 * prdx7_instance_handle_polyphony
 */
char *
prdx7_instance_handle_polyphony(prdx7_instance_t *instance, const char *value) {
    int32_t polyphony = atoi(value);

    if (polyphony < 1 || polyphony > PRDX7_MAX_POLYPHONY) {
        return dssp_error_message("error: polyphony value out of range");
    }
    /* set the new limit */
    instance->polyphony = polyphony;

    if (!instance->monophonic) {

        dssp_voicelist_mutex_lock();

        instance->max_voices = polyphony;

        /* turn off any voices above the new limit */
        for (int32_t i = 0; instance->current_voices > instance->max_voices &&
                        i < prdx7_synth().global_polyphony; i++) {
            dx7_voice_t &voice = prdx7_synth().voice[i];
            if (voice.instance == instance && _PLAYING(voice)) {
                if (voice.instance->held_keys[0] != -1)
                    prdx7_instance_clear_held_keys(voice.instance);
                dx7_voice_off(voice);
            }
        }

        dssp_voicelist_mutex_unlock();
    }

    return NULL; /* success */
}

/*
 * prdx7_synth_handle_global_polyphony
 */
char *
prdx7_synth_handle_global_polyphony(const char *value) {
    int32_t polyphony = atoi(value);
    if (polyphony < 1 || polyphony > PRDX7_MAX_POLYPHONY) {
        return dssp_error_message("error: polyphony value out of range");
    }

    dssp_voicelist_mutex_lock();

    /* set the new limit */
    prdx7_synth().global_polyphony = polyphony;

    /* turn off any voices above the new limit */
    for (int32_t i = polyphony; i < PRDX7_MAX_POLYPHONY; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (_PLAYING(voice)) {
            if (voice.instance->held_keys[0] != -1)
                prdx7_instance_clear_held_keys(voice.instance);
            dx7_voice_off(voice);
        }
    }

    dssp_voicelist_mutex_unlock();

    return NULL; /* success */
}

/*
 * prdx7_synth_render_voices
 */
void
prdx7_synth_render_voices(float *out,
                          unsigned long sample_count, int32_t do_control_update) {
    prdx7_instance_t *instance;
    /* update each LFO */

    for (instance = prdx7_synth().instances; instance;
         instance = instance->next) {
        dx7_lfo_update(instance, sample_count);
    }

    /* render each active voice */
    for (int32_t i = 0; i < prdx7_synth().global_polyphony; i++) {
        dx7_voice_t &voice = prdx7_synth().voice[i];
        if (_PLAYING(voice)) {
            if (voice.mods_serial != voice.instance->mods_serial) {
                dx7_voice_update_mod_depths(voice.instance, voice);
                voice.mods_serial = voice.instance->mods_serial;
            }
            dx7_voice_render(voice.instance, voice,
                             out,
                             sample_count, do_control_update);

        }
    }
}

// optional:
//    void (*run_synth)(LADSPA_Handle    Instance,
//                      unsigned long    SampleCount,
//                      snd_seq_event_t *Events,
//                      unsigned long    EventCount);
//    void (*run_synth_adding)(LADSPA_Handle    Instance,
//                             unsigned long    SampleCount,
//                             snd_seq_event_t *Events,
//                             unsigned long    EventCount);


/*
 * dx7_voice_set_phase
 */
static inline void
dx7_voice_set_phase(prdx7_instance_t *instance, dx7_voice_t &voice, int32_t phase) {
    for (int32_t i = 0; i < MAX_DX7_OPERATORS; i++) {
        dx7_op_eg_set_phase(instance, voice.op[i].eg, phase);
    }
    dx7_pitch_eg_set_phase(instance, voice.pitch_eg, phase);
}

/*
 * dx7_voice_set_release_phase
 */
static inline void
dx7_voice_set_release_phase(prdx7_instance_t *instance, dx7_voice_t &voice) {
    dx7_voice_set_phase(instance, voice, 3);
}

/*
 * dx7_voice_note_on
 */
static void
dx7_voice_note_on(prdx7_instance_t *instance, dx7_voice_t &voice,
                  unsigned char key, unsigned char velocity) {
    int32_t i;

    voice.key = key;
    voice.velocity = velocity;

    if (!instance->monophonic || !(_ON(voice) || _SUSTAINED(voice))) {

        /* brand-new voice, or monophonic voice in release phase; set
         * everything up */
        DEBUG_MESSAGE(DB_NOTE,
                      " dx7_voice_note_on in polyphonic/new section: key %d, mono %d, old status %d\n",
                      key, instance->monophonic, voice.status);

        dx7_voice_setup_note(instance, voice);

    } else {

        /* synth is monophonic, and we're modifying a playing voice */
        DEBUG_MESSAGE(DB_NOTE,
                      " dx7_voice_note_on in monophonic section: old key %d => new key %d\n",
                      instance->held_keys[0], key);

        /* retrigger LFO if needed */
        dx7_lfo_set(instance, voice);

        /* set new pitch */
        voice.mods_serial = instance->mods_serial - 1;
        /* -FIX- dx7_portamento_prepare(instance, voice); */
        dx7_voice_recalculate_freq_and_inc(instance, voice);

        /* if in 'on' or 'both' modes, and key has changed, then re-trigger EGs */
        if ((instance->monophonic == DSSP_MONO_MODE_ON ||
             instance->monophonic == DSSP_MONO_MODE_BOTH) &&
            (instance->held_keys[0] < 0 || instance->held_keys[0] != key)) {
            dx7_voice_set_phase(instance, voice, 0);
        }

        /* all other variables stay what they are */
    }

    instance->last_key = key;

    if (instance->monophonic) {

        /* add new key to the list of held keys */

        /* check if new key is already in the list; if so, move it to the
         * top of the list, otherwise shift the other keys down and add it
         * to the top of the list. */
        // DEBUG_MESSAGE(DB_NOTE, " note-on key list before: %d %d %d %d %d %d %d %d\n", instance->held_keys[0], instance->held_keys[1], instance->held_keys[2], instance->held_keys[3], instance->held_keys[4], instance->held_keys[5], instance->held_keys[6], instance->held_keys[7]);
        for (i = 0; i < 7; i++) {
            if (instance->held_keys[i] == key)
                break;
        }
        for (; i > 0; i--) {
            instance->held_keys[i] = instance->held_keys[i - 1];
        }
        instance->held_keys[0] = key;
        // DEBUG_MESSAGE(DB_NOTE, " note-on key list after: %d %d %d %d %d %d %d %d\n", instance->held_keys[0], instance->held_keys[1], instance->held_keys[2], instance->held_keys[3], instance->held_keys[4], instance->held_keys[5], instance->held_keys[6], instance->held_keys[7]);

    }

    if (!_PLAYING(voice)) {

        dx7_voice_start_voice(voice);

    } else if (!_ON(voice)) {  /* must be DX7_VOICE_SUSTAINED or DX7_VOICE_RELEASED */

        voice.status = DX7_VOICE_ON;

    }
}

/*
 * dx7_voice_note_off
 */
static void
dx7_voice_note_off(prdx7_instance_t *instance, dx7_voice_t &voice,
                   unsigned char key, unsigned char rvelocity) {
    DEBUG_MESSAGE(DB_NOTE, " dx7_voice_note_off: called for voice %p, key %d", voice, key);

    /* save release velocity */
    voice.rvelocity = rvelocity;

    if (instance->monophonic) {  /* monophonic mode */

        if (instance->held_keys[0] >= 0) {  /* still some keys held */

            if (voice.key != instance->held_keys[0]) {

                /* most-recently-played key has changed */
                voice.key = instance->held_keys[0];
                DEBUG_MESSAGE(DB_NOTE, " note-off in monophonic section: changing pitch to %d\n",
                              voice.key);
                voice.mods_serial = instance->mods_serial - 1;
                /* -FIX- dx7_portamento_prepare(instance, voice); */
                dx7_voice_recalculate_freq_and_inc(instance, voice);

                /* if mono mode is 'both', re-trigger EGs */
                if (instance->monophonic == DSSP_MONO_MODE_BOTH && !_RELEASED(voice)) {
                    dx7_voice_set_phase(instance, voice, 0);
                }
            }

        } else {  /* no keys still held */

            if (PRDX7_INSTANCE_SUSTAINED(instance)) {

                /* no more keys in list, but we're sustained */
                DEBUG_MESSAGE(DB_NOTE,
                              " note-off in monophonic section: sustained with no held keys\n");
                if (!_RELEASED(voice))
                    voice.status = DX7_VOICE_SUSTAINED;

            } else {  /* not sustained */

                /* no more keys in list, so turn off note */
                DEBUG_MESSAGE(DB_NOTE, " note-off in monophonic section: turning off voice %p\n",
                              voice);
                dx7_voice_set_release_phase(instance, voice);
                voice.status = DX7_VOICE_RELEASED;

            }
        }

    } else {  /* polyphonic mode */

        if (PRDX7_INSTANCE_SUSTAINED(instance)) {

            if (!_RELEASED(voice))
                voice.status = DX7_VOICE_SUSTAINED;

        } else {  /* not sustained */

            dx7_voice_set_release_phase(instance, voice);
            voice.status = DX7_VOICE_RELEASED;

        }
    }
}

/*
 * dx7_voice_release_note
 */
static void
dx7_voice_release_note(prdx7_instance_t *instance, dx7_voice_t &voice) {
    DEBUG_MESSAGE(DB_NOTE, " dx7_voice_release_note: turning off voice %p\n", voice);
    if (_ON(voice)) {
        /* dummy up a release velocity */
        voice.rvelocity = 64;
    }
    dx7_voice_set_release_phase(instance, voice);
    voice.status = DX7_VOICE_RELEASED;
}

/* ===== operator (amplitude) envelope functions ===== */

#ifdef PRDX7_DEBUG_ENGINE
#define PRDX7_DEBUG_ENGINE_SLEW_CHECK(inc, msg) \
    if (FP_ABS(inc) >= instance->dx7_eg_max_slew) printf(msg "\n");
#else
#define PRDX7_DEBUG_ENGINE_SLEW_CHECK(inc, msg)
#endif

/*
 * dx7_op_eg_set_increment
 */
static void
dx7_op_eg_set_increment(prdx7_instance_t *instance, dx7_op_eg_t &eg,
                        int32_t new_rate, int new_level) {
    int32_t current_level = FP_TO_INT(eg.value);
    int32_t need_compensation;
    float duration;

    eg.target = INT_TO_FP(new_level);

    if (eg.value <= eg.target) {  /* envelope will be rising */

        /* DX7 envelopes, when rising from levels <= 31 to levels
         * >= 32, include a compensation feature to speed the
         * attack, thereby making it sound more natural.  The
         * behavior of some of the boundary cases is bizarre, and
         * this has been exploited by some patch programmers (the
         * "Watergarden" patch found in the original ROM cartridge
         * is one example). We try to emulate it here: */

        if (eg.value <= INT_TO_FP(31)) {
            if (new_level > 31) {
                /* rise quickly to 31, then continue normally */
                need_compensation = 1;
                duration = dx7_voice_eg_rate_rise_duration[new_rate] *
                           (dx7_voice_eg_rate_rise_percent[new_level] -
                            dx7_voice_eg_rate_rise_percent[current_level]);
            } else if (new_level - current_level > 9) {
                /* these seem to take zero time */
                need_compensation = 0;
                duration = 0.0f;
            } else {
                /* these are the exploited delays */
                need_compensation = 0;
                /* -FIX- this doesn't make WATER GDN work? */
                duration = dx7_voice_eg_rate_rise_duration[new_rate] *
                           (float) (new_level - current_level) / 100.0f;
            }
        } else {
            need_compensation = 0;
            duration = dx7_voice_eg_rate_rise_duration[new_rate] *
                       (dx7_voice_eg_rate_rise_percent[new_level] -
                        dx7_voice_eg_rate_rise_percent[current_level]);
        }

    } else {

        need_compensation = 0;
        duration = dx7_voice_eg_rate_decay_duration[new_rate] *
                   (dx7_voice_eg_rate_decay_percent[current_level] -
                    dx7_voice_eg_rate_decay_percent[new_level]);

    }

    duration *= instance->sample_rate;

    eg.duration = lrintf(duration);
    if (eg.duration < 1)
        eg.duration = 1;

    if (need_compensation) {

        int32_t precomp_duration = FP_DIVIDE_CEIL(INT_TO_FP(31) - eg.value,
                                                  instance->dx7_eg_max_slew);

        if (precomp_duration >= eg.duration) {

            eg.duration = precomp_duration;
            eg.increment = (eg.target - eg.value) / (dx7_sample_t) eg.duration;
            if (eg.increment > instance->dx7_eg_max_slew) {
                eg.duration = FP_DIVIDE_CEIL(eg.target - eg.value, instance->dx7_eg_max_slew);
                eg.increment = (eg.target - eg.value) / (dx7_sample_t) eg.duration;
            }
            PRDX7_DEBUG_ENGINE_SLEW_CHECK(eg.increment, "slew violation 0");
            eg.in_precomp = 0;

        } else if (precomp_duration < 1) {

            eg.increment = (eg.target - eg.value) / (dx7_sample_t) eg.duration;
            if (eg.increment > instance->dx7_eg_max_slew) {
                eg.duration = FP_DIVIDE_CEIL(eg.target - eg.value, instance->dx7_eg_max_slew);
                eg.increment = (eg.target - eg.value) / (dx7_sample_t) eg.duration;
            }
            PRDX7_DEBUG_ENGINE_SLEW_CHECK(eg.increment, "slew violation 1");
            eg.in_precomp = 0;

        } else {

            eg.postcomp_duration = eg.duration - precomp_duration;
            eg.duration = precomp_duration;
            eg.increment = (INT_TO_FP(31) - eg.value) / (dx7_sample_t) precomp_duration;
            PRDX7_DEBUG_ENGINE_SLEW_CHECK(eg.increment, "slew violation Pa");
            eg.postcomp_increment = (eg.target - INT_TO_FP(31)) /
                                    (dx7_sample_t) eg.postcomp_duration;
            if (eg.postcomp_increment > instance->dx7_eg_max_slew) {
                eg.postcomp_duration = FP_DIVIDE_CEIL(eg.target - INT_TO_FP(31),
                                                      instance->dx7_eg_max_slew);
                eg.postcomp_increment = (eg.target - INT_TO_FP(31)) /
                                        (dx7_sample_t) eg.postcomp_duration;
            }
            PRDX7_DEBUG_ENGINE_SLEW_CHECK(eg.postcomp_increment, "slew violation Pb");
            eg.in_precomp = 1;

        }

    } else {

        eg.increment = (eg.target - eg.value) / (dx7_sample_t) eg.duration;
        if (FP_ABS(eg.increment) > instance->dx7_eg_max_slew) {
            eg.duration = FP_DIVIDE_CEIL(FP_ABS(eg.target - eg.value), instance->dx7_eg_max_slew);
            eg.increment = (eg.target - eg.value) / (dx7_sample_t) eg.duration;
        }
        PRDX7_DEBUG_ENGINE_SLEW_CHECK(eg.increment, "slew violation 2");
        eg.in_precomp = 0;

    }
#ifdef PRDX7_DEBUG_ENGINE
    if (eg.duration <= 0 || FP_ABS(eg.increment) >= instance->dx7_eg_max_slew)
#ifndef PRDX7_USE_FLOATING_POINT
        printf("eg error: rate %d, current %f, new %d, duration %d (%f), increment %d, in_precomp %d, postcomp_dur %d, postcomp_inc %d\n",
               new_rate, FP_TO_DOUBLE(eg.value), new_level, eg.duration, duration,
#else /* PRDX7_USE_FLOATING_POINT */
        printf("eg error: rate %d, current %f, new %d, duration %d (%f), increment %f, in_precomp %d, postcomp_dur %d, postcomp_inc %f\n",
               new_rate, eg.value, new_level, eg.duration, duration,
#endif /* PRDX7_USE_FLOATING_POINT */
               eg.increment, eg.in_precomp, eg.postcomp_duration, eg.postcomp_increment);
#endif /* PRDX7_DEBUG_ENGINE */
}

/*
 * dx7_op_eg_set_next_phase
 *
 * assumes a DX7_EG_RUNNING envelope
 */
static void
dx7_op_eg_set_next_phase(prdx7_instance_t *instance, dx7_op_eg_t &eg) {
    switch (eg.phase) {

        case 0:
        case 1:
            eg.phase++;
            dx7_op_eg_set_increment(instance, eg, eg.rate[eg.phase], eg.level[eg.phase]);
#ifndef PRDX7_USE_FLOATING_POINT
        if (eg.duration == 1 && eg.increment == 0)
                dx7_op_eg_set_next_phase(instance, eg);
#ifdef PRDX7_DEBUG_ENGINE
        if (eg.mode == DX7_EG_RUNNING && eg.duration == 1 && eg.increment == 0)
            printf("eg error: phase %d, current %d (%f), new %d (%f), duration %d, increment %d, in_precomp %d, postcomp_dur %d, postcomp_inc %d\n",
                   eg.phase, eg.value, FP_TO_DOUBLE(eg.value), eg.target, FP_TO_DOUBLE(eg.target), eg.duration,
                   eg.increment, eg.in_precomp, eg.postcomp_duration, eg.postcomp_increment);
#endif /* PRDX7_DEBUG_ENGINE */
#else /* PRDX7_USE_FLOATING_POINT */
            if (eg.duration == 1 && fabsf(eg.increment) < 1e-10f)
                dx7_op_eg_set_next_phase(instance, eg);
#ifdef PRDX7_DEBUG_ENGINE
        if (eg.mode == DX7_EG_RUNNING && eg.duration == 1 && eg.increment == 0)
            printf("eg error: phase %d, current %f, new %f, duration %d, increment %f, in_precomp %d, postcomp_dur %d, postcomp_inc %f\n",
                   eg.phase, eg.value, eg.target, eg.duration,
                   eg.increment, eg.in_precomp, eg.postcomp_duration, eg.postcomp_increment);
#endif /* PRDX7_DEBUG_ENGINE */
#endif /* PRDX7_USE_FLOATING_POINT */
            break;

        case 2:
            eg.mode = DX7_EG_SUSTAINING;
            eg.increment = INT_TO_FP(0);
            eg.duration = -1;
            break;

        case 3:
        default: /* shouldn't be anything but 0 to 3 */
            eg.mode = DX7_EG_FINISHED;
            eg.increment = INT_TO_FP(0);
            eg.duration = -1;
            break;

    }
}

static void
dx7_op_eg_set_phase(prdx7_instance_t *instance, dx7_op_eg_t &eg, int32_t phase) {
    eg.phase = phase;

    if (phase == 0) {

        if (eg.level[0] == eg.level[1] &&
            eg.level[1] == eg.level[2] &&
            eg.level[2] == eg.level[3]) {

            eg.mode = DX7_EG_CONSTANT;
            eg.value = INT_TO_FP(eg.level[3]);
            eg.increment = INT_TO_FP(0);
            eg.duration = -1;

        } else {

            eg.mode = DX7_EG_RUNNING;
            dx7_op_eg_set_increment(instance, eg, eg.rate[phase], eg.level[phase]);
#ifndef PRDX7_USE_FLOATING_POINT
            if (eg.duration == 1 && eg.increment == 0)
                dx7_op_eg_set_next_phase(instance, eg);
#ifdef PRDX7_DEBUG_ENGINE
            if (eg.mode == DX7_EG_RUNNING && eg.duration == 1 && eg.increment == 0)
                printf("eg error: phase %d, current %d (%f), new %d (%f), duration %d, increment %d, in_precomp %d, postcomp_dur %d, postcomp_inc %d\n",
                       eg.phase, eg.value, FP_TO_DOUBLE(eg.value), eg.target, FP_TO_DOUBLE(eg.target), eg.duration,
                       eg.increment, eg.in_precomp, eg.postcomp_duration, eg.postcomp_increment);
#endif /* PRDX7_DEBUG_ENGINE */
#else /* PRDX7_USE_FLOATING_POINT */
            if (eg.duration == 1 && fabsf(eg.increment) < 1e-10f)
                dx7_op_eg_set_next_phase(instance, eg);
#ifdef PRDX7_DEBUG_ENGINE
            if (eg.mode == DX7_EG_RUNNING && eg.duration == 1 && eg.increment == 0)
                printf("eg error: phase %d, current %f, new %f, duration %d, increment %f, in_precomp %d, postcomp_dur %d, postcomp_inc %f\n",
                       eg.phase, eg.value, eg.target, eg.duration,
                       eg.increment, eg.in_precomp, eg.postcomp_duration, eg.postcomp_increment);
#endif /* PRDX7_DEBUG_ENGINE */
#endif /* PRDX7_USE_FLOATING_POINT */

        }
    } else {

        if (eg.mode != DX7_EG_CONSTANT) {

            eg.mode = DX7_EG_RUNNING;
            dx7_op_eg_set_increment(instance, eg, eg.rate[phase], eg.level[phase]);
#ifndef PRDX7_USE_FLOATING_POINT
            if (eg.duration == 1 && eg.increment == 0)
                dx7_op_eg_set_next_phase(instance, eg);
#ifdef PRDX7_DEBUG_ENGINE
            if (eg.mode == DX7_EG_RUNNING && eg.duration == 1 && eg.increment == 0)
                printf("eg error: phase %d, current %d (%f), new %d (%f), duration %d, increment %d, in_precomp %d, postcomp_dur %d, postcomp_inc %d\n",
                       eg.phase, eg.value, FP_TO_DOUBLE(eg.value), eg.target, FP_TO_DOUBLE(eg.target), eg.duration,
                       eg.increment, eg.in_precomp, eg.postcomp_duration, eg.postcomp_increment);
#endif /* PRDX7_DEBUG_ENGINE */
#else /* PRDX7_USE_FLOATING_POINT */
            if (eg.duration == 1 && fabsf(eg.increment) < 1e-10f)
                dx7_op_eg_set_next_phase(instance, eg);
#ifdef PRDX7_DEBUG_ENGINE
            if (eg.mode == DX7_EG_RUNNING && eg.duration == 1 && eg.increment == 0)
                printf("eg error: phase %d, current %f, new %f, duration %d, increment %f, in_precomp %d, postcomp_dur %d, postcomp_inc %f\n",
                       eg.phase, eg.value, eg.target, eg.duration,
                       eg.increment, eg.in_precomp, eg.postcomp_duration, eg.postcomp_increment);
#endif /* PRDX7_DEBUG_ENGINE */
#endif /* PRDX7_USE_FLOATING_POINT */

        }
    }
}

static void
dx7_op_envelope_prepare(prdx7_instance_t *instance, dx7_op_t &op,
                        int32_t transposed_note, int velocity) {
    int32_t scaled_output_level, i, rate_bump;
    float vel_adj;

    scaled_output_level = op.output_level;

    /* things that affect breakpoint calculations: transpose, ? */
    /* things that don't affect breakpoint calculations: pitch envelope, ? */

    if (transposed_note < op.level_scaling_bkpoint + 21 && op.level_scaling_l_depth) {

        /* On the original DX7/TX7, keyboard level scaling calculations
         * group the keyboard into groups of three keys.  This can be quite
         * noticeable on patches with extreme scaling depths, so I've tried
         * to replicate it here (the steps between levels may not occur at
         * exactly the keys).  If you'd prefer smother scaling, define
         * SMOOTH_KEYBOARD_LEVEL_SCALING. */
#ifndef SMOOTH_KEYBOARD_LEVEL_SCALING
        i = op.level_scaling_bkpoint - (((transposed_note + 2) / 3) * 3) + 21;
#else
        i = op.level_scaling_bkpoint - transposed_note + 21;
#endif

        switch (op.level_scaling_l_curve) {
            case 0: /* -LIN */
                scaled_output_level -= (int) ((float) i / 45.0f * (float) op.level_scaling_l_depth);
                break;
            case 1: /* -EXP */
                scaled_output_level -= (int) (exp((float) (i - 72) / 13.5f) *
                                              (float) op.level_scaling_l_depth);
                break;
            case 2: /* +EXP */
                scaled_output_level += (int) (exp((float) (i - 72) / 13.5f) *
                                              (float) op.level_scaling_l_depth);
                break;
            case 3: /* +LIN */
                scaled_output_level += (int) ((float) i / 45.0f * (float) op.level_scaling_l_depth);
                break;
        }
        if (scaled_output_level < 0) scaled_output_level = 0;
        if (scaled_output_level > 99) scaled_output_level = 99;

    } else if (transposed_note > op.level_scaling_bkpoint + 21 && op.level_scaling_r_depth) {

#ifndef SMOOTH_KEYBOARD_LEVEL_SCALING
        i = (((transposed_note + 2) / 3) * 3) - op.level_scaling_bkpoint - 21;
#else
        i = transposed_note - op.level_scaling_bkpoint - 21;
#endif

        switch (op.level_scaling_r_curve) {
            case 0: /* -LIN */
                scaled_output_level -= (int) ((float) i / 45.0f * (float) op.level_scaling_r_depth);
                break;
            case 1: /* -EXP */
                scaled_output_level -= (int) (exp((float) (i - 72) / 13.5f) *
                                              (float) op.level_scaling_r_depth);
                break;
            case 2: /* +EXP */
                scaled_output_level += (int) (exp((float) (i - 72) / 13.5f) *
                                              (float) op.level_scaling_r_depth);
                break;
            case 3: /* +LIN */
                scaled_output_level += (int) ((float) i / 45.0f * (float) op.level_scaling_r_depth);
                break;
        }
        if (scaled_output_level < 0) scaled_output_level = 0;
        if (scaled_output_level > 99) scaled_output_level = 99;
    }

    vel_adj = dx7_voice_velocity_ol_adjustment[velocity] * (float) op.velocity_sens;

    /* DEBUG_MESSAGE(DB_NOTE, " dx7_op_envelope_prepare: s_o_l=%d, vel_adj=%f\n", scaled_output_level, vel_adj); */

    /* -FIX- This calculation comes from Pinkston/Harrington; the original "* 6.0" scaling factor
     * was close to what my TX7 does, but tended to not bump the rate as much, so I changed it
     * to "* 6.5" which seems a little closer, but it's still not spot-on. */
    /* Things which affect this calculation: transpose, ? */
    /* rate_bump = lrintf((float)op.rate_scaling * (float)(transposed_note - 21) / (126.0f - 21.0f) * 127.0f / 128.0f * 6.0f - 0.5f); */
    rate_bump = lrintf(
            (float) op.rate_scaling * (float) (transposed_note - 21) / (126.0f - 21.0f) * 127.0f /
            128.0f * 6.5f - 0.5f);
    /* -FIX- just a hunch: try it again with "* 6.0f" but also "(120.0f - 21.0f)" instead of "(126.0f - 21.0f)": */
    /* rate_bump = lrintf((float)op.rate_scaling * (float)(transposed_note - 21) / (120.0f - 21.0f) * 127.0f / 128.0f * 6.0f - 0.5f); */

    for (i = 0; i < 4; i++) {

        float level = (float) op.eg.base_level[i];

        /* -FIX- is this scaling of eg.base_level values to og.level values correct, i.e. does a softer
         * velocity shorten the time, since the rate stays the same? */
        level = level * (float) scaled_output_level / 99.0f + vel_adj;
        if (level < 0.0f)
            level = 0.0f;
        else if (level > 99.0f)
            level = 99.0f;

        op.eg.level[i] = lrintf(level);

        op.eg.rate[i] = op.eg.base_rate[i] + rate_bump;
        if (op.eg.rate[i] > 99) op.eg.rate[i] = 99;

#ifdef PRDX7_DEBUG_ENGINE
        /* printf("  rate[%d]=%d, level[%d]=%d (output_level=%d, rate_bump=%d)\n", i, op.eg.rate[i], i, op.eg.level[i], op.output_level, rate_bump); */
#endif
    }

    op.eg.value = INT_TO_FP(op.eg.level[3]);

    dx7_op_eg_set_phase(instance, op.eg, 0);
}

static void
dx7_eg_init_constants(prdx7_instance_t *instance) {
    float duration = dx7_voice_eg_rate_rise_duration[99] *
                     (dx7_voice_eg_rate_rise_percent[99] -
                      dx7_voice_eg_rate_rise_percent[0]);

    instance->dx7_eg_max_slew = FLOAT_TO_FP(99.0f / (duration * instance->sample_rate));

    instance->nugget_rate = instance->sample_rate / (float) PRDX7_NUGGET_SIZE;

    instance->ramp_duration = lrintf(instance->sample_rate * 0.006f);  /* 6ms ramp */
}

/* ===== pitch envelope functions ===== */

/*
 * dx7_pitch_eg_set_increment
 */
static void
dx7_pitch_eg_set_increment(prdx7_instance_t *instance, dx7_pitch_eg_t &eg,
                           int32_t new_rate, int new_level) {
    double duration;

    /* translate 0-99 level to shift in semitones */
    eg.target = dx7_voice_pitch_level_to_shift[new_level];

    /* -FIX- This is just a quick approximation that I derived from
     * regression of Godric Wilkie's pitch eg timings. In particular,
     * it's not accurate for very slow envelopes. */
    duration = exp(((double) new_rate - 70.337897) / -25.580953) *
               fabs((eg.target - eg.value) / 96.0);

    duration *= (double) instance->nugget_rate;

    eg.duration = lrint(duration);

    if (eg.duration > 1) {

        eg.increment = (eg.target - eg.value) / (dx7_sample_t) eg.duration;

    } else {

        eg.duration = 1;
        eg.increment = eg.target - eg.value;

    }
#ifdef PRDX7_DEBUG_ENGINE
    if (fabs(eg.increment) < 64.0 && eg.duration != 1)
        printf("pitch eg: rate = %d, current = %f, target = %f, duration = %f => %d, increment = %f\n",
               new_rate, eg.value, eg.target, duration, eg.duration, eg.increment);
#endif
}

/*
 * dx7_pitch_eg_set_next_phase
 *
 * assumes a DX7_EG_RUNNING envelope
 */
static void
dx7_pitch_eg_set_next_phase(prdx7_instance_t *instance, dx7_pitch_eg_t &eg) {
    switch (eg.phase) {

        case 0:
        case 1:
            eg.phase++;
            dx7_pitch_eg_set_increment(instance, eg, eg.rate[eg.phase],
                                       eg.level[eg.phase]);
            break;

        case 2:
            eg.mode = DX7_EG_SUSTAINING;
            break;

        case 3:
        default: /* shouldn't be anything but 0 to 3 */
            eg.mode = DX7_EG_FINISHED;
            break;

    }
}

static void
dx7_pitch_eg_set_phase(prdx7_instance_t *instance, dx7_pitch_eg_t &eg, int32_t phase) {
    eg.phase = phase;

    if (phase == 0) {

        if (eg.level[0] == eg.level[1] &&
            eg.level[1] == eg.level[2] &&
            eg.level[2] == eg.level[3]) {

            eg.mode = DX7_EG_CONSTANT;
            eg.value = dx7_voice_pitch_level_to_shift[eg.level[3]];

        } else {

            eg.mode = DX7_EG_RUNNING;
            dx7_pitch_eg_set_increment(instance, eg, eg.rate[phase], eg.level[phase]);

        }
    } else {

        if (eg.mode != DX7_EG_CONSTANT) {

            eg.mode = DX7_EG_RUNNING;
            dx7_pitch_eg_set_increment(instance, eg, eg.rate[phase], eg.level[phase]);

        }
    }
}

static void
dx7_pitch_envelope_prepare(prdx7_instance_t *instance, dx7_voice_t &voice) {
    voice.pitch_eg.value = dx7_voice_pitch_level_to_shift[voice.pitch_eg.level[3]];
    dx7_pitch_eg_set_phase(instance, voice.pitch_eg, 0);
}

/* ===== portamento functions ===== */

static void
dx7_portamento_set_segment(prdx7_instance_t *instance, dx7_portamento_t &port) {
    /* -FIX- implement portamento multi-segment curve */
    port.increment = (port.target - port.value) / (double) port.duration;
}

static void
dx7_portamento_prepare(prdx7_instance_t *instance, dx7_voice_t &voice) {
    dx7_portamento_t &port = voice.portamento;

    if (instance->portamento_time == 0 ||
        instance->last_key == voice.key) {

        port.segment = 0;
        port.value = 0.0;

    } else {

        /* -FIX- implement portamento time and multi-segment curve */
        float t = expf((float) (instance->portamento_time - 99) / 15.0f) *
                  18.0f; /* not at all related to what a real DX7 does */
        port.segment = 1;
        port.value = (double) (instance->last_key - voice.key);
        port.duration = lrintf(instance->nugget_rate * t);
        port.target = 0.0;

        dx7_portamento_set_segment(instance, port);
    }
}

/* ===== frequency related functions ===== */

static inline int
limit_note(int32_t note) {
    while (note < 0) note += 12;
    while (note > 127) note -= 12;
    return note;
}

static void
dx7_op_recalculate_increment(prdx7_instance_t *instance, dx7_op_t &op) {
    double freq;

    if (op.osc_mode) { /* fixed frequency */
        /* pitch envelope does not affect this */

        /* -FIX- convert this to a table lookup for speed? */
        freq = instance->fixed_freq_multiplier *
               exp(M_LN10 * ((double) (op.coarse & 3) + (double) op.fine / 100.0));
        /* -FIX- figure out what to do with detune */

    } else {

        freq = op.frequency;
        freq += ((double) op.detune - 7.0) / 32.0; /* -FIX- is this correct? */
        if (op.coarse) {
            freq = freq * (double) op.coarse;
        } else {
            freq = freq / 2.0;
        }
        freq *= (1.0 + ((double) op.fine / 100.0));

    }
    op.phase_increment = DOUBLE_TO_FP(freq / (double) instance->sample_rate);
#ifdef PRDX7_DEBUG_ENGINE
    #ifndef PRDX7_USE_FLOATING_POINT
    /* printf("freq=%10.6f, detune=%d, coarse=%d, fine=%d, phase_increment=%d\n", op.frequency, op.detune, */
#else /* PRDX7_USE_FLOATING_POINT */
    /* printf("freq=%10.6f, detune=%d, coarse=%d, fine=%d, phase_increment=%g\n", op.frequency, op.detune, */
#endif /* PRDX7_USE_FLOATING_POINT */
    /*        op.coarse, op.fine, op.phase_increment); */
#endif /* PRDX7_DEBUG_ENGINE */
}

static double
dx7_voice_recalculate_frequency(prdx7_instance_t *instance, dx7_voice_t &voice) {
    double freq;

    voice.last_port_tuning = instance->tuning;

    instance->fixed_freq_multiplier = instance->tuning / 440.0;

    freq = voice.pitch_eg.value + voice.portamento.value +
           instance->pitch_bend -
           instance->lfo_value_for_pitch *
           (voice.pitch_mod_depth_pmd * FP_TO_DOUBLE(voice.lfo_delay_value) +
            voice.pitch_mod_depth_mods);

    voice.last_pitch = freq;

    freq += (double) (limit_note(voice.key + voice.transpose - 24));

    /* -FIX- this maybe could be optimized */
    /*       a lookup table of 8k values would give ~1.5 cent accuracy,
     *       but then would interpolating that be faster than exp()? */
    freq = instance->tuning * exp((freq - 69.0) * M_LN2 / 12.0);

    return freq;
}

static void
dx7_voice_recalculate_freq_and_inc(prdx7_instance_t *instance,
                                   dx7_voice_t &voice) {
    double freq = dx7_voice_recalculate_frequency(instance, voice);
    int32_t i;

    for (i = 0; i < 6; i++) {
        voice.op[i].frequency = freq;
        dx7_op_recalculate_increment(instance, voice.op[i]);
    }
}

/* ===== output volume ===== */

static void
dx7_voice_recalculate_volume(prdx7_instance_t *instance, dx7_voice_t &voice) {
    float f;
    int32_t i;

    voice.last_port_volume = instance->volume;
    voice.last_cc_volume = instance->cc_volume;

    /* This 41 OL volume cc mapping matches my TX7 fairly well, to within
     * +/-0.8dB for most of the scale. (It even duplicates the "feature"
     * of not going completely silent at zero....) */
    f = (instance->volume - 20.0f) * 1.328771f + 86.0f;
    f += (float) instance->cc_volume * 41.0f / 16256.0f;
    i = lrintf(f - 0.5f);
    f -= (float) i;
    voice.volume_target = (FP_TO_FLOAT(dx7_voice_eg_ol_to_mod_index[i]) +
                           f * FP_TO_FLOAT(dx7_voice_eg_ol_to_mod_index[i + 1] -
                                           dx7_voice_eg_ol_to_mod_index[i]))
                          / 2.08855f  /* scale modulation index to output amplitude */
                          /
                          dx7_voice_carrier_count[voice.algorithm]  /* scale for number of carriers */
                          * 0.110384f;  /* Where did this value come from? It approximates the
                                          * -18.1dBFS nominal per-voice output level prdx7 should
                                          * have, but then why didn't I just use 0.125f like in
                                          * prdx7 0.5.7? */

    if (voice.volume_value < 0.0f) { /* initial setup */
        voice.volume_value = voice.volume_target;
        voice.volume_duration = 0;
    } else {
        voice.volume_duration = instance->ramp_duration;
        voice.volume_increment = (voice.volume_target - voice.volume_value) /
                                 (float) voice.volume_duration;
    }
}

/* ===== LFO functions ===== */

/* dx7_lfo_set_speed
 *
 * called by dx7_lfo_reset() and dx7_lfo_set() to set LFO speed and phase
 */
static inline void
dx7_lfo_set_speed(prdx7_instance_t *instance) {
    int32_t period = lrintf(instance->sample_rate /
                            dx7_voice_lfo_frequency[instance->lfo_speed]);

    switch (instance->lfo_wave) {
        default:
        case 0:  /* triangle */
            instance->lfo_phase = 0;
            instance->lfo_value = INT_TO_FP(0);
            instance->lfo_duration0 = period / 2;
            instance->lfo_duration1 = period - instance->lfo_duration0;
            instance->lfo_increment0 = INT_TO_FP(1) / (dx7_sample_t) instance->lfo_duration0;
            instance->lfo_increment1 = -instance->lfo_increment0;
            instance->lfo_duration = instance->lfo_duration0;
            instance->lfo_increment = instance->lfo_increment0;
            break;
        case 1:  /* saw down */
            instance->lfo_phase = 0;
            instance->lfo_value = INT_TO_FP(0);
            if (period >= (instance->ramp_duration * 4)) {
                instance->lfo_duration0 = period - instance->ramp_duration;
                instance->lfo_duration1 = instance->ramp_duration;
            } else {
                instance->lfo_duration0 = period * 3 / 4;
                instance->lfo_duration1 = period - instance->lfo_duration0;
            }
            instance->lfo_increment0 = INT_TO_FP(1) / (dx7_sample_t) instance->lfo_duration0;
            instance->lfo_increment1 = INT_TO_FP(-1) / (dx7_sample_t) instance->lfo_duration1;
            instance->lfo_duration = instance->lfo_duration0;
            instance->lfo_increment = instance->lfo_increment0;
            break;
        case 2:  /* saw up */
            instance->lfo_phase = 1;
            instance->lfo_value = INT_TO_FP(1);
            if (period >= (instance->ramp_duration * 4)) {
                instance->lfo_duration0 = instance->ramp_duration;
                instance->lfo_duration1 = period - instance->ramp_duration;
            } else {
                instance->lfo_duration1 = period * 3 / 4;
                instance->lfo_duration0 = period - instance->lfo_duration1;
            }
            instance->lfo_increment0 = INT_TO_FP(1) / (dx7_sample_t) instance->lfo_duration0;
            instance->lfo_increment1 = INT_TO_FP(-1) / (dx7_sample_t) instance->lfo_duration1;
            instance->lfo_duration = instance->lfo_duration1;
            instance->lfo_increment = instance->lfo_increment1;
            break;
        case 3:  /* square */
            instance->lfo_phase = 0;
            instance->lfo_value = INT_TO_FP(1);
            if (period >= (instance->ramp_duration * 6)) {
                instance->lfo_duration0 = (period / 2) - instance->ramp_duration;
                instance->lfo_duration1 = instance->ramp_duration;
            } else {
                instance->lfo_duration0 = period / 3;
                instance->lfo_duration1 = (period / 2) - instance->lfo_duration0;
            }
            instance->lfo_increment1 = INT_TO_FP(1) / (dx7_sample_t) instance->lfo_duration1;
            instance->lfo_increment0 = -instance->lfo_increment1;
            instance->lfo_duration = instance->lfo_duration0;
            instance->lfo_increment = INT_TO_FP(0);
            break;
        case 4:  /* sine */
#ifndef PRDX7_USE_FLOATING_POINT
            instance->lfo_value = FP_SIZE / 4; /* phase of pi/2 in cosine table */
#else /* PRDX7_USE_FLOATING_POINT */
            instance->lfo_value = 0.25f;       /* phase of pi/2 in cosine table */
#endif /* PRDX7_USE_FLOATING_POINT */
            instance->lfo_increment = INT_TO_FP(1) / (dx7_sample_t) period;
            break;
        case 5:  /* sample/hold */
            instance->lfo_phase = 0;
            instance->lfo_value = FP_RAND();
            if (period >= (instance->ramp_duration * 4)) {
                instance->lfo_duration0 = period - instance->ramp_duration;
                instance->lfo_duration1 = instance->ramp_duration;
            } else {
                instance->lfo_duration0 = period * 3 / 4;
                instance->lfo_duration1 = period - instance->lfo_duration0;
            }
            instance->lfo_duration = instance->lfo_duration0;
            instance->lfo_increment = INT_TO_FP(0);
            break;
    }
}

#ifdef PRDX7_DEBUG_CONTROL
/* for debug code in prdx7_synth.c */
void dx7_lfo_set_speed_x(prdx7_instance_t *instance) { dx7_lfo_set_speed(instance); }
#endif

/*
 * dx7_lfo_reset
 *
 * called from prdx7_activate() to give instance LFO parameters sane values
 * until they're set by a playing voice
 */
static void
dx7_lfo_reset(prdx7_instance_t *instance) {
    instance->lfo_speed = 20;
    instance->lfo_wave = 1;
    instance->lfo_delay = 255;  /* force setup at first note on */
    instance->lfo_value_for_pitch = 0.0;
    dx7_lfo_set_speed(instance);
}

static void
dx7_lfo_set(prdx7_instance_t *instance, dx7_voice_t &voice) {
    int32_t set_speed = 0;

    instance->lfo_wave = voice.lfo_wave;
    if (instance->lfo_speed != voice.lfo_speed) {
        instance->lfo_speed = voice.lfo_speed;
        set_speed = 1;
    }
    if (voice.lfo_key_sync) {
        set_speed = 1; /* because we need to reset the LFO phase */
    }
    if (set_speed)
        dx7_lfo_set_speed(instance);
    if (instance->lfo_delay != voice.lfo_delay) {
        instance->lfo_delay = voice.lfo_delay;
        if (voice.lfo_delay > 0) {
            instance->lfo_delay_value[0] = INT_TO_FP(0);
            /* -FIX- Jamie's early approximation, replace when he has more data */
            instance->lfo_delay_duration[0] =
                    lrintf(instance->sample_rate *
                           (0.00175338f * pow((float) voice.lfo_delay, 3.10454f) + 169.344f -
                            168.0f) /
                           1000.0f);
            instance->lfo_delay_increment[0] = INT_TO_FP(0);
            instance->lfo_delay_value[1] = INT_TO_FP(0);
            /* -FIX- Jamie's early approximation, replace when he has more data */
            instance->lfo_delay_duration[1] =
                    lrintf(instance->sample_rate *
                           (0.321877f * pow((float) voice.lfo_delay, 2.01163) + 494.201f - 168.0f) /
                           1000.0f);                                                 /* time from note-on until full on */
            instance->lfo_delay_duration[1] -= instance->lfo_delay_duration[0];  /* now time from end-of-delay until full */
            instance->lfo_delay_increment[1] =
                    INT_TO_FP(1) / (dx7_sample_t) instance->lfo_delay_duration[1];
            instance->lfo_delay_value[2] = INT_TO_FP(1);
            instance->lfo_delay_duration[2] = 0;
            instance->lfo_delay_increment[2] = INT_TO_FP(0);
        } else {
            instance->lfo_delay_value[0] = INT_TO_FP(1);
            instance->lfo_delay_duration[0] = 0;
            instance->lfo_delay_increment[0] = INT_TO_FP(0);
        }
        /* -FIX- The TX7 resets the lfo delay for all playing notes at each
         * new note on. We're not doing that yet, and I don't really wanna,
         * 'cause it's stupid.... */
    }
}

static void
dx7_lfo_update(prdx7_instance_t *instance, unsigned long sample_count) {
    unsigned long sample;
    switch (instance->lfo_wave) {
        default:
        case 0:  /* triangle */
        case 1:  /* saw down */
        case 2:  /* saw up */
            for (sample = 0; sample < sample_count; sample++) {
                instance->lfo_buffer[sample] = instance->lfo_value;
                instance->lfo_value += instance->lfo_increment;
                if (!(--instance->lfo_duration)) {
                    if (instance->lfo_phase) {
                        instance->lfo_phase = 0;
                        instance->lfo_value = INT_TO_FP(0);
                        instance->lfo_duration = instance->lfo_duration0;
                        instance->lfo_increment = instance->lfo_increment0;
                    } else {
                        instance->lfo_phase = 1;
                        instance->lfo_value = INT_TO_FP(1);
                        instance->lfo_duration = instance->lfo_duration1;
                        instance->lfo_increment = instance->lfo_increment1;
                    }
                }
            }
            instance->lfo_value_for_pitch = FP_TO_DOUBLE(instance->lfo_value) * 2.0 -
                                            1.0;  /* -FIX- this is still ramped for saw! */
            break;
        case 3:  /* square */
            for (sample = 0; sample < sample_count; sample++) {
                instance->lfo_buffer[sample] = instance->lfo_value;
                instance->lfo_value += instance->lfo_increment;
                if (!(--instance->lfo_duration)) {
                    switch (instance->lfo_phase) {
                        default:
                        case 0:
                            instance->lfo_phase = 1;
                            instance->lfo_duration = instance->lfo_duration1;
                            instance->lfo_increment = instance->lfo_increment0;
                            break;
                        case 1:
                            instance->lfo_phase = 2;
                            instance->lfo_value = INT_TO_FP(0);
                            instance->lfo_duration = instance->lfo_duration0;
                            instance->lfo_increment = INT_TO_FP(0);
                            break;
                        case 2:
                            instance->lfo_phase = 3;
                            instance->lfo_duration = instance->lfo_duration1;
                            instance->lfo_increment = instance->lfo_increment1;
                            break;
                        case 3:
                            instance->lfo_phase = 0;
                            instance->lfo_value = INT_TO_FP(1);
                            instance->lfo_duration = instance->lfo_duration0;
                            instance->lfo_increment = INT_TO_FP(0);
                            break;
                    }
                }
            }
            if (instance->lfo_phase == 0 || instance->lfo_phase == 3)
                instance->lfo_value_for_pitch = 1.0;
            else
                instance->lfo_value_for_pitch = -1.0;
            break;
        case 4:  /* sine */
            for (sample = 0; sample < sample_count; sample++) {
#ifndef PRDX7_USE_FLOATING_POINT
                int32_t phase, index, out;
#else /* PRDX7_USE_FLOATING_POINT */
                int32_t index;
                float phase, frac, out;
#endif /* PRDX7_USE_FLOATING_POINT */

#ifndef PRDX7_USE_FLOATING_POINT
                phase = instance->lfo_value;
                index = (phase >> FP_TO_SINE_SHIFT) & SINE_MASK;
                out = dx7_voice_sin_table[index];
                out += (((int64_t)(dx7_voice_sin_table[index + 1] - out) *
                         (int64_t)(phase & FP_TO_SINE_MASK)) >>
                                                             (FP_SHIFT + FP_TO_SINE_SHIFT));
                out = (out + FP_SIZE) >> 1;  /* shift to unipolar */
#else /* PRDX7_USE_FLOATING_POINT */
                phase = instance->lfo_value * (float) SINE_SIZE;
                index = lrintf(phase - 0.5f);
                frac = phase - (float) index;
                out = dx7_voice_sin_table[index];
                out += (dx7_voice_sin_table[index + 1] - out) * frac;
                out = (out + 1.0f) / 2.0f;  /* shift to unipolar */
#endif /* PRDX7_USE_FLOATING_POINT */
                instance->lfo_buffer[sample] = out;
                instance->lfo_value += instance->lfo_increment;
#ifdef PRDX7_USE_FLOATING_POINT
                if (instance->lfo_value > 1.0f) instance->lfo_value -= 1.0f;
#endif /* PRDX7_USE_FLOATING_POINT */
            }
            instance->lfo_value_for_pitch =
                    FP_TO_DOUBLE(instance->lfo_buffer[sample - 1]) * 2.0 - 1.0;
            break;
        case 5:  /* sample/hold */
            for (sample = 0; sample < sample_count; sample++) {
                instance->lfo_buffer[sample] = instance->lfo_value;
                instance->lfo_value += instance->lfo_increment;
                if (!(--instance->lfo_duration)) {
                    if (instance->lfo_phase) {
                        instance->lfo_phase = 0;
                        instance->lfo_value = instance->lfo_target;
                        instance->lfo_duration = instance->lfo_duration0;
                        instance->lfo_increment = INT_TO_FP(0);
                    } else {
                        instance->lfo_phase = 1;
                        instance->lfo_duration = instance->lfo_duration1;
                        instance->lfo_target = FP_RAND();
                        instance->lfo_increment = (instance->lfo_target - instance->lfo_value) /
                                                  (dx7_sample_t) instance->lfo_duration;
                    }
                }
            }
            instance->lfo_value_for_pitch = FP_TO_DOUBLE(instance->lfo_target) * 2.0 - 1.0;
            break;
    }
}

/* ===== modulation functions ===== */

/* there used to be a dx7_voice_update_pitch_bend() here, but it didn't
 * have anything to do.... */

static void
dx7_voice_update_mod_depths(prdx7_instance_t *instance, dx7_voice_t &voice) {
    unsigned char kp = instance->key_pressure[voice.key];
    unsigned char cp = instance->channel_pressure;
    float pressure;
    float pdepth, adepth, mdepth, edepth;

    /* add the channel and key pressures together in a way that 'feels' good */
    if (kp > cp) {
        pressure = (float) kp / 127.0f;
        pressure += (1.0f - pressure) * ((float) cp / 127.0f);
    } else {
        pressure = (float) cp / 127.0f;
        pressure += (1.0f - pressure) * ((float) kp / 127.0f);
    }

    /* calculate modulation depths */
    pdepth = (float) voice.lfo_pmd / 99.0f;
    voice.pitch_mod_depth_pmd = (double) dx7_voice_pms_to_semitones[voice.lfo_pms] *
                                (double) pdepth;
    // -FIX- this could be optimized:
    // -FIX- this just adds everything together -- maybe it should limit the result, or
    // combine the various mods like update_pressure() does
    pdepth = (instance->mod_wheel_assign & 0x01 ?
              // -FIX- this assumes that mod_wheel_sensitivity, etc. scale linearly => verify
              (float) instance->mod_wheel_sensitivity / 15.0f * instance->mod_wheel :
              0.0f) +
             (instance->foot_assign & 0x01 ?
              (float) instance->foot_sensitivity / 15.0f * instance->foot :
              0.0f) +
             (instance->pressure_assign & 0x01 ?
              (float) instance->pressure_sensitivity / 15.0f * pressure :
              0.0f) +
             (instance->breath_assign & 0x01 ?
              (float) instance->breath_sensitivity / 15.0f * instance->breath :
              0.0f);
    voice.pitch_mod_depth_mods = (double) dx7_voice_pms_to_semitones[voice.lfo_pms] *
                                 (double) pdepth;

    // -FIX- these are total guesses at how to combine/limit the amp mods:
    adepth = dx7_voice_amd_to_ol_adjustment[voice.lfo_amd];
    // -FIX- this could be optimized:
    mdepth = (instance->mod_wheel_assign & 0x02 ?
              dx7_voice_mss_to_ol_adjustment[instance->mod_wheel_sensitivity] *
              instance->mod_wheel :
              0.0f) +
             (instance->foot_assign & 0x02 ?
              dx7_voice_mss_to_ol_adjustment[instance->foot_sensitivity] *
              instance->foot :
              0.0f) +
             (instance->pressure_assign & 0x02 ?
              dx7_voice_mss_to_ol_adjustment[instance->pressure_sensitivity] *
              pressure :
              0.0f) +
             (instance->breath_assign & 0x02 ?
              dx7_voice_mss_to_ol_adjustment[instance->breath_sensitivity] *
              instance->breath :
              0.0f);
    edepth = // -FIX- this could be optimized:
            (instance->mod_wheel_assign & 0x04 ?
             dx7_voice_mss_to_ol_adjustment[instance->mod_wheel_sensitivity] *
             (1.0f - instance->mod_wheel) :
             0.0f) +
            (instance->foot_assign & 0x04 ?
             dx7_voice_mss_to_ol_adjustment[instance->foot_sensitivity] *
             (1.0f - instance->foot) :
             0.0f) +
            (instance->pressure_assign & 0x04 ?
             dx7_voice_mss_to_ol_adjustment[instance->pressure_sensitivity] *
             (1.0f - pressure) :
             0.0f) +
            (instance->breath_assign & 0x04 ?
             dx7_voice_mss_to_ol_adjustment[instance->breath_sensitivity] *
             (1.0f - instance->breath) :
             0.0f);

    /* full-scale amp mod for adepth and edepth should be 52.75 and
     * their sum _must_ be limited to less than 128, or bad things will happen! */
    if (adepth > 127.5f) adepth = 127.5f;
    if (adepth + mdepth > 127.5f)
        mdepth = 127.5f - adepth;
    if (adepth + mdepth + edepth > 127.5f)
        edepth = 127.5f - (adepth + mdepth);

    voice.amp_mod_lfo_amd_target = FLOAT_TO_FP(adepth);
    if (voice.amp_mod_lfo_amd_value <= INT_TO_FP(-64)) {
        voice.amp_mod_lfo_amd_value = voice.amp_mod_lfo_amd_target;
        voice.amp_mod_lfo_amd_increment = INT_TO_FP(0);
        voice.amp_mod_lfo_amd_duration = 0;
    } else {
        voice.amp_mod_lfo_amd_duration = instance->ramp_duration;
        voice.amp_mod_lfo_amd_increment =
                (voice.amp_mod_lfo_amd_target - voice.amp_mod_lfo_amd_value) /
                (dx7_sample_t) voice.amp_mod_lfo_amd_duration;
    }
    voice.amp_mod_lfo_mods_target = FLOAT_TO_FP(mdepth);
    if (voice.amp_mod_lfo_mods_value <= INT_TO_FP(-64)) {
        voice.amp_mod_lfo_mods_value = voice.amp_mod_lfo_mods_target;
        voice.amp_mod_lfo_mods_increment = INT_TO_FP(0);
        voice.amp_mod_lfo_mods_duration = 0;
    } else {
        voice.amp_mod_lfo_mods_duration = instance->ramp_duration;
        voice.amp_mod_lfo_mods_increment =
                (voice.amp_mod_lfo_mods_target - voice.amp_mod_lfo_mods_value) /
                (dx7_sample_t) voice.amp_mod_lfo_mods_duration;
    }
    voice.amp_mod_env_target = FLOAT_TO_FP(edepth);
    if (voice.amp_mod_env_value <= INT_TO_FP(-64)) {
        voice.amp_mod_env_value = voice.amp_mod_env_target;
        voice.amp_mod_env_increment = INT_TO_FP(0);
        voice.amp_mod_env_duration = 0;
    } else {
        voice.amp_mod_env_duration = instance->ramp_duration;
        voice.amp_mod_env_increment = (voice.amp_mod_env_target - voice.amp_mod_env_value) /
                                      (dx7_sample_t) voice.amp_mod_env_duration;
    }
}

/* ===== whole patch related functions ===== */

static void
dx7_voice_calculate_runtime_parameters(prdx7_instance_t *instance, dx7_voice_t &voice) {
    int32_t i;
    double freq;

    dx7_pitch_envelope_prepare(instance, voice);
    voice.amp_mod_lfo_amd_value = INT_TO_FP(-65);   /* force initial setup */
    voice.amp_mod_lfo_mods_value = INT_TO_FP(-65);
    voice.amp_mod_env_value = INT_TO_FP(-65);
    voice.lfo_delay_segment = 0;
    voice.lfo_delay_value = instance->lfo_delay_value[0];
    voice.lfo_delay_duration = instance->lfo_delay_duration[0];
    voice.lfo_delay_increment = instance->lfo_delay_increment[0];
    voice.mods_serial = instance->mods_serial - 1;  /* force mod depths update */
    dx7_portamento_prepare(instance, voice);
    freq = dx7_voice_recalculate_frequency(instance, voice);

    voice.volume_value = -1.0f;                     /* force initial setup */
    dx7_voice_recalculate_volume(instance, voice);

    for (i = 0; i < MAX_DX7_OPERATORS; i++) {
        voice.op[i].frequency = freq;
        if (voice.osc_key_sync) {
            voice.op[i].phase = INT_TO_FP(0);
        }
        dx7_op_recalculate_increment(instance, voice.op[i]);
        dx7_op_envelope_prepare(instance, voice.op[i],
                                limit_note(voice.key + voice.transpose - 24),
                                voice.velocity);
    }
}

/*
 * dx7_voice_setup_note
 */
static void
dx7_voice_setup_note(prdx7_instance_t *instance, dx7_voice_t &voice) {
    dx7_voice_set_data(instance, voice);
    prdx7_instance_set_performance_data(instance);
    dx7_lfo_set(instance, voice);
    dx7_voice_calculate_runtime_parameters(instance, voice);
}

/*
 * dx7_voice_set_data
 */
static void
dx7_voice_set_data(prdx7_instance_t *instance, dx7_voice_t &voice) {
    uint8_t *edit_buffer = instance->current_patch_buffer;
    int32_t compat059 = (instance->performance_buffer[0] & 0x01);  /* 0.5.9 compatibility */
    int32_t i, j;
    double aux_feedbk;

    for (i = 0; i < MAX_DX7_OPERATORS; i++) {
        uint8_t *eb_op = edit_buffer + ((5 - i) * 21);

        voice.op[i].output_level = limit(eb_op[16], 0, 99);

        voice.op[i].osc_mode = eb_op[17] & 0x01;
        voice.op[i].coarse = eb_op[18] & 0x1f;
        voice.op[i].fine = limit(eb_op[19], 0, 99);
        voice.op[i].detune = limit(eb_op[20], 0, 14);

        voice.op[i].level_scaling_bkpoint = limit(eb_op[8], 0, 99);
        voice.op[i].level_scaling_l_depth = limit(eb_op[9], 0, 99);
        voice.op[i].level_scaling_r_depth = limit(eb_op[10], 0, 99);
        voice.op[i].level_scaling_l_curve = eb_op[11] & 0x03;
        voice.op[i].level_scaling_r_curve = eb_op[12] & 0x03;
        voice.op[i].rate_scaling = eb_op[13] & 0x07;
        voice.op[i].amp_mod_sens = (compat059 ? 0 : eb_op[14] & 0x03);
        voice.op[i].velocity_sens = eb_op[15] & 0x07;

        for (j = 0; j < 4; j++) {
            voice.op[i].eg.base_rate[j] = limit(eb_op[j], 0, 99);
            voice.op[i].eg.base_level[j] = limit(eb_op[4 + j], 0, 99);
        }
    }

    for (i = 0; i < 4; i++) {
        voice.pitch_eg.rate[i] = limit(edit_buffer[126 + i], 0, 99);
        voice.pitch_eg.level[i] = limit(edit_buffer[130 + i], 0, 99);
    }

    voice.algorithm = edit_buffer[134] & 0x1f;

    aux_feedbk = (double) (edit_buffer[135] & 0x07) / (2.0 * M_PI) *
                 0.18 /* -FIX- feedback_scaling[voice.algorithm] */;

    /* the "99.0" here is because we're also using this multiplier to scale the
     * eg level from 0-99 to 0-1 */
    voice.feedback_multiplier = DOUBLE_TO_FP(aux_feedbk / 99.0);

    voice.osc_key_sync = edit_buffer[136] & 0x01;

    voice.lfo_speed = limit(edit_buffer[137], 0, 99);
    voice.lfo_delay = limit(edit_buffer[138], 0, 99);
    voice.lfo_pmd = limit(edit_buffer[139], 0, 99);
    voice.lfo_amd = limit(edit_buffer[140], 0, 99);
    voice.lfo_key_sync = edit_buffer[141] & 0x01;
    voice.lfo_wave = limit(edit_buffer[142], 0, 5);
    voice.lfo_pms = (compat059 ? 0 : edit_buffer[143] & 0x07);

    voice.transpose = limit(edit_buffer[144], 0, 48);
}

/*
 * decode_7in6
 *
 * decode a block of base64-ish 7-bit encoded data
 */
static int
decode_7in6(const char *string, int32_t expected_length, uint8_t *data) {
    int32_t in, stated_length, reg, above, below, shift, out;
    char *p;
    uint8_t *tmpdata;
    int32_t string_length = strlen(string);
    uint32_t sum = 0, stated_sum;

    if (string_length < 6)
        return 0;  /* too short */

    stated_length = strtol(string, &p, 10);
    in = p - string;
    if (in == 0 || string[in] != ' ')
        return 0;  /* stated length is bad */
    in++;
    if (stated_length != expected_length)
        return 0;

    if (string_length - in < ((expected_length * 7 + 5) / 6))
        return 0;  /* encoded data too short */

    if (!(tmpdata = (uint8_t *) malloc(expected_length)))
        return 0;  /* out of memory */

    reg = above = below = out = 0;
    while (1) {
        if (above == 7) {
            tmpdata[out] = reg >> 6;
            sum += tmpdata[out];
            reg &= 0x3f;
            above = 0;
            if (++out == expected_length)
                break;
        }
        if (below == 0) {
            if (!(p = strchr(base64, string[in]))) {
                return 0;  /* illegal character */
            }
            reg |= p - base64;
            below = 6;
            in++;
        }
        shift = 7 - above;
        if (below < shift) shift = below;
        reg <<= shift;
        above += shift;
        below -= shift;
    }

    if (string[in++] != ' ') {  /* encoded data wrong length */
        free(tmpdata);
        return 0;
    }

    stated_sum = strtol(string + in, &p, 10);
    if (sum != stated_sum) {
        free(tmpdata);
        return 0;
    }

    memcpy(data, tmpdata, expected_length);
    free(tmpdata);

    return 1;
}


/*
 * encode_7in6
 *
 * encode a block of 7-bit data, in base64-ish style
 */
static char *
encode_7in6(uint8_t *data, int32_t length) {
    char *buffer;
    int32_t in, reg, above, below, shift, out;
    int32_t outchars = (length * 7 + 5) / 6;
    uint32_t sum = 0;

    if (!(buffer = (char *) malloc(25 + outchars)))
        return NULL;

    out = snprintf(buffer, 12, "%d ", length);

    in = reg = above = below = 0;
    while (outchars) {
        if (above == 6) {
            buffer[out] = base64[reg >> 7];
            reg &= 0x7f;
            above = 0;
            out++;
            outchars--;
        }
        if (below == 0) {
            if (in < length) {
                reg |= data[in] & 0x7f;
                sum += data[in];
            }
            below = 7;
            in++;
        }
        shift = 6 - above;
        if (below < shift) shift = below;
        reg <<= shift;
        above += shift;
        below -= shift;
    }

    snprintf(buffer + out, 12, " %d", sum);

    return buffer;
}

/*
 * dx7_voice_copy_name
 */
static void
dx7_voice_copy_name(char *name, dx7_patch_t *patch) {
    int32_t i;
    unsigned char c;

    for (i = 0; i < 10; i++) {
        c = (unsigned char) patch->data[i + 118];
        switch (c) {
            case 92:
                c = 'Y';
                break;  /* yen */
            case 126:
                c = '>';
                break;  /* >> */
            case 127:
                c = '<';
                break;  /* << */
            default:
                if (c < 32 || c > 127) c = 32;
                break;
        }
        name[i] = c;
    }
    name[10] = 0;
}

/*
 * dx7_patch_unpack
 */
static void
dx7_patch_unpack(dx7_patch_t *packed_patch, uint8_t number, uint8_t *unpacked_patch) {
    uint8_t *up = unpacked_patch,
            *pp = (uint8_t *) (&packed_patch[number]);
    int32_t i, j;

    /* ugly because it used to be 68000 assembly... */
    for (i = 6; i > 0; i--) {
        for (j = 11; j > 0; j--) {
            *up++ = *pp++;
        }                           /* through rd */
        *up++ = (*pp) & 0x03;   /* lc */
        *up++ = (*pp++) >> 2;   /* rc */
        *up++ = (*pp) & 0x07;   /* rs */
        *(up + 6) = (*pp++) >> 3;   /* pd */
        *up++ = (*pp) & 0x03;   /* ams */
        *up++ = (*pp++) >> 2;   /* kvs */
        *up++ = *pp++;          /* ol */
        *up++ = (*pp) & 0x01;   /* m */
        *up++ = (*pp++) >> 1;   /* fc */
        *up = *pp++;          /* ff */
        up += 2;
    }                               /* operator done */
    for (i = 9; i > 0; i--) {
        *up++ = *pp++;
    }                               /* through algorithm */
    *up++ = (*pp) & 0x07;           /* feedback */
    *up++ = (*pp++) >> 3;           /* oks */
    for (i = 4; i > 0; i--) {
        *up++ = *pp++;
    }                               /* through lamd */
    *up++ = (*pp) & 0x01;           /* lfo ks */
    *up++ = ((*pp) >> 1) & 0x07;    /* lfo wave */
    *up++ = (*pp++) >> 4;           /* lfo pms */
    for (i = 11; i > 0; i--) {
        *up++ = *pp++;
    }
}

/*
 * dx7_patch_pack
 */
static void
dx7_patch_pack(uint8_t *unpacked_patch, dx7_patch_t *packed_patch, uint8_t number) {
    uint8_t *up = unpacked_patch,
            *pp = (uint8_t *) (&packed_patch[number]);
    int32_t i, j;

    /* ugly because it used to be 68000 assembly... */
    for (i = 6; i > 0; i--) {
        for (j = 11; j > 0; j--) {
            *pp++ = *up++;
        }                           /* through rd */
        *pp++ = ((*up) & 0x03) | (((*(up + 1)) & 0x03) << 2);
        up += 2;                    /* rc+lc */
        *pp++ = ((*up) & 0x07) | (((*(up + 7)) & 0x0f) << 3);
        up++;                       /* pd+rs */
        *pp++ = ((*up) & 0x03) | (((*(up + 1)) & 0x07) << 2);
        up += 2;                    /* kvs+ams */
        *pp++ = *up++;              /* ol */
        *pp++ = ((*up) & 0x01) | (((*(up + 1)) & 0x1f) << 1);
        up += 2;                    /* fc+m */
        *pp++ = *up;
        up += 2;                    /* ff */
    }                               /* operator done */
    for (i = 9; i > 0; i--) {
        *pp++ = *up++;
    }                               /* through algorithm */
    *pp++ = ((*up) & 0x07) | (((*(up + 1)) & 0x01) << 3);
    up += 2;                        /* oks+fb */
    for (i = 4; i > 0; i--) {
        *pp++ = *up++;
    }                               /* through lamd */
    *pp++ = ((*up) & 0x01) |
            (((*(up + 1)) & 0x07) << 1) |
            (((*(up + 2)) & 0x07) << 4);
    up += 3;                        /* lpms+lfw+lks */
    for (i = 11; i > 0; i--) {
        *pp++ = *up++;
    }                               /* through name */
}

/*
 * dssp_error_message
 */
char *
dssp_error_message(const char *fmt, ...) {
    va_list args;
    char buffer[256];

    va_start(args, fmt);
    vsnprintf(buffer, 256, fmt, args);
    va_end(args);
    return strdup(buffer);
}

/*
 * prdx7_data_patches_init
 *
 * initialize the patch bank, including a default set of good patches to get
 * the new user started.
 */


static void
prdx7_data_patches_init(dx7_patch_t *patches) {
    int32_t i;

    //memcpy(patches, friendly_patches, friendly_patch_count * sizeof(dx7_patch_t));
    // memcpy(patches, dx7_data, dx7tsl::app::bufsize_init);


    unsigned char* p = (unsigned char*) patches;

    memcpy(p, ROM1A_data, ROM1Atsl::app::bufsize_init);
    p += ROM1Atsl::app::bufsize_init;
    memcpy(p, ROM1B_data, ROM1Btsl::app::bufsize_init);
    p += ROM1Btsl::app::bufsize_init;
    memcpy(p, ROM2A_data, ROM1Atsl::app::bufsize_init);
    p += ROM2Atsl::app::bufsize_init;
    memcpy(p, ROM2B_data, ROM2Btsl::app::bufsize_init);
    p += ROM2Btsl::app::bufsize_init;
    memcpy(p, ROM3A_data, ROM3Atsl::app::bufsize_init);
    p += ROM3Atsl::app::bufsize_init;
    memcpy(p, ROM3B_data, ROM3Btsl::app::bufsize_init);
    p += ROM3Btsl::app::bufsize_init;
    memcpy(p, ROM4A_data, ROM4Atsl::app::bufsize_init);
    p += ROM4Atsl::app::bufsize_init;
    memcpy(p, ROM4B_data, ROM4Btsl::app::bufsize_init);

    // memcpy(patches+4096, synlib002_data, 4096);
    //memcpy(patches+8192, synlib003_data, 4096);
    //memcpy(patches+4096*3, synlib003_data, 4096);


    //  for (i = friendly_patch_count; i < 128; i++) {
    //    memcpy(&patches[i], &dx7_voice_init_voice, sizeof(dx7_patch_t));
    // }
}

/*
 * prdx7_data_performance_init
 *
 * initialize the global performance parameters.
 */
static void
prdx7_data_performance_init(uint8_t *performance) {
    memcpy(performance, &dx7_init_performance, DX7_PERFORMANCE_SIZE);
}


static inline dx7_sample_t
dx7_op_calculate_operator(dx7_sample_t eg_value, dx7_sample_t phase) {
    int32_t index;
    dx7_sample_t mod_index, out;
#ifdef PRDX7_USE_FLOATING_POINT
    float frac;
#endif /* PRDX7_USE_FLOATING_POINT */

    /* use eg_value to look up the modulation index, with interpolation */
#ifndef PRDX7_USE_FLOATING_POINT
    index = FP_TO_INT(eg_value);
    mod_index = dx7_voice_eg_ol_to_mod_index[index];
    mod_index += FP_MULTIPLY(dx7_voice_eg_ol_to_mod_index[index + 1] - mod_index,
                             eg_value & FP_MASK);
#else /* PRDX7_USE_FLOATING_POINT */
    index = lrintf(eg_value - 0.5f);
    frac = eg_value - (float) index;
    mod_index = dx7_voice_eg_ol_to_mod_index[index];
    mod_index += (dx7_voice_eg_ol_to_mod_index[index + 1] - mod_index) * frac;
#endif /* PRDX7_USE_FLOATING_POINT */

    /* use phase to look up the oscillator output, with interpolation */
#ifndef PRDX7_USE_FLOATING_POINT
    index = ((uint32_t)phase >> FP_TO_SINE_SHIFT) & SINE_MASK;
    out = dx7_voice_sin_table[index];
    out += (((int64_t)(dx7_voice_sin_table[index + 1] - out) *
             (int64_t)(phase & FP_TO_SINE_MASK)) >>
                                                 (FP_SHIFT + FP_TO_SINE_SHIFT));
#else /* PRDX7_USE_FLOATING_POINT */
    phase *= (float) SINE_SIZE;
    index = lrintf(phase - 0.5f);
    frac = phase - (float) index;
    index &= SINE_MASK;
    out = dx7_voice_sin_table[index];
    out += (dx7_voice_sin_table[index + 1] - out) * frac;
#endif /* PRDX7_USE_FLOATING_POINT */

    /* return the product of modulation index and oscillator output */
    return FP_MULTIPLY(mod_index, out);
}

static inline dx7_sample_t
dx7_op_calculate_operator_saving_feedback(dx7_voice_t &voice, dx7_sample_t eg_value,
                                          dx7_sample_t phase) {
    int32_t index;
    dx7_sample_t mod_index, out;
#ifndef PRDX7_USE_FLOATING_POINT
    int64_t out64;
#else /* PRDX7_USE_FLOATING_POINT */
    float frac;
#endif /* PRDX7_USE_FLOATING_POINT */

    /* use eg_value to look up the modulation index, with interpolation */
#ifndef PRDX7_USE_FLOATING_POINT
    index = FP_TO_INT(eg_value);
    mod_index = dx7_voice_eg_ol_to_mod_index[index];
    mod_index += FP_MULTIPLY(dx7_voice_eg_ol_to_mod_index[index + 1] - mod_index,
                             eg_value & FP_MASK);
#else /* PRDX7_USE_FLOATING_POINT */
    index = lrintf(eg_value - 0.5f);
    frac = eg_value - (float) index;
    mod_index = dx7_voice_eg_ol_to_mod_index[index];
    mod_index += (dx7_voice_eg_ol_to_mod_index[index + 1] - mod_index) * frac;
#endif /* PRDX7_USE_FLOATING_POINT */

    /* use phase to look up the oscillator output, with interpolation */
#ifndef PRDX7_USE_FLOATING_POINT
    index = ((uint32_t)phase >> FP_TO_SINE_SHIFT) & SINE_MASK;
    out = dx7_voice_sin_table[index];
    out64 = out +
            (((int64_t)(dx7_voice_sin_table[index + 1] - out) *
              (int64_t)(phase & FP_TO_SINE_MASK)) >>
                                                  (FP_SHIFT + FP_TO_SINE_SHIFT));
#else /* PRDX7_USE_FLOATING_POINT */
    phase *= (float) SINE_SIZE;
    index = lrintf(phase - 0.5f);
    frac = phase - (float) index;
    index &= SINE_MASK;
    out = dx7_voice_sin_table[index];
    out += (dx7_voice_sin_table[index + 1] - out) * frac;
#endif /* PRDX7_USE_FLOATING_POINT */

    /* save that output, scaled by our eg level, feedback amount, and a
     * constant, as our feedback modulation index */
#ifndef PRDX7_USE_FLOATING_POINT
    voice->feedback = (((out64 * (int64_t)eg_value) >> FP_SHIFT) *
                       (int64_t)voice->feedback_multiplier) >> FP_SHIFT;
#else /* PRDX7_USE_FLOATING_POINT */
    voice.feedback = out * eg_value * voice.feedback_multiplier;
#endif /* PRDX7_USE_FLOATING_POINT */

    /* return the product of modulation index and oscillator output */
#ifndef PRDX7_USE_FLOATING_POINT
    return (int32_t)(((int64_t)mod_index * out64) >> FP_SHIFT);
#else /* PRDX7_USE_FLOATING_POINT */
    return mod_index * out;
#endif /* PRDX7_USE_FLOATING_POINT */
}

static inline void
dx7_op_eg_process(prdx7_instance_t *instance, dx7_op_eg_t &eg) {
    eg.value += eg.increment;

    if (--eg.duration == 0) {

        if (eg.mode != DX7_EG_RUNNING) {
            eg.duration = -1;
            return;
        }

        if (eg.in_precomp) {

            eg.in_precomp = 0;
            eg.duration = eg.postcomp_duration;
            eg.increment = eg.postcomp_increment;

        } else {

            dx7_op_eg_set_next_phase(instance, eg);
        }
    }
}

static inline void
dx7_op_eg_adjust(dx7_op_eg_t &eg) {
    /* The constant in this next expression needs to be greater than
     * 0.000815 * (32/99) * sample_rate to avoid interaction with envelope
     * precompensation.  60 is safe to 192KHz. */
    if (eg.duration > 60) {

        if (eg.mode != DX7_EG_RUNNING)
            return;

        eg.increment = (eg.target - eg.value) / eg.duration;
    }
}

static inline void
dx7_pitch_eg_process(prdx7_instance_t *instance, dx7_pitch_eg_t &eg) {
    if (eg.mode != DX7_EG_RUNNING) return;

    eg.value += eg.increment;
    eg.duration--;

    if (eg.duration == 1) {

        eg.increment = eg.target - eg.value;  /* correct any rounding error */

    } else if (eg.duration == 0) {

        dx7_pitch_eg_set_next_phase(instance, eg);

    }
}

static inline void
dx7_portamento_process(prdx7_instance_t *instance, dx7_portamento_t &port) {
    if (port.segment == 0) return;

    port.value += port.increment;
    port.duration--;

    if (port.duration == 1) {

        port.increment = port.target - port.value;  /* correct any rounding error */

    } else if (port.duration == 0) {

        if (--port.segment > 0)
            dx7_portamento_set_segment(instance, port);
        else
            port.value = 0.0;

    }
}

static inline int
dx7_voice_check_for_dead(dx7_voice_t &voice) {
    int32_t i, b;

    for (i = 0, b = 1; i < 6; i++, b <<= 1) {

        if (!(dx7_voice_carriers[voice.algorithm] & b))
            continue;  /* not a carrier, so still a candidate for killing; continue to check next op */

        if (voice.op[i].eg.mode == DX7_EG_FINISHED)
            continue;  /* carrier, eg finished, so still a candidate */

        if ((voice.op[i].eg.mode == DX7_EG_CONSTANT ||
             voice.op[i].eg.mode == DX7_EG_SUSTAINING ||
             (voice.op[i].eg.mode == DX7_EG_RUNNING && voice.op[i].eg.phase == 3)) &&
            (FP_TO_INT(voice.op[i].eg.value) == 0))
            continue;  /* eg constant at 0 or decayed to effectively 0, still a candidate */

        return 0; /* if we got this far, this carrier still has output, so return without killing voice */
    }

    DEBUG_MESSAGE(DB_NOTE, " dx7_voice_check_for_dead: killing voice %p:%d\n", voice,
                  voice.note_id);
    dx7_voice_off(voice);
    return 1;
}

static inline int
float_equality(float a, float b) {
    union {
        float f;
        unsigned long l;
    } ua, ub;
    ua.f = a;
    ub.f = b;
    return ua.l == ub.l;
}

static inline int
double_equality(double a, double b) {
    union {
        double d;
        unsigned long l[2];
    } ua, ub;
    ua.d = a;
    ub.d = b;
    return ua.l[0] == ub.l[0] && ua.l[1] == ub.l[1];
}

/*
 * dx7_voice_render
 *
 * generate the actual sound data for this voice
 */
static void
dx7_voice_render(prdx7_instance_t *instance, dx7_voice_t &voice,
                 float *out, unsigned long sample_count,
                 int32_t do_control_update) {
    unsigned long sample;
    static dx7_sample_t ampmod[4] = {0};
    dx7_sample_t i;
    dx7_sample_t output;

    if (!float_equality(voice.last_port_volume, instance->volume) ||
        voice.last_cc_volume != instance->cc_volume)
        dx7_voice_recalculate_volume(instance, voice);
    switch (voice.algorithm) {

        case 0: /* algorithm 1 */

            /* This first algorithm is all written out, so you can see how it looks */

            for (sample = 0; sample < sample_count; sample++) {

                /* calculate amplitude modulation amounts */
                i = FP_MULTIPLY(voice.amp_mod_lfo_amd_value, voice.lfo_delay_value);
                i = voice.amp_mod_env_value +
                    FP_MULTIPLY(i + voice.amp_mod_lfo_mods_value, instance->lfo_buffer[sample]);

#ifndef PRDX7_USE_FLOATING_POINT
                #define AMPMOD2_CONSTANT  (7726076 >> (24 - FP_SHIFT))  /* 0.460510 */
#define AMPMOD1_CONSTANT  (3993950 >> (24 - FP_SHIFT))  /* 0.238058 */
#else /* PRDX7_USE_FLOATING_POINT */
#define AMPMOD2_CONSTANT  (0.460510f)
#define AMPMOD1_CONSTANT  (0.238058f)
#endif /* PRDX7_USE_FLOATING_POINT */
                ampmod[3] = i;
                ampmod[2] = FP_MULTIPLY(i, AMPMOD2_CONSTANT);
                ampmod[1] = FP_MULTIPLY(i, AMPMOD1_CONSTANT);

                output = (
                        dx7_op_calculate_operator(
                                voice.op[OP_3].eg.value - ampmod[voice.op[OP_3].amp_mod_sens],
                                voice.op[OP_3].phase +
                                dx7_op_calculate_operator(voice.op[OP_4].eg.value -
                                                          ampmod[voice.op[OP_4].amp_mod_sens],
                                                          voice.op[OP_4].phase +
                                                          dx7_op_calculate_operator(
                                                                  voice.op[OP_5].eg.value -
                                                                  ampmod[voice.op[OP_5].amp_mod_sens],
                                                                  voice.op[OP_5].phase +
                                                                  /* -FIX- need to determine if amp mod is included in feedback, or after */
                                                                  dx7_op_calculate_operator_saving_feedback(
                                                                          voice,
                                                                          voice.op[OP_6].eg.value -
                                                                          ampmod[voice.op[OP_6].amp_mod_sens],
                                                                          voice.op[OP_6].phase +
                                                                          voice.feedback)))) +
                        dx7_op_calculate_operator(
                                voice.op[OP_1].eg.value - ampmod[voice.op[OP_1].amp_mod_sens],
                                voice.op[OP_1].phase +
                                dx7_op_calculate_operator(voice.op[OP_2].eg.value -
                                                          ampmod[voice.op[OP_2].amp_mod_sens],
                                                          voice.op[OP_2].phase))
                );
                /* voice.volume_value contains a scaling factor for the number of carriers */

                /* mix voice output into output buffer */
                out[sample] += FP_TO_FLOAT(output) * voice.volume_value;

                /* update runtime parameters for next sample */
                voice.op[OP_6].phase += voice.op[OP_6].phase_increment;
                voice.op[OP_5].phase += voice.op[OP_5].phase_increment;
                voice.op[OP_4].phase += voice.op[OP_4].phase_increment;
                voice.op[OP_3].phase += voice.op[OP_3].phase_increment;
                voice.op[OP_2].phase += voice.op[OP_2].phase_increment;
                voice.op[OP_1].phase += voice.op[OP_1].phase_increment;

                dx7_op_eg_process(instance, voice.op[OP_6].eg);
                dx7_op_eg_process(instance, voice.op[OP_5].eg);
                dx7_op_eg_process(instance, voice.op[OP_4].eg);
                dx7_op_eg_process(instance, voice.op[OP_3].eg);
                dx7_op_eg_process(instance, voice.op[OP_2].eg);
                dx7_op_eg_process(instance, voice.op[OP_1].eg);

                if (voice.amp_mod_env_duration) {
                    voice.amp_mod_env_value += voice.amp_mod_env_increment;
                    voice.amp_mod_env_duration--;
                }
                if (voice.amp_mod_lfo_mods_duration) {
                    voice.amp_mod_lfo_mods_value += voice.amp_mod_lfo_mods_increment;
                    voice.amp_mod_lfo_mods_duration--;
                }
                if (voice.amp_mod_lfo_amd_duration) {
                    voice.amp_mod_lfo_amd_value += voice.amp_mod_lfo_amd_increment;
                    voice.amp_mod_lfo_amd_duration--;
                }
                if (voice.lfo_delay_duration) {
                    voice.lfo_delay_value += voice.lfo_delay_increment;
                    if (--voice.lfo_delay_duration == 0) {
                        int32_t seg = ++voice.lfo_delay_segment;
                        voice.lfo_delay_duration = instance->lfo_delay_duration[seg];
                        voice.lfo_delay_value = instance->lfo_delay_value[seg];
                        voice.lfo_delay_increment = instance->lfo_delay_increment[seg];
                    }
                }
                if (voice.volume_duration) {
                    voice.volume_value += voice.volume_increment;
                    voice.volume_duration--;
                }
            }
            break;

            /* Now we'll use some macros to make it easier to read */
#define op(_i, _p)     dx7_op_calculate_operator(voice.op[_i].eg.value - ampmod[voice.op[_i].amp_mod_sens], voice.op[_i].phase + _p)
#define op_sfb(_i, _p) dx7_op_calculate_operator_saving_feedback(voice, voice.op[_i].eg.value - ampmod[voice.op[_i].amp_mod_sens], voice.op[_i].phase + _p)

#define RENDER \
        for (sample = 0; sample < sample_count; sample++) { \
            /* calculate amplitude modulation amounts */ \
            i = FP_MULTIPLY(voice.amp_mod_lfo_amd_value, voice.lfo_delay_value); \
            i = voice.amp_mod_env_value + \
                    FP_MULTIPLY(i + voice.amp_mod_lfo_mods_value, instance->lfo_buffer[sample]); \
            ampmod[3] = i; \
            ampmod[2] = FP_MULTIPLY(i, AMPMOD2_CONSTANT); \
            ampmod[1] = FP_MULTIPLY(i, AMPMOD1_CONSTANT); \
            ALGORITHM; \
            /* voice.volume_value contains a scaling factor for the number of carriers */ \
            /* mix voice output into output buffer */ \
            out[sample] += FP_TO_FLOAT(output) * voice.volume_value; \
            /* update runtime parameters for next sample */ \
            voice.op[OP_6].phase += voice.op[OP_6].phase_increment; \
            voice.op[OP_5].phase += voice.op[OP_5].phase_increment; \
            voice.op[OP_4].phase += voice.op[OP_4].phase_increment; \
            voice.op[OP_3].phase += voice.op[OP_3].phase_increment; \
            voice.op[OP_2].phase += voice.op[OP_2].phase_increment; \
            voice.op[OP_1].phase += voice.op[OP_1].phase_increment; \
            dx7_op_eg_process(instance, voice.op[OP_6].eg); \
            dx7_op_eg_process(instance, voice.op[OP_5].eg); \
            dx7_op_eg_process(instance, voice.op[OP_4].eg); \
            dx7_op_eg_process(instance, voice.op[OP_3].eg); \
            dx7_op_eg_process(instance, voice.op[OP_2].eg); \
            dx7_op_eg_process(instance, voice.op[OP_1].eg); \
            if (voice.amp_mod_env_duration) { \
                voice.amp_mod_env_value += voice.amp_mod_env_increment; \
                voice.amp_mod_env_duration--; \
            } \
            if (voice.amp_mod_lfo_mods_duration) { \
                voice.amp_mod_lfo_mods_value += voice.amp_mod_lfo_mods_increment; \
                voice.amp_mod_lfo_mods_duration--; \
            } \
            if (voice.amp_mod_lfo_amd_duration) { \
                voice.amp_mod_lfo_amd_value += voice.amp_mod_lfo_amd_increment; \
                voice.amp_mod_lfo_amd_duration--; \
            } \
            if (voice.lfo_delay_duration) { \
                voice.lfo_delay_value += voice.lfo_delay_increment; \
                if (--voice.lfo_delay_duration == 0) { \
                    int32_t seg = ++voice.lfo_delay_segment; \
                    voice.lfo_delay_duration  = instance->lfo_delay_duration[seg]; \
                    voice.lfo_delay_value     = instance->lfo_delay_value[seg]; \
                    voice.lfo_delay_increment = instance->lfo_delay_increment[seg]; \
                } \
            } \
            if (voice.volume_duration) { \
                voice.volume_value += voice.volume_increment; \
                voice.volume_duration--; \
            } \
        }

        case 1: /* algorithm 2 */

#define ALGORITHM { \
            output = (                                             \
                      op(OP_3, op(OP_4, op(OP_5, op(OP_6, 0)))) +  \
                      op(OP_1, op_sfb(OP_2, voice.feedback))      \
                     );                                            \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 2: /* algorithm 3 */

#define ALGORITHM { \
            output = (                                                     \
                      op(OP_4, op(OP_5, op_sfb(OP_6, voice.feedback))) +  \
                      op(OP_1, op(OP_2, op(OP_3, 0)))                      \
                     );                                                    \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 3: /* algorithm 4 */

#define ALGORITHM { \
            output = (                                                     \
                      op_sfb(OP_4, op(OP_5, op(OP_6, voice.feedback))) +  \
                      op(OP_1, op(OP_2, op(OP_3, 0)))                      \
                     );                                                    \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 4: /* algorithm 5 */

#define ALGORITHM { \
            output = (                                           \
                      op(OP_5, op_sfb(OP_6, voice.feedback)) +  \
                      op(OP_3, op(OP_4, 0)) +                    \
                      op(OP_1, op(OP_2, 0))                      \
                     );                                          \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 5: /* algorithm 6 */

#define ALGORITHM { \
            output = (                                           \
                      op_sfb(OP_5, op(OP_6, voice.feedback)) +  \
                      op(OP_3, op(OP_4, 0)) +                    \
                      op(OP_1, op(OP_2, 0))                      \
                     );                                          \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 6: /* algorithm 7 */

#define ALGORITHM { \
            output = (                                                    \
                      op(OP_3, op(OP_5, op_sfb(OP_6, voice.feedback)) +  \
                               op(OP_4, 0)) +                             \
                      op(OP_1, op(OP_2, 0))                               \
                     );                                                   \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 7: /* algorithm 8 */

#define ALGORITHM { \
            output = (                                           \
                      op(OP_3, op(OP_5, op(OP_6, 0)) +           \
                               op_sfb(OP_4, voice.feedback)) +  \
                      op(OP_1, op(OP_2, 0))                      \
                     );                                          \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 8: /* algorithm 9 */

#define ALGORITHM { \
            output = (                                         \
                      op(OP_3, op(OP_5, op(OP_6, 0)) +         \
                               op(OP_4, 0)) +                  \
                      op(OP_1, op_sfb(OP_2, voice.feedback))  \
                     );                                        \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 9: /* algorithm 10 */

#define ALGORITHM { \
            output = (                                                   \
                      op(OP_4, op(OP_6, 0) +                             \
                               op(OP_5, 0)) +                            \
                      op(OP_1, op(OP_2, op_sfb(OP_3, voice.feedback)))  \
                     );                                                  \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 10: /* algorithm 11 */

#define ALGORITHM { \
            output = (                                          \
                      op(OP_4, op_sfb(OP_6, voice.feedback) +  \
                               op(OP_5, 0)) +                   \
                      op(OP_1, op(OP_2, op(OP_3, 0)))           \
                     );                                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 11: /* algorithm 12 */

#define ALGORITHM { \
            output = (                                         \
                      op(OP_3, op(OP_6, 0) +                   \
                               op(OP_5, 0) +                   \
                               op(OP_4, 0)) +                  \
                      op(OP_1, op_sfb(OP_2, voice.feedback))  \
                     );                                        \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 12: /* algorithm 13 */

#define ALGORITHM { \
            output = (                                          \
                      op(OP_3, op_sfb(OP_6, voice.feedback) +  \
                               op(OP_5, 0) +                    \
                               op(OP_4, 0)) +                   \
                      op(OP_1, op(OP_2, 0))                     \
                     );                                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 13: /* algorithm 14 */

#define ALGORITHM { \
            output = (                                                   \
                      op(OP_3, op(OP_4, op_sfb(OP_6, voice.feedback) +  \
                                        op(OP_5, 0))) +                  \
                      op(OP_1, op(OP_2, 0))                              \
                     );                                                  \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 14: /* algorithm 15 */

#define ALGORITHM { \
            output = (                                         \
                      op(OP_3, op(OP_4, op(OP_6, 0) +          \
                                        op(OP_5, 0))) +        \
                      op(OP_1, op_sfb(OP_2, voice.feedback))  \
                     );                                        \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 15: /* algorithm 16 */

#define ALGORITHM { \
            output = op(OP_1, op(OP_5, op_sfb(OP_6, voice.feedback)) +  \
                              op(OP_3, op(OP_4, 0)) +                    \
                              op(OP_2, 0));                              \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 16: /* algorithm 17 */

#define ALGORITHM { \
            output = op(OP_1, op(OP_5, op(OP_6, 0)) +          \
                              op(OP_3, op(OP_4, 0)) +          \
                              op_sfb(OP_2, voice.feedback));  \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 17: /* algorithm 18 */

#define ALGORITHM { \
            output = op(OP_1, op(OP_4, op(OP_5, op(OP_6, 0))) +  \
                              op_sfb(OP_3, voice.feedback) +    \
                              op(OP_2, 0));                      \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 18: /* algorithm 19 */

#define ALGORITHM { \
            i = op_sfb(OP_6, voice.feedback);         \
            output = (                                 \
                      op(OP_5, i) +                    \
                      op(OP_4, i) +                    \
                      op(OP_1, op(OP_2, op(OP_3, 0)))  \
                     );                                \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 19: /* algorithm 20 */

#define ALGORITHM { \
            i = op_sfb(OP_3, voice.feedback);  \
            output = (                          \
                      op(OP_4, op(OP_6, 0) +    \
                               op(OP_5, 0)) +   \
                      op(OP_2, i) +             \
                      op(OP_1, i)               \
                     );                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 20: /* algorithm 21 */

#define ALGORITHM { \
            i = op(OP_6, 0);                    \
            output = op(OP_5, i) +              \
                     op(OP_4, i);               \
            i = op_sfb(OP_3, voice.feedback);  \
            output += op(OP_2, i) +             \
                      op(OP_1, i);              \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 21: /* algorithm 22 */

#define ALGORITHM { \
            i = op_sfb(OP_6, voice.feedback);  \
            output = (                          \
                      op(OP_5, i) +             \
                      op(OP_4, i) +             \
                      op(OP_3, i) +             \
                      op(OP_1, op(OP_2, 0))     \
                     );                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 22: /* algorithm 23 */

#define ALGORITHM { \
            i = op_sfb(OP_6, voice.feedback);  \
            output = (                          \
                      op(OP_5, i) +             \
                      op(OP_4, i) +             \
                      op(OP_2, op(OP_3, 0)) +   \
                      op(OP_1, 0)               \
                     );                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 23: /* algorithm 24 */

#define ALGORITHM { \
            i = op_sfb(OP_6, voice.feedback);  \
            output = (                          \
                      op(OP_5, i) +             \
                      op(OP_4, i) +             \
                      op(OP_3, i) +             \
                      op(OP_2, 0) +             \
                      op(OP_1, 0)               \
                     );                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 24: /* algorithm 25 */

#define ALGORITHM { \
            i = op_sfb(OP_6, voice.feedback);  \
            output = (                          \
                      op(OP_5, i) +             \
                      op(OP_4, i) +             \
                      op(OP_3, 0) +             \
                      op(OP_2, 0) +             \
                      op(OP_1, 0)               \
                     );                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 25: /* algorithm 26 */

#define ALGORITHM { \
            output = (                                          \
                      op(OP_4, op_sfb(OP_6, voice.feedback) +  \
                               op(OP_5, 0)) +                   \
                      op(OP_2, op(OP_3, 0)) +                   \
                      op(OP_1, 0)                               \
                     );                                         \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 26: /* algorithm 27 */

#define ALGORITHM { \
            output = (                                           \
                      op(OP_4, op(OP_6, 0) +                     \
                               op(OP_5, 0)) +                    \
                      op(OP_2, op_sfb(OP_3, voice.feedback)) +  \
                      op(OP_1, 0)                                \
                     );                                          \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 27: /* algorithm 28 */

#define ALGORITHM { \
            output = (                                                     \
                      op(OP_6, 0) +                                        \
                      op(OP_3, op(OP_4, op_sfb(OP_5, voice.feedback))) +  \
                      op(OP_1, op(OP_2, 0))                                \
                     );                                                    \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 28: /* algorithm 29 */

#define ALGORITHM { \
            output = (                                           \
                      op(OP_5, op_sfb(OP_6, voice.feedback)) +  \
                      op(OP_3, op(OP_4, 0)) +                    \
                      op(OP_2, 0) +                              \
                      op(OP_1, 0)                                \
                     );                                          \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 29: /* algorithm 30 */

#define ALGORITHM { \
            output = (                                                     \
                      op(OP_6, 0) +                                        \
                      op(OP_3, op(OP_4, op_sfb(OP_5, voice.feedback))) +  \
                      op(OP_2, 0) +                                        \
                      op(OP_1, 0)                                          \
                     );                                                    \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 30: /* algorithm 31 */

#define ALGORITHM { \
            output = (                                           \
                      op(OP_5, op_sfb(OP_6, voice.feedback)) +  \
                      op(OP_4, 0) +                              \
                      op(OP_3, 0) +                              \
                      op(OP_2, 0) +                              \
                      op(OP_1, 0)                                \
                     );                                          \
        }

            RENDER;
            break;
#undef ALGORITHM

        case 31: /* algorithm 32 */
        default: /* just in case */

#define ALGORITHM { \
            output = (                                 \
                      op_sfb(OP_6, voice.feedback) +  \
                      op(OP_5, 0) +                    \
                      op(OP_4, 0) +                    \
                      op(OP_3, 0) +                    \
                      op(OP_2, 0) +                    \
                      op(OP_1, 0)                      \
                     );                                \
        }

            RENDER;
            break;
#undef ALGORITHM

#undef op
#undef op_sfb
    }

    if (do_control_update) {
        double new_pitch;

        /* do those things which should be done only once per control-
         * calculation interval ("nugget"), such as voice check-for-dead,
         * pitch envelope calculations, etc. */

        /* check if we've decayed to nothing, turn off voice if so */
        if (dx7_voice_check_for_dead(voice))
            return; /* we're dead now, so return */

#ifdef PRDX7_USE_FLOATING_POINT
        /* wrap oscillator phases */
        voice.op[OP_6].phase -= floorf(voice.op[OP_6].phase);
        voice.op[OP_5].phase -= floorf(voice.op[OP_5].phase);
        voice.op[OP_4].phase -= floorf(voice.op[OP_4].phase);
        voice.op[OP_3].phase -= floorf(voice.op[OP_3].phase);
        voice.op[OP_2].phase -= floorf(voice.op[OP_2].phase);
        voice.op[OP_1].phase -= floorf(voice.op[OP_1].phase);
#endif /* PRDX7_USE_FLOATING_POINT */

        /* update pitch envelope and portamento */
        dx7_pitch_eg_process(instance, voice.pitch_eg);
        dx7_portamento_process(instance, voice.portamento);

        /* update phase increments if pitch or tuning changed */
        new_pitch = voice.pitch_eg.value + voice.portamento.value +
                    instance->pitch_bend -
                    instance->lfo_value_for_pitch *
                    (voice.pitch_mod_depth_pmd * FP_TO_DOUBLE(voice.lfo_delay_value) +
                     voice.pitch_mod_depth_mods);
        if (!double_equality(voice.last_pitch, new_pitch) ||
            !float_equality(voice.last_port_tuning, instance->tuning)) {

            dx7_voice_recalculate_freq_and_inc(instance, voice);
        }

        /* op envelope rounding correction */
        dx7_op_eg_adjust(voice.op[OP_6].eg);
        dx7_op_eg_adjust(voice.op[OP_5].eg);
        dx7_op_eg_adjust(voice.op[OP_4].eg);
        dx7_op_eg_adjust(voice.op[OP_3].eg);
        dx7_op_eg_adjust(voice.op[OP_2].eg);
        dx7_op_eg_adjust(voice.op[OP_1].eg);

        /* mods and output volume */
        if (!voice.amp_mod_env_duration)
            voice.amp_mod_env_value = voice.amp_mod_env_target;
        if (!voice.amp_mod_lfo_mods_duration)
            voice.amp_mod_lfo_mods_value = voice.amp_mod_lfo_mods_target;
        if (!voice.amp_mod_lfo_amd_duration)
            voice.amp_mod_lfo_amd_value = voice.amp_mod_lfo_amd_target;
        if (!voice.volume_duration)
            voice.volume_value = voice.volume_target;
    }
}

