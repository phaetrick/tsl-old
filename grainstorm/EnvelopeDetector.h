#pragma once
//
// Created by pr on 09.01.18.
//

#ifndef GRAINSTORM_ENVELOPEDETECTOR_H
#define GRAINSTORM_ENVELOPEDETECTOR_H

// constants for dealing with overflow or underflow
#include <atomic>
#include <cmath>
#include <defines.h>

#define FLT_EPSILON_PLUS      1.192092896e-07         /* smallest such that 1.0+FLT_EPSILON != 1.0 */
#define FLT_EPSILON_MINUS    -1.192092896e-07         /* smallest such that 1.0-FLT_EPSILON != 1.0 */
#define FLT_MIN_PLUS          1.175494351e-38         /* min positive value */
#define FLT_MIN_MINUS        -1.175494351e-38         /* min negative value */

// Use these "codes"
#define DETECT_PEAK 0
#define DETECT_MS 1
#define DETECT_RMS 2

#define DETECT_ADD 0
#define DETECT_SUB 1

namespace tsl {
    struct AppState;
}

struct TRACK;

class CEnvelopeDetector;

typedef MYFLOAT (CEnvelopeDetector::*EnvfDetect)(MYFLOAT);


class CEnvelopeDetector {
public:
    explicit CEnvelopeDetector(tsl::AppState*);

    std::atomic<MYFLOAT> *power;
    const char *name;

    // Call the Init Function to initialize and setup all at once; this can be called as many times
    // as you want
    void
    setup(std::atomic<MYFLOAT> *att, std::atomic<MYFLOAT> *rel, std::atomic<MYFLOAT> *g,
         bool bAnalogTC, int32_t uDetect, bool bLogDetector, bool ksr_ = false);

    // these functions allow you to change modes and attack/release one at a time during
    // realtime operation
    void setTCModeAnalog(bool bAnalogTC); // {m_bAnalogTC = bAnalogTC;}

    // THEN do these after init
    void setAttackTime(MYFLOAT attack_in_ms);

    void setReleaseTime(MYFLOAT release_in_ms);


    //
    void setDetectMode(int32_t uDetect) { m_uDetectMode = uDetect; }

    void setAddSub(int32_t addsub) { m_AddSub = addsub; }


    void OnSampleRateChanged();

    void setLogDetect(bool b) { m_bLogDetector = b; }

    void setSource(TRACK *track) { m_Track = track; }

    TRACK *getSource() { return m_Track; }

    // call this to detect; it returns the peak ms or rms value at that instant
    MYFLOAT detectL(MYFLOAT fInput);

    MYFLOAT detectR(MYFLOAT fInput);

    void detectL(const MYFLOAT *fInput, MYFLOAT *fOutput, uint32_t size);

    void detectR(const MYFLOAT *fInput, MYFLOAT *fOutput, uint32_t size);

    // call this from your prepareForPlay() function each time to reset the detector
    void prepareForPlay();

    int32_t getType() { return m_uDetectMode; }

    int32_t getAddSub() { return m_AddSub; }

    void prepareComputation();

    std::atomic<MYFLOAT> *attackTime, *releaseTime, *addsub, *detectmode, *srctrack, *preGain;

    EnvfDetect detect[2] = {&CEnvelopeDetector::detectL, &CEnvelopeDetector::detectR};
protected:
    tsl::AppState* _appState{};
    int32_t m_nSample;
    double attackOld, releaseOld, oldGain;
    MYFLOAT m_fPreGain;
    MYFLOAT m_fAttackTime;
    MYFLOAT m_fReleaseTime;
    MYFLOAT m_fEnvelopeL, m_fEnvelopeR;
    TRACK *m_Track;
    int32_t m_uDetectMode;
    int32_t m_AddSub;
    bool m_bAnalogTC;
    bool m_bLogDetector;
    bool ksr{};
};

#endif //GRAINSTORM_ENVELOPEDETECTOR_H
