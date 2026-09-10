#include "gui.h"
#include <chrono>
#include <logger.h>
#include "defines.h"
#include "Queue.h"
#include "grainstorm.h"
#include "meter.h"
#include "infopanel.h"
#include "button.h"
#include "knob.h"
#include "buttonview.h"
#include "synth.h"
#include "vco.h"
#include "slider.h"
#include "tools.h"
#include "view.h"
#include "textview.h"
#include <PlusMinusControlBase.h>
#include "selector.h"
#include "envelope.h"
#include "queue.h"
#include "slider.h"
#include "ffttools.h"
#include <SkPath.h>
#include <SkGradientShader.h>
#include "checkbox.h"
#include "callbacks_loop_controls.h"
#include "Input.h"
#include "pianoview.h"
#include <app.h>
#include <scrollview.h>
#include <MidiLearning.h>
#include <sstream>

using namespace tsl::graphics;


enum spaces {
    SPACE_GEN = 0,
    SPACE_EG,
    SPACE_VCO1,
    SPACE_VCO2,
    SPACE_VCO3,
    SPACE_FILTER,
    SPACE_LFO,
    SPACE_ARP,
    SPACE_EFFECTS,
    SPACE_ROUTE,
    SPACE_NOISE,
    NUMSPACES
};


// Tab labels shortened to 3 chars so 11 tabs fit the bar without clipping.
// Visual order is set here; the matching spacevals entry is the stored value
// (= the SPACE_* enum), so NOISE can sit after SUB while keeping enum value 10.
// Widening these is bounded by ButtonView's horizontal layout, which caps every
// button at width/count and CLIPS rather than shrinking the text (see buttonview.h:
// `button.width = max > tmp ? tmp : max`). The widest label sets the cap for all of
// them, so five characters is the practical ceiling at eleven tabs — NOISE and FILT
// sit exactly on it. Anything longer clips on a narrow phone.
static std::vector<std::string> spacenames = {"GEN", "EG", "OSC1", "OSC2", "SUB", "NOISE", "FILT", "LFO",
                                              "SEQ", "FX", "RTE"};
static std::vector<float> spacevals = {SPACE_GEN, SPACE_EG, SPACE_VCO1, SPACE_VCO2, SPACE_VCO3,
                                       SPACE_NOISE, SPACE_FILTER, SPACE_LFO, SPACE_ARP,
                                       SPACE_EFFECTS, SPACE_ROUTE};

static std::vector<float> vcomodes = {-1, 4, 0, 2, 99};
static std::vector<std::string> vcomodenames = {"SINE ", "TRI  ", "SAW  ", "PULSE", "WT   "};
// The three WAVE selectors get their own copy: vcomodes/vcomodenames above are also
// handed to PresetSelector as its placeholder list, so adding an oscillator type
// there would leak into the preset widget. Keep in step with vcoTypeValues /
// vcoTypeNames in setup.cpp, which is the HOST parameter's list — both must agree.
#if PA_ENABLE_PAD
static std::vector<float> vcowavevals = {-1, 4, 0, 2, 99, 97, 98};
static std::vector<std::string> vcowavenames = {"SINE ", "TRI  ", "SAW  ", "PULSE", "WT   ", "MODAL", "PAD  "};
#else
static std::vector<float> vcowavevals = {-1, 4, 0, 2, 99, 97};
static std::vector<std::string> vcowavenames = {"SINE ", "TRI  ", "SAW  ", "PULSE", "WT   ", "MODAL"};
#endif

// These must READ the same here as in the DAW's parameter list, though they are no
// longer in the same ORDER — see below. The names once differed: this copy carried
// five-character squeezes (HOLOW, CZRES, EPIAN, DIGIT, SHAPE, FIFTH, FORM) of what
// wtNames already spells out in setup.cpp, so the host and the GUI disagreed about
// what the same table was called. The spellings are kept identical to wtNames.
//
// ── DISPLAY ORDER ONLY. THE PAIRING IS THE CONTRACT ─────────────────────────────
// Selector2 shows names[i] and writes values[i], and finds the current row with
// findIndexFloat(values, stored). So these two arrays may be permuted freely AS LONG
// AS THEY ARE PERMUTED TOGETHER: nothing on disk moves, every saved patch keeps its
// sound, and VCOxWTSEL still stores the same table id it always did.
//
// DO NOT "tidy" wtselvals back to 0..26, and do not reorder one array without the
// other. Either one silently repoints every wavetable in every preset ever saved.
// setup.cpp's wtNames is indexed BY VALUE and therefore stays in id order — the two
// lists disagreeing about order is deliberate, not a bug to fix.
//
// The order is by FAMILY, and soft→hard by measurement inside each family. A single
// linear brightness sort was measured and rejected: several tables travel further
// across their own morph than the gaps between them (SWEEP spans 4.9 octaves of
// spectral centroid, BUZZ 4.6, FORMANT 3.9, BASIC alone goes sine→square), so any
// fixed slot on a brightness axis would be decided by the morph knob rather than by
// the table. Grouping survives that; a flat sort does not.
static std::vector<std::string> wtselnames = {
        // classic analogue shapes
        "SOFT",   "ODD",    "SHAPER", "BASIC",  "PULSE",
        // sparse / interval structure
        "HOLLOW", "CHIME",  "ORGAN",  "FIFTHS", "STACK",  "METAL",
        // FM and digital
        "EPIANO", "FM",     "FM2",    "DIGITAL",
        // formant / voice
        "GROWL",  "VOWEL",  "FORMANT",
        // filter-like sweeps
        "CLIMB",  "RESO",   "CZ RES", "BUZZ",   "SWEEP",
        // comb, noise and air
        "PLUCK",  "COMB",   "AIR",    "NOISE"};
static std::vector<float> wtselvals = {
        7,  2,  15, 0,  6,       // classic
        17, 11, 8,  9,  26, 5,   // structure
        25, 13, 21, 12,          // FM / digital
        10, 4,  3,               // formant
        1,  23, 20, 24, 16,      // sweeps
        22, 14, 18, 19};         // comb / noise

// Kept identical to warpNames in setup.cpp — FORMNT was this copy's squeeze of FORMANT.
static std::vector<std::string> warpselnames = {"OFF", "SYNC", "BEND", "RING", "FOLD", "FORMANT", "BITS", "RATE", "DRIVE", "SAT", "STEP"};
static std::vector<float> warpselvals = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

static std::vector<float> noisemodes = {-1, 0, 1, 2};
static std::vector<std::string> noisemodenames = {"OFF  ", "WHITE", "PINK ", "BROWN"};


static const char *subnames[] = {"NORMAL", "RINGMOD", "AM", "PM"};
static const int subvals[] = {VCO3NORMAL, VCO3RINGMOD, VCO3AM, VCO3PM};


// egnames/egvals are the EG PAGE picker (which of the four you are editing), where NONE
// is meaningless. Every EG *routing* selector uses the filteg pair — that now includes
// the four AMP EGs, so a source can be left unenveloped; the misleading name is kept
// because these lists are also matched against setup.cpp by shape.
static std::vector<std::string> egnames = {"EG1", "EG2", "EG3", "EG4"};
static std::vector<float> egvals = {0, 1, 2, 3};
static std::vector<std::string> filtegnames = {"NONE", "EG1", "EG2", "EG3", "EG4"};
static std::vector<float> filtegvals = {-1, 0, 1, 2, 3};
// 7..16 are the ZDF filters. Keep in step with filtModeNames in setup.cpp.
//
// The first seven keep their shorthand: LP4/BP2/HP4 is what every synth calls these
// and spelling them out would make the list harder to scan, not easier. The rest are
// named by family, where the one-letter suffix was genuinely unreadable — LADB and
// SEMN tell you nothing, and MODL was simply MODAL with a letter missing.
static std::vector<std::string> filtmodenames = {"LP4", "LP2", "BP2", "BP4", "HP2", "HP4", "NOTCH",
                                                 "LADDER LP", "LADDER BP", "LADDER HP",
                                                 "SEM LP", "SEM BP", "SEM HP", "SEM NOTCH",
                                                 "303 LP", "303 BP", "303 HP",
                                                 "MODAL"};
static std::vector<float> filtmodevals = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
static std::vector<std::string> pwmodsrcnames = {"OFF", "EG1", "EG2", "EG3", "EG4"};
static std::vector<float> pwmodsrcvals = {0, 1, 2, 3, 4};

static std::vector<std::string> syncnames = {"OFF", "ON"};
static std::vector<float> syncvals = {0, 1};


enum fxspaces {
    SPACEPHASER = 0,
    SPACEDELAY,
    SPACEREV,
    SPACE_ARP1,
    SPACE_ARP2,
    SPACE_ARP3,
    SPACE_ARP4,
    SPACE_ROUTE_VEL,
    SPACE_ROUTE_AT,
    SPACE_ROUTE_MW,
    SPACE_ROUTE_KEY,
    // Per-oscillator subsection blocks. callbackSubFXSwitch indexes these as
    // fxspaces[savedSubIndex + SPACE_VCOn_1], so each block must stay contiguous —
    // a new subsection goes INSIDE its osc's run, not appended after the others.
    // Only the sub index (0..n) is persisted, so extending a run is backward safe.
    // MODAL sits before PAD in every run on purpose: flipping PA_ENABLE_PAD then
    // cannot shift MODAL's subsection index. (VCOxSPACE carries Param::NoAssignment
    // so it is never written to a preset, but keeping the index stable still saves
    // the "wrong tab opens" surprise when the flag is toggled during development.)
    SPACE_VCO1_1,
    SPACE_VCO1_2,
    SPACE_VCO1_3,
    SPACE_VCO1_4,
    SPACE_VCO1_5,   // MODAL
#if PA_ENABLE_PAD
    SPACE_VCO1_6,   // PAD
#endif
    SPACE_VCO2_1,
    SPACE_VCO2_2,
    SPACE_VCO2_3,
    SPACE_VCO2_4,
    SPACE_VCO2_5,   // MODAL
#if PA_ENABLE_PAD
    SPACE_VCO2_6,   // PAD
#endif
    SPACE_VCO3_1,
    SPACE_VCO3_2,
    SPACE_VCO3_3,
    SPACE_VCO3_4,
    SPACE_VCO3_5,
    SPACE_VCO3_6,   // MODAL
#if PA_ENABLE_PAD
    SPACE_VCO3_7,   // PAD
#endif
    SPACE_LFO1,
    SPACE_LFO2,
    SPACE_LFO3,
    SPACE_LFO4,
    // Appended at the END, never mid-enum: FXSPACE persists the id, and a shifted
    // id is what made a saved space open the wrong tab (and worse) before.
    SPACECHORUS,
    NUMFXSPACES
};
static std::vector<std::string> fxspacenames = {"PHASER", "DELAY", "REVERB", "CHORUS"};
static std::vector<float> fxvals = {0, 1, 2, SPACECHORUS};
static Layout *spaces[NUMSPACES];
static Layout *fxspaces[NUMFXSPACES];

#include "sequencer.h"
#include <EnterValue.h>

class CurrentNoteView : public View {
public:
    CurrentNoteView(tsl::AppState* appState) : View(appState, WRAP, 0, START_ALIGN) { perm = true; };

    void render(void *ca) override {
        auto c = (SkCanvas *) ca;
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2 * .9);
        SkPaint paint;
        paint.setColor(skcol::fg);
        paint.setAntiAlias(true);
        flush(c);
        std::stringstream s;
        s << (int) _STATE->params[0][CURRENTNOTE].load();
        textDisplayCenteredFixed(this, c, paint, font, s.str().c_str());
    }
};

class TasksView : public View {
public:
    TasksView(tsl::AppState* appState) : View(appState, WRAP, 0, START_ALIGN) { perm = true; };

    void render(void *ca) override {
        auto c = (SkCanvas *) ca;
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2 * .9);
        SkPaint paint;
        paint.setColor(skcol::fg);
        paint.setAntiAlias(true);
        flush(c);
        std::stringstream s;
        s << _DATA->toAudioThreadQueue.size();
        textDisplayCenteredFixed(this, c, paint, font, s.str().c_str());
    }
};


