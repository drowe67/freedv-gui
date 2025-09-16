/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/* analog_agc.c
 *
 * Using a feedback system, determines an appropriate analog volume level
 * given an input signal and current volume level. Targets a conservative
 * signal level and is intended for use with a digital AGC to apply
 * additional gain.
 *
 */

#include "agc.h"
#include <stdlib.h>
#include <math.h>

#ifdef WEBRTC_AGC_DEBUG_DUMP
#include <stdio.h>
#endif

#ifndef MIN
#define MIN(A, B)        ((A) < (B) ? (A) : (B))  // Get min value
#endif
#ifndef MAX
#define MAX(A, B)        ((A) > (B) ? (A) : (B))  // Get max value
#endif

// Shifting with negative numbers allowed
// Positive means left shift
#ifndef SHIFT_W32
#define SHIFT_W32(x, c) ((c) >= 0 ? (x) * (1 << (c)) : (x) >> -(c))
#endif
// C + the 32 most significant bits of A * B
#ifndef AGC_SCALEDIFF32
#define AGC_SCALEDIFF32(A, B, C) \
  ((C) + ((B) >> 16) * (A) + (((0x0000FFFF & (B)) * (A)) >> 16))
#endif
#ifndef AGC_MUL32
// the 32 most significant bits of A(19) * B(26) >> 13
#define AGC_MUL32(A, B) (((B) >> 13) * (A) + (((0x00001FFF & (B)) * (A)) >> 13))
#endif

/* The slope of in Q13*/
static const int16_t kSlope1[8] = {21793, 12517, 7189, 4129,
                                   2372, 1362, 472, 78};

/* The offset in Q14 */
static const int16_t kOffset1[8] = {25395, 23911, 22206, 20737,
                                    19612, 18805, 17951, 17367};

/* The slope of in Q13*/
static const int16_t kSlope2[8] = {2063, 1731, 1452, 1218, 1021, 857, 597, 337};

/* The offset in Q14 */
static const int16_t kOffset2[8] = {18432, 18379, 18290, 18177,
                                    18052, 17920, 17670, 17286};

static const int16_t kMuteGuardTimeMs = 8000;
static const int16_t kInitCheck = 42;
static const size_t kNumSubframes = 10;

/* Default settings if config is not used */
#define AGC_DEFAULT_TARGET_LEVEL 3
#define AGC_DEFAULT_COMP_GAIN 9
/* This is the target level for the analog part in ENV scale. To convert to RMS
 * scale you
 * have to add OFFSET_ENV_TO_RMS.
 */
#define ANALOG_TARGET_LEVEL 11
#define ANALOG_TARGET_LEVEL_2 5  // ANALOG_TARGET_LEVEL / 2
/* Offset between RMS scale (analog part) and ENV scale (digital part). This
 * value actually
 * varies with the FIXED_ANALOG_TARGET_LEVEL, hence we should in the future
 * replace it with
 * a table.
 */
#define OFFSET_ENV_TO_RMS 9
/* The reference input level at which the digital part gives an output of
 * targetLevelDbfs
 * (desired level) if we have no compression gain. This level should be set high
 * enough not
 * to compress the peaks due to the dynamics.
 */
#define DIGITAL_REF_AT_0_COMP_GAIN 4
/* Speed of reference level decrease.
 */
#define DIFF_REF_TO_ANALOG 5

#ifdef MIC_LEVEL_FEEDBACK
#define NUM_BLOCKS_IN_SAT_BEFORE_CHANGE_TARGET 7
#endif
/* Size of analog gain table */
#define GAIN_TBL_LEN 32
/* Matlab code:
 * fprintf(1, '\t%i, %i, %i, %i,\n', round(10.^(linspace(0,10,32)/20) * 2^12));
 */
/* Q12 */
static const uint16_t kGainTableAnalog[GAIN_TBL_LEN] = {
        4096, 4251, 4412, 4579, 4752, 4932, 5118, 5312, 5513, 5722, 5938,
        6163, 6396, 6638, 6889, 7150, 7420, 7701, 7992, 8295, 8609, 8934,
        9273, 9623, 9987, 10365, 10758, 11165, 11587, 12025, 12480, 12953};

/* Gain/Suppression tables for virtual Mic (in Q10) */
static const uint16_t kGainTableVirtualMic[128] = {
        1052, 1081, 1110, 1141, 1172, 1204, 1237, 1271, 1305, 1341, 1378,
        1416, 1454, 1494, 1535, 1577, 1620, 1664, 1710, 1757, 1805, 1854,
        1905, 1957, 2010, 2065, 2122, 2180, 2239, 2301, 2364, 2428, 2495,
        2563, 2633, 2705, 2779, 2855, 2933, 3013, 3096, 3180, 3267, 3357,
        3449, 3543, 3640, 3739, 3842, 3947, 4055, 4166, 4280, 4397, 4517,
        4640, 4767, 4898, 5032, 5169, 5311, 5456, 5605, 5758, 5916, 6078,
        6244, 6415, 6590, 6770, 6956, 7146, 7341, 7542, 7748, 7960, 8178,
        8402, 8631, 8867, 9110, 9359, 9615, 9878, 10148, 10426, 10711, 11004,
        11305, 11614, 11932, 12258, 12593, 12938, 13292, 13655, 14029, 14412, 14807,
        15212, 15628, 16055, 16494, 16945, 17409, 17885, 18374, 18877, 19393, 19923,
        20468, 21028, 21603, 22194, 22801, 23425, 24065, 24724, 25400, 26095, 26808,
        27541, 28295, 29069, 29864, 30681, 31520, 32382};
static const uint16_t kSuppressionTableVirtualMic[128] = {
        1024, 1006, 988, 970, 952, 935, 918, 902, 886, 870, 854, 839, 824, 809, 794,
        780, 766, 752, 739, 726, 713, 700, 687, 675, 663, 651, 639, 628, 616, 605,
        594, 584, 573, 563, 553, 543, 533, 524, 514, 505, 496, 487, 478, 470, 461,
        453, 445, 437, 429, 421, 414, 406, 399, 392, 385, 378, 371, 364, 358, 351,
        345, 339, 333, 327, 321, 315, 309, 304, 298, 293, 288, 283, 278, 273, 268,
        263, 258, 254, 249, 244, 240, 236, 232, 227, 223, 219, 215, 211, 208, 204,
        200, 197, 193, 190, 186, 183, 180, 176, 173, 170, 167, 164, 161, 158, 155,
        153, 150, 147, 145, 142, 139, 137, 134, 132, 130, 127, 125, 123, 121, 118,
        116, 114, 112, 110, 108, 106, 104, 102};

/* Table for target energy levels. Values in Q(-7)
 * Matlab code
 * targetLevelTable = fprintf('%d,\t%d,\t%d,\t%d,\n',
 * round((32767*10.^(-(0:63)'/20)).^2*16/2^7) */

static const int32_t kTargetLevelTable[64] = {
        134209536, 106606424, 84680493, 67264106, 53429779, 42440782, 33711911,
        26778323, 21270778, 16895980, 13420954, 10660642, 8468049, 6726411,
        5342978, 4244078, 3371191, 2677832, 2127078, 1689598, 1342095,
        1066064, 846805, 672641, 534298, 424408, 337119, 267783,
        212708, 168960, 134210, 106606, 84680, 67264, 53430,
        42441, 33712, 26778, 21271, 16896, 13421, 10661,
        8468, 6726, 5343, 4244, 3371, 2678, 2127,
        1690, 1342, 1066, 847, 673, 534, 424,
        337, 268, 213, 169, 134, 107, 85,
        67};


static __inline int16_t DivW32W16ResW16(int32_t num, int16_t den) {
    // Guard against division with 0
    return (den != 0) ? (int16_t) (num / den) : (int16_t) 0x7FFF;
}

static __inline int32_t DivW32W16(int32_t num, int16_t den) {
    // Guard against division with 0
    return (den != 0) ? (int32_t) (num / den) : (int32_t) 0x7FFFFFFF;
}

static __inline uint32_t __clz_uint32(uint32_t v) {
// Never used with input 0
    assert(v > 0);
#if defined(__INTEL_COMPILER)
    return _bit_scan_reverse(v) ^ 31U;
#elif defined(__GNUC__) && (__GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
// This will translate either to (bsr ^ 31U), clz , ctlz, cntlz, lzcnt depending on
// -march= setting or to a software routine in exotic machines.
    return __builtin_clz(v);
#elif defined(_MSC_VER)
    // for _BitScanReverse
#include <intrin.h>
    {
        uint32_t idx;
        _BitScanReverse(&idx, v);
        return idx ^ 31U;
    }
#else
// Will never be emitted for MSVC, GCC, Intel compilers
    static const uint8_t byte_to_unary_table[] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    return word > 0xffffff ? byte_to_unary_table[v >> 24] :
        word > 0xffff ? byte_to_unary_table[v >> 16] + 8 :
        word > 0xff ? byte_to_unary_table[v >> 8] + 16 :
        byte_to_unary_table[v] + 24;
#endif
}

// Return the number of steps a can be left-shifted without overflow,
// or 0 if a == 0.
static __inline int16_t NormU32(uint32_t a) {

    if (a == 0) return 0;
    return (int16_t) __clz_uint32(a);
}


static __inline int16_t SatW32ToW16(int32_t value32) {
    return (int16_t) value32 > 32767 ? (int16_t) 32767 : (value32 < -32768) ? (int16_t) (-32768) : (int16_t) value32;
}


// Return the number of steps a can be left-shifted without overflow,
// or 0 if a == 0.
static __inline int16_t NormW32(int32_t a) {

    if (a == 0) return 0;
    uint32_t v = (uint32_t) (a < 0 ? ~a : a);
    // Returns the number of leading zero bits in the argument.
    return (int16_t) (__clz_uint32(v) - 1);
}

static __inline int16_t WebRtcSpl_AddSatW16(int16_t a, int16_t b) {
    return SatW32ToW16((int32_t) a + (int32_t) b);
}

int32_t DotProductWithScale(const int16_t *vector1,
                            const int16_t *vector2,
                            size_t length,
                            int scaling) {
    int64_t sum = 0;
    size_t i = 0;

    /* Unroll the loop to improve performance. */
    for (i = 0; i + 3 < length; i += 4) {
        sum += (vector1[i + 0] * vector2[i + 0]) >> scaling;
        sum += (vector1[i + 1] * vector2[i + 1]) >> scaling;
        sum += (vector1[i + 2] * vector2[i + 2]) >> scaling;
        sum += (vector1[i + 3] * vector2[i + 3]) >> scaling;
    }
    for (; i < length; i++) {
        sum += (vector1[i] * vector2[i]) >> scaling;
    }

    return (int32_t) (sum);
}


static float fast_sqrt(float x) {
    float s;
#if defined(__x86_64__)
    __asm__ __volatile__ ("sqrtss %1, %0" : "=x"(s) : "x"(x));
#elif defined(__i386__)
    s = x;
    __asm__ __volatile__ ("fsqrt" : "+t"(s));
#elif defined(__arm__) && defined(__VFP_FP__)
    __asm__ __volatile__ ("vsqrt.f32 %0, %1" : "=w"(s) : "w"(x));
#else
    s = sqrtf(x);
#endif
    return s;
}

