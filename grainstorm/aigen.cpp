#include "aigen.h"

#ifdef GS_AIGEN

// Engine headers first: grainstorm's defines.h macros (_STATE/_DATA) must not
// leak into them.
#include "engine/framework/runtime/registry.h"
#include "engine/framework/runtime/run_control.h"
#include "engine/framework/runtime/session.h"

#include "app.h"
#include "grainstorm.h"
#include "track.h"
#include "waveform.h"
#include "DynamicDialog.h"
#include "resample.h"
#include "colours.h"
#include <audio/Recording.h>
#include <FloatingView.h>
#include <SkFont.h>

#ifdef __ANDROID__
#include <jni.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>


namespace tsl::aigen {
namespace {

// ---------------------------------------------------------------------------
// Model registry
//
// SFX is the default: Grainstorm eats textures and hits, and the music model's
// riffs are poor grain fodder (ear-tested). "m;" / "m:" prefixes a prompt to
// use the music model instead; "sfx;" / "sfx:" still explicitly selects sfx.
//
// URLs are pinned to a Hugging Face commit sha, so upstream force-pushes or
// renames cannot change what ships. sha256 values were computed locally AND
// matched against HF's x-linked-etag for the same URLs — the file we tested is
// the file users download. HF serves ranges (resumable) from their CDN, so no
// bandwidth lands on our own servers.
// ---------------------------------------------------------------------------
struct ModelDef {
    const char* key;
    const char* dir;
    const char* file;
    const char* url;
    const char* sha256;
    long long bytes;
    const char* title;  // DownloadManager notification + settings label
};

inline constexpr ModelDef kModels[] = {
    {"sfx", "stable-audio-3-small-sfx", "stable-audio-3-small-sfx-q8_0.gguf",
     "https://huggingface.co/TheSecretLaboratory/grainstorm-models/resolve/"
     "45b75f8786b3ca53ad24dc2fd57e57872c271e82/stable-audio-3-small-sfx-q8_0.gguf",
     "5bb1ec653134e63dac46264e336ae198a2590195fc996fcd0605021215e9b26f",
     1683570688LL, "AI SFX MODEL"},
    {"music", "stable-audio-3-small-music", "stable-audio-3-small-music-q8_0.gguf",
     "https://huggingface.co/TheSecretLaboratory/grainstorm-models/resolve/"
     "45b75f8786b3ca53ad24dc2fd57e57872c271e82/stable-audio-3-small-music-q8_0.gguf",
     "89bb22db5fa68ab8f1d90af0a9f88121977c945bdb99a57c96b1e88fa34bf1c8",
     1683570752LL, "AI MUSIC MODEL"},
};

const ModelDef* modelByKey(const char* key) {
    for (const auto& m : kModels)
        if (std::string_view(m.key) == key) return &m;
    return nullptr;
}

std::string modelsRoot() {
#ifdef __ANDROID__
    // MainActivity.getAiModelsDir(): <external files dir>/aigen-models, a
    // plain adb-pushable path — the SAF/fd layer is no use for 1.7 GB ggufs.
    ATTACH
    std::string ret;
    if (env && tsl::android::activityclass) {
        jmethodID jfid = env->GetStaticMethodID(tsl::android::activityclass, "getAiModelsDir",
            "()Ljava/lang/String;");
        if (jfid) {
            auto string = (jstring)env->CallStaticObjectMethod(tsl::android::activityclass, jfid);
            if (string) {
                const char* tmp = env->GetStringUTFChars(string, nullptr);
                ret = tmp;
                env->ReleaseStringUTFChars(string, tmp);
            }
        }
        else {
            env->ExceptionClear();
        }
    }
    DETACH
    return ret;
#else
    if (const char* env = std::getenv("GS_AIGEN_MODELS_ROOT")) return env;
    const char* home = std::getenv("HOME");
    return std::string(home ? home : "") + "/programming/grain-prompt-test/models";
#endif
}

// The download/verify/install flow lives on the JAVA side (MainActivity +
// MainSettingFragment): Android's settings UI is the Java preferences screen,
// and the system DownloadManager doing the transfer is a Java API anyway. The
// model table there mirrors kModels — keep the two in sync.

// The loaded model stays resident between generations (~2 GB) so repeat
// prompts skip the ~2.7 s load. Switching music<->sfx swaps it out.
struct Engine {
    std::mutex m;
    std::string loadedDir;
    std::unique_ptr<engine::runtime::ILoadedVoiceModel> model;
    std::unique_ptr<engine::runtime::IVoiceTaskSession> session;
    engine::runtime::IOfflineVoiceTaskSession* offline{};
};

Engine& eng() {
    static Engine e;
    return e;
}

// ggml-metal aborts at exit when Metal buffers are still registered while its
// device statics tear down (GGML_ASSERT in ggml_metal_rsets_free), and static
// destruction would free our resident session too late. Ordering trap, learned
// the hard way: the atexit/static-dtor list is ONE LIFO, and ggml's Metal
// device static is created lazily during session creation / first run — so
// this hook must be registered AFTER the first run() returns, or ggml's
// teardown is the newer entry and runs first (verified both ways with
// grain-prompt-test/exit_test.cpp). The mutex also parks exit behind any
// generation still running.
void releaseEngineAtExit() {
    auto& e = eng();
    std::lock_guard lk(e.m);
    e.offline = nullptr;
    e.session.reset();
    e.model.reset();
    e.loadedDir.clear();
}

// Caller holds eng().m.
engine::runtime::IOfflineVoiceTaskSession* ensureSession(const std::string& dir) {
    auto& e = eng();
    if (e.offline != nullptr && e.loadedDir == dir) return e.offline;

    e.offline = nullptr;
    e.session.reset();
    e.model.reset();
    e.loadedDir.clear();

    auto registry = engine::runtime::make_default_registry();
    engine::runtime::ModelLoadRequest load;
    load.model_path = dir;
    load.family_hint = "stable_audio";
    e.model = registry.load(load);

    engine::runtime::TaskSpec task;
    task.task = engine::runtime::VoiceTaskKind::AudioGeneration;
    task.mode = engine::runtime::RunMode::Offline;
    engine::runtime::SessionOptions options;
#ifdef __ANDROID__
    // CPU backend (dotprod/fp16 kernels). Measured on a Helio G88 (2x A75 +
    // 6x A55) for a 10 s clip: 2 threads 116 s, 4 threads 92.9 s, 6 threads
    // 80.4 s, 8 threads 69.4 s — it keeps scaling to every core even though
    // most of them are little. The GPU is NOT the answer here: ggml-vulkan
    // runs on the Mali-G52 but its q8_0 matmul measures 4.33 GFLOPS against
    // the CPU's 8.75.
    //
    // mem_saver stays on: without it the same job takes 134.7 s and takes
    // 344k major faults thrashing against a 1.68 GB model.
    options.backend.type = engine::core::BackendType::Cpu;
    options.backend.threads = std::clamp((int)std::thread::hardware_concurrency(), 2, 8);
    options.options["stable_audio.mem_saver"] = "true";
#else
    options.backend.type = engine::core::BackendType::Metal;
    options.backend.threads = 8;
#endif
    e.session = e.model->create_task_session(task, options);
    e.offline = dynamic_cast<engine::runtime::IOfflineVoiceTaskSession*>(e.session.get());
    if (e.offline != nullptr) e.loadedDir = dir;
    return e.offline;
}

// Interleaved float [-1,1] at the model rate -> de-interleaved int16 at
// _STATE->sr, same shape flac.cpp uses for preset audio.
void toEngineChannels(tsl::AppState* _appState, const engine::runtime::AudioBuffer& in,
                      std::vector<short> out[MAX_CHANNELS]) {
    const int outCh = std::clamp(_STATE->channels, 1, (int)MAX_CHANNELS);
    const int inCh = std::max(in.channels, 1);
    const size_t frames = in.samples.size() / inCh;
    const auto toShort = [](double v) {
        return (short)std::lround(std::clamp(v, -32768.0, 32767.0));
    };
    const auto sampleAt = [&](size_t f, int ch) -> double {
        if (outCh == 1 && inCh == 2)
            return (in.samples[f * 2] + in.samples[f * 2 + 1]) * 0.5 * 32767.0;
        return in.samples[f * inCh + std::min(ch, inCh - 1)] * 32767.0;
    };

    for (int ch = 0; ch < outCh; ch++) {
        if (in.sample_rate == _STATE->sr) {
            out[ch].reserve(frames);
            for (size_t f = 0; f < frames; f++)
                out[ch].push_back(toShort(sampleAt(f, ch)));
        }
        else {
            ResamplerRounded<double> rs;
            rs.init(in.sample_rate * _STATE->onedsr);
            out[ch].reserve((size_t)(frames * (double)_STATE->sr / in.sample_rate) + 8);
            size_t f = 0;
            while (f < frames) {
                if (rs.isWriteNeeded()) rs.writeNextFrame(sampleAt(f, ch)), ++f;
                else out[ch].push_back(toShort(rs.readNextFrame()));
            }
            while (!rs.isWriteNeeded())
                out[ch].push_back(toShort(rs.readNextFrame()));
        }
    }
}

// ---------------------------------------------------------------------------
// Progress + cancel
//
// One generation may be in flight at a time. `cancel` is read by ggml's worker
// threads through the run_control patch, so it must outlive the run — it lives
// here, in a function-local static, not on any stack.
// ---------------------------------------------------------------------------

class AiProgress;

struct Progress {
    std::atomic<bool> busy{false};
    std::atomic<bool> cancel{false};
    std::atomic<int> step{-1};  // -1 until the first denoising step reports
    std::atomic<int> total{8};
    std::atomic<bool> decoding{false};
    std::atomic<long long> startMs{0};
    std::mutex viewMutex;
    std::shared_ptr<AiProgress> view;  // guarded by viewMutex
    // The prompt exactly as the engine will receive it, displayed in the
    // dialog. Written before busy goes true, read by the render thread —
    // guarded by viewMutex since strings are not atomic.
    std::string promptShown;
};

Progress& prog() {
    static Progress p;
    return p;
}

long long nowMs() {
    return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

// Dismissing the window IS the cancel: FloatingView routes its corner-tap close
// through delRecursive*, so overriding those hooks it without touching
// tslgraphics. Closing it ourselves after the job ends must NOT read as a
// cancel, hence selfClosing.
class AiProgress : public tsl::graphics::FloatingView {
public:
    explicit AiProgress(tsl::AppState* appState) : FloatingView(appState) {}

    void markSelfClosing() { selfClosing = true; }

    // FloatingView::setTitle stores the raw pointer, so a composed title has
    // to live as long as the view does.
    void setTrackTitle(std::string t) {
        titleStore = std::move(t);
        setTitle(titleStore.c_str());
    }

protected:
    void computeContent(int maxWidth, int maxHeight) override {
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2 * .9f);
        SkRect bounds{};
        const char* widest = "GENERATING - STEP 8/8 - 000s";
        font.measureText(widest, strlen(widest), SkTextEncoding::kUTF8, &bounds);
        contentWidth = std::min<int>(maxWidth, (int)bounds.width() + 6 * borderSize);
        contentHeight = std::min<int>(maxHeight, rowHeight * 4);
    }

    int cb(float, float, int, int) override { return 0; }

    void renderContent(void* c) override {
        auto canvas = static_cast<SkCanvas*>(c);
        SkPaint paint;
        paint.setAntiAlias(true);
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2 * .9f);

        auto& p = prog();
        const bool running = p.busy.load();
        const int step = p.step.load();
        const int total = std::max(1, p.total.load());
        const long long start = p.startMs.load();
        const int secs = start > 0 ? (int)((nowMs() - start) / 1000) : 0;

        // Show the prompt as the engine sees it — if the sound ignores the
        // text, this line says whether the text even arrived.
        std::string shownPrompt;
        {
            std::lock_guard lk(p.viewMutex);
            shownPrompt = p.promptShown;
        }

        // Keep repainting while there is something to watch; a static overlay
        // would freeze the step counter and the clock.
        perm = running;

        char line[96];
        if (!running) snprintf(line, sizeof(line), "FINISHING - %ds", secs);
        else if (p.decoding.load()) snprintf(line, sizeof(line), "DECODING - %ds", secs);
        else if (step < 0) snprintf(line, sizeof(line), "READING PROMPT - %ds", secs);
        else snprintf(line, sizeof(line), "GENERATING - STEP %d/%d - %ds", step + 1, total, secs);

        paint.setColor(skcol::fg);
        if (!shownPrompt.empty()) {
            // Ellipsized to the window, never clipped mid-glyph: the prompt
            // can be up to 200 chars now. Truncate the prompt BEFORE quoting,
            // or a capped line loses its closing quote.
            const float quotesW = font.measureText("\"\"", 2, SkTextEncoding::kUTF8);
            std::string pl = "\"" +
                truncateText(font, shownPrompt,
                             (float)contentWidth - 4.f * borderSize - quotesW) +
                "\"";
            canvas->drawSimpleText(pl.c_str(), pl.size(), SkTextEncoding::kUTF8, 2.f * borderSize,
                                   rowHeight * 0.7f, font, paint);
        }
        canvas->drawSimpleText(line, strlen(line), SkTextEncoding::kUTF8, 2.f * borderSize,
                               rowHeight * 1.7f, font, paint);

        // Step count is the only honest progress signal we have: text encoding
        // before step 1 and the decode after the last one are single opaque
        // calls, so the bar deliberately starts and ends short of the ends.
        const float barX = 2.f * borderSize;
        const float barW = (float)contentWidth - 4.f * borderSize;
        const float barY = rowHeight * 2.15f;
        const float barH = rowHeight * 0.35f;
        paint.setColor(skcol::lg);
        canvas->drawRect(SkRect::MakeXYWH(barX, barY, barW, barH), paint);
        float frac = 0.f;
        if (p.decoding.load() || !running) frac = 1.f;
        else if (step >= 0) frac = (float)(step + 1) / (float)(total + 1);
        paint.setColor(skcol::blue_transparent);
        canvas->drawRect(SkRect::MakeXYWH(barX, barY, barW * frac, barH), paint);

        const char* hint = p.cancel.load() ? "CANCELLING..." : "CLOSE THIS WINDOW TO CANCEL";
        paint.setColor(skcol::orange);
        canvas->drawSimpleText(hint, strlen(hint), SkTextEncoding::kUTF8, barX,
                               rowHeight * 3.6f, font, paint);
    }