static void callbackSubFXSwitch(tsl::AppState* _appState, int id, int space) {
    switch (id) {
        case EGSPACE:
            _STATE->params[0][EGSPACE].store(space);
            spaces[SPACE_EG]->redraw();
            spaces[SPACE_EG]->addDraw();
            break;
        case FXSPACE: {
            int active = (int) _STATE->params[0][FXSPACE].load();
            fxspaces[active]->delCB();
            fxspaces[active]->deldraw();
            _STATE->params[0][FXSPACE].store(space);
            fxspaces[space]->redraw();
            fxspaces[space]->addDraw();
            fxspaces[space]->addCB();
            break;
        }
        case ARPSPACE: {
            int active = (int) _STATE->params[0][ARPSPACE].load() + SPACE_ARP1;
            fxspaces[active]->delCB();
            fxspaces[active]->deldraw();
            _STATE->params[0][ARPSPACE].store(space);
            fxspaces[space + SPACE_ARP1]->redraw();
            fxspaces[space + SPACE_ARP1]->addDraw();
            fxspaces[space + SPACE_ARP1]->addCB();
            break;
        }
        case LFOSPACE: {
            int active = (int) _STATE->params[0][LFOSPACE].load() + SPACE_LFO1;
            fxspaces[active]->delCB();
            fxspaces[active]->deldraw();
            _STATE->params[0][LFOSPACE].store(space);
            fxspaces[space + SPACE_LFO1]->redraw();
            fxspaces[space + SPACE_LFO1]->addDraw();
            fxspaces[space + SPACE_LFO1]->addCB();
            break;
        }
        case ROUTESPACE: {
            int active = (int) _STATE->params[0][ROUTESPACE].load() + SPACE_ROUTE_VEL;
            fxspaces[active]->delCB();
            fxspaces[active]->deldraw();
            _STATE->params[0][ROUTESPACE].store(space);
            fxspaces[space + SPACE_ROUTE_VEL]->redraw();
            fxspaces[space + SPACE_ROUTE_VEL]->addDraw();
            fxspaces[space + SPACE_ROUTE_VEL]->addCB();
            break;
        }
        case VCO1SPACE: {
            int active = (int) _STATE->params[0][VCO1SPACE].load() + SPACE_VCO1_1;
            fxspaces[active]->delCB();
            fxspaces[active]->deldraw();
            _STATE->params[0][VCO1SPACE].store(space);
            fxspaces[space + SPACE_VCO1_1]->redraw();
            fxspaces[space + SPACE_VCO1_1]->addDraw();
            fxspaces[space + SPACE_VCO1_1]->addCB();
            break;
        }
        case VCO2SPACE: {
            int active = (int) _STATE->params[0][VCO2SPACE].load() + SPACE_VCO2_1;
            fxspaces[active]->delCB();
            fxspaces[active]->deldraw();
            _STATE->params[0][VCO2SPACE].store(space);
            fxspaces[space + SPACE_VCO2_1]->redraw();
            fxspaces[space + SPACE_VCO2_1]->addDraw();
            fxspaces[space + SPACE_VCO2_1]->addCB();
            break;
        }
        case VCO3SPACE: {
            int active = (int) _STATE->params[0][VCO3SPACE].load() + SPACE_VCO3_1;
            fxspaces[active]->delCB();
            fxspaces[active]->deldraw();
            _STATE->params[0][VCO3SPACE].store(space);
            fxspaces[space + SPACE_VCO3_1]->redraw();
            fxspaces[space + SPACE_VCO3_1]->addDraw();
            fxspaces[space + SPACE_VCO3_1]->addCB();
            break;
        }
        default:
            break;
    }
}

// All three oscillator subsection switches are Selector2 popups rather than
// ButtonView columns. ButtonView gives every entry a fixed textsize1 height with no
// scale-to-fit, so as soon as a run grows past four entries the extra ones are drawn
// past the bottom of the space and clipped — not merely ugly, unreachable. This first
// bit SUB when PAD made it six; MODAL takes OSC1/OSC2 to five and SUB to six with PAD
// off, so all three had to move.
//
// Selector2's callback is a plain function pointer and cannot capture, so _appState
// comes off the Selector itself (it is a View). Routing through callbackSubFXSwitch
// rather than the default element_callback_new is deliberate: that function stores
// the space param directly, which is what the ButtonView did too.
static int callbackVco1Space(Selector* s, int space) {
    callbackSubFXSwitch(s->_appState, VCO1SPACE, space);
    return 0;
}
static int callbackVco2Space(Selector* s, int space) {
    callbackSubFXSwitch(s->_appState, VCO2SPACE, space);
    return 0;
}
static int callbackVco3Space(Selector* s, int space) {
    callbackSubFXSwitch(s->_appState, VCO3SPACE, space);
    return 0;
}

void callbackFXSwitch(tsl::AppState* _appState, int id, int space) {
    int active = (int) _STATE->params[0][id].load();
    spaces[active]->delCB();
    spaces[active]->deldraw();
    _STATE->params[0][id].store(space);
    spaces[space]->redraw();
    spaces[space]->addDraw();
    spaces[space]->addCB();
    if (space == SPACE_EFFECTS) {
        callbackSubFXSwitch(_appState, FXSPACE, _STATE->params[0][FXSPACE].load());
    } else if (space == SPACE_LFO) {
        callbackSubFXSwitch(_appState, LFOSPACE, _STATE->params[0][LFOSPACE].load());
    } else if (space == SPACE_ARP) {
        callbackSubFXSwitch(_appState, ARPSPACE, _STATE->params[0][ARPSPACE].load());
    } else if (space == SPACE_ROUTE) {
        callbackSubFXSwitch(_appState, ROUTESPACE, _STATE->params[0][ROUTESPACE].load());
    } else if (space == SPACE_VCO1) {
        callbackSubFXSwitch(_appState, VCO1SPACE, _STATE->params[0][VCO1SPACE].load());
    } else if (space == SPACE_VCO2) {
        callbackSubFXSwitch(_appState, VCO2SPACE, _STATE->params[0][VCO2SPACE].load());
    } else if (space == SPACE_VCO3) {
        callbackSubFXSwitch(_appState, VCO3SPACE, _STATE->params[0][VCO3SPACE].load());
    }
}


#include <SkFontMetrics.h>
#include <SkData.h>
#include <SkStream.h>
#include <include/codec/SkCodec.h>
#include <window.h>
#include "infopanel.h"
//#include "iconrender.h"

sk_sp<SkImage> LoadPNG();

// VOLTAIC mark, baked in icon_voltaic.cpp (512x512 RGBA, transparent bg) and
// declared by app.h as tsl::app::icon_data / icon_size — the same bytes the
// splash animation and the desktop window icon draw.
//
// Decoded through SkCodec rather than stbi: the stbi implementation used to
// live in va/window.cpp, part of the old GLFW standalone port that has since
// been deleted, whereas Skia is always available here. Cached because render()
// runs every frame.
static sk_sp<SkImage> LoadVoltaicPNG() {
    static sk_sp<SkImage> cached;
    if (cached)
        return cached;
    auto data = SkData::MakeWithoutCopy(tsl::app::icon_data, tsl::app::icon_size);
    auto codec = SkCodec::MakeFromStream(std::make_unique<SkMemoryStream>(data));
    if (!codec) {
        LOGE("VOLTAIC icon: failed to create SkCodec");
        return nullptr;
    }
    auto [image, result] = codec->getImage();
    if (result != SkCodec::Result::kSuccess &&
        result != SkCodec::Result::kIncompleteInput) {
        LOGE("VOLTAIC icon: failed to decode, result=%d", static_cast<int>(result));
        return nullptr;
    }
    cached = image;
    return cached;
}


#ifdef ANDROID

// Java calls this once per billing answer -- a cached grant, the query result,
// an error -- and the later ones carry the better answer, so dofastrender is
// always updated. The handshake is not: readyForUiSetup is a binary_semaphore
// acquired exactly once per process (uiSetupThr runs behind initdone), and
// release() past a count of one is undefined. Only the first call signals it.
//
// A late answer is not lost by stopping at the semaphore: the cap thread
// re-reads dofastrender after its sleep, so a purchase confirmed while the
// countdown runs still disarms the wall.
static std::atomic<bool> gUiSetupSignalled{false};

void guiSetup(JNIEnv *env, jclass obj, jboolean fastrender, jint screenWidth, jint screenHeight) {
    tsl::AppState* _appState = __STATE;
    if (_appState) {
        _STATE->dofastrender = fastrender;
        if (!gUiSetupSignalled.exchange(true))
            _STATE->readyForUiSetup.release();
    }
}
#endif
class Roottmp : public HorizontalLayout {
public:
    Roottmp(tsl::AppState* appState) : HorizontalLayout(appState, WRAP, 0, CENTER_ALIGN) {};

    void init() override {
        width = _STATE->windowWidth;
        height = _STATE->windowHeight;
        _DATA->panelheight = width / 15.f;
        _STATE->textsize2 = width * 0.025;
        _STATE->textsize1 = width * 0.05;
        _STATE->circleradius = _STATE->textsize2 * .9;

        _STATE->knob_height_Knob = width * 0.11;
        _STATE->knob_height_buttonsandKnob = _STATE->textsize1 + _STATE->knob_height_Knob;
        _STATE->knob_height_titleandKnob = _STATE->textsize1 + _STATE->knob_height_Knob;
        _STATE->knob_height_total = _STATE->textsize1 + _STATE->textsize1 + _STATE->knob_height_Knob;
        _STATE->knob_default_width = width / 6;

        startx = 0;
        stopx = width.load();
        starty = 0;
        stopy = height.load();
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2 * .9f);
        SkFontMetrics metrics{};
        font.getMetrics(&metrics);
        _STATE->maxCharWidtht2 = metrics.fAvgCharWidth;
        float _dummy_x = 0;
        View::measureTextFixed(100, _STATE->textsize1, font, "0", &_dummy_x, &_STATE->startyt2, _STATE->textsize2 * .9f);
        font.setSize(_STATE->textsize1 * .9);
        font.getMetrics(&metrics);
        _STATE->maxCharWidtht1 = metrics.fAvgCharWidth;
        HorizontalLayout::init();
        _DATA->views.loadInfo->init();
        _DATA->views.infopanel->init();
    }
};

class SequenceTicker : public View {
public:
    SequenceTicker(tsl::AppState* appState, int _alignment, int _id, Layout *_parent = nullptr) : View(
            appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
            _alignment, 10, true) {
        id = _id;
        _STATE->parameters[id].view = this;
        size_reference = &_STATE->textsize2;
        if (_parent != nullptr)
            _parent->addChild(this);
        //v->size_reference_scale = .9f;
    }

    void render(void *ctx) override {
        auto *canvas = (SkCanvas *) ctx;
        SkPaint paint;
        paint.setStrokeWidth(lw);
        paint.setAntiAlias(true);
        flush(canvas);
        canvas->save();
        canvas->translate(startx, starty);
        int active = (int) _STATE->params[0][id].load();
        float wfield = width / 32.f;
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2);
        SkRect bounds{};
        auto midi = _STATE->midilearning.load();
        for (int i = 0; i < 32; i++) {
            if (i == hot) {
                paint.setColor(skcol::blue_transparent);
                canvas->drawRect(SkRect::MakeXYWH(i * wfield, 0, wfield, height), paint);
            }
            paint.setColor(midi ? skcol::orange : i == active ? skcol::custom_green : skcol::fg);
            font.measureText(nums32[i], i > 8 ? 2 : 1, SkTextEncoding::kUTF8, &bounds);
            canvas->drawSimpleText(nums32[i], i > 8 ? 2 : 1, SkTextEncoding::kUTF8,
                                   i * wfield + wfield * .5f - bounds.centerX(),
                                   height * .5f - bounds.centerY(),
                                   font, paint);
        }
        canvas->restore();
    };

    void callback(const InputEvent &e) override {
        switch (e.action) {
            case ACTION_DOWN: {
                pointerid = e.pointer_id;
                oldx = e.x;
                oldy = e.y;
                auto act = (int) floorf((e.x - startx) / width * 32.);
                if (act < 0)act = 0;
                else if (act > 31)
                    act = 31;
                hot.store(act);
            }
                break;
            case ACTION_MOVE:
                if (e.pointer_id == pointerid) {
                    if (spacing(oldx, e.x, oldy, e.y) > _STATE->textsize2) {
                        hot.store(-1);
                        pointerid = -1;
                    }
                }
                break;
            case ACTION_UP:
                if (pointerid == e.pointer_id) {
                    pointerid = -1;
                    int step = hot.load();
                    hot.store(-1);
                    if (_STATE->midilearning.load()) {
                        int paramId = ARPSTEP01 + step;
                        _STATE->UiTasksQueue.add_task([this, paramId] {
                            MidiLearning::wait(_STATE, _STATE->active_track.load(), paramId);
                        });
                    } else {
                        tsl::parameters::Event ev;
                        ev.setup(_STATE, 0, ARPSTEP01 + step);
                        _STATE->UiTasksQueue.add_task([this, ev]() mutable {
                            EnterValue::Task(_appState, ev);
                        });
                    }
                }
                break;

        }
    }

private:
    int pointerid{-1};
    float oldx{-1}, oldy{-1};
    std::atomic<int> hot{-1};
};

class IconRender : public View {
public:
    IconRender(tsl::AppState* appState, int a, int b, float c) : View(appState, a, b, c) {};

    void render(void *_c) override {
        auto c = (SkCanvas *) _c;
        auto icon = LoadVoltaicPNG();
        if (!icon)
            return;
        SkPaint paint;
        paint.setColor(skcol::bg);
        c->drawImageRect(icon, SkRect::MakeXYWH(startx, starty, width, height),
                         SkSamplingOptions(SkFilterMode::kLinear,
                                           SkMipmapMode::kNearest), &paint);
    }
};