static __inline void downsampleBy2(const int16_t *in, size_t len,
                                   int16_t *out, int32_t *filtState) {
    int32_t tmp1, tmp2, diff, in32, out32;
    size_t i;

    register int32_t state0 = filtState[0];
    register int32_t state1 = filtState[1];
    register int32_t state2 = filtState[2];
    register int32_t state3 = filtState[3];
    register int32_t state4 = filtState[4];
    register int32_t state5 = filtState[5];
    register int32_t state6 = filtState[6];
    register int32_t state7 = filtState[7];

    for (i = (len >> 1); i > 0; i--) {
        // lower allpass filter
        in32 = (int32_t) (*in++) * (1 << 10);
        diff = in32 - state1;
        tmp1 = ((state0) + ((diff) >> 16) * (kResampleAllpass2[0]) +
                (((uint32_t) ((diff) & 0x0000FFFF) * (kResampleAllpass2[0])) >> 16));
        state0 = in32;
        diff = tmp1 - state2;
        tmp2 = ((state1) + ((diff) >> 16) * (kResampleAllpass2[1]) +
                (((uint32_t) ((diff) & 0x0000FFFF) * (kResampleAllpass2[1])) >> 16));
        state1 = tmp1;
        diff = tmp2 - state3;
        state3 = ((state2) + ((diff) >> 16) * (kResampleAllpass2[2]) +
                  (((uint32_t) ((diff) & 0x0000FFFF) * (kResampleAllpass2[2])) >> 16));
        state2 = tmp2;

        // upper allpass filter
        in32 = (int32_t) (*in++) * (1 << 10);
        diff = in32 - state5;
        tmp1 = ((state4) + ((diff) >> 16) * (kResampleAllpass1[0]) +
                (((uint32_t) ((diff) & 0x0000FFFF) * (kResampleAllpass1[0])) >> 16));

        state4 = in32;
        diff = tmp1 - state6;
        tmp2 = ((state5) + ((diff) >> 16) * (kResampleAllpass1[1]) +
                (((uint32_t) ((diff) & 0x0000FFFF) * (kResampleAllpass1[1])) >> 16));

        state5 = tmp1;
        diff = tmp2 - state7;
        state7 = ((state6) + ((diff) >> 16) * (kResampleAllpass1[2]) +
                  (((uint32_t) ((diff) & 0x0000FFFF) * (kResampleAllpass1[2])) >> 16));

        state6 = tmp2;

        // add two allpass outputs, divide by two and round
        out32 = (state3 + state7 + 1024) >> 11;

        // limit amplitude to prevent wrap-around, and write to output array
        *out++ = SatW32ToW16(out32);
    }

    filtState[0] = state0;
    filtState[1] = state1;
    filtState[2] = state2;
    filtState[3] = state3;
    filtState[4] = state4;
    filtState[5] = state5;
    filtState[6] = state6;
    filtState[7] = state7;
}

// To generate the gaintable, copy&paste the following lines to a Matlab window:
// MaxGain = 6; MinGain = 0; CompRatio = 3; Knee = 1;
// zeros = 0:31; lvl = 2.^(1-zeros);
// A = -10*log10(lvl) * (CompRatio - 1) / CompRatio;
// B = MaxGain - MinGain;
// gains = round(2^16*10.^(0.05 * (MinGain + B * (
// log(exp(-Knee*A)+exp(-Knee*B)) - log(1+exp(-Knee*B)) ) /
// log(1/(1+exp(Knee*B))))));
// fprintf(1, '\t%i, %i, %i, %i,\n', gains);
// % Matlab code for plotting the gain and input/output level characteristic
// (copy/paste the following 3 lines):
// in = 10*log10(lvl); out = 20*log10(gains/65536);
// subplot(121); plot(in, out); axis([-30, 0, -5, 20]); grid on; xlabel('Input
// (dB)'); ylabel('Gain (dB)');
// subplot(122); plot(in, in+out); axis([-30, 0, -30, 5]); grid on;
// xlabel('Input (dB)'); ylabel('Output (dB)');
// zoom on;

// Generator table for y=log2(1+e^x) in Q8.
enum {
    kGenFuncTableSize = 128
};
static const uint16_t kGenFuncTable[kGenFuncTableSize] = {
        256, 485, 786, 1126, 1484, 1849, 2217, 2586, 2955, 3324, 3693,
        4063, 4432, 4801, 5171, 5540, 5909, 6279, 6648, 7017, 7387, 7756,
        8125, 8495, 8864, 9233, 9603, 9972, 10341, 10711, 11080, 11449, 11819,
        12188, 12557, 12927, 13296, 13665, 14035, 14404, 14773, 15143, 15512, 15881,
        16251, 16620, 16989, 17359, 17728, 18097, 18466, 18836, 19205, 19574, 19944,
        20313, 20682, 21052, 21421, 21790, 22160, 22529, 22898, 23268, 23637, 24006,
        24376, 24745, 25114, 25484, 25853, 26222, 26592, 26961, 27330, 27700, 28069,
        28438, 28808, 29177, 29546, 29916, 30285, 30654, 31024, 31393, 31762, 32132,
        32501, 32870, 33240, 33609, 33978, 34348, 34717, 35086, 35456, 35825, 36194,
        36564, 36933, 37302, 37672, 38041, 38410, 38780, 39149, 39518, 39888, 40257,
        40626, 40996, 41365, 41734, 42104, 42473, 42842, 43212, 43581, 43950, 44320,
        44689, 45058, 45428, 45797, 46166, 46536, 46905};

static const int16_t kAvgDecayTime = 250;  // frames; < 3000

int32_t WebRtcAgc_CalculateGainTable(int32_t *gainTable,       // Q16
                                     int16_t digCompGaindB,    // Q0
                                     int16_t targetLevelDbfs,  // Q0
                                     uint8_t limiterEnable,
                                     int16_t analogTarget)  // Q0
{
    // This function generates the compressor gain table used in the fixed digital
    // part.
    uint32_t tmpU32no1, tmpU32no2, absInLevel, logApprox;
    int32_t inLevel, limiterLvl;
    int32_t tmp32, tmp32no1, tmp32no2, numFIX, den, y32;
    const uint16_t kLog10 = 54426;    // log2(10)     in Q14
    const uint16_t kLog10_2 = 49321;  // 10*log10(2)  in Q14
    const uint16_t kLogE_1 = 23637;   // log2(e)      in Q14
    uint16_t constMaxGain;
    uint16_t tmpU16, intPart, fracPart;
    const int16_t kCompRatio = 3;
    //  const int16_t kSoftLimiterLeft = 1;
    int16_t limiterOffset = 0;  // Limiter offset
    int16_t limiterIdx, limiterLvlX;
    int16_t constLinApprox, maxGain, diffGain;//zeroGainLvl
    int16_t i, tmp16, tmp16no1;
    int zeros, zerosScale;

    // Constants
    //    kLogE_1 = 23637; // log2(e)      in Q14
    //    kLog10 = 54426; // log2(10)     in Q14
    //    kLog10_2 = 49321; // 10*log10(2)  in Q14

    // Calculate maximum digital gain and zero gain level
    tmp32no1 = (digCompGaindB - analogTarget) * (kCompRatio - 1);
    tmp16no1 = analogTarget - targetLevelDbfs;
    tmp16no1 +=
            DivW32W16ResW16(tmp32no1 + (kCompRatio >> 1), kCompRatio);
    maxGain = MAX(tmp16no1, (analogTarget - targetLevelDbfs));
    //  tmp32no1 = maxGain * kCompRatio;
    //  zeroGainLvl = digCompGaindB;
    // zeroGainLvl -= WebRtcSpl_DivW32W16ResW16(tmp32no1 + ((kCompRatio - 1) >> 1),      kCompRatio - 1);
    if ((digCompGaindB <= analogTarget) && (limiterEnable)) {
        //zeroGainLvl += (analogTarget - digCompGaindB + kSoftLimiterLeft);
        limiterOffset = 0;
    }

    // Calculate the difference between maximum gain and gain at 0dB0v:
    //  diffGain = maxGain + (compRatio-1)*zeroGainLvl/compRatio
    //           = (compRatio-1)*digCompGaindB/compRatio
    tmp32no1 = digCompGaindB * (kCompRatio - 1);
    diffGain =
            DivW32W16ResW16(tmp32no1 + (kCompRatio >> 1), kCompRatio);
    if (diffGain < 0 || diffGain >= kGenFuncTableSize) {
        assert(0);
        return -1;
    }

    // Calculate the limiter level and index:
    //  limiterLvlX = analogTarget - limiterOffset
    //  limiterLvl  = targetLevelDbfs + limiterOffset/compRatio
    limiterLvlX = analogTarget - limiterOffset;
    limiterIdx = 2 + DivW32W16ResW16((int32_t) limiterLvlX * (1 << 13),
                                     kLog10_2 / 2);
    tmp16no1 =
            DivW32W16ResW16(limiterOffset + (kCompRatio >> 1), kCompRatio);
    limiterLvl = targetLevelDbfs + tmp16no1;

    // Calculate (through table lookup):
    //  constMaxGain = log2(1+2^(log2(e)*diffGain)); (in Q8)
    constMaxGain = kGenFuncTable[diffGain];  // in Q8

    // Calculate a parameter used to approximate the fractional part of 2^x with a
    // piecewise linear function in Q14:
    //  constLinApprox = round(3/2*(4*(3-2*sqrt(2))/(log(2)^2)-0.5)*2^14);
    constLinApprox = 22817;  // in Q14

    // Calculate a denominator used in the exponential part to convert from dB to
    // linear scale:
    //  den = 20*constMaxGain (in Q8)
    den = ((int32_t) (int16_t) (20) * (uint16_t) (constMaxGain));  // in Q8

    for (i = 0; i < 32; i++) {
        // Calculate scaled input level (compressor):
        //  inLevel =
        //  fix((-constLog10_2*(compRatio-1)*(1-i)+fix(compRatio/2))/compRatio)
        tmp16 = (int16_t) ((kCompRatio - 1) * (i - 1));       // Q0
        tmp32 = ((int32_t) (int16_t) (tmp16) * (uint16_t) (kLog10_2)) + 1;  // Q14
        inLevel = DivW32W16(tmp32, kCompRatio);    // Q14

        // Calculate diffGain-inLevel, to map using the genFuncTable
        inLevel = (int32_t) diffGain * (1 << 14) - inLevel;  // Q14

        // Make calculations on abs(inLevel) and compensate for the sign afterwards.

        absInLevel = (uint32_t) (((int32_t) (inLevel) >= 0) ? ((int32_t) (inLevel)) : -((int32_t) (inLevel))); // Q14

        // LUT with interpolation
        intPart = (uint16_t) (absInLevel >> 14);
        fracPart =
                (uint16_t) (absInLevel & 0x00003FFF);  // extract the fractional part
        tmpU16 = kGenFuncTable[intPart + 1] - kGenFuncTable[intPart];  // Q8
        tmpU32no1 = tmpU16 * fracPart;                                 // Q22
        tmpU32no1 += (uint32_t) kGenFuncTable[intPart] << 14;           // Q22
        logApprox = tmpU32no1 >> 8;                                    // Q14
        // Compensate for negative exponent using the relation:
        //  log2(1 + 2^-x) = log2(1 + 2^x) - x
        if (inLevel < 0) {
            zeros = NormU32(absInLevel);
            zerosScale = 0;
            if (zeros < 15) {
                // Not enough space for multiplication
                tmpU32no2 = absInLevel >> (15 - zeros);                 // Q(zeros-1)


                tmpU32no2 = ((uint32_t) ((uint32_t) (tmpU32no2) * (uint16_t) (kLogE_1)));  // Q(zeros+13)
                if (zeros < 9) {
                    zerosScale = 9 - zeros;
                    tmpU32no1 >>= zerosScale;  // Q(zeros+13)
                } else {
                    tmpU32no2 >>= zeros - 9;  // Q22
                }
            } else {
                tmpU32no2 = ((uint32_t) ((uint32_t) (absInLevel) * (uint16_t) (kLogE_1)));  // Q28
                tmpU32no2 >>= 6;                                         // Q22
            }
            logApprox = 0;
            if (tmpU32no2 < tmpU32no1) {
                logApprox = (tmpU32no1 - tmpU32no2) >> (8 - zerosScale);  // Q14
            }
        }
        numFIX = (maxGain * constMaxGain) * (1 << 6);  // Q14
        numFIX -= (int32_t) logApprox * diffGain;       // Q14

        // Calculate ratio
        // Shift |numFIX| as much as possible.
        // Ensure we avoid wrap-around in |den| as well.
        if (numFIX > (den >> 8) || -numFIX > (den >> 8))  // |den| is Q8.
        {
            zeros = NormW32(numFIX);
        } else {
            zeros = NormW32(den) + 8;
        }
        numFIX *= 1 << zeros;  // Q(14+zeros)

        // Shift den so we end up in Qy1
        tmp32no1 = SHIFT_W32(den, zeros - 9);  // Q(zeros - 1)
        y32 = numFIX / tmp32no1;  // in Q15
        // This is to do rounding in Q14.
        y32 = y32 >= 0 ? (y32 + 1) >> 1 : -((-y32 + 1) >> 1);

        if (limiterEnable && (i < limiterIdx)) {
            tmp32 = ((int32_t) (int16_t) (i - 1) * (uint16_t) (kLog10_2));  // Q14
            tmp32 -= limiterLvl * (1 << 14);                 // Q14
            y32 = DivW32W16(tmp32 + 10, 20);
        }
        if (y32 > 39000) {
            tmp32 = (y32 >> 1) * kLog10 + 4096;  // in Q27
            tmp32 >>= 13;                        // In Q14.
        } else {
            tmp32 = y32 * kLog10 + 8192;  // in Q28
            tmp32 >>= 14;                 // In Q14.
        }
        tmp32 += 16 << 14;  // in Q14 (Make sure final output is in Q16)

        // Calculate power
        if (tmp32 > 0) {
            intPart = (int16_t) (tmp32 >> 14);
            fracPart = (uint16_t) (tmp32 & 0x00003FFF);  // in Q14
            if ((fracPart >> 13) != 0) {
                tmp16 = (2 << 14) - constLinApprox;
                tmp32no2 = (1 << 14) - fracPart;
                tmp32no2 *= tmp16;
                tmp32no2 >>= 13;
                tmp32no2 = (1 << 14) - tmp32no2;
            } else {
                tmp16 = constLinApprox - (1 << 14);
                tmp32no2 = (fracPart * tmp16) >> 13;
            }
            fracPart = (uint16_t) tmp32no2;
            gainTable[i] =
                    (1 << intPart) + SHIFT_W32(fracPart, intPart - 14);
        } else {
            gainTable[i] = 0;
        }
    }

    return 0;
}

