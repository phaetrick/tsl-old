//
// Created by pr on 09.01.18.
//

#include "logger.h"
#include "EnvelopeDetector.h"
#include "grainstorm.h"
#include "app.h"

// CEnvelopeDetector Implementation ----------------------------------------------------------------
//
CEnvelopeDetector::CEnvelopeDetector(tsl::AppState *appState): _appState(appState)  {
    attackOld = releaseOld = -1.0; // sentinel: times are >= 0, so the first prepareComputation always computes
    oldGain = 0.0;                 // matches m_fPreGain = 1.0 below (0 dB)
    m_fAttackTime = 0.0;
    m_fReleaseTime = 0.0;
    m_fEnvelopeL = m_fEnvelopeR = 0.0;
    m_uDetectMode = DETECT_PEAK;
    m_AddSub = DETECT_ADD;
    m_nSample = 0;
    m_bAnalogTC = true;
    m_bLogDetector = false;
    m_fPreGain = 1.0;
    detectmode = addsub = srctrack = nullptr;
}

void CEnvelopeDetector::OnSampleRateChanged() {
    setAttackTime((MYFLOAT) *attackTime);
    setReleaseTime((MYFLOAT) *releaseTime);
}

void CEnvelopeDetector::prepareForPlay() {
    m_fEnvelopeL = m_fEnvelopeR = 0.0;
    m_nSample = 0;
}

void
CEnvelopeDetector::setup(std::atomic<MYFLOAT> *att, std::atomic<MYFLOAT> *rel, std::atomic<MYFLOAT> *g,
                         bool bAnalogTC, int32_t uDetect, bool bLogDetector, bool ksr_) {
    attackTime = att;
    releaseTime = rel;
    preGain = g;
    m_fEnvelopeL = m_fEnvelopeR = 0.0;
    //m_bAnalogTC = bAnalogTC;
    m_uDetectMode = uDetect;
    m_bLogDetector = bLogDetector;
    ksr = ksr_;
}

void CEnvelopeDetector::setAttackTime(MYFLOAT attack_in_ms) {
    attackOld = attack_in_ms; // cache the requested time, not the offset one
    attack_in_ms += 0.25;
     if(attack_in_ms == 0.0)
         m_fAttackTime = 0;
    else if (m_bAnalogTC)
        m_fAttackTime = exp(ANALOG_TC / (attack_in_ms * (ksr ? _STATE->ksr : _STATE->sr) * 0.001));
    else
        m_fAttackTime = exp(DIGITAL_TC / (attack_in_ms * (ksr ? _STATE->ksr : _STATE->sr) * 0.001));
}

void CEnvelopeDetector::setReleaseTime(MYFLOAT release_in_ms) {
    releaseOld = release_in_ms; // cache the requested time, not the offset one
    release_in_ms += 0.25;
    if(release_in_ms == 0.0)
        m_fReleaseTime = 0;
    else if (m_bAnalogTC)
        m_fReleaseTime = exp(ANALOG_TC / (release_in_ms * 0.001 * (ksr ? _STATE->ksr : _STATE->sr)));
    else
        m_fReleaseTime = exp(DIGITAL_TC / (release_in_ms * 0.001 * (ksr ? _STATE->ksr : _STATE->sr)));
}

void CEnvelopeDetector::setTCModeAnalog(bool bAnalogTC) {
    m_bAnalogTC = bAnalogTC;
    setAttackTime(*attackTime);
    setReleaseTime(*releaseTime);
}

void CEnvelopeDetector::prepareComputation() {

    if (*attackTime != attackOld) {
        setAttackTime((MYFLOAT) *attackTime);
    }
    if (*releaseTime != releaseOld) {
        setReleaseTime((MYFLOAT) *releaseTime);
    }
    if (*preGain != oldGain) {
        m_fPreGain = pow(10., preGain->load() * .05);
        oldGain = *preGain;
    }
    if (addsub != nullptr) {
        m_AddSub = addsub->load();
    }
    if (detectmode != nullptr) {
        setDetectMode(detectmode->load());
    }
    if (srctrack != nullptr)
        setSource(_DATA->tracks[(int) srctrack->load()]);
}