// The wavetable stack, drawn RANGE-BASED: it shows the frames the synth will
// actually sound. MORPH and WARP AMT are each an A/B pair setting the range
// modulation sweeps, so the stack walks both ranges together — the front slice is
// the current unmodulated sound (both at A), the back slice is full modulation
// (both at B), running backwards when B < A.
//
// This view NO LONGER LOOKS ANYTHING UP. It used to call getWavetableDisplayFrames
// on the render thread, which took gWtCacheMtx — the mutex the worker holds while
// inserting a finished set — and then ran the extraction itself: nf × 110 samples of
// four table reads and two interpolations. Worse, it asked with its OWN key, so the
// picture could legitimately disagree with the sound.
//
// Now the audio thread publishes the frame block of the set it has actually selected
// (wtPublishDisplay, from warmWtSets) and this drains the mailbox to the newest one.
// The render thread's whole job is a pointer swap and the drawing.
//
// REPAINT CONTRACT: perm, so render() runs every frame, but it paints only when the
// picture would differ from what is already on the retained root canvas — a new block
// arrived, the live warp amount moved (runtime warps only, see below), or _dirty says
// something outside all that invalidated the rect (subspace entry, resize, redraw).
class WavetableDisplay : public View {
public:
    // TABLE, MORPH A and MORPH B are deliberately NOT passed: this view no longer
    // decides what to show. The audio thread publishes the block for the set it
    // selected, and the only parameters left here are the runtime-warp amount, which
    // is the one thing that cannot be baked into that block.
    WavetableDisplay(tsl::AppState* appState, float sf, int aspect, int align, int osc,
                     int warpTypeId, int warpAmtId, int warpToId)
        : View(appState, sf, aspect, align, 10, true, "WTDisp"), _osc(osc),
          _warpTypeId(warpTypeId), _warpAmtId(warpAmtId), _warpToId(warpToId) {}

    // The PAD page's form of the same view: `osc` is a PAD mailbox slot
    // (WT_DISPLAY_OSCS + oscillator) and there is no warp machinery on PAD, so the
    // runtime-warp pass is parked and the published block is drawn as-is.
    WavetableDisplay(tsl::AppState* appState, float sf, int aspect, int align, int osc)
        : View(appState, sf, aspect, align, 10, true, "WTDisp"), _osc(osc),
          _warpTypeId(-1), _warpAmtId(-1), _warpToId(-1) {}

    void addRecursiveDraw() override { _dirty = true; View::addRecursiveDraw(); }
    void redraw() override           { _dirty = true; View::redraw(); }

    void render(void *_c) override {
        // Newest block the audio thread published, or null if nothing changed. This is
        // the only cross-thread read here, and it is a pointer move.
        if (auto d = wtTakeDisplay(_osc)) { _frames = std::move(d); _dirty = true; }

        // A PAD instance has no warp ids; 0 amounts make every warp branch below a
        // no-op (wtApplyRuntimeWarp bails on amount 0), so one guard here covers it.
        const bool  hasWarp = _warpTypeId >= 0;
        const int   warpT = hasWarp ? (int)std::lround(_STATE->params[0][_warpTypeId].load()) : 0;
        const float warpA = hasWarp ? (float)_STATE->params[0][_warpAmtId].load() : 0.f;
        const float warpB = hasWarp ? (float)_STATE->params[0][_warpToId].load() : 0.f;

        // Runtime warps are not baked into the block: wtKey folds them onto the clean
        // set, so their amount never reaches the key and no republish happens when it
        // moves. Watching the live amount here is what covers that. Checked for every
        // warp type rather than only the runtime ones -- for a baked type the amount IS
        // in the key, so a move republishes anyway and this only costs one repaint of
        // an identical picture while the new set builds.
        const int wAQ = (int)std::lround(warpA * 256.f), wBQ = (int)std::lround(warpB * 256.f);
        if (wAQ != _cWarpA || wBQ != _cWarpB || warpT != _cWarpT) _dirty = true;
        _cWarpA = wAQ; _cWarpB = wBQ; _cWarpT = warpT;

        // Where modulation actually has this oscillator right now, or "no voice is
        // sounding". Quantised before the compare so a held note at a standstill does
        // not repaint on float noise -- while an LFO IS sweeping this legitimately
        // repaints every frame, which is the cost of showing a live value.
        float mpos = 0.f;
        const bool modLive = wtTakeMorph(_osc, mpos);
        const int  mQ = modLive ? (int)std::lround(mpos * 256.f) : -1;
        if (mQ != _cMorphPos) { _cMorphPos = mQ; _dirty = true; }

        if (!_dirty) return;   // the root canvas already holds this exact picture
        _dirty = false;

        auto c = (SkCanvas *) _c;
        const SkRect r = SkRect::MakeXYWH(startx, starty, width, height);
        c->save();
        c->clipRect(r, false);   // HARD (non-AA) clip: no partial edge pixels to accumulate
        SkPaint bg; bg.setColor(skcol::bg);
        c->drawRect(r, bg);

        // Nothing published yet — the set is still building. Leave the box empty and
        // say nothing: the panel over the keyboard is the one place that reports a
        // build, and a second notice in the corner only competes with it.
        const int F = _frames ? _frames->n : 0;
        if (F <= 0) { c->restore(); return; }

        // Draw the published block directly unless a runtime warp has to go on top --
        // it is shared and const (two oscillators on the same table hold the same
        // pointer), so that pass writes to our own buffer.
        const float* src = _frames->f;
        if (wtApplyRuntimeWarp(warpT, warpA, warpB, *_frames, _warped)) src = _warped;

        // Draw into a 2.6:1 box centred in the widget — the stack needs a landscape
        // rect, and offsetting here avoids resizing the view (PERCENTAGE_FROM_PARENT
        // scales both dimensions together, so the height can't be shrunk via sizing).
        const float pad = 6.f;
        const float iw  = width - 2.f * pad;
        float ih = iw / 2.6f;
        if (ih > height - 2.f * pad) ih = height - 2.f * pad;
        if (iw <= 0.f || ih <= 0.f) { c->restore(); return; }
        const float ix = startx + pad;
        const float iy = starty + (height - ih) * 0.5f;

        const float rise  = ih * 0.40f;                       // vertical depth of stack
        const float amp   = (ih - rise) * 0.5f * 0.92f;       // what's left, above+below
        const float dy    = (F > 1) ? rise / (F - 1) : 0.f;
        const float shear = 0.30f;                            // total sideways travel
        const float dx    = (F > 1) ? iw * shear / (F - 1) : 0.f;
        const float plotw = iw * (0.93f - shear);             // rest of it; the two must
                                                              // sum below 1 or the back
                                                              // frames hit the clip edge
        const float baseY = iy + ih - amp;

        SkPaint fillp; fillp.setColor(skcol::bg); fillp.setStyle(SkPaint::kFill_Style);
        SkPaint strokep; strokep.setStyle(SkPaint::kStroke_Style); strokep.setAntiAlias(true);

        auto buildPath = [&](const float* s, float ox, float oy, SkPath& curve) {
            curve.moveTo(ox, oy - s[0] * amp);
            for (int i = 1; i < N; i++)
                curve.lineTo(ox + plotw * (float)i / (N - 1), oy - s[i] * amp);
        };
        auto drawTrace = [&](const float* s, float ox, float oy, SkColor col, float w) {
            SkPath curve; buildPath(s, ox, oy, curve);
            SkPath solid = curve;                              // opaque skirt below the
            solid.lineTo(ox + plotw, oy + amp * 1.3f);         // curve hides the frames
            solid.lineTo(ox, oy + amp * 1.3f);                 // behind it (painter's)
            solid.close();
            c->drawPath(solid, fillp);
            strokep.setColor(col); strokep.setStrokeWidth(w);
            c->drawPath(curve, strokep);
        };

        // The live trace, blended between the two slices it sits between. Blue against
        // the orange stack for the same reason a knob is blue against its orange mod
        // arc: orange is the range modulation can cover, blue is where the sound is in
        // it. Drawn at its own depth in the painter's order below, so slices in front
        // of it still occlude it.
        const float hPos = modLive ? mpos * (F - 1) : -1.f;
        auto drawLive = [&]() {
            float blend[N];
            int f0 = (int)hPos; if (f0 > F - 2) f0 = (F > 1) ? F - 2 : 0;
            const int   f1 = (F > 1) ? f0 + 1 : f0;
            const float fr = hPos - f0;
            for (int i = 0; i < N; i++) {
                const float s0 = src[(size_t)f0 * N + i], s1 = src[(size_t)f1 * N + i];
                blend[i] = s0 + (s1 - s0) * fr;
            }
            drawTrace(blend, ix + hPos * dx, baseY - hPos * dy, skcol::blue, lw);
        };

        // Back to front, so nearer frames occlude the ones behind. Slice 0 is the
        // unmodulated sound and carries the most alpha; the stack fades away toward B,
        // which is where modulation can take it. The live trace goes down after every
        // slice behind it and before the first one in front — strict <, so a position
        // sitting exactly on a slice draws over that slice rather than under it.
        bool liveDrawn = !modLive;
        for (int f = F - 1; f >= 0; f--) {
            if (!liveDrawn && (float)f < hPos) { drawLive(); liveDrawn = true; }
            const float t = (F > 1) ? (float)f / (F - 1) : 0.f;
            const int   a = (int)(255.f * (0.85f - 0.62f * t));
            drawTrace(src + (size_t)f * N, ix + f * dx, baseY - f * dy,
                      SkColorSetA(skcol::orange, (U8CPU)(a < 0 ? 0 : (a > 255 ? 255 : a))), lw);
        }
        if (!liveDrawn) drawLive();   // modulation sitting on the very front slice
        c->restore();
    }

private:
    static constexpr int N    = WtDisplayFrames::POINTS;
    static constexpr int MAXF = WtDisplayFrames::SLICES;
    int _osc;
    int _warpTypeId, _warpAmtId, _warpToId;
    // The block the audio thread published. Shared and const -- two oscillators on the
    // same table hold the same one -- so the runtime-warp pass writes to _warped.
    std::shared_ptr<const WtDisplayFrames> _frames;
    float _warped[MAXF * N]{};
    int _cWarpT{-1}, _cWarpA{-1}, _cWarpB{-1};   // live runtime-warp amount, quantised
    int _cMorphPos{-1};                          // live modulated position, -1 = silent
    bool _dirty{true};                           // first frame always paints
};

// A translucent panel over the piano while a wavetable or PADsynth set is being
// built off-thread. The wavetable display already says COMPUTING, but it is a
// thumbnail in one oscillator's tab -- it says nothing when you are on another
// tab, and nothing at all about a PAD build, which is the slow one (16.75 MB).
//
// LIFETIME: pushed into queue_draw by the worker thread that runs the build and
// removed when it finishes, via BuildOverlayScope below -- the same shape lc.cpp
// uses for AnimatedSpinner. perm = true so that WHILE it is in the queue it
// renders every frame (the fade and the sweep both need that), but it is only in
// the queue while there is something to report. It does not poll anything.
//
// The first cut had it in the queue permanently, polling wtBuildsInFlight() every
// frame. That is what put a panel on screen at startup that never left: the
// counter reads garbage until its sentinel is seeded, and nothing had seeded it.
// A view that is only present while the work is means there is no idle state to
// get wrong.
//
// It is a composited window of its own (Graphics::getCanvas / deleteWindow), not
// a draw into the root canvas. The piano underneath is never painted over, so
// when the panel goes away there is nothing to repair -- swapWindows() simply
// stops compositing it. Drawing into the root canvas instead would leave a hole
// that only a full piano repaint (63 white keys, 44 black) could fill.
//
// Deliberately NOT added to queue_callback: it does not take touches. A build in
// flight does not stop the keyboard -- held and new notes keep sounding on the
// set the voices already hold -- so swallowing key presses would break playing
// to report something that does not.
class BuildOverlay : public View {
public:
    BuildOverlay(tsl::AppState* appState, View* over)
        : View(appState, WRAP, 0, CENTER_ALIGN, 0, true, "BuildOverlay"), _over(over) {}

    // Written by the worker (the scope), read by the render thread. Plain atomics
    // rather than queue_draw-guarded members because the destructor deliberately takes
    // no draw lock -- see the scope below for why the close is deferred at all.
    std::atomic<bool>    closing{false};
    std::atomic<int64_t> closeAt{0};

    // Runs under queue_draw, so everything it touches is safe against the render walk.
    void addRecursiveDraw() override {
        perm    = true;         // may have dropped itself out of the queue last time
        closing.store(false);   // cancels a linger in progress: a run of builds is ONE panel
        // Only a panel that had actually gone starts a new grace period. Re-opening
        // during a linger has to carry the clock, or every task boundary would restart
        // the wait and a chain of builds would never announce itself.
        if (!_live) { _live = true; _openedAt = tsl::time::nanosecondsSinceEpoch();
                      _lastTick = 0; _fade = 0.f; _phase = 0.f; }
        View::addRecursiveDraw();
    }

    void delRecursiveDraw() override {
        View::delRecursiveDraw();
        if (_windex >= 0) { _STATE->graphics.deleteWindow(_windex); _windex = -1; }
    }