int32_t WebRtcAgc_InitDigital(DigitalAgc *stt, int16_t agcMode) {
    if (agcMode == kAgcModeFixedDigital) {
        // start at minimum to find correct gain faster
        stt->capacitorSlow = 0;
    } else {
        // start out with 0 dB gain
        stt->capacitorSlow = 134217728;  // (int32_t)(0.125f * 32768.0f * 32768.0f);
    }
    stt->capacitorFast = 0;
    stt->gain = 65536;
    stt->gatePrevious = 0;
    stt->agcMode = agcMode;
#ifdef WEBRTC_AGC_DEBUG_DUMP
    stt->frameCounter = 0;
#endif

    // initialize VADs
    WebRtcAgc_InitVad(&stt->vadNearend);
    WebRtcAgc_InitVad(&stt->vadFarend);

    return 0;
}

int32_t WebRtcAgc_AddFarendToDigital(DigitalAgc *stt,
                                     const int16_t *in_far,
                                     size_t nrSamples) {
    assert(stt);
    // VAD for far end
    WebRtcAgc_ProcessVad(&stt->vadFarend, in_far, nrSamples);

    return 0;
}

int32_t WebRtcAgc_ProcessDigital(DigitalAgc *stt,
                                 const int16_t *const *in_near,
                                 size_t num_bands,
                                 int16_t *const *out,
                                 uint32_t FS,
                                 int16_t lowlevelSignal) {
    // array for gains (one value per ms, incl start & end)
    int32_t gains[11];

    int32_t out_tmp, tmp32;
    int32_t env[10];
    int32_t max_nrg;
    int32_t cur_level;
    int32_t gain32, delta;
    int16_t logratio;
    int16_t lower_thr, upper_thr;
    int16_t zeros = 0, zeros_fast, frac = 0;
    int16_t decay;
    int16_t gate, gain_adj;
    int16_t k;
    size_t n, i, L;
    int16_t L2;  // samples/subframe

    // determine number of samples per ms
    if (FS == 8000) {
        L = 8;
        L2 = 3;
    } else if (FS == 16000 || FS == 32000 || FS == 48000) {
        L = 16;
        L2 = 4;
    } else {
        return -1;
    }

    for (i = 0; i < num_bands; ++i) {
        if (in_near[i] != out[i]) {
            // Only needed if they don't already point to the same place.
            memcpy(out[i], in_near[i], 10 * L * sizeof(in_near[i][0]));
        }
    }
    // VAD for near end
    logratio = WebRtcAgc_ProcessVad(&stt->vadNearend, out[0], L * 10);

    // Account for far end VAD
    if (stt->vadFarend.counter > 10) {
        tmp32 = 3 * logratio;
        logratio = (int16_t) ((tmp32 - stt->vadFarend.logRatio) >> 2);
    }

    // Determine decay factor depending on VAD
    //  upper_thr = 1.0f;
    //  lower_thr = 0.25f;
    upper_thr = 1024;  // Q10
    lower_thr = 0;     // Q10
    if (logratio > upper_thr) {
        // decay = -2^17 / DecayTime;  ->  -65
        decay = -65;
    } else if (logratio < lower_thr) {
        decay = 0;
    } else {
        // decay = (int16_t)(((lower_thr - logratio)
        //       * (2^27/(DecayTime*(upper_thr-lower_thr)))) >> 10);
        // SUBSTITUTED: 2^27/(DecayTime*(upper_thr-lower_thr))  ->  65
        tmp32 = (lower_thr - logratio) * 65;
        decay = (int16_t) (tmp32 >> 10);
    }

    // adjust decay factor for long silence (detected as low standard deviation)
    // This is only done in the adaptive modes
    if (stt->agcMode != kAgcModeFixedDigital) {
        if (stt->vadNearend.stdLongTerm < 4000) {
            decay = 0;
        } else if (stt->vadNearend.stdLongTerm < 8096) {
            // decay = (int16_t)(((stt->vadNearend.stdLongTerm - 4000) * decay) >>
            // 12);
            tmp32 = (stt->vadNearend.stdLongTerm - 4000) * decay;
            decay = (int16_t) (tmp32 >> 12);
        }

        if (lowlevelSignal != 0) {
            decay = 0;
        }
    }
#ifdef WEBRTC_AGC_DEBUG_DUMP
    stt->frameCounter++;
    fprintf(stt->logFile, "%5.2f\t%d\t%d\t%d\t", (float)(stt->frameCounter) / 100,
            logratio, decay, stt->vadNearend.stdLongTerm);
#endif
    // Find max amplitude per sub frame
    // iterate over sub frames
    for (k = 0; k < 10; k++) {
        // iterate over samples
        max_nrg = 0;
        for (n = 0; n < L; n++) {
            int32_t nrg = out[0][k * L + n] * out[0][k * L + n];
            if (nrg > max_nrg) {
                max_nrg = nrg;
            }
        }
        env[k] = max_nrg;
    }

    // Calculate gain per sub frame
    gains[0] = stt->gain;
    for (k = 0; k < 10; k++) {
        // Fast envelope follower
        //  decay time = -131000 / -1000 = 131 (ms)
        stt->capacitorFast =
                AGC_SCALEDIFF32(-1000, stt->capacitorFast, stt->capacitorFast);
        if (env[k] > stt->capacitorFast) {
            stt->capacitorFast = env[k];
        }
        // Slow envelope follower
        if (env[k] > stt->capacitorSlow) {
            // increase capacitorSlow
            stt->capacitorSlow = AGC_SCALEDIFF32(500, (env[k] - stt->capacitorSlow),
                                                 stt->capacitorSlow);
        } else {
            // decrease capacitorSlow
            stt->capacitorSlow =
                    AGC_SCALEDIFF32(decay, stt->capacitorSlow, stt->capacitorSlow);
        }

        // use maximum of both capacitors as current level
        if (stt->capacitorFast > stt->capacitorSlow) {
            cur_level = stt->capacitorFast;
        } else {
            cur_level = stt->capacitorSlow;
        }
        // Translate signal level into gain, using a piecewise linear approximation
        // find number of leading zeros
        zeros = NormU32((uint32_t) cur_level);
        if (cur_level == 0) {
            zeros = 31;
        }
        tmp32 = ((uint32_t) cur_level << zeros) & 0x7FFFFFFF;
        frac = (int16_t) (tmp32 >> 19);  // Q12.
        tmp32 = (stt->gainTable[zeros - 1] - stt->gainTable[zeros]) * frac;
        gains[k + 1] = stt->gainTable[zeros] + (tmp32 >> 12);
#ifdef WEBRTC_AGC_DEBUG_DUMP
        if (k == 0) {
          fprintf(stt->logFile, "%d\t%d\t%d\t%d\t%d\n", env[0], cur_level,
                  stt->capacitorFast, stt->capacitorSlow, zeros);
        }
#endif
    }

    // Gate processing (lower gain during absence of speech)
    zeros = (zeros << 9) - (frac >> 3);
    // find number of leading zeros
    zeros_fast = NormU32((uint32_t) stt->capacitorFast);
    if (stt->capacitorFast == 0) {
        zeros_fast = 31;
    }
    tmp32 = ((uint32_t) stt->capacitorFast << zeros_fast) & 0x7FFFFFFF;
    zeros_fast <<= 9;
    zeros_fast -= (int16_t) (tmp32 >> 22);

    gate = 1000 + zeros_fast - zeros - stt->vadNearend.stdShortTerm;

    if (gate < 0) {
        stt->gatePrevious = 0;
    } else {
        tmp32 = stt->gatePrevious * 7;
        gate = (int16_t) ((gate + tmp32) >> 3);
        stt->gatePrevious = gate;
    }
    // gate < 0     -> no gate
    // gate > 2500  -> max gate
    if (gate > 0) {
        if (gate < 2500) {
            gain_adj = (2500 - gate) >> 5;
        } else {
            gain_adj = 0;
        }
        for (k = 0; k < 10; k++) {
            if ((gains[k + 1] - stt->gainTable[0]) > 8388608) {
                // To prevent wraparound
                tmp32 = (gains[k + 1] - stt->gainTable[0]) >> 8;
                tmp32 *= 178 + gain_adj;
            } else {
                tmp32 = (gains[k + 1] - stt->gainTable[0]) * (178 + gain_adj);
                tmp32 >>= 8;
            }
            gains[k + 1] = stt->gainTable[0] + tmp32;
        }
    }

    // Limit gain to avoid overload distortion
    for (k = 0; k < 10; k++) {
        // To prevent wrap around
        zeros = 10;
        if (gains[k + 1] > 47453132) {
            zeros = 16 - NormW32(gains[k + 1]);
        }
        gain32 = (gains[k + 1] >> zeros) + 1;
        gain32 *= gain32;
        // check for overflow
        while (AGC_MUL32((env[k] >> 12) + 1, gain32) >
               SHIFT_W32((int32_t) 32767, 2 * (1 - zeros + 10))) {
            // multiply by 253/256 ==> -0.1 dB
            if (gains[k + 1] > 8388607) {
                // Prevent wrap around
                gains[k + 1] = (gains[k + 1] / 256) * 253;
            } else {
                gains[k + 1] = (gains[k + 1] * 253) / 256;
            }
            gain32 = (gains[k + 1] >> zeros) + 1;
            gain32 *= gain32;
        }
    }
    // gain reductions should be done 1 ms earlier than gain increases
    for (k = 1; k < 10; k++) {
        if (gains[k] > gains[k + 1]) {
            gains[k] = gains[k + 1];
        }
    }
    // save start gain for next frame
    stt->gain = gains[10];

    // Apply gain
    // handle first sub frame separately
    delta = (gains[1] - gains[0]) * (1 << (4 - L2));
    gain32 = gains[0] * (1 << 4);
    // iterate over samples
    for (n = 0; n < L; n++) {
        for (i = 0; i < num_bands; ++i) {
            tmp32 = out[i][n] * ((gain32 + 127) >> 7);
            out_tmp = tmp32 >> 16;
            if (out_tmp > 4095) {
                out[i][n] = (int16_t) 32767;
            } else if (out_tmp < -4096) {
                out[i][n] = (int16_t) -32768;
            } else {
                tmp32 = out[i][n] * (gain32 >> 4);
                out[i][n] = (int16_t) (tmp32 >> 16);
            }
        }
        //

        gain32 += delta;
    }
    // iterate over subframes
    for (k = 1; k < 10; k++) {
        delta = (gains[k + 1] - gains[k]) * (1 << (4 - L2));
        gain32 = gains[k] * (1 << 4);
        // iterate over samples
        for (n = 0; n < L; n++) {
            for (i = 0; i < num_bands; ++i) {
                int64_t tmp64 = ((int64_t) (out[i][k * L + n])) * (gain32 >> 4);
                tmp64 = tmp64 >> 16;
                if (tmp64 > 32767) {
                    out[i][k * L + n] = 32767;
                } else if (tmp64 < -32768) {
                    out[i][k * L + n] = -32768;
                } else {
                    out[i][k * L + n] = (int16_t) (tmp64);
                }
            }
            gain32 += delta;
        }
    }

    return 0;
}

