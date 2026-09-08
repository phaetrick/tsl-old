//
// Created by pr on 12.07.25.
//

#ifndef GRAINSTORM_SPECTRUMANALYZERVIEW_H
#define GRAINSTORM_SPECTRUMANALYZERVIEW_H
#include <view.h>
#include "app.h"
#include <include/core/SkPath.h>
#include <tools/RingBufferQueue.h>

namespace tsl::graphics{
class SpectrumAnalyzerView : public View {
public:

    SpectrumAnalyzerView(
        tsl::AppState* appState,
        std::atomic<bool> (&updateRenderThread)[4],
        std::atomic<double*>(&input)[4],
        float minDb = -60.f,
        float maxDb = 0.f)
        :
        View(appState),
        updateRenderThread_{ updateRenderThread },
        input_{ input },
        minDb_{ minDb },
        maxDb_{ maxDb }
    {
    }

    void render(void *ctx) override;

    void callback(const InputEvent &) override {
    }
    std::atomic<bool>(&updateRenderThread_)[4];
    std::atomic<double*>(&input_)[4];
    int activeTrack_{};

protected:


private:
    float minDb_{};
    float maxDb_{};
    alignas(64) double smoothed[4][tsl::displayChannelsSpectrum]{};
    SkPath path[4]{};
};

}
#endif //GRAINSTORM_SPECTRUMANALYZERVIEW_H