    void render(void *) override {
        const int64_t now = tsl::time::nanosecondsSinceEpoch();
        // Fade off the wall clock rather than a per-frame constant, so it takes the
        // same time at 60 and at 120 Hz. Clamped, or the first frame after a resume
        // (or any stall) would jump the ramp straight to the end.
        double dt = _lastTick ? (double) (now - _lastTick) * 1e-9 : 0.0;
        _lastTick = now;
        if (dt > 0.1) dt = 0.1;

        // A closed panel is not gone yet. The worker is one thread, so three
        // oscillators rebuilding means three sequential tasks, and taking the panel
        // down between them made it blink once per boundary. It stays up through the
        // gap and only really leaves if nothing reopens it inside LINGER_NS.
        const bool lingering = closing.load() && (now - closeAt.load()) < LINGER_NS;
        const bool graced    = (now - _openedAt) >= SHOW_AFTER_NS;
        // THE LINGER MAY ONLY HOLD A PANEL THAT IS ALREADY UP -- it must never raise
        // one. _openedAt is carried across the gaps between tasks so a chain of short
        // builds still gets announced, which means the grace can elapse AFTER the last
        // build finished, while the panel is closing. Without the `_fade > 0.f` term
        // that raised a panel over work that was already done: the sound had changed,
        // and then a notice appeared saying it was busy.
        const bool wantUp    = graced && (!closing.load() || (lingering && _fade > 0.f));

        const float target = wantUp ? 1.f : 0.f;
        const float step = (float) (dt / FADE_SEC);
        if (_fade < target) { _fade += step; if (_fade > target) _fade = target; }
        else if (_fade > target) { _fade -= step; if (_fade < target) _fade = target; }

        if (_fade <= 0.f) {
            if (_windex >= 0) { _STATE->graphics.deleteWindow(_windex); _windex = -1; }
            // Faded out and nothing reopened it: drop out of the draw queue the way
            // LoadInfo does -- clear perm and let the render walk reap the node. A view
            // must NOT delete its own node from inside render(); the walk is still
            // holding it.
            if (closing.load() && !lingering) { _live = false; perm = false; }
            return;
        }

        // Live off the piano's rect every frame: a rotation re-inits the layout
        // under us and getCanvas() resizes the window to match.
        const int x = _over->startx.load(), y = _over->starty.load();
        const int w = _over->width.load(),  h = _over->height.load();
        if (w <= 0 || h <= 0) return;

        SkCanvas *c = _STATE->graphics.getCanvas(_windex, (float) x, (float) y, w, h);
        if (!c) return;      // no GL context yet, or the surface was lost; retry next frame
        c->clear(SK_ColorTRANSPARENT);   // window coords are local: 0,0 is the panel corner

        const U8CPU wa = (U8CPU) (_fade * 220.f);
        SkPaint wash; wash.setColor(SkColorSetA(skcol::bg, wa));
        c->drawRect(SkRect::MakeWH((float) w, (float) h), wash);

        // A soft band travelling left to right once every SWEEP_SEC, and the only
        // thing on the panel. No text and no border: the keyboard has its own frame,
        // and a panel that only ever appears for a build slow enough to outlast the
        // grace has to look BUSY rather than say so. Motion carries that; a caption
        // just puts words over the keys.
        _phase += (float) (dt / SWEEP_SEC);
        _phase -= floorf(_phase);
        const float bandW = w * .30f;
        const float cx = -bandW + _phase * (w + 2.f * bandW);   // fully off both edges
        const SkPoint pts[2] = {{cx - bandW * .5f, 0.f}, {cx + bandW * .5f, 0.f}};
        const SkColor band[3] = {SkColorSetA(skcol::fg, 0),
                                 SkColorSetA(skcol::fg, (U8CPU) (_fade * 34.f)),
                                 SkColorSetA(skcol::fg, 0)};
        const SkScalar stops[3] = {0.f, .5f, 1.f};
        SkPaint sweep;   // kClamp is safe: both end colours are already alpha 0
        sweep.setShader(SkGradientShader::MakeLinear(pts, band, stops, 3, SkTileMode::kClamp));
        c->drawRect(SkRect::MakeWH((float) w, (float) h), sweep);
    }

private:
    // Long, on purpose. Only builds that outlast this are ever announced, and the
    // overwhelming majority do not -- an ordinary wavetable rebuild after a fader
    // release finishes far inside it and is never seen. Short enough and the panel
    // appears on routine work, which reads as a fault rather than as information.
    static constexpr int64_t SHOW_AFTER_NS = 600'000'000;
    // How long a closed panel stays up waiting to be reopened. Only has to outlast the
    // worker popping its next task, which is microseconds -- the margin is for a queue
    // that has something else in front of the next build.
    static constexpr int64_t LINGER_NS = 200'000'000;
    static constexpr double  FADE_SEC = 0.15;
    static constexpr double  SWEEP_SEC = 1.2;               // one pass of the band

    View *_over;
    int64_t _openedAt{0}, _lastTick{0};
    float _fade{0.f}, _phase{0.f};
    bool _live{false};          // on screen or lingering; guarded by queue_draw
    int _windex{-1};
};

// One instance for the app (it needs the piano to borrow a rect from, which a
// stack-local in vco.cpp has no way to reach); the SCOPE is what comes and goes.
static BuildOverlay *gBuildOverlay = nullptr;
static std::mutex gOverlayMtx;          // ordered BEFORE queue_draw, never under it
static int gOverlayCount = 0;

tsl::app::BuildOverlayScope::BuildOverlayScope(tsl::AppState* appState) {
    if (!appState || appState->destroyRequested.load()) return;
    std::lock_guard lk(gOverlayMtx);
    if (!gBuildOverlay) return;      // no GUI yet: build silently, take no reference
    _held = true;
    if (gOverlayCount++ > 0) return;
    // Always call this, in the queue or not: queue_draw.add() drops the duplicate, but
    // addRecursiveDraw() still runs and is what cancels a linger.
    gBuildOverlay->addDraw();
}

tsl::app::BuildOverlayScope::~BuildOverlayScope() {
    if (!_held) return;
    std::lock_guard lk(gOverlayMtx);
    if (--gOverlayCount > 0) return;
    // NO deldraw() here. Removing the view took its window with it, so the gap between
    // one build's task ending and the next one's starting showed a frame with no panel
    // -- a hard blink, once per task boundary. Mark it closing and let the view decide
    // when to leave; a scope opening inside LINGER_NS cancels it and nothing on screen
    // ever changes.
    gBuildOverlay->closeAt.store(tsl::time::nanosecondsSinceEpoch());
    gBuildOverlay->closing.store(true);
}

class Dumm : public View {
public:
    Dumm(tsl::AppState* appState) : View(appState, WRAP, 0, END_ALIGN) {}

    void render(void *_c) override {
        auto a = alpha.load();
        if(a != oldalpha){
            tukey.gen(width, tsl::envelope::wtTUKEY, oldalpha = a);
        }
        auto c = (SkCanvas *) _c;
        flush(c);
        SkPaint paint;
        paint.setColor(skcol::orange);
        paint.setStrokeWidth(lw);
        for (int i = 0; i < width; i++) {
            c->drawLine(startx + i, starty + (height - lw) - tukey[i] * (height -  2*lw), startx + i + 1,starty+ height - lw - tukey[i + 1] * (height - 2 * lw),
                        paint);
        }
        SkFont font(_STATE->font_normal);
        font.setSize(_STATE->textsize2*.9);
        std::stringstream  s;
        s<< oldalpha;
        c->drawSimpleText(s.str().c_str(), s.str().size(),SkTextEncoding::kUTF8, startx + 5, starty + 5 + _STATE->textsize2 * .9, font, paint);
    }

    void callback(const InputEvent &e) override {
        switch (e.action) {
            case ACTION_DOWN: {
                olde = e;
            }
                break;
            case ACTION_MOVE:
                if (e.pointer_id == olde.pointer_id) {
                     auto a = alpha.load() + (olde.y - e.y) / (MYFLOAT) height;
                     if(a<0) a = 0;
                     else if(a>1) a=1;
                     alpha.store(a);
                     olde = e;
                     redraw();
                }
                break;
            case ACTION_UP:
                olde.pointer_id = -1;
        }
    }

private:
    InputEvent olde{};
    MYFLOAT oldalpha{};
    std::atomic<MYFLOAT> alpha{.5};
    tsl::envelope::Window<MYFLOAT> tukey{1024, tsl::envelope::wtTUKEY, 0.5};
};

void tsl::app::guiSetup(tsl::AppState* _appState) {
    // Let every knob draw the range its modulation sources can reach. The hook is
    // static on Knob, so it is installed once rather than per-widget; left unset,
    // tslgraphics renders exactly as it did before.
    tsl::graphics::Knob::modRangeProvider = &tsl::app::modRangeFor2;

#if defined PLATFORM_MOBILE
#if defined OS_IOS
    _appState->dofastrender = true;
#else
    _STATE->readyForUiSetup.try_acquire_for(std::chrono::seconds(10));
#endif
#else
    _appState->dofastrender = true;
#endif
    // User presets and MIDI mappings load for everyone: the free tier is
    // time-limited (startSessionCap), not feature-limited. Presets saved by
    // free users before this model existed were always written to disk, so
    // they appear here for the first time.
    Preset::readPresets(_appState);
    {
        std::vector<std::shared_ptr<MIDIHEADER2>> mappings;
        getMappings(mappings);
        if (!mappings.empty())
            loadMidiMapping(_appState, mappings.front());
    }


    _STATE->rootwin = std::make_unique<Roottmp>(_appState);
    auto main = _STATE->rootwin.get();

    auto pianoview = new PianoView(_appState, 38.2, PERCENTAGE_FROM_PARENT_View, END_ALIGN);
    main->addChild(pianoview);
    // Not a child of anything and not in any queue: it has no place in the layout,
    // it just borrows the piano's rect. BuildOverlayScope pushes and pops it.
    gBuildOverlay = new BuildOverlay(_appState, pianoview);
    //auto ic = new IconRender2(50., PERCENTAGE_FROM_PARENT_View, START_ALIGN);
    //main->addChild(ic);


    main->addChild(new Divider(_appState, END_ALIGN));


    main->addChild(new Meter(_appState, 2.5, PERCENTAGE_FROM_PARENT_View,
                             END_ALIGN));

    _DATA->views.infopanel = new InfoPanel(_appState, WRAP, 0, CENTER_ALIGN);;
    _DATA->views.loadInfo = new LoadInfo(_appState, WRAP, 0, CENTER_ALIGN);

    main->addChild(new Divider(_appState, END_ALIGN));


    auto centerview = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, main);


    auto paramBV = new ButtonView<ButtonViewButton>(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER,
                                                    START_ALIGN, HORIZONTAL, spacenames,
                                                    spacevals, GUISPACE);
    paramBV->size_reference = &_DATA->panelheight;
    paramBV->paddingtop = paramBV->paddingbottom = 10.f;
    paramBV->setCB([_appState](int id, int space){ callbackFXSwitch(_appState, id, space); });
    main->addChild(paramBV);

    auto rootfx = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, centerview);
    auto spacegen = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spacegen->overlap = true;
    spaces[SPACE_GEN] = spacegen;
    auto space_eg = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    space_eg->overlap = true;
    spaces[SPACE_EG] = space_eg;
    auto spaceosc1 = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spaceosc1->overlap = true;
    spaces[SPACE_VCO1] = spaceosc1;
    auto spaceosc2 = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spaceosc2->overlap = true;
    spaces[SPACE_VCO2] = spaceosc2;
    auto spaceosc3 = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spaceosc3->overlap = true;
    spaces[SPACE_VCO3] = spaceosc3;
    auto spacefilt = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spacefilt->overlap = true;
    spaces[SPACE_FILTER] = spacefilt;
    auto spacelfo = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spacelfo->overlap = true;
    spaces[SPACE_LFO] = spacelfo;
    auto spacearp = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spacearp->overlap = true;
    spaces[SPACE_ARP] = spacearp;
    auto spacefx = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spacefx->overlap = true;
    spaces[SPACE_EFFECTS] = spacefx;
    auto spaceroute = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spaceroute->overlap = true;
    spaces[SPACE_ROUTE] = spaceroute;
    auto spacenoise = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, rootfx);
    spacenoise->overlap = true;
    spaces[SPACE_NOISE] = spacenoise;
    // Noise generator section: colour selector + envelope + level, mirroring
    // the oscillator sections (WAVE / AMP EG / GAIN).
    spacenoise->addChild(new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN,
                                       noisemodenames, noisemodes, NOISEMODE, 0, 1, "MODE"));
    spacenoise->addChild(new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN,
                                       filtegnames, filtegvals, NOISEEG, 0, 1, "AMP EG"));
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacenoise);
      auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, NOISEGAIN);
      _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }


    auto panel = new HorizontalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN,
                                      spacegen);
    _DATA->views.toppanel_main = panel;
    //panel->addChild(new Divider(_appState, START_ALIGN));

    panel->size_reference = &_DATA->panelheight;
    //  auto dividertoppanel = new Divider(_appState, START_ALIGN, spacegen);


    auto offbutton = new BypassOffButton(_appState, SYM, 0, START_ALIGN,
                                         reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                         reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                         POWERButton);
    offbutton->padding = 10.f;
    panel->addChild(offbutton);

    View *settingsbutton = new NormalButton(_appState, SYM, 0, START_ALIGN,
                                            reinterpret_cast<const char8_t *>(ICON_MD_MORE_VERT),
                                            reinterpret_cast<const char8_t *>(ICON_MD_MORE_VERT),
                                            SETTINGSBUTTON);
    settingsbutton->padding = 10.f;
    panel->addChild(settingsbutton);

    {
        // Recording is free too — the session cap is the only gate.
        View *recb = new RecordButton(_appState, SYM, 0, START_ALIGN);
        recb->padding = 10.f;
        panel->addChild(recb);
    }
    spacegen->addChild(new VerticalLayout(_appState, 18.f, RATIO_FROM_PARENT_View, START_ALIGN));

    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacegen); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, POSTGAIN); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacegen); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, JITTERCENTS); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    auto ls = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN,
                                   spacegen);
    ls->size_reference = &_STATE->maxCharWidtht2;
    ls->size_reference_scale = 15.f;
    ls->addChild(_DATA->views.presetSelector = new PresetSelector(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE,
                                                                 START_ALIGN, vcomodenames,
                                                                 vcomodes, PRESETSELECT, 0, 1,
                                                                 "LOAD PRESET"));
    // ls->addChild(new TextButton("LOAD PRESET", LOADPRESETBUTTON));

   // if (_STATE->dofastrender.load()) {
        ls->addChild(new PermButton(_appState, "SAVE PRESET", SAVEPRESETBUTTON));
    //}
    ls->addChild(new TasksView(_appState));

    auto modeholder = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacegen);
    modeholder->addChild(new TextToggle(_appState, "MIDI LEARN", MIDILEARNBUTTON));
    modeholder->addChild(new PermButton(_appState, "CLEAR SYNTH", SYNTHRESET));
    modeholder->addChild(new PermButton(_appState, "CLEAR TASKS", CLEARTASKS));

    auto m2 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, END_ALIGN, spacegen);
    m2->addChild(new IconRender(_appState, SYM, 9, START_ALIGN));
    m2->padding = 10;

    auto egbv = new ButtonView<ButtonViewButton>(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, VERTICAL,
                                                 egnames, egvals, EGSPACE);
    space_eg->addChild(egbv);
    egbv->setCB([_appState](int id, int space){ callbackSubFXSwitch(_appState, id, space); });
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, space_eg); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, EG1ATTACK, EGSPACE, 4); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, space_eg); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, EG1DECAY, EGSPACE, 4); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, space_eg); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, EG1SUSTAIN, EGSPACE, 4); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, space_eg); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, EG1RELEASE, EGSPACE, 4); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