void WebRtcAgc_InitVad(AgcVad *state) {
    int16_t k;

    state->HPstate = 0;   // state of high pass filter
    state->logRatio = 0;  // log( P(active) / P(inactive) )
    // average input level (Q10)
    state->meanLongTerm = 15 << 10;

    // variance of input level (Q8)
    state->varianceLongTerm = 500 << 8;

    state->stdLongTerm = 0;  // standard deviation of input level in dB
    // short-term average input level (Q10)
    state->meanShortTerm = 15 << 10;

    // short-term variance of input level (Q8)
    state->varianceShortTerm = 500 << 8;

    state->stdShortTerm =
            0;               // short-term standard deviation of input level in dB
    state->counter = 3;  // counts updates
    for (k = 0; k < 8; k++) {
        // downsampling filter
        state->downState[k] = 0;
    }
}

int16_t WebRtcAgc_ProcessVad(AgcVad *state,      // (i) VAD state
                             const int16_t *in,  // (i) Speech signal
                             size_t nrSamples)   // (i) number of samples
{
    uint32_t nrg;
    int32_t out, tmp32, tmp32b;
    uint16_t tmpU16;
    int16_t k, subfr, tmp16;
    int16_t buf1[8];
    int16_t buf2[4];
    int16_t HPstate;
    int16_t zeros, dB;

    // process in 10 sub frames of 1 ms (to save on memory)
    nrg = 0;
    HPstate = state->HPstate;
    for (subfr = 0; subfr < 10; subfr++) {
        // downsample to 4 kHz
        if (nrSamples == 160) {
            for (k = 0; k < 8; k++) {
                tmp32 = (int32_t) in[2 * k] + (int32_t) in[2 * k + 1];
                tmp32 >>= 1;
                buf1[k] = (int16_t) tmp32;
            }
            in += 16;

            downsampleBy2(buf1, 8, buf2, state->downState);
        } else {
            downsampleBy2(in, 8, buf2, state->downState);
            in += 8;
        }

        // high pass filter and compute energy
        for (k = 0; k < 4; k++) {
            out = buf2[k] + HPstate;
            tmp32 = 600 * out;
            HPstate = (int16_t) ((tmp32 >> 10) - buf2[k]);

            // Add 'out * out / 2**6' to 'nrg' in a non-overflowing
            // way. Guaranteed to work as long as 'out * out / 2**6' fits in
            // an int32_t.
            nrg += out * (out / (1 << 6));
            nrg += out * (out % (1 << 6)) / (1 << 6);
        }
    }
    state->HPstate = HPstate;

    // find number of leading zeros
    if (!(0xFFFF0000 & nrg)) {
        zeros = 16;
    } else {
        zeros = 0;
    }
    if (!(0xFF000000 & (nrg << zeros))) {
        zeros += 8;
    }
    if (!(0xF0000000 & (nrg << zeros))) {
        zeros += 4;
    }
    if (!(0xC0000000 & (nrg << zeros))) {
        zeros += 2;
    }
    if (!(0x80000000 & (nrg << zeros))) {
        zeros += 1;
    }

    // energy level (range {-32..30}) (Q10)
    dB = (15 - zeros) * (1 << 11);

    // Update statistics

    if (state->counter < kAvgDecayTime) {
        // decay time = AvgDecTime * 10 ms
        state->counter++;
    }

    // update short-term estimate of mean energy level (Q10)
    tmp32 = state->meanShortTerm * 15 + dB;
    state->meanShortTerm = (int16_t) (tmp32 >> 4);

    // update short-term estimate of variance in energy level (Q8)
    tmp32 = (dB * dB) >> 12;
    tmp32 += state->varianceShortTerm * 15;
    state->varianceShortTerm = tmp32 / 16;

    // update short-term estimate of standard deviation in energy level (Q10)
    tmp32 = state->meanShortTerm * state->meanShortTerm;
    tmp32 = (state->varianceShortTerm << 12) - tmp32;
    state->stdShortTerm = (int16_t) fast_sqrt(tmp32);

    // update long-term estimate of mean energy level (Q10)
    tmp32 = state->meanLongTerm * state->counter + dB;
    state->meanLongTerm =
            DivW32W16ResW16(tmp32, WebRtcSpl_AddSatW16(state->counter, 1));

    // update long-term estimate of variance in energy level (Q8)
    tmp32 = (dB * dB) >> 12;
    tmp32 += state->varianceLongTerm * state->counter;
    state->varianceLongTerm =
            DivW32W16(tmp32, WebRtcSpl_AddSatW16(state->counter, 1));

    // update long-term estimate of standard deviation in energy level (Q10)
    tmp32 = state->meanLongTerm * state->meanLongTerm;
    tmp32 = (state->varianceLongTerm << 12) - tmp32;
    state->stdLongTerm = (int16_t) fast_sqrt(tmp32);

    // update voice activity measure (Q10)
    tmp16 = 3 << 12;
    // TODO(bjornv): (dB - state->meanLongTerm) can overflow, e.g., in
    // ApmTest.Process unit test. Previously the macro WEBRTC_SPL_MUL_16_16()
    // was used, which did an intermediate cast to (int16_t), hence losing
    // significant bits. This cause logRatio to max out positive, rather than
    // negative. This is a bug, but has very little significance.
    tmp32 = tmp16 * (int16_t) (dB - state->meanLongTerm);
    tmp32 = DivW32W16(tmp32, state->stdLongTerm);
    tmpU16 = (13 << 12);
    tmp32b = ((int32_t) (int16_t) (state->logRatio) * (uint16_t) (tmpU16));
    tmp32 += tmp32b >> 10;

    state->logRatio = (int16_t) (tmp32 >> 6);

    // limit
    if (state->logRatio > 2048) {
        state->logRatio = 2048;
    }
    if (state->logRatio < -2048) {
        state->logRatio = -2048;
    }

    return state->logRatio;  // Q10
}

int WebRtcAgc_AddMic(void *state,
                     int16_t *const *in_mic,
                     size_t num_bands,
                     size_t samples) {
    int32_t nrg, max_nrg, sample, tmp32;
    int32_t *ptr;
    uint16_t targetGainIdx, gain;
    size_t i;
    int16_t n, L, tmp16, tmp_speech[16];
    LegacyAgc *stt;
    stt = (LegacyAgc *) state;

    if (stt->fs == 8000) {
        L = 8;
        if (samples != 80) {
            return -1;
        }
    } else {
        L = 16;
        if (samples != 160) {
            return -1;
        }
    }

    /* apply slowly varying digital gain */
    if (stt->micVol > stt->maxAnalog) {
        /* |maxLevel| is strictly >= |micVol|, so this condition should be
         * satisfied here, ensuring there is no divide-by-zero. */
        assert(stt->maxLevel > stt->maxAnalog);

        /* Q1 */
        tmp16 = (int16_t) (stt->micVol - stt->maxAnalog);
        tmp32 = (GAIN_TBL_LEN - 1) * tmp16;
        tmp16 = (int16_t) (stt->maxLevel - stt->maxAnalog);
        targetGainIdx = tmp32 / tmp16;
        assert(targetGainIdx < GAIN_TBL_LEN);

        /* Increment through the table towards the target gain.
         * If micVol drops below maxAnalog, we allow the gain
         * to be dropped immediately. */
        if (stt->gainTableIdx < targetGainIdx) {
            stt->gainTableIdx++;
        } else if (stt->gainTableIdx > targetGainIdx) {
            stt->gainTableIdx--;
        }

        /* Q12 */
        gain = kGainTableAnalog[stt->gainTableIdx];

        for (i = 0; i < samples; i++) {
            size_t j;
            for (j = 0; j < num_bands; ++j) {
                sample = (in_mic[j][i] * gain) >> 12;
                if (sample > 32767) {
                    in_mic[j][i] = 32767;
                } else if (sample < -32768) {
                    in_mic[j][i] = -32768;
                } else {
                    in_mic[j][i] = (int16_t) sample;
                }
            }
        }
    } else {
        stt->gainTableIdx = 0;
    }

    /* compute envelope */
    if (stt->inQueue > 0) {
        ptr = stt->env[1];
    } else {
        ptr = stt->env[0];
    }

    for (i = 0; i < kNumSubframes; i++) {
        /* iterate over samples */
        max_nrg = 0;
        for (n = 0; n < L; n++) {
            nrg = in_mic[0][i * L + n] * in_mic[0][i * L + n];
            if (nrg > max_nrg) {
                max_nrg = nrg;
            }
        }
        ptr[i] = max_nrg;
    }

    /* compute energy */
    if (stt->inQueue > 0) {
        ptr = stt->Rxx16w32_array[1];
    } else {
        ptr = stt->Rxx16w32_array[0];
    }

    for (i = 0; i < kNumSubframes / 2; i++) {
        if (stt->fs == 16000) {
            downsampleBy2(&in_mic[0][i * 32], 32, tmp_speech,
                          stt->filterState);
        } else {
            memcpy(tmp_speech, &in_mic[0][i * 16], 16 * sizeof(short));
        }
        /* Compute energy in blocks of 16 samples */
        ptr[i] = DotProductWithScale(tmp_speech, tmp_speech, 16, 4);
    }

    /* update queue information */
    if (stt->inQueue == 0) {
        stt->inQueue = 1;
    } else {
        stt->inQueue = 2;
    }

    /* call VAD (use low band only) */
    WebRtcAgc_ProcessVad(&stt->vadMic, in_mic[0], samples);

    return 0;
}