MYFLOAT CEnvelopeDetector::detectL(MYFLOAT fInput) {
    fInput = fInput * m_fPreGain;
    if (m_uDetectMode == DETECT_MS)
        fInput = fInput * fInput;
    else
        fInput = fabs(fInput);

    if (fInput > m_fEnvelopeL)
        m_fEnvelopeL = m_fAttackTime * (m_fEnvelopeL - fInput) + fInput;
    else
        m_fEnvelopeL = m_fReleaseTime * (m_fEnvelopeL - fInput) + fInput;

    if (m_fEnvelopeL > 0.0 && m_fEnvelopeL < FLT_MIN_PLUS) m_fEnvelopeL = 0;
    if (m_fEnvelopeL < 0.0 && m_fEnvelopeL > FLT_MIN_MINUS) m_fEnvelopeL = 0;

    // bound them; can happen when using pre-detector gains of more than 1.0

    return m_fEnvelopeL = MAX(MIN(m_fEnvelopeL, 1.0), 0.0);
}

MYFLOAT CEnvelopeDetector::detectR(MYFLOAT fInput) {
    fInput = fInput * m_fPreGain;
    //fInput = fInput * powf(10.f, (MYFLOAT) preGain * .05f);
    if (m_uDetectMode == DETECT_MS)
        fInput = fInput * fInput;
    else
        fInput = fabs(fInput);

    if (fInput > m_fEnvelopeR)
        m_fEnvelopeR = m_fAttackTime * (m_fEnvelopeR - fInput) + fInput;
    else
        m_fEnvelopeR = m_fReleaseTime * (m_fEnvelopeR - fInput) + fInput;

    if (m_fEnvelopeR > 0.0 && m_fEnvelopeR < FLT_MIN_PLUS) m_fEnvelopeR = 0;
    if (m_fEnvelopeR < 0.0 && m_fEnvelopeR > FLT_MIN_MINUS) m_fEnvelopeR = 0;

    // bound them; can happen when using pre-detector gains of more than 1.0
    return m_fEnvelopeR = MAX(MIN(m_fEnvelopeR, 1.0), 0.0);
}


void CEnvelopeDetector::detectL(const MYFLOAT *fInput, MYFLOAT *fOutput, uint32_t size) {
    //fInput = fInput;
    //fInput = fInput * powf(10.f, (MYFLOAT) preGain * .05f);
    for (int32_t i = 0; i < size; i++) {
        MYFLOAT smpl = fInput[i] * m_fPreGain;
        if (m_uDetectMode == DETECT_MS)
            smpl = smpl * smpl;
        else
            smpl = fabs(smpl);

        if (smpl > m_fEnvelopeL)
            m_fEnvelopeL = m_fAttackTime * (m_fEnvelopeL - smpl) + smpl;
        else
            m_fEnvelopeL = m_fReleaseTime * (m_fEnvelopeL - smpl) + smpl;

        if (m_fEnvelopeL > 0.0 && m_fEnvelopeL < FLT_MIN_PLUS) m_fEnvelopeL = 0;
        if (m_fEnvelopeL < 0.0 && m_fEnvelopeL > FLT_MIN_MINUS) m_fEnvelopeL = 0;

        // bound them; can happen when using pre-detector gains of more than 1.0
        fOutput[i] = m_fEnvelopeL = MAX(MIN(m_fEnvelopeL, 1.0), 0.0);
    }
}

void CEnvelopeDetector::detectR(const MYFLOAT *fInput, MYFLOAT *fOutput, uint32_t size) {
    //fInput = fInput;
    //fInput = fInput * powf(10.f, (MYFLOAT) preGain * .05f);
    for (int32_t i = 0; i < size; i++) {
        MYFLOAT smpl = fInput[i] * m_fPreGain;
        if (m_uDetectMode == DETECT_MS)
            smpl = smpl * smpl;
        else
            smpl = fabs(smpl);

        if (smpl > m_fEnvelopeR)
            m_fEnvelopeR = m_fAttackTime * (m_fEnvelopeR - smpl) + smpl;
        else
            m_fEnvelopeR = m_fReleaseTime * (m_fEnvelopeR - smpl) + smpl;

        if (m_fEnvelopeR > 0.0 && m_fEnvelopeR < FLT_MIN_PLUS) m_fEnvelopeR = 0;
        if (m_fEnvelopeR < 0.0 && m_fEnvelopeR > FLT_MIN_MINUS) m_fEnvelopeR = 0;

        // bound them; can happen when using pre-detector gains of more than 1.0
        fOutput[i] = m_fEnvelopeR = MAX(MIN(m_fEnvelopeR, 1.0), 0.0);
    }
}