#if PA_ENABLE_PAD
    static std::vector<std::string> vcosubnames = {"OSC", "PWM", "WT", "UNI", "MODAL", "PAD"};
    static std::vector<float> vcosubvals = {0, 1, 2, 3, 4, 5};
    // SUB has an extra subsection (MOD) holding all its routing options.
    static std::vector<std::string> vco3subnames = {"OSC", "PWM", "MOD", "WT", "UNI", "MODAL", "PAD"};
    static std::vector<float> vco3subvals = {0, 1, 2, 3, 4, 5, 6};
    // Curated PADsynth tables — a subset of wtselnames, see PAD_TABLES in vco.cpp.
    // Spellings identical to padNames in setup.cpp, which this copy used to abbreviate.
    //
    // DISPLAY ORDER ONLY, exactly as for wtselnames above: Selector2 shows names[i] and
    // writes values[i], so these two may be permuted freely PROVIDED THEY ARE PERMUTED
    // TOGETHER. Nothing stored moves and VCOxPADSEL keeps meaning what it meant.
    // Do not renumber padselvals to 0..17 and do not reorder one array alone.
    // padNames in setup.cpp and PAD_TABLES in vco.cpp are indexed BY VALUE and stay in
    // id order; the difference in order is deliberate.
    //
    // Sorted soft→hard on measured spectral centroid, averaged over PADsynth's own
    // 0.30..1.0 recipe range (SOFT 0.07 octaves above the fundamental → AIR 5.16). The
    // flat sort is honest here in a way it is not for the wavetables: these tables
    // mostly travel only 0.2–1.2 octaves across their morph, so each really does sit at
    // a point on the axis. BUZZ is the one exception, spanning 3.5.
    static std::vector<std::string> padselnames = {
        "SOFT",   "ODD",    "EPIANO", "HOLLOW", "CHIME",  "SHAPER",
        "ORGAN",  "BASIC",  "FM",     "VOWEL",  "GROWL",  "FM2",
        "FIFTHS", "STACK",  "BUZZ",   "METAL",  "FORMANT","AIR"};
    static std::vector<float> padselvals = {
        1,  3,  11, 14, 7,  9,
        6,  0,  10, 4,  16, 15,
        8,  12, 2,  13, 5,  17};
#else
    static std::vector<std::string> vcosubnames = {"OSC", "PWM", "WT", "UNI", "MODAL"};
    static std::vector<float> vcosubvals = {0, 1, 2, 3, 4};
    // SUB has an extra subsection (MOD) holding all its routing options.
    static std::vector<std::string> vco3subnames = {"OSC", "PWM", "MOD", "WT", "UNI", "MODAL"};
    static std::vector<float> vco3subvals = {0, 1, 2, 3, 4, 5};
#endif
    // Modal body list — mirrors kModalCharacters in modal.h, same order (dark/low to
    // bright/high). Keep the two in step: the selector writes an index, not a name.
    // Identical to modalNames in setup.cpp. The padding-to-five that forced STRNG was
    // for a fixed-width inline widget; this is a full-row Selector2 popup and sizes
    // itself, so the names are just the names.
    static std::vector<std::string> modalchnames = {"DEEP", "STRING", "TINE", "WOOD", "BELL", "GLASS"};
    static std::vector<float> modalchvals = {0, 1, 2, 3, 4, 5};

    // WT subsection (WARP folded in): waveform display (left third) + 2 rows of
    // selectors (3 per row) + MORPH and WARP-AMT knobs. VerticalLayout lays its
    // children out horizontally; each inner HorizontalLayout stacks its selectors.
    // `osc` is the 0..2 index the audio thread publishes display frames under
    // (wtPublishDisplay), NOT a parameter id.
    auto buildWtSpace = [&](Layout* spaceosc, int fxidx, int osc, int wtsel, int wtpos,
                            int morphEg, int warpType, int warpAmt, int warpEg, int warpTo, int morphTo) {
        auto sp = fxspaces[fxidx] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc, true);
        // This 33.3 sets the width budget for every title on the page, so it is not a
        // free number to change.
        //
        // sp is a ROW (VerticalLayout lays out along x). disp is its only non-wrap
        // child, so it is sized first and the four WRAP children — TABLE/MRPH EG,
        // the MORPH faders, WARP/WARP EG, the WARP faders — split whatever is left
        // EVENLY: each column is (100 - disp) / 4, i.e. 16.7% of the page. That fits
        // a seven-character title and clips an eight-character one, which is why the
        // morph EG selector is MRPH EG and not MORPH EG. Titles are clipped rather
        // than scaled (ButtonBase.cpp clips to the widget box, font fixed at
        // textsize2 * 0.9), so anything longer simply loses its right-hand end.
        //
        // The levers, if a longer title is ever needed: shrink this number (25 buys
        // one more character, at the cost of the display), or use fewer columns —
        // merging the two fader pairs into one block takes each column to 25%. Giving
        // the selector columns a LARGER share than the fader columns is not available:
        // an explicit width moves a child out of the wrap group into the centre group,
        // which initVertical centres over the full width without accounting for disp,
        // so it would overlap it.
        auto disp = new WavetableDisplay(_appState, 33.3f, PERCENTAGE_FROM_PARENT_View, END_ALIGN,
                                         osc, warpType, warpAmt, warpTo);
        disp->paddingleft = disp->paddingright = disp->paddingtop = disp->paddingbottom = 10.f;
        sp->addChild(disp);
        // The `3., RATIO_FROM_PARENT_View` on these selectors is DEAD and has always
        // been: initHorizontal forces `temp->width = width` on every child before
        // sizing it, and Selector2 overrides computeHeight() to a fixed textsize1*2,
        // so neither the scalefactor nor the aspect ratio is ever read. The selector
        // gets row1's width, and row1's width comes from the split described on disp
        // above. Changing the 3. here does nothing — the budget is disp's number.
        auto row1 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, sp);
        row1->addChild(new Selector2(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN, wtselnames, wtselvals, wtsel, 0, 1, "TABLE"));
        row1->addChild(new Selector2(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN, pwmodsrcnames, pwmodsrcvals, morphEg, 0, 1, "MRPH EG"));
               { auto _w = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, sp);   // MORPH A + B faders, side by side
                 auto _a = new SliderVert(_appState, WRAP, 0, CENTER_ALIGN, wtpos,   0, 1, _w); _a->paddingbottom = 5.f;
                 auto _b = new SliderVert(_appState, WRAP, 0, CENTER_ALIGN, morphTo, 0, 1, _w); _b->paddingbottom = 5.f; }
 auto row2 = new HorizontalLayout(_appState, WRAP, 0, CENTER_ALIGN, sp);
        row2->addChild(new Selector2(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN, warpselnames, warpselvals, warpType, 0, 1, "WARP"));
        row2->addChild(new Selector2(_appState, 3., RATIO_FROM_PARENT_View, START_ALIGN, pwmodsrcnames, pwmodsrcvals, warpEg, 0, 1, "WARP EG"));
        { auto _w = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, sp);   // WARP A + B faders, side by side
          auto _a = new SliderVert(_appState, WRAP, 0, CENTER_ALIGN, warpAmt, 0, 1, _w); _a->paddingbottom = 5.f;
          auto _b = new SliderVert(_appState, WRAP, 0, CENTER_ALIGN, warpTo,  0, 1, _w); _b->paddingbottom = 5.f; }
      };

    // MODAL subsection. Nothing here is build-time — the bank is 14 complex rotations
    // with no table behind it, so every control is live. BODY and STRIKE restructure
    // the bank and so land on the next note-on; DECAY / BRIGHT / MALLET reach the
    // note that is already ringing.
    auto buildModalSpace = [&](Layout* spaceosc, int fxidx, int ch, int dec, int brt,
                               int hrd, int pos) {
        auto sp = fxspaces[fxidx] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc, true);
        auto row1 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, sp);
        row1->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN,
                                     modalchnames, modalchvals, ch, 0, 1, "BODY"));
        { auto _w = new VerticalLayout(_appState, WRAP, 0, END_ALIGN, sp);
          const int ids[4] = {dec, brt, hrd, pos};
          for (int i = 0; i < 4; i++) {
              auto _s = new SliderVert(_appState, WRAP, 0, CENTER_ALIGN, ids[i], 0, 1, _w);
              _s->paddingbottom = 5.f;
          } }
      };

#if PA_ENABLE_PAD
    // PAD subsection. TABLE / BANDW / BW SCL / STRETCH / SEED are all BUILD-time:
    // each change rebuilds ~2.4 MB of tables on the UiTasks worker, so they are
    // design controls. MORPH is the modulatable one and shares VCOxWTPOS +
    // VCOxMORPHTO with the WT oscillator, including its EG / LFO / AT / MW routing.
    // BANDW / BW SCL / STRETCH / SEED are deliberately absent — they are fixed
    // constants now, see PAD_FIXED_BW in vco.h. All four were BUILD inputs, so each
    // one cost a set rebuild to move; what is left is the table, the morph range and
    // its EG, which is everything that responds in real time.
    auto buildPadSpace = [&](Layout* spaceosc, int fxidx, int osc, int padsel,
                             int wtpos, int morphEg, int morphTo) {
        auto sp = fxspaces[fxidx] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc, true);
        // The same stack view the WT page has, fed from the PAD mailbox slot (the
        // audio thread publishes PadSet::display under WT_DISPLAY_OSCS + osc). No
        // warp ids: PAD has no warp. END_ALIGN docks it right, exactly as on the WT
        // page; the existing START_ALIGN children lay out from the left edge
        // independently (initVertical's le group), and their ~40% plus this 33.3%
        // leave clear water between the two groups at every checked size.
        auto disp = new WavetableDisplay(_appState, 33.3f, PERCENTAGE_FROM_PARENT_View, END_ALIGN,
                                         WT_DISPLAY_OSCS + osc);
        disp->paddingleft = disp->paddingright = disp->paddingtop = disp->paddingbottom = 10.f;
        sp->addChild(disp);
        // Selectors first at a fixed ratio, START_ALIGN — children compute their own
        // size inside it (same idiom as the OSC subsection's WAVE / AMP EG column).
        auto row1 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, sp);
        row1->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN,
                                     padselnames, padselvals, padsel, 0, 1, "TABLE"));
        // "MRPH EG", not "MORPH EG": titles are CLIPPED, not scaled (ButtonBase.cpp
        // clips to the widget box with the font fixed), so eight characters lose their
        // right-hand end — the same reason the WT subsection abbreviates it, see the
        // note above buildWtSpace.
        //
        // These are PAD's OWN morph ids now (VCOxPADPOS / VCOxPADMTO / VCOxPADMEG), not
        // the wavetable oscillator's, which they used to share.
        row1->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN,
                                     pwmodsrcnames, pwmodsrcvals, morphEg, 0, 1, "MRPH EG"));
        // MORPH A and B beside the selectors, as left-aligned knobs — the ROUTE VEL
        // idiom (knob in its own column, sized by knob_height_total). This used to be
        // a fader column that also carried the four build-time params; those are
        // fixed constants now, so the modulatable pair is all that is left.
        { const int ids[2] = {wtpos, morphTo};
          for (int i = 0; i < 2; i++) {
              auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, sp);
              auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, ids[i]);
              _k->size_reference = &_STATE->knob_height_total;
              _w->addChild(_k);
          } }
      };