int WebRtcAgc_AddFarend(void *state, const int16_t *in_far, size_t samples) {
    LegacyAgc *stt = (LegacyAgc *) state;

    int err = WebRtcAgc_GetAddFarendError(state, samples);

    if (err != 0)
        return err;

    return WebRtcAgc_AddFarendToDigital(&stt->digitalAgc, in_far, samples);
}

int WebRtcAgc_GetAddFarendError(void *state, size_t samples) {
    LegacyAgc *stt;
    stt = (LegacyAgc *) state;

    if (stt == NULL)
        return -1;

    if (stt->fs == 8000) {
        if (samples != 80)
            return -1;
    } else if (stt->fs == 16000 || stt->fs == 32000 || stt->fs == 48000) {
        if (samples != 160)
            return -1;
    } else {
        return -1;
    }

    return 0;
}

int WebRtcAgc_VirtualMic(void *agcInst,
                         int16_t *const *in_near,
                         size_t num_bands,
                         size_t samples,
                         int32_t micLevelIn,
                         int32_t *micLevelOut) {
    int32_t tmpFlt, micLevelTmp, gainIdx;
    uint16_t gain;
    size_t ii, j;
    LegacyAgc *stt;

    uint32_t nrg;
    size_t sampleCntr;
    uint32_t frameNrg = 0;
    uint32_t frameNrgLimit = 5500;
    int16_t numZeroCrossing = 0;
    const int16_t kZeroCrossingLowLim = 15;
    const int16_t kZeroCrossingHighLim = 20;

    stt = (LegacyAgc *) agcInst;

    /*
     *  Before applying gain decide if this is a low-level signal.
     *  The idea is that digital AGC will not adapt to low-level
     *  signals.
     */
    if (stt->fs != 8000) {
        frameNrgLimit = frameNrgLimit << 1;
    }

    frameNrg = (uint32_t) (in_near[0][0] * in_near[0][0]);
    for (sampleCntr = 1; sampleCntr < samples; sampleCntr++) {
        // increment frame energy if it is less than the limit
        // the correct value of the energy is not important
        if (frameNrg < frameNrgLimit) {
            nrg = (uint32_t) (in_near[0][sampleCntr] * in_near[0][sampleCntr]);
            frameNrg += nrg;
        }

        // Count the zero crossings
        numZeroCrossing +=
                ((in_near[0][sampleCntr] ^ in_near[0][sampleCntr - 1]) < 0);
    }

    if ((frameNrg < 500) || (numZeroCrossing <= 5)) {
        stt->lowLevelSignal = 1;
    } else if (numZeroCrossing <= kZeroCrossingLowLim) {
        stt->lowLevelSignal = 0;
    } else if (frameNrg <= frameNrgLimit) {
        stt->lowLevelSignal = 1;
    } else if (numZeroCrossing >= kZeroCrossingHighLim) {
        stt->lowLevelSignal = 1;
    } else {
        stt->lowLevelSignal = 0;
    }

    micLevelTmp = micLevelIn << stt->scale;
    /* Set desired level */
    gainIdx = stt->micVol;
    if (stt->micVol > stt->maxAnalog) {
        gainIdx = stt->maxAnalog;
    }
    if (micLevelTmp != stt->micRef) {
        /* Something has happened with the physical level, restart. */
        stt->micRef = micLevelTmp;
        stt->micVol = 127;
        *micLevelOut = 127;
        stt->micGainIdx = 127;
        gainIdx = 127;
    }
    /* Pre-process the signal to emulate the microphone level. */
    /* Take one step at a time in the gain table. */
    if (gainIdx > 127) {
        gain = kGainTableVirtualMic[gainIdx - 128];
    } else {
        gain = kSuppressionTableVirtualMic[127 - gainIdx];
    }
    for (ii = 0; ii < samples; ii++) {
        tmpFlt = (in_near[0][ii] * gain) >> 10;
        if (tmpFlt > 32767) {
            tmpFlt = 32767;
            gainIdx--;
            if (gainIdx >= 127) {
                gain = kGainTableVirtualMic[gainIdx - 127];
            } else {
                gain = kSuppressionTableVirtualMic[127 - gainIdx];
            }
        }
        if (tmpFlt < -32768) {
            tmpFlt = -32768;
            gainIdx--;
            if (gainIdx >= 127) {
                gain = kGainTableVirtualMic[gainIdx - 127];
            } else {
                gain = kSuppressionTableVirtualMic[127 - gainIdx];
            }
        }
        in_near[0][ii] = (int16_t) tmpFlt;
        for (j = 1; j < num_bands; ++j) {
            tmpFlt = (in_near[j][ii] * gain) >> 10;
            if (tmpFlt > 32767) {
                tmpFlt = 32767;
            }
            if (tmpFlt < -32768) {
                tmpFlt = -32768;
            }
            in_near[j][ii] = (int16_t) tmpFlt;
        }
    }
    /* Set the level we (finally) used */
    stt->micGainIdx = gainIdx;
    //    *micLevelOut = stt->micGainIdx;
    *micLevelOut = stt->micGainIdx >> stt->scale;
    /* Add to Mic as if it was the output from a true microphone */
    if (WebRtcAgc_AddMic(agcInst, in_near, num_bands, samples) != 0) {
        return -1;
    }
    return 0;
}

void WebRtcAgc_UpdateAgcThresholds(LegacyAgc *stt) {
    int16_t tmp16;
#ifdef MIC_LEVEL_FEEDBACK
    int zeros;

    if (stt->micLvlSat) {
      /* Lower the analog target level since we have reached its maximum */
      zeros = WebRtcSpl_NormW32(stt->Rxx160_LPw32);
      stt->targetIdxOffset = (3 * zeros - stt->targetIdx - 2) / 4;
    }
#endif

    /* Set analog target level in envelope dBOv scale */
    tmp16 = (DIFF_REF_TO_ANALOG * stt->compressionGaindB) + ANALOG_TARGET_LEVEL_2;
    tmp16 = DivW32W16ResW16((int32_t) tmp16, ANALOG_TARGET_LEVEL);
    stt->analogTarget = DIGITAL_REF_AT_0_COMP_GAIN + tmp16;
    if (stt->analogTarget < DIGITAL_REF_AT_0_COMP_GAIN) {
        stt->analogTarget = DIGITAL_REF_AT_0_COMP_GAIN;
    }
    if (stt->agcMode == kAgcModeFixedDigital) {
        /* Adjust for different parameter interpretation in FixedDigital mode */
        stt->analogTarget = stt->compressionGaindB;
    }
#ifdef MIC_LEVEL_FEEDBACK
    stt->analogTarget += stt->targetIdxOffset;
#endif
    /* Since the offset between RMS and ENV is not constant, we should make this
     * into a
     * table, but for now, we'll stick with a constant, tuned for the chosen
     * analog
     * target level.
     */
    stt->targetIdx = ANALOG_TARGET_LEVEL + OFFSET_ENV_TO_RMS;
#ifdef MIC_LEVEL_FEEDBACK
    stt->targetIdx += stt->targetIdxOffset;
#endif
    /* Analog adaptation limits */
    /* analogTargetLevel = round((32767*10^(-targetIdx/20))^2*16/2^7) */
    stt->analogTargetLevel =
            RXX_BUFFER_LEN * kTargetLevelTable[stt->targetIdx]; /* ex. -20 dBov */
    stt->startUpperLimit =
            RXX_BUFFER_LEN * kTargetLevelTable[stt->targetIdx - 1]; /* -19 dBov */
    stt->startLowerLimit =
            RXX_BUFFER_LEN * kTargetLevelTable[stt->targetIdx + 1]; /* -21 dBov */
    stt->upperPrimaryLimit =
            RXX_BUFFER_LEN * kTargetLevelTable[stt->targetIdx - 2]; /* -18 dBov */
    stt->lowerPrimaryLimit =
            RXX_BUFFER_LEN * kTargetLevelTable[stt->targetIdx + 2]; /* -22 dBov */
    stt->upperSecondaryLimit =
            RXX_BUFFER_LEN * kTargetLevelTable[stt->targetIdx - 5]; /* -15 dBov */
    stt->lowerSecondaryLimit =
            RXX_BUFFER_LEN * kTargetLevelTable[stt->targetIdx + 5]; /* -25 dBov */
    stt->upperLimit = stt->startUpperLimit;
    stt->lowerLimit = stt->startLowerLimit;
}

void WebRtcAgc_SaturationCtrl(LegacyAgc *stt,
                              uint8_t *saturated,
                              const int32_t *env) {
    int16_t i, tmpW16;

    /* Check if the signal is saturated */
    for (i = 0; i < 10; i++) {
        tmpW16 = (int16_t) (env[i] >> 20);
        if (tmpW16 > 875) {
            stt->envSum += tmpW16;
        }
    }

    if (stt->envSum > 25000) {
        *saturated = 1;
        stt->envSum = 0;
    }

    /* stt->envSum *= 0.99; */
    stt->envSum = (int16_t) ((stt->envSum * 32440) >> 15);
}