    void delRecursiveDraw() override {
        cancelIfRunning();
        FloatingView::delRecursiveDraw();
    }

    void delRecursiveCB() override {
        cancelIfRunning();
        FloatingView::delRecursiveCB();
    }

private:
    void cancelIfRunning() {
        if (!selfClosing && prog().busy.load()) prog().cancel.store(true);
    }
    std::string titleStore;
    bool selfClosing{false};
};

// UI thread only.
void showProgressView(tsl::AppState* _appState, TRACK* track) {
    auto view = std::make_shared<AiProgress>(_appState);
    view->setTrackTitle(std::string("AI - ") + track->name);
    view->init();
    view->addDraw();
    view->addCB();
    std::lock_guard lk(prog().viewMutex);
    prog().view = std::move(view);
}

// Any thread. Tears the window down on the UI thread without it reading as a
// user cancel.
void closeProgressView(tsl::AppState* _appState) {
    std::shared_ptr<AiProgress> view;
    {
        std::lock_guard lk(prog().viewMutex);
        view = std::move(prog().view);
        prog().view.reset();
    }
    if (view == nullptr) return;
    view->markSelfClosing();
    if (!_STATE->toUiThreadQueue.try_push([view]() mutable {
            view->deldraw();
            view->delCB();
            view.reset();
        })) {
        // Queue full: leaving the window up is better than freeing a live view
        // off the UI thread, and its own close still works.
        showToast(_STATE, "AI: could not close progress window.");
    }
}

struct Job {
    tsl::AppState* _appState{};
    TRACK* track{};
    std::string prompt;
    float seconds{10.f};
};

// RecordingQueue thread. Long-running: model load + diffusion + resample.
void runJob(const Job& job) {
    auto _appState = job._appState;

    // Every exit from here — success, failure, cancel, early return — has to
    // clear busy and take the window down, or the feature is dead until restart.
    struct Finish {
        tsl::AppState* _appState;
        ~Finish() {
            closeProgressView(_appState);
            prog().step.store(-1);
            prog().decoding.store(false);
            prog().busy.store(false);
            prog().cancel.store(false);
        }
    } finish{_appState};

    if (_STATE->destroyRequested.load()) return;

    std::string prompt = job.prompt;
    // SFX is the default; "m:" switches to the music model. The ';' variants
    // stay accepted for hardware keyboards (the on-screen keyboards only have
    // ':' now), and "sfx:" for symmetry.
    const ModelDef* model = modelByKey("sfx");
    {
        std::string head = prompt.substr(0, 4);
        std::transform(head.begin(), head.end(), head.begin(), ::tolower);
        size_t cut = 0;
        if (head.rfind("m:", 0) == 0 || head.rfind("m;", 0) == 0) {
            model = modelByKey("music");
            cut = 2;
        }
        else if (head == "sfx:" || head == "sfx;") {
            cut = 4;
        }
        if (cut > 0) {
            prompt.erase(0, cut);
            prompt.erase(0, prompt.find_first_not_of(' '));
        }
    }
    const std::string dir = modelsRoot() + "/" + model->dir;
    if (std::error_code ec; !std::filesystem::exists(dir, ec)) {
        showToast(_STATE, (std::string("AI: ") + model->key +
                           " model not installed - see settings.").c_str());
        return;
    }
    if (prompt.empty()) {
        showToast(_STATE, "AI: empty prompt.");
        return;
    }

    // Slider value from the dialog; the env override stays for the adb harness.
    // One decimal, matching the slider display - fractional durations verified
    // on the rig (-d 2.7 renders exactly 2.700 s).
    const char* secs = std::getenv("GS_AIGEN_SECONDS");
    char secsBuf[16];
    snprintf(secsBuf, sizeof(secsBuf), "%.1f", std::clamp(job.seconds, 1.f, 60.f));
    const std::string secsStr = (secs != nullptr) ? secs : secsBuf;

    // After the sfx: strip this is byte-for-byte what the text encoder gets;
    // the dialog shows it so "the sound ignored my text" can be told apart
    // from "the text never arrived".
    {
        std::lock_guard lk(prog().viewMutex);
        prog().promptShown = prompt;
    }

    engine::runtime::TaskResult result;
    try {
        std::lock_guard lk(eng().m);
        auto* offline = ensureSession(dir);
        if (offline == nullptr) {
            showToast(_STATE, "AI: no offline session.");
            return;
        }
        engine::runtime::TaskRequest request;
        request.text_input = engine::runtime::Transcript{prompt, "en"};
        request.options["duration_seconds"] = secsStr;
        request.options["num_inference_steps"] = "8";
        request.options["guidance_scale"] = "1.0";
        // The engine default is 6 s of extra generated tail that is truncated
        // away again, and on CPU the cost is linear in generated length — 77.5 s
        // at the default versus 49.9 s here for the same 10 s of output. It buys
        // nothing: the model is conditioned on request.durations_seconds (see
        // conditioner.cpp normalized_seconds), so it composes its ending at the
        // REQUESTED duration and the clip fades at 10 s either way — measured
        // identical at padding 1 and 6 with a fixed seed. Keeping 1 s rather
        // than 0 as cheap insurance against autoencoder edge effects.
        request.options["duration_padding_seconds"] = "1";

        // Progress and cancel, via the local run_control patch to audio.cpp
        // (see grainstorm-run-control.patch in the rig). Installed on THIS
        // thread because the hook is thread_local, and pointing at the static
        // cancel flag because ggml's workers read it too. Measured on device:
        // cancel unwinds 160 ms after the request, mid-step.
        engine::runtime::RunControl control;
        control.cancel = &prog().cancel;
        control.on_step = [](int step, int total) {
            prog().step.store(step);
            prog().total.store(total);
        };
        engine::runtime::set_run_control(&control);
        struct ClearControl {
            ~ClearControl() { engine::runtime::set_run_control(nullptr); }
        } clearControl;

        offline->prepare(engine::runtime::build_preparation_request(request));
        result = offline->run(request);
        // Steps are done; the autoencoder decode is one more opaque call.
        prog().decoding.store(true);

        // Only now are all of ggml's lazy statics alive; see releaseEngineAtExit.
        static std::once_flag exitHook;
        std::call_once(exitHook, [] { std::atexit(releaseEngineAtExit); });
    }
    catch (const engine::runtime::RunCancelled&) {
        showToast(_STATE, "AI cancelled.");
        return;
    }
    catch (const std::exception& error) {
        showToast(_STATE, (std::string("AI failed: ") + error.what()).substr(0, 90).c_str());
        return;
    }

    if (!result.audio_output.has_value() || result.audio_output->samples.empty()) {
        showToast(_STATE, "AI returned no audio.");
        return;
    }
    if (_STATE->destroyRequested.load()) return;

    std::vector<short> chans[MAX_CHANNELS];
    toEngineChannels(_appState, *result.audio_output, chans);

    // The model composes its ending at the requested duration, but "ending"
    // is no guarantee of silence: a tail cut at the boundary can land at full
    // level and click (heard on device at 10 s). 40 ms raised-cosine at the
    // very end is inaudible on a natural fade and saves the abrupt ones.
    {
        const int outCh = std::clamp(_STATE->channels, 1, (int)MAX_CHANNELS);
        for (int ch = 0; ch < outCh; ch++) {
            auto& v = chans[ch];
            const size_t fade = std::min<size_t>((size_t)(_STATE->sr * 0.04), v.size());
            for (size_t k = 0; k < fade; k++) {
                const float g = 0.5f + 0.5f * std::cos((float)M_PI * (float)(k + 1) / (float)fade);
                auto& s = v[v.size() - fade + k];
                s = (short)std::lround(s * g);
            }
        }
    }

    auto rec = std::make_shared<tsl::Recording>(_STATE, chans, _STATE->channels,
                                                "AI " + prompt.substr(0, 40));

    // Same order as Recorder.cpp mic stop: waveform BEFORE loadAudio, event
    // into snapShot only when the pool took the outgoing take.
    auto track = job.track;
    const bool queued = _DATA->snapShot.add_task([_appState, track, rec]() mutable {
        track->waveform->setup(rec);
        auto e = track->loadAudio(rec);
        if (e.poolHandle < tsl::INVALID_POOL_HANDLE)
            _DATA->snapShot.addEvent(e);
        showToast(_STATE, (std::string("AI pushed to ") + track->name).c_str());
    });
    if (!queued) showToast(_STATE, "AI: snapshot queue full, sample dropped.");
}

// UiTasksQueue (modal) thread: allowed to park on the popup.
void promptFlow(tsl::AppState* _appState, TRACK* track) {
    static std::string lastPrompt;
    static float lastSeconds = 10.f;

    // DynamicDialog: wrapping multi-line title (the AlphaPopUp title band cut
    // it off), a labelled prompt field, and the LENGTH slider as a native
    // field type drawn to the slider.cpp convention.
    tsl::graphics::DynamicDialog dlg(_STATE);
    dlg.setTitle(std::string("AI - ") + track->name + " (m: PREFIX FOR MUSIC)");
    dlg.addInputField("PROMPT", lastPrompt, false, tsl::graphics::LabelPosition::ABOVE);
    dlg.setFieldMaxLength("PROMPT", 200);
    // Log scale: equal travel multiplies the length, so the 1-10 s range
    // where most grain fodder lives gets as much of the track as 10-60.
    dlg.addSlider("LENGTH", 1.f, 60.f, lastSeconds, true, "s");
    dlg.setButtonMode(tsl::graphics::DynamicDialog::ButtonMode::OK_CANCEL);
    dlg.setOkCancelLabels("GENERATE", "CANCEL");

    auto token = _STATE->waitNotify.begin_wait();
    tsl::graphics::DynamicDialog::DialogResult res{};
    dlg.onCompleteCallback =
        [&](const tsl::graphics::DynamicDialog::DialogResult& r) {
            res = r;
            _STATE->waitNotify.complete(token);
        };
    dlg.init();
    dlg.addDraw();
    dlg.addCB();
    _STATE->waitNotify.wait_for_signal(token);
    // Deregister BEFORE the destroyRequested early-out: dlg is stack-allocated,
    // and returning while it is still in queue_draw/queue_callback leaves the
    // render/input threads holding a pointer into a dead frame (audit finding).
    dlg.delCB();
    dlg.deldraw();
    if (_STATE->destroyRequested.load()) return;

    if (!res.confirmed) return;
    std::string prompt = res.namedValues["PROMPT"];
    prompt.erase(0, prompt.find_first_not_of(' '));
    if (auto end = prompt.find_last_not_of(' '); end != std::string::npos)
        prompt.erase(end + 1);
    if (prompt.empty()) {
        showToast(_STATE, "AI: empty prompt.");
        return;
    }
    lastPrompt = prompt;
    // Rounded to the tenth the slider displays, so what you see is what the
    // engine renders.
    const float seconds = std::clamp(
        std::round(res.sliderValues["LENGTH"] * 10.f) / 10.f, 1.f, 60.f);
    lastSeconds = seconds;

    // Claim the slot before queueing so a second prompt cannot slip in between
    // here and the worker actually starting.
    bool expected = false;
    if (!prog().busy.compare_exchange_strong(expected, true)) {
        showToast(_STATE, "AI already generating.");
        return;
    }
    prog().cancel.store(false);
    prog().step.store(-1);
    prog().decoding.store(false);
    prog().startMs.store(nowMs());
    {
        std::lock_guard lk(prog().viewMutex);
        prog().promptShown = prompt;
    }

    auto job = std::make_shared<Job>(Job{_appState, track, std::move(prompt), seconds});
    if (_STATE->RecordingQueue.add_task([job] { runJob(*job); })) {
        showProgressView(_appState, track);
    }
    else {
        prog().busy.store(false);
        showToast(_STATE, "AI: recorder busy, try again.");
    }
}

}  // namespace

bool available() {
    std::error_code ec;
    for (const auto& m : kModels)
        if (std::filesystem::exists(modelsRoot() + "/" + m.dir, ec)) return true;
    return false;
}

void recordToTrack(tsl::AppState* _appState, TRACK* track) {
    // Checked here as well as when claiming the slot, so a second tap does not
    // pointlessly open the keyboard for a prompt that will be refused.
    if (prog().busy.load()) {
        showToast(_STATE, "AI already generating.");
        return;
    }
    if (!available()) {
        showToast(_STATE, "AI models not installed - download in settings.");
        return;
    }
    if (!_STATE->UiTasksQueue.add_task(promptFlow, _appState, track))
        showToast(_STATE, "AI: UI busy, try again.");
}

}  // namespace tsl::aigen

#else  // !GS_AIGEN

namespace tsl::aigen {
bool available() { return false; }
void recordToTrack(tsl::AppState*, TRACK*) {}
}

#endif