#endif // PA_ENABLE_PAD

    { auto _sw = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spaceosc1);
      _sw->addChild(new Selector2(_appState, 2.f, RATIO_FROM_PARENT_View, START_ALIGN,
                                  vcosubnames, vcosubvals, VCO1SPACE, 0, 1, "SECTION",
                                  callbackVco1Space)); }

    auto spacevco1_1 = fxspaces[SPACE_VCO1_1] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc1, true);
    auto main1 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacevco1_1);
    main1->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, vcowavenames,
                                  vcowavevals, VCO1TYPE, 0, 1, "WAVE"));
    main1->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, filtegnames,
                                  filtegvals, VCO1EG, 0, 1, "AMP EG"));
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1GAIN); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1COARSE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1FINE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    auto htune1 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacevco1_1);
    htune1->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, filtegnames, filtegvals,
                                   VCO1TUNEEG, 0, 1, "TUNE EG"));
    htune1->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, syncnames, syncvals,
                                   VCO1SYNC, 0, 1, "SYNC"));

    auto spacevco1_2 = fxspaces[SPACE_VCO1_2] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc1, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_2); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1PW); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    spacevco1_2->addChild(new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, pwmodsrcnames, pwmodsrcvals,
                                        VCO1PWMODSRC, 0, 1, "PW EG"));
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_2); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1PWMODDEPTH); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    // WT subsection (WARP folded in).
    buildWtSpace(spaceosc1, SPACE_VCO1_3, 0, VCO1WTSEL, VCO1WTPOS, VCO1PWMODSRC, VCO1WARPTYPE, VCO1WARPAMT, VCO1WARPEG, VCO1WARPTO, VCO1MORPHTO);

#if PA_ENABLE_PAD
    buildPadSpace(spaceosc1, SPACE_VCO1_6, 0, VCO1PADSEL, VCO1PADPOS, VCO1PADMEG, VCO1PADMTO);
#endif
    buildModalSpace(spaceosc1, SPACE_VCO1_5, VCO1MODALCH, VCO1MODALDEC, VCO1MODALBRT, VCO1MODALHRD, VCO1MODALPOS);

    // UNI subsection: per-osc unison voices / detune / blend.
    auto spacevco1_4 = fxspaces[SPACE_VCO1_4] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc1, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_4); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1UNIVOICES); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_4); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1UNIDETUNE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco1_4); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO1UNIBLEND); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    { auto _sw = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spaceosc2);
      _sw->addChild(new Selector2(_appState, 2.f, RATIO_FROM_PARENT_View, START_ALIGN,
                                  vcosubnames, vcosubvals, VCO2SPACE, 0, 1, "SECTION",
                                  callbackVco2Space)); }

    auto spacevco2_1 = fxspaces[SPACE_VCO2_1] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc2, true);
    auto main2 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacevco2_1);
    main2->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, vcowavenames, vcowavevals,
                                  VCO2TYPE, 0, 1, "WAVE"));
    main2->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, filtegnames, filtegvals,
                                  VCO2EG, 0, 1, "AMP EG"));
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2GAIN); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2COARSE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2FINE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    auto htune2 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacevco2_1);
    htune2->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, filtegnames, filtegvals,
                                   VCO2TUNEEG, 0, 1, "TUNE EG"));
    htune2->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, syncnames, syncvals,
                                   VCO2SYNC, 0, 1, "SYNC"));

    auto spacevco2_2 = fxspaces[SPACE_VCO2_2] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc2, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_2); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2PW); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    spacevco2_2->addChild(new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, pwmodsrcnames, pwmodsrcvals,
                                        VCO2PWMODSRC, 0, 1, "PW EG"));
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_2); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2PWMODDEPTH); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    buildWtSpace(spaceosc2, SPACE_VCO2_3, 1, VCO2WTSEL, VCO2WTPOS, VCO2PWMODSRC, VCO2WARPTYPE, VCO2WARPAMT, VCO2WARPEG, VCO2WARPTO, VCO2MORPHTO);

#if PA_ENABLE_PAD
    buildPadSpace(spaceosc2, SPACE_VCO2_6, 1, VCO2PADSEL, VCO2PADPOS, VCO2PADMEG, VCO2PADMTO);
#endif
    buildModalSpace(spaceosc2, SPACE_VCO2_5, VCO2MODALCH, VCO2MODALDEC, VCO2MODALBRT, VCO2MODALHRD, VCO2MODALPOS);

    // UNI subsection
    auto spacevco2_4 = fxspaces[SPACE_VCO2_4] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc2, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_4); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2UNIVOICES); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_4); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2UNIDETUNE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco2_4); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO2UNIBLEND); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    { auto _sw = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spaceosc3);
      _sw->addChild(new Selector2(_appState, 2.f, RATIO_FROM_PARENT_View, START_ALIGN,
                                  vco3subnames, vco3subvals, VCO3SPACE, 0, 1, "SECTION",
                                  callbackVco3Space)); }

    auto spacevco3_1 = fxspaces[SPACE_VCO3_1] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc3, true);
    auto main3 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacevco3_1);
    main3->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, vcowavenames, vcowavevals,
                                  VCO3TYPE, 0, 1, "WAVE"));
    main3->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, filtegnames,
                                  filtegvals, VCO3EG, 0, 1, "AMP EG"));
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3GAIN); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3COARSEST); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3FINE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    auto modeholder2 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacevco3_1);
    modeholder2->addChild(new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, filtegnames,
                                        filtegvals, VCO3TUNEEG, 0, 1, "TUNE EG"));

    auto spacevco3_2 = fxspaces[SPACE_VCO3_2] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc3, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_2); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3PW); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    spacevco3_2->addChild(new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, pwmodsrcnames, pwmodsrcvals,
                                        VCO3PWMODSRC, 0, 1, "PW EG"));
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_2); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3PWMODDEPTH); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    // MOD subsection: all four SUB routing toggles (NORMAL/RINGMOD/AM/PM) in one
    // place, plus the AM and PM depth knobs.
    //
    // These are independent multiplicative stages, not alternatives — RingModFast
    // applies ring (bipolar, carrier suppressed) and AM (unipolar, carrier retained)
    // in series, so both together gives in*(m + depth*m^2), which is its own sound
    // rather than a redundant one.
    //
    // They briefly lived in a separate MODE subsection when PAD was enabled, which put
    // "MOD" and "MODE" next to each other in the SECTION switch — indistinguishable at
    // a glance, and a split with no reason behind it once the toggles and their depth
    // knobs belong together. One subsection again, in both flag states.
    auto spacevco3_3 = fxspaces[SPACE_VCO3_3] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc3, true);
    // Toggles keep TextToggle's own START_ALIGN. The MODE subsection overrode it to
    // CENTER_ALIGN, which was fine while they were the only thing in a subsection of
    // their own but centres the stack vertically here — NORMAL then starts a slot down
    // and stops lining up with the SECTION selector's title beside it.
    { auto _m = new HorizontalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacevco3_3);
      for (int i = 0; i < ARRAY_LEN(subnames); i++)
          _m->addChild(new TextToggle(_appState, subnames[i], subvals[i]));
    }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_3); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3AMDEPTH); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_3); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3PMDEPTH); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    buildWtSpace(spaceosc3, SPACE_VCO3_4, 2, VCO3WTSEL, VCO3WTPOS, VCO3PWMODSRC, VCO3WARPTYPE, VCO3WARPAMT, VCO3WARPEG, VCO3WARPTO, VCO3MORPHTO);

    // UNI subsection
    auto spacevco3_5 = fxspaces[SPACE_VCO3_5] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceosc3, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_5); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3UNIVOICES); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_5); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3UNIDETUNE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacevco3_5); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VCO3UNIBLEND); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    buildModalSpace(spaceosc3, SPACE_VCO3_6, VCO3MODALCH, VCO3MODALDEC, VCO3MODALBRT, VCO3MODALHRD, VCO3MODALPOS);

#if PA_ENABLE_PAD
    buildPadSpace(spaceosc3, SPACE_VCO3_7, 2, VCO3PADSEL, VCO3PADPOS, VCO3PADMEG, VCO3PADMTO);

#endif // PA_ENABLE_PAD

    spacefilt->addChild(
            new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, filtmodenames, filtmodevals, FILT_MODE, 0, 1,
                          "MODE"));
    spacefilt->addChild(
            new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, filtegnames, filtegvals, FILTEG, 0, 1,
                          "CUT EG"));
    spacefilt->addChild(
            new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, filtegnames, filtegvals, RESEG, 0, 1,
                          "RES EG"));
    auto kncentwrapper = new HorizontalLayout(_STATE, 6.,  RATIO_FROM_PARENT_View, START_ALIGN);
    auto knreswrapper = new HorizontalLayout(_STATE, 6.,  RATIO_FROM_PARENT_View, START_ALIGN);
    auto kncent = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, FILT_CUT);
    auto knres = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, FILTRES);
    knres->paddingleft = knres->paddingright = kncent->paddingleft = kncent->paddingright = 5.f;
    kncent->size_reference = knres->size_reference = &_STATE->knob_height_total;
    knreswrapper->addChild(knres);
    kncentwrapper->addChild(kncent);
    spacefilt->addChild(kncentwrapper);
    spacefilt->addChild(knreswrapper);
    // MODAL filter (mode 17) extras. Only these two: CUT is the bank's fundamental
    // and RES its decay, so they are already on the knobs above. Left visible in
    // every mode rather than swapped in — the filter space is one flat row, not a
    // subsection stack, so there is nothing here to hide them behind.
    spacefilt->addChild(
            new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, modalchnames, modalchvals,
                          FILTMODALBODY, 0, 1, "BODY"));
    { auto _w = new HorizontalLayout(_STATE, 6., RATIO_FROM_PARENT_View, START_ALIGN);
      auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, FILTMODALPOS);
      _k->paddingleft = _k->paddingright = 5.f;
      _k->size_reference = &_STATE->knob_height_total;
      _w->addChild(_k);
      spacefilt->addChild(_w); }