void WebRtcAgc_ZeroCtrl(LegacyAgc *stt, int32_t *inMicLevel, const int32_t *env) {
    int16_t i;
    int64_t tmp = 0;
    int32_t midVal;

    /* Is the input signal zero? */
    for (i = 0; i < 10; i++) {
        tmp += env[i];
    }

    /* Each block is allowed to have a few non-zero
     * samples.
     */
    if (tmp < 500) {
        stt->msZero += 10;
    } else {
        stt->msZero = 0;
    }

    if (stt->muteGuardMs > 0) {
        stt->muteGuardMs -= 10;
    }

    if (stt->msZero > 500) {
        stt->msZero = 0;

        /* Increase microphone level only if it's less than 50% */
        midVal = (stt->maxAnalog + stt->minLevel + 1) / 2;
        if (*inMicLevel < midVal) {
            /* *inMicLevel *= 1.1; */
            *inMicLevel = (1126 * *inMicLevel) >> 10;
            /* Reduces risk of a muted mic repeatedly triggering excessive levels due
             * to zero signal detection. */
            *inMicLevel = MIN(*inMicLevel, stt->zeroCtrlMax);
            stt->micVol = *inMicLevel;
        }

#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt,
                "\t\tAGC->zeroCntrl, frame %d: 500 ms under threshold,"
                " micVol: %d\n",
                stt->fcount, stt->micVol);
#endif

        stt->activeSpeech = 0;
        stt->Rxx16_LPw32Max = 0;

        /* The AGC has a tendency (due to problems with the VAD parameters), to
         * vastly increase the volume after a muting event. This timer prevents
         * upwards adaptation for a short period. */
        stt->muteGuardMs = kMuteGuardTimeMs;
    }
}

void WebRtcAgc_SpeakerInactiveCtrl(LegacyAgc *stt) {
    /* Check if the near end speaker is inactive.
     * If that is the case the VAD threshold is
     * increased since the VAD speech model gets
     * more sensitive to any sound after a long
     * silence.
     */

    int32_t tmp32;
    int16_t vadThresh;

    if (stt->vadMic.stdLongTerm < 2500) {
        stt->vadThreshold = 1500;
    } else {
        vadThresh = kNormalVadThreshold;
        if (stt->vadMic.stdLongTerm < 4500) {
            /* Scale between min and max threshold */
            vadThresh += (4500 - stt->vadMic.stdLongTerm) / 2;
        }

        /* stt->vadThreshold = (31 * stt->vadThreshold + vadThresh) / 32; */
        tmp32 = vadThresh + 31 * stt->vadThreshold;
        stt->vadThreshold = (int16_t) (tmp32 >> 5);
    }
}

void WebRtcAgc_ExpCurve(int16_t volume, int16_t *index) {
    // volume in Q14
    // index in [0-7]
    /* 8 different curves */
    if (volume > 5243) {
        if (volume > 7864) {
            if (volume > 12124) {
                *index = 7;
            } else {
                *index = 6;
            }
        } else {
            if (volume > 6554) {
                *index = 5;
            } else {
                *index = 4;
            }
        }
    } else {
        if (volume > 2621) {
            if (volume > 3932) {
                *index = 3;
            } else {
                *index = 2;
            }
        } else {
            if (volume > 1311) {
                *index = 1;
            } else {
                *index = 0;
            }
        }
    }
}

int32_t WebRtcAgc_ProcessAnalog(void *state,
                                int32_t inMicLevel,
                                int32_t *outMicLevel,
                                int16_t vadLogRatio,
                                int16_t echo,
                                uint8_t *saturationWarning) {
    uint32_t tmpU32;
    int32_t Rxx16w32, tmp32;
    int32_t inMicLevelTmp, lastMicVol;
    int16_t i;
    uint8_t saturated = 0;
    LegacyAgc *stt;

    stt = (LegacyAgc *) state;
    inMicLevelTmp = inMicLevel << stt->scale;

    if (inMicLevelTmp > stt->maxAnalog) {
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt, "\tAGC->ProcessAnalog, frame %d: micLvl > maxAnalog\n",
                stt->fcount);
#endif
        return -1;
    } else if (inMicLevelTmp < stt->minLevel) {
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt, "\tAGC->ProcessAnalog, frame %d: micLvl < minLevel\n",
                stt->fcount);
#endif
        return -1;
    }

    if (stt->firstCall == 0) {
        int32_t tmpVol;
        stt->firstCall = 1;
        tmp32 = ((stt->maxLevel - stt->minLevel) * 51) >> 9;
        tmpVol = (stt->minLevel + tmp32);

        /* If the mic level is very low at start, increase it! */
        if ((inMicLevelTmp < tmpVol) && (stt->agcMode == kAgcModeAdaptiveAnalog)) {
            inMicLevelTmp = tmpVol;
        }
        stt->micVol = inMicLevelTmp;
    }

    /* Set the mic level to the previous output value if there is digital input
     * gain */
    if ((inMicLevelTmp == stt->maxAnalog) && (stt->micVol > stt->maxAnalog)) {
        inMicLevelTmp = stt->micVol;
    }

    /* If the mic level was manually changed to a very low value raise it! */
    if ((inMicLevelTmp != stt->micVol) && (inMicLevelTmp < stt->minOutput)) {
        tmp32 = ((stt->maxLevel - stt->minLevel) * 51) >> 9;
        inMicLevelTmp = (stt->minLevel + tmp32);
        stt->micVol = inMicLevelTmp;
#ifdef MIC_LEVEL_FEEDBACK
        // stt->numBlocksMicLvlSat = 0;
#endif
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt,
                "\tAGC->ProcessAnalog, frame %d: micLvl < minLevel by manual"
                " decrease, raise vol\n",
                stt->fcount);
#endif
    }

    if (inMicLevelTmp != stt->micVol) {
        if (inMicLevel == stt->lastInMicLevel) {
            // We requested a volume adjustment, but it didn't occur. This is
            // probably due to a coarse quantization of the volume slider.
            // Restore the requested value to prevent getting stuck.
            inMicLevelTmp = stt->micVol;
        } else {
            // As long as the value changed, update to match.
            stt->micVol = inMicLevelTmp;
        }
    }

    if (inMicLevelTmp > stt->maxLevel) {
        // Always allow the user to raise the volume above the maxLevel.
        stt->maxLevel = inMicLevelTmp;
    }

    // Store last value here, after we've taken care of manual updates etc.
    stt->lastInMicLevel = inMicLevel;
    lastMicVol = stt->micVol;

    /* Checks if the signal is saturated. Also a check if individual samples
     * are larger than 12000 is done. If they are the counter for increasing
     * the volume level is set to -100ms
     */
    WebRtcAgc_SaturationCtrl(stt, &saturated, stt->env[0]);

    /* The AGC is always allowed to lower the level if the signal is saturated */
    if (saturated == 1) {
        /* Lower the recording level
         * Rxx160_LP is adjusted down because it is so slow it could
         * cause the AGC to make wrong decisions. */
        /* stt->Rxx160_LPw32 *= 0.875; */
        stt->Rxx160_LPw32 = (stt->Rxx160_LPw32 / 8) * 7;

        stt->zeroCtrlMax = stt->micVol;

        /* stt->micVol *= 0.903; */
        tmp32 = inMicLevelTmp - stt->minLevel;
        tmpU32 = ((uint32_t) ((uint32_t) (29591) * (uint32_t) (tmp32)));
        stt->micVol = (tmpU32 >> 15) + stt->minLevel;
        if (stt->micVol > lastMicVol - 2) {
            stt->micVol = lastMicVol - 2;
        }
        inMicLevelTmp = stt->micVol;

#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt,
                "\tAGC->ProcessAnalog, frame %d: saturated, micVol = %d\n",
                stt->fcount, stt->micVol);
#endif

        if (stt->micVol < stt->minOutput) {
            *saturationWarning = 1;
        }

        /* Reset counter for decrease of volume level to avoid
         * decreasing too much. The saturation control can still
         * lower the level if needed. */
        stt->msTooHigh = -100;

        /* Enable the control mechanism to ensure that our measure,
         * Rxx160_LP, is in the correct range. This must be done since
         * the measure is very slow. */
        stt->activeSpeech = 0;
        stt->Rxx16_LPw32Max = 0;

        /* Reset to initial values */
        stt->msecSpeechInnerChange = kMsecSpeechInner;
        stt->msecSpeechOuterChange = kMsecSpeechOuter;
        stt->changeToSlowMode = 0;

        stt->muteGuardMs = 0;

        stt->upperLimit = stt->startUpperLimit;
        stt->lowerLimit = stt->startLowerLimit;
#ifdef MIC_LEVEL_FEEDBACK
        // stt->numBlocksMicLvlSat = 0;