//    spacefilt->addChild(new Dumm());
    // LFO category
    // DEST selector that MARKS active routes: any destination whose matrix slot
    // (lfoMdId) is nonzero gets a bullet, in the popup and on the value label, so
    // the routes an LFO is driving are visible without cursoring through all 21.
    // The mark state is polled in render() against a bitmask — string rebuilds and
    // the popup re-init only happen when a route appears or disappears.
    class LfoDestSelector : public tsl::graphics::Selector2 {
    public:
        LfoDestSelector(tsl::AppState* appState, float sf, int ar, int align,
                        std::vector<std::string>& n, std::vector<float>& v,
                        uint16_t id, const char* title, int lfoIndex)
            : Selector2(appState, sf, ar, align, n, v, id, 0, 6, title),
              lfo(lfoIndex), base(n) {}

        void render(void* ctx) override {
            refreshMarks();
            Selector2::render(ctx);
        }

    protected:
        void addRecursiveDraw() override {
            lastMask = 0xffffffffu;   // force one rebuild against current state
            Selector2::addRecursiveDraw();
        }

    private:
        void refreshMarks() {
            uint32_t mask = 0;
            for (int d = 1; d <= LFO_MD_NDEST; d++)
                if (_appState->params[0][lfoMdId(lfo, d)].load() != 0.f)
                    mask |= 1u << d;
            if (mask == lastMask) return;
            lastMask = mask;
            // names has no OFF row: list index = dest - 1
            for (int d = 1; d <= LFO_MD_NDEST; d++)
                names[d - 1] = (mask & (1u << d)) ? base[d - 1] + " \xE2\x80\xA2" : base[d - 1];
            popupview.init();
        }
        int lfo;
        std::vector<std::string> base;
        uint32_t lastMask{0xffffffffu};
    };
    static std::vector<std::string> lfosubnames = {"LFO1", "LFO2", "LFO3", "LFO4"};
    static std::vector<float> lfosubvals = {0, 1, 2, 3};
    static std::vector<std::string> lfowavenames = {"SIN", "TRI", "SAW", "SQR"};
    static std::vector<float> lfowavevals = {0, 1, 2, 3};
    // Keep in step with lfoDestNames in setup.cpp — the two lists are duplicated.
    // This is a full-row popup, so there is no reason for the vowel-dropping: MRPH
    // and STRK were squeezes of words that fit perfectly well.
    // Keep in step with lfoDestNames in setup.cpp — that copy is the host's.
    // No OFF entry: with the multi-dest matrix the cursor always points at a real
    // destination and the DEPTH knob always edits something. "Off" is a zero slot.
    // Values 1..21 are the STORED dest ids and must never be renumbered; the old
    // 0 (OFF) is remapped to 1 at load (foldLegacyLfoRoutes) — sound-neutral,
    // because a cursor carries no route of its own.
    static std::vector<std::string> lfodestnames = {"PITCH", "FILT", "AMP", "PW1", "PW2", "PW3", "RES",
                                                    "MORPH1", "MORPH2", "MORPH3", "WARP1", "WARP2", "WARP3",
                                                    "UNI1", "UNI2", "UNI3", "STRIKE",
                                                    "GAIN1", "GAIN2", "GAIN3", "NOISE"};
    static std::vector<float> lfodestvals = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                                             18, 19, 20, 21};
    {
        auto lfodummy = new HorizontalLayout(_appState, 6., RATIO_FROM_PARENT_View, START_ALIGN, spacelfo);
        auto lfobv = new ButtonView<ButtonViewButton>(_appState, WRAP, 0, START_ALIGN, VERTICAL,
                                                      lfosubnames, lfosubvals, LFOSPACE);
        lfodummy->addChild(lfobv);
        lfobv->setCB([_appState](int id, int space){ callbackSubFXSwitch(_appState, id, space); });

        auto buildLfoSpace = [&](int spaceIdx, int rateParam, int depthParam, int waveParam,
                                 int destParam, int phaseParam) {
            auto s = fxspaces[spaceIdx] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacelfo, true);
            { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, s);
              auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, rateParam);
              _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
            { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, s);
              auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, depthParam);
              _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
            { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, s);
              auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, phaseParam);
              _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
            s->addChild(new Selector2(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, lfowavenames, lfowavevals,
                                      waveParam, 0, 3, "WAVE"));
            s->addChild(new LfoDestSelector(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN,
                                            lfodestnames, lfodestvals, destParam, "DEST",
                                            spaceIdx - SPACE_LFO1));
        };
        // DEPTH here is the WINDOW knob: it shows and edits the multi-dest matrix
        // slot the DEST selector points at (lfoWindowTick in synth.cpp). Browsing
        // DEST re-seats the knob; turning the knob writes the selected route.
        buildLfoSpace(SPACE_LFO1, LFO1RATE, LFO1DEPTH, LFO1WAVE, LFO1DEST, LFO1PHASE);
        buildLfoSpace(SPACE_LFO2, LFO2RATE, LFO2DEPTH, LFO2WAVE, LFO2DEST, LFO2PHASE);
        buildLfoSpace(SPACE_LFO3, LFO3RATE, LFO3DEPTH, LFO3WAVE, LFO3DEST, LFO3PHASE);
        buildLfoSpace(SPACE_LFO4, LFO4RATE, LFO4DEPTH, LFO4WAVE, LFO4DEST, LFO4PHASE);
    }

    // Two SEQUENCE pages used to share one label; OPTIONS is the toggles page.
    static std::vector<std::string> arpnames = {"SEQUENCE", "OPTIONS", "ARP", "ARPSTEP"};
    static std::vector<float> arpspacevals = {0, 1, 2, 3};   // own list, not egvals
    auto arpdummy = new HorizontalLayout(_appState, 6., RATIO_FROM_PARENT_View, START_ALIGN, spacearp);
    auto arpbv = new ButtonView<ButtonViewButton>(_appState, WRAP, 0, START_ALIGN, VERTICAL,
                                                  arpnames, arpspacevals, ARPSPACE);
    arpdummy->addChild(arpbv);
    arpbv->setCB([_appState](int id, int space){ callbackSubFXSwitch(_appState, id, space); });

    auto spacearp1 = fxspaces[SPACE_ARP1] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacearp,
                                                               true);
    auto spacearp2 = fxspaces[SPACE_ARP2] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacearp,
                                                               true);
    auto spacearp3 = fxspaces[SPACE_ARP3] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacearp,
                                                               true);
    auto spacearp4 = fxspaces[SPACE_ARP4] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacearp, true);

    static std::vector<std::string> arpmodes = {"OFF", "UP", "DOWN", "UP/DOWN", "BOUNCE"};
    static std::vector<float> arpmodevals = {-1, 0, 1, 2, 3};

    auto seq1dum2 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp1);
    seq1dum2->addChild(new TextToggle(_appState, "LEARN", SEQLEARNING));
    seq1dum2->addChild(new PermButton(_appState, "CLEAR", ARPRESET));
    seq1dum2->addChild(new CurrentNoteView(_appState));
    seq1dum2->addChild(new TextToggle(_appState, "ZERONOTE", ZERONOTE));

    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, SEQ_BPM); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, SEQ_SWING); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp1); auto seqsteps = new PlusMinusControl(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, SEQ_STEPS); seqsteps->size_reference = &_STATE->knob_height_total; seqsteps->paddingtop = seqsteps->paddingbottom = 5.; _w->addChild(seqsteps); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp1); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, SEQ_GAIN); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }


    auto seq2dum1 = new HorizontalLayout(_appState, 3.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp2);
    seq2dum1->addChild(new TextToggle(_appState, "WAIT FOR ZERO", WAITFORZERO));
    seq2dum1->addChild(new TextToggle(_appState, "REINIT NOTES", NOTESETTINGSFROMSYNTH));
    seq2dum1->addChild(new TextToggle(_appState, "DON'T IMPORT NOTES", NONOTES));
    seq2dum1->addChild(new TextToggle(_appState, "SYNC DAW", SEQ_SYNCDAW));

    auto seq2dum2 = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp2);

    static std::vector<std::string> learnmodes = {"APPEND", "REPLACE"};
    static std::vector<float> learnmodevals = {0, 1};
    seq2dum2->addChild(
            new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, learnmodes, learnmodevals,
                          LEARNMODE, 0, 1, "LEARNMODE"));
    static std::vector<std::string> timedivs = {"1/64","1/32","1/16","1/8","1/4","1/2","1","2","4"};
    static std::vector<float> timedivvals = {0,1,2,3,4,5,6,7,8};
    seq2dum2->addChild(
            new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, timedivs, timedivvals,
                          SEQ_TIMEDIV,
                          0, 8, "TIME DIV"));


    auto seq3dum1 = new HorizontalLayout(_appState, 6., RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp3);

    seq3dum1->addChild(
            new Selector2(_appState, VIEW_COMPUTESIZE, VIEW_COMPUTESIZE, START_ALIGN, arpmodes, arpmodevals,
                          ARP_MODE,
                          0, 1, "MODE"));


    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacearp3); auto hh1 = new PlusMinusControl(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, ARP_STEPS); hh1->size_reference = &_STATE->knob_height_total; hh1->paddingtop = hh1->paddingbottom = 5.; _w->addChild(hh1); }


    auto sv = new ScrollView(_appState, WRAP, 0, CENTER_ALIGN, VERTICAL, 8.);
    sv->func = [sv, _appState]() {
        return (_STATE->windowWidth - _STATE->windowWidth / 15. * 0.7) / 6. * 32. /
               (float) sv->window->viewport.width();
    };
    spacearp4->addChild(sv);
    sv->window->addChild(new View(_STATE,1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
    sv->window->addChild(new SequenceTicker(_appState, START_ALIGN, ARP_STEP));
    sv->window->addChild(new View(_STATE,1.25, PERCENTAGE_FROM_MAIN_WINDOW, START_ALIGN));
    auto win = new VerticalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN);
    win->size_reference = &_appState->knob_height_buttonsandKnob;
    sv->window->addChild(win);
    for (int i = 0; i < NUM_ARP_STEPS; i++) {
        auto kn = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, ARPSTEP01 + i);
        kn->size_reference = &_STATE->knob_default_width;
        kn->drawTitle = false;
        kn->padding = 0;
        kn->paddingleft = kn->paddingright = 5.;
        win->addChild(kn);
    }

    auto fxbv = new ButtonView<ButtonViewButton>(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, VERTICAL,
                                                 fxspacenames, fxvals, FXSPACE);
    spacefx->addChild(fxbv);
    fxbv->setCB([_appState](int id, int space){ callbackSubFXSwitch(_appState, id, space); });

    auto spacephaser = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacefx);
    spacephaser->overlap = true;
    fxspaces[SPACEPHASER] = spacephaser;

    auto onffdumph = new VerticalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacephaser);
    auto onoffdumph2 = new HorizontalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, CENTER_ALIGN,
                                          onffdumph);
    onoffdumph2->size_reference = &_DATA->panelheight;
    auto offbuttonph = new NormalButton(_appState, SYM, 0, START_ALIGN,
                                         reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                         reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                         PHASERPOW);
    offbuttonph->padding = 10.f;
    onoffdumph2->addChild(offbuttonph);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacephaser); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, PHASERRANGE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacephaser); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, PHASERFB); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacephaser); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, PHASERRATE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    auto spacerev = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacefx);
    spacerev->overlap = true;
    fxspaces[SPACEREV] = spacerev;

    auto onffdum = new VerticalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacerev);
    auto onoffdum2 = new HorizontalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, CENTER_ALIGN,
                                          onffdum);
    onoffdum2->size_reference = &_DATA->panelheight;
    auto offbuttonrev = new NormalButton(_appState, SYM, 0, START_ALIGN,
                                         reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                         reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                         REVPOW);
    offbuttonrev->padding = 10.f;
    onoffdum2->addChild(offbuttonrev);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerev); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, REV3REF); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerev); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, REV3DAMP); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerev); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, REV3MIX); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerev); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, REV3GAIN); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    auto spacechorus = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacefx);
    spacechorus->overlap = true;
    fxspaces[SPACECHORUS] = spacechorus;
    {
        auto onffdumch = new VerticalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacechorus);
        auto onoffdumch2 = new HorizontalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, CENTER_ALIGN,
                                                onffdumch);
        onoffdumch2->size_reference = &_DATA->panelheight;
        auto offbuttonch = new NormalButton(_appState, SYM, 0, START_ALIGN,
                                            reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                            reinterpret_cast<const char8_t *>(ICON_MD_POWER_SETTINGS_NEW),
                                            CHORUSPOW);
        offbuttonch->padding = 10.f;
        onoffdumch2->addChild(offbuttonch);
    }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacechorus); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CHORUSMIX); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacechorus); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CHORUSDEPTH); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacechorus); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CHORUSRATE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    auto spacedel = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spacefx);
    spacedel->overlap = true;
    fxspaces[SPACEDELAY] = spacedel;
    onffdum = new VerticalLayout(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, spacedel);
    onoffdum2 = new HorizontalLayout(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, CENTER_ALIGN, onffdum);
    onoffdum2->size_reference = &_DATA->panelheight;
    offbuttonrev = new NormalButton(_appState, SYM, 0, START_ALIGN,
                                    reinterpret_cast<const char *>(ICON_MD_POWER_SETTINGS_NEW),
                                    reinterpret_cast<const char *>(ICON_MD_POWER_SETTINGS_NEW),
                                    CDELPOW);
    offbuttonrev->padding = 10.f;
    onoffdum2->addChild(offbuttonrev);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacedel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CDELDEL); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacedel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CDELFB); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacedel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CDDELMODRATE); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacedel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CDDELMODDEPTH); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacedel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, CDELMIX); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    // spacedel->addChild(new Knob(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, REV3GAIN));
    // --- ROUTE tab ---
    static std::vector<std::string> routespacenames = {"VEL", "AT", "MW", "KEY"};
    static std::vector<float> routevals = {0, 1, 2, 3};
    auto routebv = new ButtonView<ButtonViewButton>(_appState, 6.f, RATIO_FROM_PARENT_View, START_ALIGN, VERTICAL,
                                                    routespacenames, routevals, ROUTESPACE);
    spaceroute->addChild(routebv);
    routebv->setCB([_appState](int id, int space){ callbackSubFXSwitch(_appState, id, space); });

    auto spacerouteVel = fxspaces[SPACE_ROUTE_VEL] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceroute, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerouteVel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VEL_TO_FILT); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerouteVel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VEL_TO_AMP); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerouteVel); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, VEL_TO_RES); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

    // Vertical faders in a horizontal bank — fit more routes than stacked knobs.
    auto addFader = [&](Layout* row, int pid) {
        auto s = new SliderVert(_appState, WRAP, 0, CENTER_ALIGN, pid, 0, 1, row);
        s->paddingbottom = 5.f;
    };

    // Aftertouch routing — faders flow left-to-right in a HorizontalLayout row.
    auto spacerouteAt = fxspaces[SPACE_ROUTE_AT] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceroute, true);
    addFader(spacerouteAt, AT_TO_FILT);
    addFader(spacerouteAt, AT_TO_AMP);
    addFader(spacerouteAt, AT_TO_VIBRATO);
    addFader(spacerouteAt, AT_TO_RES);
    addFader(spacerouteAt, AT_TO_LFORATE);
    addFader(spacerouteAt, AT_TO_LFODEPTH);
    addFader(spacerouteAt, AT_TO_WARP);
    addFader(spacerouteAt, AT_TO_MORPH);
    addFader(spacerouteAt, AT_TO_UNI);
    addFader(spacerouteAt, AT_TO_DECAY);

    // Mod wheel routing.
    auto spacerouteMw = fxspaces[SPACE_ROUTE_MW] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceroute, true);
    addFader(spacerouteMw, MW_TO_FILT);
    addFader(spacerouteMw, MW_TO_VIBRATO);
    addFader(spacerouteMw, MW_TO_RES);
    addFader(spacerouteMw, MW_TO_LFORATE);
    addFader(spacerouteMw, MW_TO_LFODEPTH);
    addFader(spacerouteMw, MW_TO_MORPH);
    addFader(spacerouteMw, MW_TO_WARP);
    addFader(spacerouteMw, MW_TO_UNI);
    addFader(spacerouteMw, MW_TO_DECAY);

    // Keytrack routing. The tab is new — SPACE_ROUTE_KEY has been in the fxspaces
    // enum since the ROUTE page was built, but nothing ever added it to
    // routespacenames, so it was never reachable and FILT had no control at all.
    // Two routes only, so they get the VEL page's left-aligned knobs rather than
    // the AT/MW fader bank.
    //
    // FILT defaults to the TOP of its range, unlike every other control in this
    // section. That is not a quirk: the cutoff has always been built upward from the
    // played note, so full tracking is the behaviour this synth has always had and
    // the knob only ever takes it away. Pulling it down is what makes a fixed
    // cutoff — a formant, or a MODL body that stays put while the notes move.
    auto spacerouteKey = fxspaces[SPACE_ROUTE_KEY] = new VerticalLayout(_appState, WRAP, 0, CENTER_ALIGN, spaceroute, true);
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerouteKey); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, KEYTRACK_TO_FILT); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }
    { auto _w = new HorizontalLayout(_appState, 6.f, RATIO_FROM_MAIN_WINDOW, START_ALIGN, spacerouteKey); auto _k = new Knob(_appState, VALUE_FROM_POINTER, VALUE_FROM_POINTER, START_ALIGN, KEYTRACK_TO_DECAY); _k->size_reference = &_STATE->knob_height_total; _w->addChild(_k); }