#endif
    }

    /* Check if the input speech is zero. If so the mic volume
     * is increased. On some computers the input is zero up as high
     * level as 17% */
    WebRtcAgc_ZeroCtrl(stt, &inMicLevelTmp, stt->env[0]);

    /* Check if the near end speaker is inactive.
     * If that is the case the VAD threshold is
     * increased since the VAD speech model gets
     * more sensitive to any sound after a long
     * silence.
     */
    WebRtcAgc_SpeakerInactiveCtrl(stt);

    for (i = 0; i < 5; i++) {
        /* Computed on blocks of 16 samples */

        Rxx16w32 = stt->Rxx16w32_array[0][i];

        /* Rxx160w32 in Q(-7) */
        tmp32 = (Rxx16w32 - stt->Rxx16_vectorw32[stt->Rxx16pos]) >> 3;
        stt->Rxx160w32 = stt->Rxx160w32 + tmp32;
        stt->Rxx16_vectorw32[stt->Rxx16pos] = Rxx16w32;

        /* Circular buffer */
        stt->Rxx16pos++;
        if (stt->Rxx16pos == RXX_BUFFER_LEN) {
            stt->Rxx16pos = 0;
        }

        /* Rxx16_LPw32 in Q(-4) */
        tmp32 = (Rxx16w32 - stt->Rxx16_LPw32) >> kAlphaShortTerm;
        stt->Rxx16_LPw32 = (stt->Rxx16_LPw32) + tmp32;

        if (vadLogRatio > stt->vadThreshold) {
            /* Speech detected! */

            /* Check if Rxx160_LP is in the correct range. If
             * it is too high/low then we set it to the maximum of
             * Rxx16_LPw32 during the first 200ms of speech.
             */
            if (stt->activeSpeech < 250) {
                stt->activeSpeech += 2;

                if (stt->Rxx16_LPw32 > stt->Rxx16_LPw32Max) {
                    stt->Rxx16_LPw32Max = stt->Rxx16_LPw32;
                }
            } else if (stt->activeSpeech == 250) {
                stt->activeSpeech += 2;
                tmp32 = stt->Rxx16_LPw32Max >> 3;
                stt->Rxx160_LPw32 = tmp32 * RXX_BUFFER_LEN;
            }

            tmp32 = (stt->Rxx160w32 - stt->Rxx160_LPw32) >> kAlphaLongTerm;
            stt->Rxx160_LPw32 = stt->Rxx160_LPw32 + tmp32;

            if (stt->Rxx160_LPw32 > stt->upperSecondaryLimit) {
                stt->msTooHigh += 2;
                stt->msTooLow = 0;
                stt->changeToSlowMode = 0;

                if (stt->msTooHigh > stt->msecSpeechOuterChange) {
                    stt->msTooHigh = 0;

                    /* Lower the recording level */
                    /* Multiply by 0.828125 which corresponds to decreasing ~0.8dB */
                    tmp32 = stt->Rxx160_LPw32 >> 6;
                    stt->Rxx160_LPw32 = tmp32 * 53;

                    /* Reduce the max gain to avoid excessive oscillation
                     * (but never drop below the maximum analog level).
                     */
                    stt->maxLevel = (15 * stt->maxLevel + stt->micVol) / 16;
                    stt->maxLevel = MAX(stt->maxLevel, stt->maxAnalog);

                    stt->zeroCtrlMax = stt->micVol;

                    /* 0.95 in Q15 */
                    tmp32 = inMicLevelTmp - stt->minLevel;
                    tmpU32 = ((uint32_t) ((uint32_t) (31130) * (uint32_t) (tmp32)));
                    stt->micVol = (tmpU32 >> 15) + stt->minLevel;
                    if (stt->micVol > lastMicVol - 1) {
                        stt->micVol = lastMicVol - 1;
                    }
                    inMicLevelTmp = stt->micVol;

                    /* Enable the control mechanism to ensure that our measure,
                     * Rxx160_LP, is in the correct range.
                     */
                    stt->activeSpeech = 0;
                    stt->Rxx16_LPw32Max = 0;
#ifdef MIC_LEVEL_FEEDBACK
                    // stt->numBlocksMicLvlSat = 0;
#endif
#ifdef WEBRTC_AGC_DEBUG_DUMP
                    fprintf(stt->fpt,
                            "\tAGC->ProcessAnalog, frame %d: measure >"
                            " 2ndUpperLim, micVol = %d, maxLevel = %d\n",
                            stt->fcount, stt->micVol, stt->maxLevel);
#endif
                }
            } else if (stt->Rxx160_LPw32 > stt->upperLimit) {
                stt->msTooHigh += 2;
                stt->msTooLow = 0;
                stt->changeToSlowMode = 0;

                if (stt->msTooHigh > stt->msecSpeechInnerChange) {
                    /* Lower the recording level */
                    stt->msTooHigh = 0;
                    /* Multiply by 0.828125 which corresponds to decreasing ~0.8dB */
                    stt->Rxx160_LPw32 = (stt->Rxx160_LPw32 / 64) * 53;

                    /* Reduce the max gain to avoid excessive oscillation
                     * (but never drop below the maximum analog level).
                     */
                    stt->maxLevel = (15 * stt->maxLevel + stt->micVol) / 16;
                    stt->maxLevel = MAX(stt->maxLevel, stt->maxAnalog);

                    stt->zeroCtrlMax = stt->micVol;

                    /* 0.965 in Q15 */
                    //tmp32 = inMicLevelTmp - stt->minLevel;
                    tmpU32 = ((uint32_t) ((uint32_t) (31621) * (uint32_t) ((inMicLevelTmp - stt->minLevel))));
                    stt->micVol = (tmpU32 >> 15) + stt->minLevel;
                    if (stt->micVol > lastMicVol - 1) {
                        stt->micVol = lastMicVol - 1;
                    }
                    inMicLevelTmp = stt->micVol;

#ifdef MIC_LEVEL_FEEDBACK
                    // stt->numBlocksMicLvlSat = 0;
#endif
#ifdef WEBRTC_AGC_DEBUG_DUMP
                    fprintf(stt->fpt,
                            "\tAGC->ProcessAnalog, frame %d: measure >"
                            " UpperLim, micVol = %d, maxLevel = %d\n",
                            stt->fcount, stt->micVol, stt->maxLevel);
#endif
                }
            } else if (stt->Rxx160_LPw32 < stt->lowerSecondaryLimit) {
                stt->msTooHigh = 0;
                stt->changeToSlowMode = 0;
                stt->msTooLow += 2;

                if (stt->msTooLow > stt->msecSpeechOuterChange) {
                    /* Raise the recording level */
                    int16_t index, weightFIX;
                    int16_t volNormFIX = 16384;  // =1 in Q14.

                    stt->msTooLow = 0;

                    /* Normalize the volume level */
                    tmp32 = (inMicLevelTmp - stt->minLevel) << 14;
                    if (stt->maxInit != stt->minLevel) {
                        volNormFIX = tmp32 / (stt->maxInit - stt->minLevel);
                    }

                    /* Find correct curve */
                    WebRtcAgc_ExpCurve(volNormFIX, &index);

                    /* Compute weighting factor for the volume increase, 32^(-2*X)/2+1.05
                     */
                    weightFIX =
                            kOffset1[index] - (int16_t) ((kSlope1[index] * volNormFIX) >> 13);

                    /* stt->Rxx160_LPw32 *= 1.047 [~0.2 dB]; */
                    stt->Rxx160_LPw32 = (stt->Rxx160_LPw32 / 64) * 67;

                    //tmp32 = inMicLevelTmp - stt->minLevel;
                    tmpU32 =
                            ((uint32_t) weightFIX * (uint32_t) (inMicLevelTmp - stt->minLevel));
                    stt->micVol = (tmpU32 >> 14) + stt->minLevel;
                    if (stt->micVol < lastMicVol + 2) {
                        stt->micVol = lastMicVol + 2;
                    }

                    inMicLevelTmp = stt->micVol;

#ifdef MIC_LEVEL_FEEDBACK
                    /* Count ms in level saturation */
                    // if (stt->micVol > stt->maxAnalog) {
                    if (stt->micVol > 150) {
                      /* mic level is saturated */
                      stt->numBlocksMicLvlSat++;
                      fprintf(stderr, "Sat mic Level: %d\n", stt->numBlocksMicLvlSat);
                    }
#endif
#ifdef WEBRTC_AGC_DEBUG_DUMP
                    fprintf(stt->fpt,
                            "\tAGC->ProcessAnalog, frame %d: measure <"
                            " 2ndLowerLim, micVol = %d\n",
                            stt->fcount, stt->micVol);
#endif
                }
            } else if (stt->Rxx160_LPw32 < stt->lowerLimit) {
                stt->msTooHigh = 0;
                stt->changeToSlowMode = 0;
                stt->msTooLow += 2;

                if (stt->msTooLow > stt->msecSpeechInnerChange) {
                    /* Raise the recording level */
                    int16_t index, weightFIX;
                    int16_t volNormFIX = 16384;  // =1 in Q14.

                    stt->msTooLow = 0;

                    /* Normalize the volume level */
                    tmp32 = (inMicLevelTmp - stt->minLevel) << 14;
                    if (stt->maxInit != stt->minLevel) {
                        volNormFIX = tmp32 / (stt->maxInit - stt->minLevel);
                    }

                    /* Find correct curve */
                    WebRtcAgc_ExpCurve(volNormFIX, &index);

                    /* Compute weighting factor for the volume increase, (3.^(-2.*X))/8+1
                     */
                    weightFIX =
                            kOffset2[index] - (int16_t) ((kSlope2[index] * volNormFIX) >> 13);

                    /* stt->Rxx160_LPw32 *= 1.047 [~0.2 dB]; */
                    stt->Rxx160_LPw32 = (stt->Rxx160_LPw32 / 64) * 67;

                    //   tmp32 = inMicLevelTmp - stt->minLevel;
                    tmpU32 =
                            ((uint32_t) weightFIX * (uint32_t) (inMicLevelTmp - stt->minLevel));
                    stt->micVol = (tmpU32 >> 14) + stt->minLevel;
                    if (stt->micVol < lastMicVol + 1) {
                        stt->micVol = lastMicVol + 1;
                    }

                    inMicLevelTmp = stt->micVol;

#ifdef MIC_LEVEL_FEEDBACK
                    /* Count ms in level saturation */
                    // if (stt->micVol > stt->maxAnalog) {
                    if (stt->micVol > 150) {
                      /* mic level is saturated */
                      stt->numBlocksMicLvlSat++;
                      fprintf(stderr, "Sat mic Level: %d\n", stt->numBlocksMicLvlSat);
                    }
#endif
#ifdef WEBRTC_AGC_DEBUG_DUMP
                    fprintf(stt->fpt,
                            "\tAGC->ProcessAnalog, frame %d: measure < LowerLim, micVol "
                            "= %d\n",
                            stt->fcount, stt->micVol);
#endif
                }
            } else {
                /* The signal is inside the desired range which is:
                 * lowerLimit < Rxx160_LP/640 < upperLimit
                 */
                if (stt->changeToSlowMode > 4000) {
                    stt->msecSpeechInnerChange = 1000;
                    stt->msecSpeechOuterChange = 500;
                    stt->upperLimit = stt->upperPrimaryLimit;
                    stt->lowerLimit = stt->lowerPrimaryLimit;
                } else {
                    stt->changeToSlowMode += 2;  // in milliseconds
                }
                stt->msTooLow = 0;
                stt->msTooHigh = 0;

                stt->micVol = inMicLevelTmp;
            }
#ifdef MIC_LEVEL_FEEDBACK
            if (stt->numBlocksMicLvlSat > NUM_BLOCKS_IN_SAT_BEFORE_CHANGE_TARGET) {
              stt->micLvlSat = 1;
              fprintf(stderr, "target before = %d (%d)\n", stt->analogTargetLevel,
                      stt->targetIdx);
              WebRtcAgc_UpdateAgcThresholds(stt);
              WebRtcAgc_CalculateGainTable(
                  &(stt->digitalAgc.gainTable[0]), stt->compressionGaindB,
                  stt->targetLevelDbfs, stt->limiterEnable, stt->analogTarget);
              stt->numBlocksMicLvlSat = 0;
              stt->micLvlSat = 0;
              fprintf(stderr, "target offset = %d\n", stt->targetIdxOffset);
              fprintf(stderr, "target after  = %d (%d)\n", stt->analogTargetLevel,
                      stt->targetIdx);
            }
#endif
        }
    }

    /* Ensure gain is not increased in presence of echo or after a mute event
     * (but allow the zeroCtrl() increase on the frame of a mute detection).
     */
    if (echo == 1 ||
        (stt->muteGuardMs > 0 && stt->muteGuardMs < kMuteGuardTimeMs)) {
        if (stt->micVol > lastMicVol) {
            stt->micVol = lastMicVol;
        }
    }

    /* limit the gain */
    if (stt->micVol > stt->maxLevel) {
        stt->micVol = stt->maxLevel;
    } else if (stt->micVol < stt->minOutput) {
        stt->micVol = stt->minOutput;
    }

    *outMicLevel = MIN(stt->micVol, stt->maxAnalog) >> stt->scale;

    return 0;
}

int WebRtcAgc_Process(void *agcInst,
                      const int16_t *const *in_near,
                      size_t num_bands,
                      size_t samples,
                      int16_t *const *out,
                      int32_t inMicLevel,
                      int32_t *outMicLevel,
                      int16_t echo,
                      uint8_t *saturationWarning) {
    LegacyAgc *stt;

    stt = (LegacyAgc *) agcInst;

    //
    if (stt == NULL) {
        return -1;
    }
    //

    if (stt->fs == 8000) {
        if (samples != 80) {
            return -1;
        }
    } else if (stt->fs == 16000 || stt->fs == 32000 || stt->fs == 48000) {
        if (samples != 160) {
            return -1;
        }
    } else {
        return -1;
    }

    *saturationWarning = 0;
    // TODO(minyue): PUT IN RANGE CHECKING FOR INPUT LEVELS
    *outMicLevel = inMicLevel;

#ifdef WEBRTC_AGC_DEBUG_DUMP
    stt->fcount++;
#endif

    if (WebRtcAgc_ProcessDigital(&stt->digitalAgc, in_near, num_bands, out,
                                 stt->fs, stt->lowLevelSignal) == -1) {
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt, "AGC->Process, frame %d: Error from DigAGC\n\n",
                stt->fcount);
#endif
        return -1;
    }
    if (stt->agcMode < kAgcModeFixedDigital &&
        (stt->lowLevelSignal == 0 || stt->agcMode != kAgcModeAdaptiveDigital)) {
        if (WebRtcAgc_ProcessAnalog(agcInst, inMicLevel, outMicLevel,
                                    stt->vadMic.logRatio, echo,
                                    saturationWarning) == -1) {
            return -1;
        }
    }
#ifdef WEBRTC_AGC_DEBUG_DUMP
    fprintf(stt->agcLog, "%5d\t%d\t%d\t%d\t%d\n", stt->fcount, inMicLevel,
            *outMicLevel, stt->maxLevel, stt->micVol);
#endif

    /* update queue */
    if (stt->inQueue > 1) {
        memcpy(stt->env[0], stt->env[1], 10 * sizeof(int32_t));
        memcpy(stt->Rxx16w32_array[0], stt->Rxx16w32_array[1], 5 * sizeof(int32_t));
    }

    if (stt->inQueue > 0) {
        stt->inQueue--;
    }

    return 0;
}

int WebRtcAgc_set_config(void *agcInst, WebRtcAgcConfig agcConfig) {
    LegacyAgc *stt;
    stt = (LegacyAgc *) agcInst;

    if (stt == NULL) {
        return -1;
    }

    if (stt->initFlag != kInitCheck) {
        stt->lastError = AGC_UNINITIALIZED_ERROR;
        return -1;
    }

    if (agcConfig.limiterEnable != kAgcFalse &&
        agcConfig.limiterEnable != kAgcTrue) {
        stt->lastError = AGC_BAD_PARAMETER_ERROR;
        return -1;
    }
    stt->limiterEnable = agcConfig.limiterEnable;
    stt->compressionGaindB = agcConfig.compressionGaindB;
    if ((agcConfig.targetLevelDbfs < 0) || (agcConfig.targetLevelDbfs > 31)) {
        stt->lastError = AGC_BAD_PARAMETER_ERROR;
        return -1;
    }
    stt->targetLevelDbfs = agcConfig.targetLevelDbfs;

    if (stt->agcMode == kAgcModeFixedDigital) {
        /* Adjust for different parameter interpretation in FixedDigital mode */
        stt->compressionGaindB += agcConfig.targetLevelDbfs;
    }

    /* Update threshold levels for analog adaptation */
    WebRtcAgc_UpdateAgcThresholds(stt);

    /* Recalculate gain table */
    if (WebRtcAgc_CalculateGainTable(
            &(stt->digitalAgc.gainTable[0]), stt->compressionGaindB,
            stt->targetLevelDbfs, stt->limiterEnable, stt->analogTarget) == -1) {
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt, "AGC->set_config, frame %d: Error from calcGainTable\n\n",
                stt->fcount);
#endif
        return -1;
    }
    /* Store the config in a WebRtcAgcConfig */
    stt->usedConfig.compressionGaindB = agcConfig.compressionGaindB;
    stt->usedConfig.limiterEnable = agcConfig.limiterEnable;
    stt->usedConfig.targetLevelDbfs = agcConfig.targetLevelDbfs;

    return 0;
}

int WebRtcAgc_get_config(void *agcInst, WebRtcAgcConfig *config) {
    LegacyAgc *stt;
    stt = (LegacyAgc *) agcInst;

    if (stt == NULL) {
        return -1;
    }

    if (config == NULL) {
        stt->lastError = AGC_NULL_POINTER_ERROR;
        return -1;
    }

    if (stt->initFlag != kInitCheck) {
        stt->lastError = AGC_UNINITIALIZED_ERROR;
        return -1;
    }

    config->limiterEnable = stt->usedConfig.limiterEnable;
    config->targetLevelDbfs = stt->usedConfig.targetLevelDbfs;
    config->compressionGaindB = stt->usedConfig.compressionGaindB;

    return 0;
}

void *WebRtcAgc_Create() {
    LegacyAgc *stt = malloc(sizeof(LegacyAgc));

#ifdef WEBRTC_AGC_DEBUG_DUMP
    stt->fpt = fopen("./agc_test_log.txt", "wt");
    stt->agcLog = fopen("./agc_debug_log.txt", "wt");
    stt->digitalAgc.logFile = fopen("./agc_log.txt", "wt");
#endif

    stt->initFlag = 0;
    stt->lastError = 0;

    return stt;
}

void WebRtcAgc_Free(void *state) {
    LegacyAgc *stt;

    stt = (LegacyAgc *) state;
#ifdef WEBRTC_AGC_DEBUG_DUMP
    fclose(stt->fpt);
    fclose(stt->agcLog);
    fclose(stt->digitalAgc.logFile);
#endif
    free(stt);
}

/* minLevel     - Minimum volume level
 * maxLevel     - Maximum volume level
 */
int WebRtcAgc_Init(void *agcInst,
                   int32_t minLevel,
                   int32_t maxLevel,
                   int16_t agcMode,
                   uint32_t fs) {
    int32_t max_add, tmp32;
    int16_t i;
    int tmpNorm;
    LegacyAgc *stt;

    /* typecast state pointer */
    stt = (LegacyAgc *) agcInst;

    if (WebRtcAgc_InitDigital(&stt->digitalAgc, agcMode) != 0) {
        stt->lastError = AGC_UNINITIALIZED_ERROR;
        return -1;
    }

    /* Analog AGC variables */
    stt->envSum = 0;

/* mode     = 0 - Only saturation protection
 *            1 - Analog Automatic Gain Control [-targetLevelDbfs (default -3
 * dBOv)]
 *            2 - Digital Automatic Gain Control [-targetLevelDbfs (default -3
 * dBOv)]
 *            3 - Fixed Digital Gain [compressionGaindB (default 8 dB)]
 */
#ifdef WEBRTC_AGC_DEBUG_DUMP
    stt->fcount = 0;
    fprintf(stt->fpt, "AGC->Init\n");
#endif
    if (agcMode < kAgcModeUnchanged || agcMode > kAgcModeFixedDigital) {
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt, "AGC->Init: error, incorrect mode\n\n");
#endif
        return -1;
    }
    stt->agcMode = agcMode;
    stt->fs = fs;

    /* initialize input VAD */
    WebRtcAgc_InitVad(&stt->vadMic);

    /* If the volume range is smaller than 0-256 then
     * the levels are shifted up to Q8-domain */
    tmpNorm = NormU32((uint32_t) maxLevel);
    stt->scale = tmpNorm - 23;
    if (stt->scale < 0) {
        stt->scale = 0;
    }
    // TODO(bjornv): Investigate if we really need to scale up a small range now
    // when we have
    // a guard against zero-increments. For now, we do not support scale up (scale
    // = 0).
    stt->scale = 0;
    maxLevel <<= stt->scale;
    minLevel <<= stt->scale;

    /* Make minLevel and maxLevel static in AdaptiveDigital */
    if (stt->agcMode == kAgcModeAdaptiveDigital) {
        minLevel = 0;
        maxLevel = 255;
        stt->scale = 0;
    }
    /* The maximum supplemental volume range is based on a vague idea
     * of how much lower the gain will be than the real analog gain. */
    max_add = (maxLevel - minLevel) / 4;

    /* Minimum/maximum volume level that can be set */
    stt->minLevel = minLevel;
    stt->maxAnalog = maxLevel;
    stt->maxLevel = maxLevel + max_add;
    stt->maxInit = stt->maxLevel;

    stt->zeroCtrlMax = stt->maxAnalog;
    stt->lastInMicLevel = 0;

    /* Initialize micVol parameter */
    stt->micVol = stt->maxAnalog;
    if (stt->agcMode == kAgcModeAdaptiveDigital) {
        stt->micVol = 127; /* Mid-point of mic level */
    }
    stt->micRef = stt->micVol;
    stt->micGainIdx = 127;
#ifdef MIC_LEVEL_FEEDBACK
    stt->numBlocksMicLvlSat = 0;
    stt->micLvlSat = 0;
#endif
#ifdef WEBRTC_AGC_DEBUG_DUMP
    fprintf(stt->fpt, "AGC->Init: minLevel = %d, maxAnalog = %d, maxLevel = %d\n",
            stt->minLevel, stt->maxAnalog, stt->maxLevel);
#endif

    /* Minimum output volume is 4% higher than the available lowest volume level
     */
    tmp32 = ((stt->maxLevel - stt->minLevel) * 10) >> 8;
    stt->minOutput = (stt->minLevel + tmp32);

    stt->msTooLow = 0;
    stt->msTooHigh = 0;
    stt->changeToSlowMode = 0;
    stt->firstCall = 0;
    stt->msZero = 0;
    stt->muteGuardMs = 0;
    stt->gainTableIdx = 0;

    stt->msecSpeechInnerChange = kMsecSpeechInner;
    stt->msecSpeechOuterChange = kMsecSpeechOuter;

    stt->activeSpeech = 0;
    stt->Rxx16_LPw32Max = 0;

    stt->vadThreshold = kNormalVadThreshold;
    stt->inActive = 0;

    for (i = 0; i < RXX_BUFFER_LEN; i++) {
        stt->Rxx16_vectorw32[i] = (int32_t) 1000; /* -54dBm0 */
    }
    stt->Rxx160w32 =
            125 * RXX_BUFFER_LEN; /* (stt->Rxx16_vectorw32[0]>>3) = 125 */

    stt->Rxx16pos = 0;
    stt->Rxx16_LPw32 = (int32_t) 16284; /* Q(-4) */

    for (i = 0; i < 5; i++) {
        stt->Rxx16w32_array[0][i] = 0;
    }
    for (i = 0; i < 10; i++) {
        stt->env[0][i] = 0;
        stt->env[1][i] = 0;
    }
    stt->inQueue = 0;

#ifdef MIC_LEVEL_FEEDBACK
    stt->targetIdxOffset = 0;
#endif

    memset(stt->filterState, 0, 8 * sizeof(int32_t));

    stt->initFlag = kInitCheck;
    // Default config settings.
    stt->defaultConfig.limiterEnable = kAgcTrue;
    stt->defaultConfig.targetLevelDbfs = AGC_DEFAULT_TARGET_LEVEL;
    stt->defaultConfig.compressionGaindB = AGC_DEFAULT_COMP_GAIN;

    if (WebRtcAgc_set_config(stt, stt->defaultConfig) == -1) {
        stt->lastError = AGC_UNSPECIFIED_ERROR;
        return -1;
    }
    stt->Rxx160_LPw32 = stt->analogTargetLevel;  // Initialize rms value

    stt->lowLevelSignal = 0;

    /* Only positive values are allowed that are not too large */
    if ((minLevel >= maxLevel) || (maxLevel & 0xFC000000)) {
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt, "minLevel, maxLevel value(s) are invalid\n\n");
#endif
        return -1;
    } else {
#ifdef WEBRTC_AGC_DEBUG_DUMP
        fprintf(stt->fpt, "\n");
#endif
        return 0;
    }
}