#ifdef __ANDROID__
    ATTACH
    if (env && tsl::android::appclass) {
        jfieldID jfield = env->GetStaticFieldID(tsl::android::appclass, "startPoweredOn", "Z");
        if (jfield) {
            if ((bool) env->GetStaticBooleanField(tsl::android::appclass, jfield))
                _STATE->player.play();
        } else {
            env->ExceptionClear();
        }
    }
    DETACH
#else
#endif

}

// ---------------------------------------------------------------------------
// Free-session cap. The free tier is the full instrument — factory bank,
// preset saving, MIDI mapping, recording — for five minutes per process, then
// a licence-style modal overlay (the lc.cpp dialog mechanism: the focus grab
// kills every input behind it) ends the session. One purchase (dofastrender)
// removes the cap; desktop and iOS force that flag, so only Android arms this.
#ifdef __ANDROID__
#include <DynamicDialog.h>
#include <chrono>

// Process-lifetime, like gBuildOverlay: a new session is a new process, so the
// wall never comes down once it is up — it only re-registers after a rebuild.
static std::shared_ptr<tsl::graphics::DynamicDialog> gSessionWall;

static void invokeUpgradeActivity() {
    ATTACH
    if (env && tsl::android::activityclass) {
        jmethodID jmid = env->GetStaticMethodID(tsl::android::activityclass, "invokeUpgrade", "()V");
        if (jmid) {
            env->CallStaticVoidMethod(tsl::android::activityclass, jmid);
        } else {
            env->ExceptionClear();
        }
    }
    DETACH
}

// One round trip: reports whether the model notice was ever shown and marks it
// shown, so the dialog appears exactly once per install. Defaults to NOT seen:
// if the JNI lookup fails (e.g. the method lost its proguard keep), the notice
// nags every launch — visible and reported — instead of silently never showing.
static bool capNoticeSeen() {
    bool seen = false;
    ATTACH
    if (env && tsl::android::activityclass) {
        jmethodID jmid = env->GetStaticMethodID(tsl::android::activityclass, "capNoticeSeen", "()Z");
        if (jmid) {
            seen = env->CallStaticBooleanMethod(tsl::android::activityclass, jmid);
        } else {
            env->ExceptionClear();
        }
    }
    DETACH
    return seen;
}

// The trial gate's verdict, pushed from Java once resolved: 0 none, 1 active,
// 2 expired. File-scope rather than in DATA because Java can answer before
// native setup has allocated it -- the mint runs on its own thread from the
// first onCreate.
std::atomic<int> gTrialState{tsl::app::kTrialNone};

static void showWall(tsl::AppState* _appState, const char* title) {
    if (auto old = gSessionWall) {
        // The window was rebuilt behind an up wall; re-register on the new root.
        old->delCB();
        old->deldraw();
    }
    auto d = std::make_shared<tsl::graphics::DynamicDialog>(_appState);
    d->setTitle(title);
    d->setShowKeyboard(false);
    d->setCustomButton("UNLOCK FULL VERSION", [_appState]() {
        _appState->WorkerQueue.add_task([]() { invokeUpgradeActivity(); });
    });
    // The wall must not be dismissable. cancelDialog()'s no-callback fallback
    // removes the view, so an outside tap or BACK (ESC) would take the wall
    // down. A present-but-inert callback swallows every dismissal instead.
    d->onCompleteCallback = [](const tsl::graphics::DynamicDialog::DialogResult&) {};
    d->init();
    d->addDraw();
    d->addCB();
    gSessionWall = d;
}

static void showSessionWall(tsl::AppState* _appState) {
    // Only reachable when the gate could not start or check a trial, so say that
    // rather than describing a "free session" as though ten minutes were the
    // offer. It tells the user what happened and what fixes it.
    showWall(_appState,
             "This session has ended. Voltaic could not reach us to start your "
             "14-day trial, so sessions run for ten minutes until it can. Connect "
             "to the internet and reopen the app to begin the trial.");
}

// Distinct copy on purpose. "Your session has ended" is wrong for somebody whose
// 14 days are up -- they did not run out of a session, they ran out of trial --
// and it is wrong again for a first launch that never had a session at all.
static void showTrialEndedWall(tsl::AppState* _appState) {
    showWall(_appState,
             "Your 14-day trial has ended. Unlock the full version to keep "
             "playing - one payment, no subscription.");
}

// Pushed from TrialGate.java. May land before the UI exists, during the
// countdown, or after the wall is already up -- all three are handled here.
void tsl::app::setTrialState(tsl::AppState* _appState, int state) {
    gTrialState.store(state);
    if (state != kTrialExpired || _appState == nullptr)
        return;
    // A purchase outranks any trial verdict, always. Without this a device that
    // trialled, expired, and THEN bought gets walled on the very next launch:
    // the stored token still says expired, this fires before Play has answered,
    // and the wall cannot be dismissed. The customer pays and the app dies.
    if (_STATE->dofastrender.load())
        return;
    // A signed "expired" verdict is the server telling a device that wiped its
    // token that its turn is over. Show it now rather than letting the countdown
    // run: the answer is already known, and waiting would look like a grant.
    if (_DATA->sessionExpired.exchange(true))
        return;                      // a wall is already up; nothing to add
    _DATA->sessionMute.store(true);
    showTrialEndedWall(_appState);
}

void tsl::app::startSessionCap(tsl::AppState* _appState) {
    if (_STATE->dofastrender.load())
        return;

    const int trial = gTrialState.load();
    // A valid trial is the full instrument. No cap runs underneath it -- that
    // would defeat the point of having a trial at all.
    if (trial == kTrialActive)
        return;
    if (trial == kTrialExpired) {
        _DATA->sessionExpired.store(true);
        _DATA->sessionMute.store(true);
        showTrialEndedWall(_appState);
        return;
    }

    if (_DATA->sessionExpired.load()) {
        // A window rebuild (activity relaunch), not a new process: the session
        // stays over, the wall comes straight back.
        showSessionWall(_appState);
        return;
    }
    if (_DATA->capThread.joinable())
        return; // timer already armed this process
    _DATA->capThread = std::thread([_appState]() {
        constexpr int64_t kFreeSessionMs = 10 * 60 * 1000;

        // Let the trial gate answer before committing to the capped story. The
        // mint is a network round trip kicked off at onCreate and normally lands
        // in well under a second, but this runs the moment the window is up --
        // so without the wait a perfectly good trial gets told "we could not
        // reach you", which is wrong, alarming, and self-contradicting.
        //
        // Costs nothing when there IS no trial: the notice is not urgent, and
        // the app plays uncapped throughout the wait either way.
        for (int i = 0; i < 24 && gTrialState.load() == kTrialNone; i++) {
            _STATE->waitNotify.sleep_for(250);
            if (_STATE->destroyRequested.load())
                return;
        }
        {
            const int resolved = gTrialState.load();
            if (resolved == kTrialActive || _STATE->dofastrender.load())
                return;                       // trial arrived, or a purchase did
            if (resolved == kTrialExpired) {
                if (_DATA->sessionExpired.exchange(true))
                    return;
                _DATA->sessionMute.store(true);
                showTrialEndedWall(_appState);
                return;
            }
        }

        if (!capNoticeSeen()) {
            // Token rendezvous, not acquire_slot()+wait: only THIS dialog's OK
            // (or shutdown) can satisfy the wait.
            auto token = _STATE->waitNotify.begin_wait();
            auto notice = std::make_unique<tsl::graphics::DynamicDialog>(_appState);
            // Shown once per install, and only ahead of a capped countdown -- an
            // ordinary first launch mints a trial and never gets here.
            notice->setTitle("Voltaic could not reach us to start your 14-day free "
                             "trial, so sessions are limited to ten minutes for now. "
                             "Connect to the internet and reopen the app to start "
                             "the trial.");
            notice->setShowKeyboard(false);
            notice->setButtonMode(tsl::graphics::DynamicDialog::ButtonMode::OK_ONLY);
            notice->onCompleteCallback = [_appState, token](const tsl::graphics::DynamicDialog::DialogResult& r) {
                // OK only: an outside tap or BACK arrives as confirmed=false and
                // is ignored, so the notice cannot be lost to a stray touch (and
                // the present callback keeps cancelDialog from removing the view).
                if (r.confirmed)
                    _STATE->waitNotify.complete(token);
            };
            notice->init();
            notice->addDraw();
            notice->addCB();
            _STATE->waitNotify.wait_for_signal(token); // blocks until OK / shutdown
            notice->delCB();
            notice->deldraw();
            if (_STATE->destroyRequested.load())
                return;
            // The notice blocks until OK, which can be minutes. The world may
            // have moved on underneath it -- a phone that found signal, or a
            // purchase that completed -- so re-read before starting a countdown
            // that no longer applies.
            if (gTrialState.load() == kTrialActive || _STATE->dofastrender.load())
                return;
        }
        // The clock starts after the notice: the notice is modal, so counting
        // under it would bill time the user cannot play. sleep_for is immune to
        // stray wakes — only shutdown ends it early.
        _STATE->waitNotify.sleep_for(kFreeSessionMs);
        if (_STATE->destroyRequested.load())
            return;
        if (_STATE->dofastrender.load())
            return;
        // The gate may have answered while the countdown ran -- a phone that
        // found signal, or a purchase that completed. Re-read it here for the
        // same reason dofastrender is re-read: the thread slept through it.
        const int late = gTrialState.load();
        if (late == kTrialActive)
            return;                  // trial arrived; no wall, no cap
        if (_DATA->sessionExpired.exchange(true))
            return;                  // setTrialState already put a wall up
        _DATA->sessionMute.store(true); // synth.cpp glides postgain to 0
        if (late == kTrialExpired)
            showTrialEndedWall(_appState);
        else
            showSessionWall(_appState);
        // Let the fade finish before the stream stops — stream->stop() cuts at
        // a buffer boundary, which clicks if anything is still sounding.
        _STATE->waitNotify.sleep_for(400);
        if (_STATE->destroyRequested.load())
            return;
        _appState->WorkerQueue.add_task([_appState]() {
            // recStop after the fade: the take ends on the fade-out, not a chop.
            if (_appState->player.isrecording.load())
                _appState->player.recStop();
            if (_appState->player.isPlaying())
                _appState->player.stop();
        });
    });
}
#endif // __ANDROID__

void tsl::app::setup_main_window(tsl::AppState* _appState) {
    _STATE->rootwin->redraw();
    _STATE->rootwin->addCB();
    _STATE->rootwin->addDraw();
    callbackFXSwitch(_appState, GUISPACE, (int) _STATE->params[0][GUISPACE].load());
    _DATA->views.loadInfo->redraw();
#ifdef __ANDROID__
    startSessionCap(_appState);
#endif
}
