#include "preset.h"
#include "grainstorm.h"
#include "gui.h"
#include "defines.h"
#include "button.h"
#include "buttonview.h"
#include "tools.h"
#include "envelope.h"
#include <logger.h>
#include "synth.h"
#include "queue.h"
#include "view.h"
#include "Input.h"
#include <sstream>
#include <algorithm>
#include <map>
#include <dirent.h>
#include <logger.h>

struct PresetHeader {
    char header[11]{"TSLVASYNTH"};
    int version{4};
    int64_t date{};  // fixed-width: plain `long` is 4 bytes on Windows (LLP64) vs
                      // 8 bytes on macOS/iOS/Android (LP64) — would misalign every
                      // field after it when reading a preset saved on the other kind
                      // of platform. int64_t is 8 bytes everywhere.
    int namelen{};
    int numparams{};
};

// Old v1/v2 MIDI assignment struct — used only for skipping legacy bytes safely
struct LegacyMidiAssignment {
    uint8_t trackindex{};
    uint16_t miditarget{};
    double min{}, max{};
    uint8_t active_space_lfo{};
    uint8_t active_space_windows{};
    uint8_t active_space_envf{};
    uint8_t active_space_mdelay{};
    uint8_t active_space_granulation{};
    uint8_t active_space_fx{};
    uint8_t active_space_stereo_fx{};
    uint8_t active_space_cross{};
    uint8_t active_space_pv{};
    uint8_t active_space_main{};
    uint8_t dummy2{}, dummy3{}, dummy4{}, dummy5{};
    uint8_t dummy6{}, dummy7{}, dummy8{}, dummy9{}, dummy10{};
};
struct LegacyPresetMidiAssignment {
    uint8_t type{};
    uint8_t channel{};
    uint8_t num{};
    LegacyMidiAssignment midiAssignment{};
};

static const char *headername = "TSLVASYNTH";
static const int hasNotesCode = 13031974;
static const int hasMidiEventsCode = 20250629;

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <tools/PlatformPaths.h>
#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>
#endif

bool Preset::contains(tsl::AppState* _appState, int64_t date) {
    for (auto& p : _DATA->presets)
        if (p.date == date)
            return true;
    return false;
}

bool Preset::get(tsl::AppState* _appState, Preset& preset, int64_t date) {
    for (auto& p : _DATA->presets) {
        if (p.date == date) {
            preset = p;
            return true;
        }
    }
    return false;
}

namespace fs = std::filesystem;

// ── Storage back end ─────────────────────────────────────────────────────────
// See the comment on Preset::customFolder in preset.h. Handles are filesystem
// paths everywhere except an Android install whose user chose their own folder,
// where they are content:// URIs.

#ifdef __ANDROID__
// One static String-returning call into MainActivity. Returns empty on any
// failure -- a missing method (proguard) must degrade to "no custom folder",
// never to a crash.
static std::string jStaticString(const char* name, const char* sig,
                                 const std::string& a, const std::string& b,
                                 const std::string* c = nullptr) {
    std::string out;
    ATTACH
    if (env && tsl::android::activityclass) {
        jmethodID m = env->GetStaticMethodID(tsl::android::activityclass, name, sig);
        if (m) {
            jstring ja = env->NewStringUTF(a.c_str());
            jstring jb = env->NewStringUTF(b.c_str());
            jstring jc = c ? env->NewStringUTF(c->c_str()) : nullptr;
            auto res = (jstring)(c ? env->CallStaticObjectMethod(tsl::android::activityclass, m, ja, jb, jc)
                                   : env->CallStaticObjectMethod(tsl::android::activityclass, m, ja, jb));
            if (res) {
                const char* s = env->GetStringUTFChars(res, nullptr);
                out = s;
                env->ReleaseStringUTFChars(res, s);
            }
        } else {
            env->ExceptionClear();
        }
    }
    DETACH
    return out;
}

// Java hands back "pipe:/<fd>|<uri>": it created or opened the document and
// detached a file descriptor for us. Splitting it here keeps the URI, which is
// what a later delete needs -- a bare fd cannot be turned back into one.
static FILE* fromPipeResult(const std::string& s, const char* mode, std::string* uriOut) {
    if (s.rfind("pipe:/", 0) != 0) return nullptr;
    const auto bar = s.find('|');
    if (bar == std::string::npos) return nullptr;
    const long fd = std::strtol(s.c_str() + 6, nullptr, 10);
    if (fd <= 0) return nullptr;
    if (uriOut) *uriOut = s.substr(bar + 1);
    FILE* f = fdopen((int)fd, mode);
    if (!f) ::close((int)fd);
    return f;
}
#endif

bool Preset::customFolder() {
#ifdef __ANDROID__
    bool custom = false;
    ATTACH
    if (env && tsl::android::activityclass) {
        jmethodID m = env->GetStaticMethodID(tsl::android::activityclass,
                                             "presetFolderIsCustom", "()Z");
        if (m) custom = env->CallStaticBooleanMethod(tsl::android::activityclass, m);
        else env->ExceptionClear();
    }
    DETACH
    return custom;
#else
    return false;
#endif
}

#ifdef __ANDROID__
static bool presetFolderReachable() {
    bool ok = true;
    ATTACH
    if (env && tsl::android::activityclass) {
        jmethodID m = env->GetStaticMethodID(tsl::android::activityclass,
                                             "presetFolderReachable", "()Z");
        if (m) ok = env->CallStaticBooleanMethod(tsl::android::activityclass, m);
        else env->ExceptionClear();
    }
    DETACH
    return ok;
}
#endif

FILE* Preset::openPreset(const std::string& handle, const char* mode) {
    if (handle.rfind("content://", 0) == 0)
        return tsl::app::getFdFromUri(handle, mode);
    return fopen(handle.c_str(), mode);
}

bool Preset::removePreset(const std::string& handle) {
    if (handle.rfind("content://", 0) == 0)
        return tsl::app::deleteFromUri(handle);
    std::error_code ec;
    fs::remove(handle, ec);
    return !ec;
}


static bool readOnePreset(tsl::AppState* _appState, FILE* fd, const std::string& path) {
    PresetHeader header{};
    if (fread(&header, 1, sizeof(PresetHeader), fd) != sizeof(PresetHeader))
        return false;
    if (strcmp(header.header, headername) != 0)
        return false;
    if (Preset::contains(_appState, header.date))
        return false;
    // File-sourced counts drive allocations and reads below — a corrupt or truncated
    // file must fail here, not crash at startup. Bounds are generous: names are a few
    // dozen chars, and numparams can never legitimately exceed a future NUM_PARAMS by
    // more than a version's worth of growth.
    if (header.namelen < 0 || header.namelen > 4096 ||
        header.numparams < 0 || header.numparams > 65536)
        return false;

    std::vector<char> namebuf(header.namelen + 1, '\0');
    if (fread(namebuf.data(), header.namelen, 1, fd) != 1)
        return false;
    std::string n(namebuf.data());

    std::vector<Preset::PresetParam> vals;
    if (header.version == 1) {
        std::vector<float> tmp(header.numparams);
        if (fread(tmp.data(), sizeof(float), tmp.size(), fd) != (size_t)header.numparams)
            return false;
        // A v1 file wider than this build's parameter table would read initvalue past
        // the end of parameters[] — the extra entries have no meaning here anyway.
        const int nCompare = std::min((int)tmp.size(), (int)NUM_PARAMS);
        for (int i = 0; i < nCompare; i++) {
            if (tmp[i] != static_cast<float>(_STATE->parameters[i].initvalue))
                vals.push_back({i, tmp[i]});
        }
    } else {
        vals.resize(header.numparams);
        if (fread(vals.data(), sizeof(Preset::PresetParam), vals.size(), fd) != (size_t)header.numparams)
            return false;
        // Ids are applied as raw params[0] indices at load — drop anything outside
        // this build's table (same guard as unserializeState). This is also what makes
        // a preset from a NEWER build load cleanly: its unknown ids are skipped
        // instead of writing out of bounds. 0 is the reserved slot, never stored.
        vals.erase(std::remove_if(vals.begin(), vals.end(),
                                  [](const Preset::PresetParam& p) {
                                      return p.num <= 0 || p.num >= NUM_PARAMS;
                                  }),
                   vals.end());
    }

    // FILTEG's initvalue changed from 0 (EG1) to -1 (NONE) at version 4. Presets are
    // stored sparsely against initvalue, so a v<=3 file that simply left the filter
    // envelope alone has no FILTEG entry and would now load with it OFF — a silent
    // change to a patch the user already saved. Write the old default back in.
    if (header.version <= 3) {
        bool hasFiltEg = false;
        for (const auto& v : vals)
            if (v.num == FILTEG) { hasFiltEg = true; break; }
        if (!hasFiltEg)
            vals.push_back({(uint16_t)FILTEG, 0.f});
    }

    auto offset = ftell(fd);
    int num{};
    std::vector<VcoPreNote> notes;
    if (fread(&num, sizeof(int), 1, fd) == 1) {
        if (num == hasNotesCode) {
            if (fread(&num, sizeof(int), 1, fd) != 1)
                return false;
            if (num < 0 || num > 10000)   // same style of cap as the midi-events count
                return false;
            notes.resize(num);
            if (fread(notes.data(), sizeof(VcoPreNote), notes.size(), fd) != (size_t)num)
                return false;
        } else {
            fseek(fd, offset, SEEK_SET);
        }
    }

    // MIDI assignments
    std::vector<tsl::parameters::Event> midiEvents;
    if (header.version <= 2) {
        // Old format: drain remaining LegacyPresetMidiAssignment structs — convert to Events
        LegacyPresetMidiAssignment lma;
        while (fread(&lma, sizeof(LegacyPresetMidiAssignment), 1, fd) == 1) {
            if (lma.midiAssignment.miditarget == 0 || lma.midiAssignment.miditarget >= NUM_PARAMS)
                continue;
            tsl::parameters::Event e;
            e.setup(_STATE, 0, lma.midiAssignment.miditarget);
            auto& m = _STATE->parameters[lma.midiAssignment.miditarget];
            e.midiState.channel = lma.channel;
            e.midiState.num = lma.num;
            if (lma.type == 0) {
                e.midiState.type = ParameterType_double;
                auto& m = _STATE->parameters[lma.midiAssignment.miditarget];
                e.midiState.min = (uint16_t)(std::clamp(m.toNormalized(lma.midiAssignment.min), 0., 1.) * UINT16_MAX);
                e.midiState.max = (uint16_t)(std::clamp(m.toNormalized(lma.midiAssignment.max), 0., 1.) * UINT16_MAX);
            } else {
                e.midiState.type = ParameterType_bool;
                e.midiState.inc = 1;
            }
            midiEvents.push_back(e);
        }
    } else {
        // New format: optional hasMidiEventsCode marker + count + Events
        int marker{};
        auto markerOffset = ftell(fd);
        if (fread(&marker, sizeof(int), 1, fd) == 1 && marker == hasMidiEventsCode) {
            int count{};
            if (fread(&count, sizeof(int), 1, fd) == 1 && count > 0 && count < 10000) {
                for (int i = 0; i < count; i++) {
                    tsl::parameters::Event e{};
                    if (fread(&e, sizeof(tsl::parameters::Event), 1, fd) != 1) break;
                    midiEvents.push_back(e);
                }
            }
        } else {
            fseek(fd, markerOffset, SEEK_SET);
        }
    }

    Preset::Preset up{};
    up.name = n; up.path = path; up.date = header.date; up.isSystem = false;
    up.category = Preset::CAT_USER;
    // order is assigned by readPresets (date-desc) and may then be overridden by
    // the saved arrangement; uid keys off the date, which for a user preset is a
    // real creation timestamp rather than a positional value.
    up.uid = Preset::uidOf(std::to_string(header.date).c_str());
    up.values = std::move(vals);
    up.notes = std::move(notes);
    up.midiEvents = std::move(midiEvents);
    _DATA->presets.push_back(std::move(up));
    return true;
}

// User presets default to newest-first inside CAT_USER; loadOrder then applies
// any arrangement the user dragged out in the browser, and sorts. Both listing
// paths below have to end here or the bank comes out unsorted.
static void sortUserAndApplyOrder(tsl::AppState* _appState) {
    std::vector<Preset::Preset*> mine;
    for (auto& p : _DATA->presets)
        if (p.category == Preset::CAT_USER) mine.push_back(&p);
    std::sort(mine.begin(), mine.end(),
              [](const Preset::Preset* a, const Preset::Preset* b) { return a->date > b->date; });
    for (size_t i = 0; i < mine.size(); i++) mine[i]->order = (int)i;
    Preset::loadOrder(_appState);
}

void Preset::readPresets(tsl::AppState* _appState) {
#ifdef __ANDROID__
    if (customFolder()) {
        // The user's own folder: no filesystem path exists behind it, so the
        // listing comes back as content:// URIs and each is opened through a
        // ContentResolver fd. An empty listing here is a real possibility (the
        // grant was revoked, the card was ejected) -- readPresets leaves the
        // bank as-is rather than treating it as "the user has no presets".
        const auto handles = tsl::app::getDirContent("presets");
        if (handles.empty() && !presetFolderReachable()) {
            // The grant was revoked, the card was ejected, or the folder is
            // gone. Presets were MOVED there, so app storage has nothing to
            // fall back to -- saying so beats an empty list that reads as
            // "the update ate my presets".
            showToast(_appState, "Can't reach your preset folder. Your presets are still "
                                 "in it - open Settings, Presets to choose it again.");
        }
        for (const auto& h : handles) {
            FILE* fd = openPreset(h, "rb");
            if (!fd) { LOGE("readPresets: cannot open %s", h.c_str()); continue; }
            readOnePreset(_appState, fd, h);
            fclose(fd);
        }
        sortUserAndApplyOrder(_appState);
        return;
    }
#endif
    std::string dir = tsl::app::getStoragePath("presets");
    if (dir.empty()) return;
#ifdef __ANDROID__
    DIR* d = opendir(dir.c_str());
    if (!d) { LOGE("readPresets: cannot open dir %s", dir.c_str()); return; }
    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (ent->d_type != DT_REG) continue;
        std::string path = dir + "/" + ent->d_name;
        FILE* fd = fopen(path.c_str(), "rb");
        if (!fd) { LOGE("readPresets: cannot open %s", path.c_str()); continue; }
        readOnePreset(_appState, fd, path);
        fclose(fd);
    }
    closedir(d);
#else
    if (!fs::exists(dir)) return;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        FILE* fd = fopen(entry.path().string().c_str(), "rb");
        if (!fd) { LOGE("readPresets: cannot open %s", entry.path().string().c_str()); continue; }
        readOnePreset(_appState, fd, entry.path().string());
        fclose(fd);
    }
#endif
    sortUserAndApplyOrder(_appState);
};

const char* const Preset::categoryNames[CAT_COUNT] = {
    "INIT", "LEADS", "PADS", "BASS", "KEYS", "PLUCKS", "BELLS", "WINDS",
    "TEXTURES", "USER"
};

// (category, order) — the deque's sort, and therefore both the browser's order and
// the LOAD PRESET selector's. Deliberately NOT by date: a factory date is positional
// and gets renumbered by any bank edit, which is exactly what a saved arrangement
// must survive. Ties break on uid so the sort is total and stable across runs.
void Preset::sortAll(tsl::AppState* _appState) {
    std::sort(_DATA->presets.begin(), _DATA->presets.end(),
              [](const Preset& a, const Preset& b) {
                  if (a.category != b.category) return a.category < b.category;
                  if (a.order != b.order) return a.order < b.order;
                  return a.uid < b.uid;
              });
    // Compact `order` to 0..n-1 within each category. Sorting alone leaves holes
    // (a preset dragged into another category takes its number with it) and
    // duplicates (the baked orders of two categories merged by such a move), and
    // both make the NEXT drop land somewhere unexpected. Compacting here means
    // every caller of sortAll ends with canonical numbers, including the ones
    // that only meant to sort.
    int cat = -1, n = 0;
    for (auto& p : _DATA->presets) {
        if (p.category != cat) { cat = p.category; n = 0; }
        p.order = n++;
    }
}

static std::string orderFilePath() {
    const std::string dir = tsl::app::getStoragePath(tsl::app::appName);
    return dir.empty() ? std::string() : dir + "/presetorder.conf";
}

// `uid=category,order` per line. Unknown uids are ignored (a preset that no longer
// exists), and presets with no line keep their baked position — so a bank edit
// degrades to "the new ones sit where they were baked" rather than to a scrambled
// list. A line with a single number is the older `uid=order` form and moves the
// preset within whatever category it was baked into.
void Preset::loadOrder(tsl::AppState* _appState) {
    const std::string path = orderFilePath();
    FILE* fd = path.empty() ? nullptr : fopen(path.c_str(), "rb");
#ifdef __ANDROID__
    // App storage dies with an uninstall; the user's own folder does not. If the
    // local file is gone but the folder still holds its mirror, adopt that --
    // this is what makes a reinstall come back ARRANGED and not merely populated.
    if (!fd && customFolder()) {
        const std::string mode = "r";
        fd = fromPipeResult(jStaticString(
                 "openStorageFile",
                 "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                 "presets", "presetorder.conf", &mode), "rb", nullptr);
    }
#endif
    if (!fd) return;
    std::map<uint32_t, std::pair<int, int>> saved;   // uid -> (category, order)
    char line[128];
    while (fgets(line, sizeof(line), fd)) {
        unsigned long uid = 0; int a = 0, b = 0;
        const int n = sscanf(line, "%lu=%d,%d", &uid, &a, &b);
        if (n == 3)
            saved[(uint32_t)uid] = {a >= 0 && a < CAT_COUNT ? a : -1, b};
        else if (n == 2)
            saved[(uint32_t)uid] = {-1, a};
    }
    fclose(fd);
    for (auto& p : _DATA->presets) {
        auto it = saved.find(p.uid);
        if (it == saved.end()) continue;
        if (it->second.first >= 0) p.category = it->second.first;
        p.order = it->second.second;
    }
    // CAT_INIT has no column in the browser (Default is reached by its RESET SYNTH
    // button), so anything but Default landing there would be invisible and
    // unrecoverable. Only reachable from a file written while INIT was still a
    // drop target, but the cost of the guard is two lines.
    for (auto& p : _DATA->presets)
        if (p.category == CAT_INIT && p.date != LONG_MAX) p.category = CAT_LEADS;
    sortAll(_appState);
}

static void writeOrderLines(tsl::AppState* _appState, FILE* fd) {
    for (const auto& p : _DATA->presets)
        fprintf(fd, "%u=%d,%d\n", p.uid, p.category, p.order);
}

// Written whole on every drop. Via a temp file and a rename so a crash or a kill
// mid-write cannot leave a half file behind: fopen("wb") truncates first, and the
// surviving lines would then apply while the rest silently fell back to their baked
// positions. rename is atomic within a directory on every platform this ships to.
void Preset::saveOrder(tsl::AppState* _appState) {
    const std::string path = orderFilePath();
    if (path.empty()) return;
    const std::string dir = tsl::app::getStoragePath(tsl::app::appName);
    if (!fs::exists(dir)) fs::create_directories(dir);
    const std::string tmp = path + ".tmp";
    FILE* fd = fopen(tmp.c_str(), "wb");
    if (!fd) { LOGE("saveOrder: cannot write %s", tmp.c_str()); return; }
    writeOrderLines(_appState, fd);
    const bool ok = fflush(fd) == 0 && ferror(fd) == 0;
    fclose(fd);
    if (!ok || std::rename(tmp.c_str(), path.c_str()) != 0) {
        LOGE("saveOrder: cannot replace %s", path.c_str());
        std::remove(tmp.c_str());
    }
#ifdef __ANDROID__
    // Mirror into the user's folder so the arrangement survives an uninstall
    // along with the presets themselves. Deliberately a BACKUP, not the
    // authority: SAF has no rename-over-existing, so this write truncates in
    // place and cannot be made atomic the way the file above is. A torn mirror
    // costs nothing -- it is only ever read when the real file is missing, and
    // unparseable lines fall back to baked positions.
    if (customFolder()) {
        const std::string mode = "w";
        if (FILE* m = fromPipeResult(jStaticString(
                "openStorageFile",
                "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                "presets", "presetorder.conf", &mode), "wb", nullptr)) {
            writeOrderLines(_appState, m);
            fclose(m);
        }
    }
#endif
}

void Preset::setupDefault(tsl::AppState* _appState) {
    Preset preset{};
    std::vector<PresetParam> values;
    // From 1: index 0 is PARAM_NOT_ASSIGNED, the reserved slot no preset stores —
    // the same convention unserializeState and the file-read guard enforce.
    for (int i = 1; i < NUM_PARAMS; i++)
        values.push_back({i, static_cast<float>(_STATE->parameters[i].initvalue)});
    Preset d{};
    d.name = "Default"; d.path = "null"; d.date = LONG_MAX; d.isSystem = true;
    d.category = CAT_INIT; d.order = 0; d.uid = uidOf("Default");
    d.values = std::move(values);
    _DATA->presets.push_back(std::move(d));
    _DATA->presetDate = LONG_MAX;
};

// =============================================================================
// 75 baked factory presets, initialized at startup like Default. Dates run
// LONG_MAX-1 downward so the date-descending sort places them directly below
// Default and above every user preset; isSystem=true keeps them out of the
// save/overwrite list and undeletable. loadPreset resets all params to
// initvalue first, so each preset lists only its deviations.
//
// No effects and no sequencer anywhere in the bank, by design: these exist to
// demonstrate the synthesis engine itself, so nothing is propped up by reverb.
//
// ORDER HERE IS THE UI ORDER. The dates below count down from LONG_MAX-1 in
// declaration order and the selector sorts date-descending, so these sections are
// exactly what the user scrolls past. Grouped by what a preset SOUNDS like rather
// than by which engine feature it happens to demonstrate, and ordered
// most-reached-for first (Patrick's call, 2026-08-18):
//   13 leads / 13 pads / 12 bass / 9 keys & organs / 9 plucks /
//   11 bells & mallets / 5 winds & voices / 3 textures & fx = 75.
// The first 26 were cut from 42 for release: one preset per engine feature, no two
// demonstrating the same trick. Everything removed is in git history (see the
// commit that cut it).
// The 2026-08-18 expansion adds 50 with a different charter: LAYERING. Where the
// core 26 demo one feature each, these celebrate multi-oscillator patches and the
// then-new LFO machinery — bipolar DEPTH gain-crossfades between oscillators
// (dests 18/19 with opposite signs at one locked rate), LFO PHASE quadrature and
// saw-phase start parking, per-osc unison on VCO2/VCO3, VCO2SYNC, EG4 transient
// layers, and the returning FOLD / STEP / SAT warps. Inserting them into the
// sections MOVED EVERY DATE below the first insertion point (see the date note
// below) — deliberate, while nothing has shipped. The section REORDER on the same
// day moved every date again, for the same reason and with the same licence.
// Rain Stick was cut that day: a driven WOOD modal body excited by noise, ear-
// reported as "a dumb click on all cps". Its measured fault (brown noise choking
// modalSoftClip, noise reading as a NEGATIVE power contributor) was fixed and the
// preset still did not earn a slot — the fixed version is in git history.
// Two of the 42 (Chime Glass Pad, Ring Mod Bell) are kept from the old PocketAnalog
// file-based banks rather than written for this one - see their own comments.
// Moving a preset between sections therefore also moves its date, and a DAW project
// saved earlier stored the date it was on. The SOUND still restores correctly - the
// project stores every parameter value, not a reference - but the selector will
// highlight whichever preset now holds that date. So reorder deliberately, not
// casually, once this ships.
//
// Design rules held throughout (verified against setup.cpp, synth.cpp, vco.h):
//   * NO effects. REVPOW / CDELPOW / PHASERPOW are never written, and none of them
//     has an initvalue, so all three stay off. Every preset is the raw voice.
//   * NO sequencer / arp: no notes, no SEQ_* / ARP_*.
//   * Presets are SPARSE against initvalue. All three VCOxGAIN init at 0 dB (unity)
//     and all three VCOxTYPE init at 0 (SAW), so VCO2GAIN MUST be written -60 when
//     VCO2 is unused, or every patch has a stray saw in it.
//   * VCOxTUNEEG = -1 wherever a static FINE/COARSE detune is set, otherwise the
//     detune rides EG1 (TUNEEG inits at 0 = EG1) and glides in with the note.
//   * FILTEG also inits at 0 (= EG1), so a patch that wants no filter envelope has
//     to write FILTEG = -1 explicitly. "Filter open" below means
//     {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1} — LP4 at the top of its range with the
//     envelope off. That is a real statement (this voice is unfiltered), unlike an
//     LP2 parked at cutoff 0.9, which is a bypass wearing a filter's name.
//   * WARP is a wavetable-path transform (Vco::aimWt / ensureWt), so it is only ever
//     set on VCOxTYPE 99. It is a silent no-op on SINE/TRI/SAW/PULSE/MODAL/PAD.
//   * PM is skipped whenever the carrier is WT/PAD/MODAL (synth.cpp, `wtCarrier`),
//     so the PM patch uses a plain SINE carrier. This is the same trap that left the
//     old PM Growl Bass a plain saw. PM depth also has no EG of its own — VCO3's gain
//     and amp EG do NOT feed the PM path — so a PM patch's movement must come from
//     somewhere else (here, the filter).
//   * RING / AM depth, unlike PM, IS scaled by VCO3's amp EG (RingModFast::tick gets
//     egtmp[egs[2]] * gains[2]), so VCO3EG is the way to make a ring fade.
//   * A MODAL oscillator is still gated by the amp EG (g0 = egtmp[egs[0]]*gains[0]),
//     so struck patches run EG1 ATTACK 0 / SUSTAIN 1 and let the bank's own DECAY be
//     the sound; RELEASE only sets how fast note-off damps the ring.
//   * Voice lifetime is adsr[egs[0]] and adsr[egs[1]] (VcoNote::isDead), i.e. the two
//     main oscillators' amp EGs — which is why the noise-only patches still set EG1.
//   * POLYPHONY IS 10 (SynthQueue(10)), and a note arriving on a full pool now STEALS
//     a voice instead of being dropped — see the long note on SynthQueue::stealVoice.
//     Until 2026-08-16 it was dropped SILENTLY, which made amp-EG RELEASE a polyphony
//     budget rather than a taste control: at RELEASE .85 (~3.7 s) a preset exhausted
//     all ten slots within seconds of normal playing and every further note was
//     silent. That is why the presets below keep RELEASE <= ~.7. It is no longer a
//     reason to, but the constraint has not vanished, only softened: ten voices is
//     still ten voices, and a long release now means old notes are CUT SHORT under
//     fast playing (quietest and already-released first) rather than new notes going
//     missing. A pad meant to be held in thick chords still should not ask for a
//     release it cannot afford.
//   * FILT_MODE 17 (MODAL filter): CUT is the bank's fundamental (3 octaves below the
//     keytracked note at CUT 0), RES is its T60 across 0.03..0.5 s. KEYTRACK_TO_FILT 0
//     pins the body so the notes move through a fixed resonator.
//   * A DRIVEN modal bank has ~30 dB of insertion loss on broadband input — measured
//     31.3 dB (Glass Body) and 29.5 dB (Struck String) — because a resonator only
//     captures what lands inside its skirts, and MODAL_DRIVEN_TRIM is a further -26 dB.
//     The exciter carries the makeup: NOISEGAIN runs -60..+60 and a noise-fed mode-17
//     patch wants it WELL POSITIVE, not attenuated. Both presets first shipped with the
//     noise turned DOWN and were near-inaudible. Tonal input gets through ~33 dB better
//     than broadband, but only where a harmonic happens to land on a partial, so it is
//     note-dependent and not a substitute for driving the thing properly.
//   * VCO1/VCO2 COARSE is 0..36 semitones and FINE 0..20 cents, both UP only. Only
//     VCO3COARSEST is signed (-60..+60).
//
// EG1 DECAY only shapes the segment between peak and SUSTAIN, so on the high-sustain
// pads it is nearly inaudible and sits at 0.5. On the 14 presets with SUSTAIN <= 0.6
// it is the main shape, and there it is spread 0.28..0.70 on purpose. Do not "tidy"
// those toward each other — that clustering is what made the old bank's envelopes
// interchangeable.
//
// Enums:
//   VCO type   -1 SINE  4 TRI  0 SAW  2 PULSE  99 WT  97 MODAL  98 PAD
//   FILT_MODE   0 LP4  1 LP2  2 BP2  3 BP4  4 HP2  5 HP4  6 NOTCH
//               7 LAD LP  8 LAD BP  9 LAD HP  10 SEM LP  11 SEM BP  12 SEM HP
//               13 SEM NOTCH  14 303 LP  15 303 BP  16 303 HP  17 MODAL
//   WT table    0 BASIC 1 CLIMB 2 ODD 3 FORMANT 4 VOWEL 5 METAL 6 PULSE 7 SOFT
//               8 ORGAN 9 FIFTHS 10 GROWL 11 CHIME 12 DIGITAL 13 FM 14 COMB
//               15 SHAPER 16 SWEEP 17 HOLLOW 18 AIR 19 NOISE 20 CZ RES 21 FM2
//               22 PLUCK 23 RESO 24 BUZZ 25 EPIANO 26 STACK
//   PAD table   0 BASIC 1 SOFT 2 BUZZ 3 ODD 4 VOWEL 5 FORMANT 6 ORGAN 7 CHIME
//               8 FIFTHS 9 SHAPER 10 FM 11 EPIANO 12 STACK 13 METAL 14 HOLLOW
//               15 FM2 16 GROWL 17 AIR
//               NOT the WT numbering above — PAD offers a curated subset, so the two
//               lists disagree on every index. This line is a FOURTH copy of that
//               subset (with PAD_TABLES in vco.cpp, padNames in setup.cpp and
//               padselnames in gui.cpp) and it has been stale before; re-read
//               PAD_TABLES rather than trusting it.
//   WARP        0 OFF 1 SYNC 2 BEND 3 RING 4 FOLD 5 FORMANT 6 BITS 7 RATE
//               8 DRIVE 9 SAT 10 STEP
//   MODAL body  0 DEEP 1 STRING 2 TINE 3 WOOD 4 BELL 5 GLASS
//   LFO wave    0 SIN 1 TRI 2 SAW 3 SQR
//   LFO dest    1 PITCH 2 FILT 3 AMP 4-6 PW1-3 7 RES 8-10 MORPH1-3
//               11-13 WARP1-3 14-16 UNI1-3 17 STRIKE 18-20 GAIN1-3 21 NOISE
//   NOISEMODE  -1 OFF 0 WHITE 1 PINK 2 BROWN
//   VCOxEG / NOISEEG (the amp EGs)  -1 NONE, 0..3 = EG1..EG4. NONE holds the source at
//               its GAIN for the life of the voice; on VCO1/VCO2 it also gives up that
//               source's vote on when the note ends (VcoNote::deriveEgRouting).
//   FILTEG / RESEG / VCOxTUNEEG   -1 NONE, 0..3 = EG1..EG4
//   VCOxWARPEG / VCOxPADMEG        0 OFF, 1..4 = EG1..EG4
//   VCOxPWMODSRC (= WT morph EG)   0 LFO, 1..4 = EG1..EG4
//
// THE BANDPASS AND HIGHPASS MODES ARE FINE. Measured against a saw at CUT .2, the
// ZDF bandpasses sit 17-20 dB below their own lowpass (LADDER BP -50.4 vs LADDER LP
// -30.6, SEM BP -44.6 vs SEM LP -27.8) and lose about 20 dB more across the whole
// cutoff range. That is what a bandpass on a harmonic source does - it is lossy, not
// broken, and POSTGAIN covers it. The classic BP2/BP4 barely lose anything at all.
//   This is on record because it has now been misdiagnosed TWICE, both times from a
// broken measurement: once as a ZDF gain bug, and once as "BP/HP only work on flat
// input, the usable range is the bottom .02 of the knob". The second reading came
// from a harness whose oscillators were not oscillating (see the cps note below) -
// a bandpass rejects DC almost completely, so every bandpass preset read as silent
// while every lowpass preset read normal. Four presets were rewritten off the back
// of that before the harness was checked, and the rewrite has been reverted.
//
// POSTGAIN IS SOLVED, NOT GUESSED - do not hand-tune it. Every value below comes
// from the offline harness, iterated to convergence so that a FOUR-NOTE CHORD at
// velocity 110 reads -27 LUFS (BS.1770 max-momentary), measured at C3 and C4.
// All 26 land inside 1.9 dB, -26.0 to -27.9; worst sample peak is 0.537.
//
// Level on the CHORD, not on one note. Nobody plays one note, four voices sum ~6 dB
// above one, and polyphony here is 10. Levelled on single notes instead, 21 of the
// 40 presets hit the 0.99 hard clip the moment a chord was played.
//
// Four ways this measurement has been got wrong, each of which produced confident,
// self-consistent, entirely fictional numbers:
//   * VCOPreEvent::cps IS A FREQUENCY IN HZ despite the name - the engine does
//     _cps = _freq * onedsr itself. Pre-dividing by the sample rate runs every
//     oscillator at 0.005 Hz, so the render is an amplitude envelope with no audio
//     in it: a DC ramp that never crosses zero. It still has a plausible RMS, which
//     is how it survived two whole gain solves. A K-weighted meter catches it at
//     once (the highpass eats DC), and so does printing min/max of the buffer.
//   * Plain RMS instead of K-weighted loudness, at a target 7 dB hotter than this
//     bank's -27 LUFS standard.
//   * Mean RMS over a window running past note-off makes every pluck and bell read
//     ~20 dB quiet, and the solve then pins them to POSTGAIN +6.
//   * Note-on on a live noteNum RETRIGGERS that voice rather than building a new
//     one, keeping the old oscillator setup while picking up the new POSTGAIN. Send
//     All Sound Off (CC 120) before every measured note.
// =============================================================================

void Preset::setupFactory(tsl::AppState* _appState) {
    // Four presets use the PADsynth oscillator. With PA_ENABLE_PAD 0 the VCOxPAD*
    // ids live PAST NUM_PARAMS and writing them would run off the params array.
    static_assert(PA_ENABLE_PAD, "factory bank uses PAD presets; see types_pocketanalog.h");
    // Log10 params store 20*log10(x): LFO and jitter rates in Hz, MODAL DECAY in s.
    auto Lg = [](float x) { return 20.f * std::log10(x); };
    struct FP { const char* name; std::vector<PresetParam> vals; };
    const FP bank[] = {
    // ----------------------------------------------------------- LEADS ---

    { "Wide Saw Lead", {    // a 7-voice unison stack with nothing behind it — the raw width
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO1UNIVOICES,7},{VCO1UNIDETUNE,.42f},{VCO1UNIBLEND,.8f},
        {VCO2GAIN,-60},
        {FILT_MODE,7},{FILT_CUT,.4f},{FILTRES,.2f},{FILTEG,1},
        {EG2ATTACK,.02f},{EG2DECAY,.5f},{EG2SUSTAIN,.45f},{EG2RELEASE,.3f},
        {EG1ATTACK,.04f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.3f},
        {JITTERCENTS,5},{MW_TO_UNI,.6f},{VEL_TO_FILT,.4f},
        {POSTGAIN,-23} } },

    { "Sync Sweep Lead", {  // real hard sync: VCO1SYNC + COARSE, EG3 sweeps the slave pitch
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},{VCO1SYNC,1},{VCO1COARSE,19},{VCO1TUNEEG,2},
        {EG3ATTACK,0},{EG3DECAY,.55f},{EG3SUSTAIN,.2f},{EG3RELEASE,.3f},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-9},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,7},{FILT_CUT,.62f},{FILTRES,.15f},{FILTEG,-1},
        {EG1ATTACK,.03f},{EG1DECAY,.45f},{EG1SUSTAIN,.75f},{EG1RELEASE,.2f},
        {VEL_TO_AMP,.35f},{MW_TO_VIBRATO,.4f},
        {POSTGAIN,-22} } },

    { "303 Scream", {       // 303 HIGHPASS with a resonance envelope — the counterpart to Acid
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.25f},{VCO1EG,0},
        {VCO2GAIN,-60},
        {FILT_MODE,16},{FILT_CUT,.12f},{FILTRES,.45f},{FILTEG,1},
        {RESEG,1},
        {EG2ATTACK,0},{EG2DECAY,.42f},{EG2SUSTAIN,.16f},{EG2RELEASE,.25f},
        {EG1ATTACK,0},{EG1DECAY,.6f},{EG1SUSTAIN,.6f},{EG1RELEASE,.15f},
        {VEL_TO_FILT,.5f},{VEL_TO_RES,.35f},
        {POSTGAIN,-21} } },

    { "Buzz Reed", {        // BEND warp: the read phase leans and the harmonics tilt with it
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,24},{VCO1WTPOS,.4f},{VCO1MORPHTO,.4f},
        {VCO1WARPTYPE,2},{VCO1WARPAMT,.08f},{VCO1WARPTO,.32f},{VCO1WARPEG,1},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-8},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,2},{FILT_CUT,.2f},{FILTRES,.45f},{FILTEG,-1},
        {EG1ATTACK,.12f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.25f},
        {AT_TO_WARP,.5f},
        {POSTGAIN,-20} } },

    { "Twin Supersaw", {    // Dual saw stacks - 7 voices on VCO1, 5 on VCO2 at a looser detune
                            // and 4 cents sharp - through a ladder LP; the Grand Supersaw
                            // reborn, wheel widens both stacks.
        {VCO1UNIVOICES,7},{VCO1UNIDETUNE,.5f},
        {VCO2GAIN,-3},{VCO2FINE,4},{VCO2TUNEEG,-1},{VCO2UNIVOICES,5},{VCO2UNIDETUNE,.65f},
        {FILT_MODE,7},{FILT_CUT,.5f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.03f},{EG1DECAY,.4f},{EG1SUSTAIN,.85f},{EG1RELEASE,.4f},
        {VEL_TO_AMP,.4f},
        {MW_TO_UNI,.7f},
        {JITTERCENTS,5},
        {POSTGAIN,-25} } },

    { "Sync Snarl", {       // First factory VCO2SYNC: a plain saw anchor under a hard-synced
                            // second osc whose +12st tune falls on EG3 - the snarl lives in osc
                            // two, aftertouch adds vibrato.
        {VCO1GAIN,-3},
        {VCO2SYNC,1},{VCO2COARSE,12},{VCO2TUNEEG,2},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTEG,-1},
        {EG1ATTACK,.01f},{EG1DECAY,.5f},{EG1SUSTAIN,.75f},{EG1RELEASE,.3f},
        {EG3ATTACK,.0f},{EG3DECAY,.55f},{EG3SUSTAIN,.25f},{EG3RELEASE,.3f},
        {VEL_TO_AMP,.4f},
        {AT_TO_VIBRATO,.35f},
        {POSTGAIN,-26} } },

    { "Quadra Pulse", {     // Two pulses PWM-ed at one locked rate in quadrature - LFO2 reads 90
                            // deg behind LFO1 - over a triangle sub; liquid chorused square
                            // lead, wheel deepens the swirl.
        {VCO1TYPE,2},{VCO1PW,.3f},
        {VCO2TYPE,2},{VCO2PW,.55f},{VCO2FINE,3},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,4},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3GAIN,-6},
        {LFO1RATE,Lg(0.7f)},{LFO1DEPTH,.35f},{LFO1DEST,4},
        {LFO2RATE,Lg(0.7f)},{LFO2DEPTH,.35f},{LFO2DEST,5},{LFO2PHASE,90},
        {FILT_MODE,1},{FILT_CUT,.55f},{FILTEG,-1},
        {EG1ATTACK,.05f},{EG1DECAY,.45f},{EG1SUSTAIN,.85f},{EG1RELEASE,.4f},
        {VEL_TO_AMP,.4f},
        {MW_TO_LFODEPTH,.6f},
        {POSTGAIN,-34} } },

    { "Acid Squelch", {     // Saw plus square sub through the 303 LOWPASS; EG2 kicks the cutoff
                            // while EG3 squeezes resonance - velocity-accented squelch, press
                            // for scream.
                            // 303 LP (14), was BP (15), ear-picked 2026-08-30: the ZDF BP's
                            // ~20 dB passband loss forced enough makeup gain that the broadband
                            // top read as fizz (hi-band share .008 vs .000 on LP), and the body
                            // came back 7 dB louder. The real 303 is a lowpass anyway.
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,2},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3GAIN,-8},
        {FILT_MODE,14},{FILT_CUT,.08f},{FILTRES,.75f},{FILTEG,1},
        {EG1ATTACK,.0f},{EG1DECAY,.35f},{EG1SUSTAIN,.6f},{EG1RELEASE,.25f},
        {EG2ATTACK,.0f},{EG2DECAY,.38f},{EG2SUSTAIN,.15f},{EG2RELEASE,.25f},
        {EG3ATTACK,.0f},{EG3DECAY,.5f},{EG3SUSTAIN,.4f},{EG3RELEASE,.25f},
        {RESEG,2},
        {VEL_TO_FILT,.5f},
        {AT_TO_FILT,.4f},
        {POSTGAIN,-14} } },

    { "Pressure Tube", {    // SOFT wavetable driven from clean to overdriven by the returning
                            // SAT warp - aftertouch leans on the saturation like a tube stage,
                            // sine sub keeps the floor.
        {VCO1TYPE,99},{VCO1WTSEL,7},{VCO1WTPOS,.3f},{VCO1MORPHTO,.3f},{VCO1WARPTYPE,9},{VCO1WARPAMT,.25f},{VCO1WARPTO,.8f},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3GAIN,-7},
        {FILT_MODE,10},{FILT_CUT,.5f},{FILTEG,-1},
        {EG1ATTACK,.08f},{EG1DECAY,.55f},{EG1SUSTAIN,.9f},{EG1RELEASE,.45f},
        {VEL_TO_AMP,.35f},
        {AT_TO_WARP,.8f},
        {MW_TO_WARP,.5f},
        {POSTGAIN,-27} } },

    { "Wolf Throat", {      // GROWL table morphed by LFO on VCO2 snarling over a saw and sine
                            // sub - mod wheel speeds the growl, pressure opens the filter.
        {VCO1GAIN,-4},
        {VCO2TYPE,99},{VCO2WTSEL,10},{VCO2WTPOS,.1f},{VCO2MORPHTO,.9f},{VCO2PWMODSRC,0},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3GAIN,-6},
        {LFO1RATE,Lg(1.4f)},{LFO1DEPTH,.7f},{LFO1DEST,9},
        {FILT_MODE,1},{FILT_CUT,.5f},{FILTEG,-1},
        {EG1ATTACK,.03f},{EG1DECAY,.6f},{EG1SUSTAIN,.8f},{EG1RELEASE,.35f},
        {VEL_TO_AMP,.4f},
        {AT_TO_FILT,.4f},
        {MW_TO_LFORATE,.7f},
        {POSTGAIN,-30} } },

    { "Power Fifths", {     // FIFTHS wavetable stack over a 3-voice detuned saw with a slow
                            // triangle resonance wobble on the filter - wheel morphs the stack
                            // open.
        {VCO1TYPE,99},{VCO1WTSEL,9},{VCO1WTPOS,.2f},{VCO1MORPHTO,.8f},
        {VCO2GAIN,-3},{VCO2FINE,5},{VCO2TUNEEG,-1},{VCO2UNIVOICES,3},{VCO2UNIDETUNE,.25f},
        {LFO1RATE,Lg(0.33f)},{LFO1DEPTH,.4f},{LFO1WAVE,1},{LFO1DEST,7},
        {FILT_CUT,.35f},{FILTRES,.55f},{FILTEG,-1},
        {EG1ATTACK,.04f},{EG1DECAY,.7f},{EG1SUSTAIN,.7f},{EG1RELEASE,.4f},
        {VEL_TO_FILT,.4f},
        {MW_TO_MORPH,.6f},
        {POSTGAIN,-23} } },

    { "High Wire", {        // Saw and thin pulse thinned to silver by a ladder HP while a full-
                            // gain triangle sub an octave down carries the weight - bright top,
                            // anchored bottom.
        {VCO2TYPE,2},{VCO2PW,.7f},{VCO2FINE,6},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,4},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3GAIN,0},
        {FILT_MODE,9},{FILT_CUT,.05f},{FILTRES,.2f},{FILTEG,-1},
        {EG1ATTACK,.01f},{EG1DECAY,.3f},{EG1SUSTAIN,.75f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.45f},
        {AT_TO_VIBRATO,.35f},
        {POSTGAIN,-19} } },

    { "Shape Shifter", {    // Saw and square trade places on opposite-sign gain-duck LFOs at one
                            // locked rate - a lead that breathes between shapes; wheel speeds
                            // the trade.
        {VCO2TYPE,2},{VCO2FINE,4},{VCO2TUNEEG,-1},
        {LFO1RATE,Lg(0.5f)},{LFO1DEPTH,.9f},{LFO1DEST,18},
        {LFO2RATE,Lg(0.5f)},{LFO2DEPTH,-.9f},{LFO2DEST,19},
        {FILT_MODE,1},{FILT_CUT,.5f},{FILTEG,-1},
        {EG1ATTACK,.02f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.4f},
        {AT_TO_VIBRATO,.3f},
        {MW_TO_LFORATE,.6f},
        {POSTGAIN,-30} } },

    // ------------------------------------------------------------ PADS ---

    { "Air Choir", {        // PADsynth VOWEL; the movement is a slow MORPH EG, not an LFO.
                            // The morph EG here is AUDIBLE - measured as mean |dB| difference
                            // of the log spectrum early vs late on a held note: 3.87 dB against
                            // a 1.8 dB floor from the PAD's own random-phase read drifting, on a
                            // scale where SINE vs SAW is 5.41 dB. An earlier pass moved this to
                            // FORMANT believing VOWEL's morph was inert; that came from a metric
                            // (cosine similarity of magnitude spectra) which scored SINE vs SAW
                            // at 0.974 and so could not tell anything apart. Do not re-measure
                            // timbre change that way - calibrate on known pairs first.
        {VCO1TYPE,98},{VCO1GAIN,0},{VCO1EG,0},
        // BANDW / BW SCL / STRETCH / SEED are fixed constants now (PAD_FIXED_BW in
        // vco.h), so this preset no longer sets them. It IS revoiced by that: it used
        // to ask for 2.5 cents of smear and now gets VOWEL's half-cap, ~9 cents.
        {VCO1PADSEL,4},   // 4 = VOWEL
        {VCO1PADPOS,.15f},{VCO1PADMTO,.85f},{VCO1PADMEG,3},
        {EG3ATTACK,.8f},{EG3DECAY,.6f},{EG3SUSTAIN,1},{EG3RELEASE,.7f},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.62f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,.62f},{EG1DECAY,.55f},{EG1SUSTAIN,.9f},{EG1RELEASE,.68f},
        {MW_TO_MORPH,.5f},
        {POSTGAIN,-18} } },

    { "PWM Strings", {      // no wavetable anywhere: two pulses, two LFOs on pulse width
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.45f},{VCO1EG,0},
        {VCO2TYPE,2},{VCO2GAIN,-2},{VCO2PW,.55f},{VCO2FINE,7},{VCO2EG,0},{VCO2TUNEEG,-1},
        {LFO1RATE,Lg(.16f)},{LFO1DEPTH,.6f},{LFO1WAVE,0},{LFO1DEST,4},
        {LFO2RATE,Lg(.23f)},{LFO2DEPTH,.6f},{LFO2WAVE,1},{LFO2DEST,5},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-12},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.5f},{FILTRES,.15f},{FILTEG,-1},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.7f},
        {POSTGAIN,-30} } },

    { "Chime Glass Pad", {  // kept from the old PocketAnalog bank. Two CHIME wavetables a
                            // tenth apart with a slow sine LFO walking OSC1's morph; the
                            // shimmer is JITTERCENTS, not a chorus.
                            // The original leaned on REVERB (REVPOW on, both mixes at .5).
                            // Dropped here because this bank is deliberately effect-free.
                            // The old restore recipe — {REVPOW,1},{REV3MIX,.5f},{REV4MIX,.5f},
                            // {REV3REF,.6f},{REV4REF,.6f},{REV3LPCUT,78.06f},{REV4LPCUT,78.06f}
                            // — is HISTORICAL: only Progenitor1Dark is instantiated in this
                            // build and it reads none of the REV4/LPCUT ids, so of that list
                            // only REVPOW, REV3MIX and REV3REF still do anything.
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,11},{VCO1WTPOS,.5f},
        {VCO2TYPE,99},{VCO2GAIN,-4},{VCO2WTSEL,11},{VCO2WTPOS,.7f},{VCO2FINE,10},
        {VCO2EG,0},{VCO2TUNEEG,-1},
        {LFO1RATE,Lg(.09f)},{LFO1DEPTH,.5f},{LFO1WAVE,0},{LFO1DEST,8},
        {FILT_MODE,1},{FILT_CUT,.65f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.75f},
        {JITTERCENTS,9},
        {POSTGAIN,-26} } },

    { "Aurora Veil", {      // Flagship WT-to-WT gain-crossfade: breathy AIR dissolves into
                            // glassy SWEEP every ~16 s over a steady sine sub; the wheel speeds
                            // the tide.
        {VCO1TYPE,99},{VCO1WTSEL,18},{VCO1WTPOS,.4f},{VCO1MORPHTO,.4f},
        {VCO2TYPE,99},{VCO2WTSEL,16},{VCO2WTPOS,.55f},{VCO2MORPHTO,.55f},{VCO2FINE,6},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-9},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.06f)},{LFO1DEPTH,.85f},{LFO1DEST,18},
        {LFO2RATE,Lg(0.06f)},{LFO2DEPTH,-.85f},{LFO2DEST,19},
        {FILT_MODE,1},{FILT_CUT,.13f},{FILTEG,-1},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.65f},
        {VEL_TO_AMP,.25f},
        {AT_TO_FILT,.35f},
        {MW_TO_LFORATE,.5f},
        {POSTGAIN,-19} } },

    { "Orbit Bloom", {      // True quadrature pair: CZ-RES morph and SEM-LP cutoff circle 90 deg
                            // apart so the timbre orbits instead of pumping; sine sub anchors.
        {VCO1TYPE,99},{VCO1WTSEL,20},{VCO1WTPOS,.15f},{VCO1MORPHTO,.75f},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-8},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.10f)},{LFO1DEPTH,.6f},{LFO1DEST,8},
        {LFO2RATE,Lg(0.10f)},{LFO2DEPTH,.35f},{LFO2DEST,2},{LFO2PHASE,90},
        {FILT_MODE,10},{FILT_CUT,.15f},{FILTRES,.3f},{FILTEG,-1},
        {EG1ATTACK,.5f},{EG1DECAY,.6f},{EG1SUSTAIN,.8f},{EG1RELEASE,.6f},
        {VEL_TO_AMP,.3f},
        {MW_TO_LFODEPTH,.6f},
        {POSTGAIN,-23} } },

    { "Cantor Dusk", {      // Dark saw under a PAD FORMANT choir whose vowel drifts on LFO1; a
                            // NOTCH carves the mids while the sub holds the floor, inverted LFO2
                            // walks the notch.
        {VCO1GAIN,-7},{VCO1FINE,5},{VCO1TUNEEG,-1},
        {VCO2TYPE,98},{VCO2PADSEL,5},{VCO2PADPOS,.2f},{VCO2PADMTO,.8f},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-7},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.08f)},{LFO1DEPTH,.55f},{LFO1DEST,9},
        {LFO2RATE,Lg(0.05f)},{LFO2DEPTH,-.3f},{LFO2DEST,2},
        {FILT_MODE,6},{FILT_CUT,.08f},{FILTEG,-1},
        {EG1ATTACK,.58f},{EG1DECAY,.5f},{EG1SUSTAIN,.8f},{EG1RELEASE,.62f},
        {VEL_TO_AMP,.3f},
        {MW_TO_MORPH,.5f},
        {POSTGAIN,-23} } },

    { "Silk Ensemble", {    // First factory VCO2 unison: 5-voice detuned saw ensemble breathing
                            // wider on LFO1, SOFT wavetable felt underneath, SEM-HP cleans the
                            // mud while the sub carries.
        {VCO1TYPE,99},{VCO1WTSEL,7},{VCO1WTPOS,.3f},{VCO1MORPHTO,.3f},{VCO1GAIN,-6},
        {VCO2UNIVOICES,5},{VCO2UNIDETUNE,.45f},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-6},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.07f)},{LFO1DEPTH,.45f},{LFO1DEST,15},
        {FILT_MODE,12},{FILT_CUT,.04f},{FILTRES,.15f},{FILTEG,-1},
        {EG1ATTACK,.5f},{EG1DECAY,.55f},{EG1SUSTAIN,.85f},{EG1RELEASE,.65f},
        {VEL_TO_AMP,.3f},
        {MW_TO_UNI,.6f},
        {POSTGAIN,-19} } },

    { "Comb Tide", {        // Slow SAW LFO ramps the filter open over ~30 s, PHASE 270 so each
                            // note enters part-lit; COMB table with an octave sine and a late
                            // brown-noise swell on EG4.
        {VCO1TYPE,99},{VCO1WTSEL,14},{VCO1WTPOS,.3f},{VCO1MORPHTO,.3f},
        {VCO2TYPE,-1},{VCO2COARSE,12},{VCO2TUNEEG,-1},{VCO2GAIN,-14},
        {NOISEMODE,2},{NOISEGAIN,-20},{NOISEEG,3},
        {LFO1RATE,Lg(0.03f)},{LFO1DEPTH,.55f},{LFO1WAVE,2},{LFO1DEST,2},{LFO1PHASE,270},
        {FILT_CUT,.10f},{FILTEG,-1},
        {EG1ATTACK,.62f},{EG1DECAY,.55f},{EG1SUSTAIN,.8f},{EG1RELEASE,.66f},
        {EG4ATTACK,.78f},{EG4DECAY,.5f},{EG4SUSTAIN,.7f},{EG4RELEASE,.6f},
        {VEL_TO_FILT,.3f},
        {MW_TO_LFODEPTH,.5f},
        {POSTGAIN,-18} } },

    { "Vesper Rise", {      // PAD FIFTHS organum with a quiet sine that bends up 19 semitones
                            // into place on EG3 (TUNEEG as a riser) over a sine sub; aftertouch
                            // adds vibrato.
        {VCO1TYPE,98},{VCO1PADSEL,8},{VCO1PADPOS,.3f},{VCO1PADMTO,.3f},
        {VCO2TYPE,-1},{VCO2COARSE,19},{VCO2TUNEEG,2},{VCO2GAIN,-16},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-7},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.12f},{FILTEG,-1},
        {EG1ATTACK,.45f},{EG1DECAY,.6f},{EG1SUSTAIN,.75f},{EG1RELEASE,.68f},
        {EG3ATTACK,.8f},{EG3DECAY,.5f},{EG3SUSTAIN,1},{EG3RELEASE,.6f},
        {AT_TO_VIBRATO,.35f},
        {MW_TO_FILT,.5f},
        {POSTGAIN,-21} } },

    { "Deep Undertow", {    // GROWL table creeping open on EG3 through a LADDER-BP whose band
                            // sinks on an inverted LFO while resonance breathes at its own rate;
                            // sub holds the seabed.
        {VCO1TYPE,99},{VCO1WTSEL,10},{VCO1WTPOS,.15f},{VCO1MORPHTO,.6f},{VCO1PWMODSRC,3},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-6},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.07f)},{LFO1DEPTH,-.45f},{LFO1DEST,2},
        {LFO2RATE,Lg(0.13f)},{LFO2DEPTH,.4f},{LFO2DEST,7},
        {FILT_MODE,8},{FILT_CUT,.07f},{FILTRES,.35f},{FILTEG,-1},
        {EG1ATTACK,.4f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.6f},
        {EG3ATTACK,.85f},{EG3DECAY,.5f},{EG3SUSTAIN,1},{EG3RELEASE,.5f},
        {AT_TO_RES,.4f},
        {MW_TO_FILT,.45f},
        {POSTGAIN,-16} } },

    { "Folding Dawn", {     // FOLD warp is the star: a near-sine BASIC table folds into
                            // harmonics on slow EG3 and STAYS folded, pink-noise air above,
                            // sub below; the wheel accelerates the dawn while it rises.
                            // WARPTO .9 (was .55, ear-picked 2026-08-30): the fold depth is
                            // what makes the bloom audible — .05->.55 measured a 36 Hz
                            // centroid drift, .05->.9 swings it 458 Hz.
        {VCO1TYPE,99},{VCO1WTSEL,0},{VCO1WTPOS,.2f},{VCO1MORPHTO,.2f},{VCO1WARPTYPE,4},{VCO1WARPAMT,.05f},{VCO1WARPTO,.9f},{VCO1WARPEG,3},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-8},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {NOISEMODE,1},{NOISEGAIN,-24},
        // Cutoff rides the SAME slow EG3 as the fold (FILTEG is 0-BASED: 2 = EG3,
        // unlike WARPEG which is 1-based). Deep fold under the old static CUT .16
        // read as the spectrum CLOSING — the LP2 shaved every harmonic the fold
        // added. Starting at the note floor and opening to .45 makes brightness
        // arrive WITH the fold. Ear-picked over CUT .3 and static variants.
        {FILT_MODE,1},{FILT_CUT,.45f},{FILTEG,2},
        {EG1ATTACK,.52f},{EG1DECAY,.55f},{EG1SUSTAIN,.8f},{EG1RELEASE,.6f},
        // ATTACK .95: the fold is the dawn, and at .8 the RC curve put 86% of the
        // travel inside the first second — ear-reported as barely noticeable. At
        // .95 (k=0.63/s) the bloom is audible for ~5 s. SUSTAIN 1 so it arrives
        // and STAYS: the old .7 sustain pulled most of the travel back out.
        {EG3ATTACK,.95f},{EG3DECAY,.6f},{EG3SUSTAIN,1},{EG3RELEASE,.55f},
        {VEL_TO_AMP,.3f},
        {MW_TO_WARP,.6f},
        {POSTGAIN,-28} } },

    { "Hollow Moon", {      // PAD HOLLOW dark core with a jittered sine two octaves up and a
                            // sine sub; EG2 blooms the SEM-LP cutoff late while a slow pitch LFO
                            // drifts the whole thing.
        {VCO1TYPE,98},{VCO1PADSEL,14},{VCO1PADPOS,.5f},{VCO1PADMTO,.5f},
        {VCO2TYPE,-1},{VCO2COARSE,24},{VCO2TUNEEG,-1},{VCO2GAIN,-18},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-7},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.11f)},{LFO1DEPTH,.06f},{LFO1DEST,1},
        {FILT_MODE,10},{FILT_CUT,.06f},{FILTEG,1},
        {EG1ATTACK,.55f},{EG1DECAY,.6f},{EG1SUSTAIN,.8f},{EG1RELEASE,.64f},
        {EG2ATTACK,.75f},{EG2DECAY,.7f},{EG2SUSTAIN,.5f},{EG2RELEASE,.6f},
        {AT_TO_FILT,.4f},
        {MW_TO_LFODEPTH,.5f},
        {JITTERCENTS,6},
        {POSTGAIN,-23} } },

    { "Vaulted Fifths", {   // Parallel organum with every osc a real layer: 3-voice saw
                            // unison at the root, a saw a FIFTH UP blooming in late on
                            // EG2's slow attack, a triangle a FOURTH DOWN. Both added
                            // notes are the same pitch class, so chords stay chords.
                            // LP4 breathes on a ~20 s LFO; the wheel opens the vault.
        {VCO1UNIVOICES,3},{VCO1UNIDETUNE,.28f},
        {VCO2COARSE,7},{VCO2FINE,5},{VCO2TUNEEG,-1},{VCO2GAIN,-5},{VCO2EG,1},
        {EG2ATTACK,.75f},{EG2DECAY,.5f},{EG2SUSTAIN,1},{EG2RELEASE,.6f},
        {VCO3NORMAL,1},{VCO3TYPE,4},{VCO3COARSEST,-5},{VCO3TUNEEG,-1},{VCO3GAIN,-4},{VCO3EG,0},
        {LFO1RATE,Lg(0.05f)},{LFO1DEPTH,.4f},{LFO1DEST,2},
        {FILT_MODE,0},{FILT_CUT,.14f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.55f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.66f},
        {JITTERCENTS,4},
        {VEL_TO_AMP,.3f},
        {MW_TO_FILT,.5f},
        {AT_TO_VIBRATO,.3f},
        {POSTGAIN,-25} } },

    { "Rose Window", {      // Three octaves, three characters: EPIANO tines morphing on
                            // a slow LFO at the root, a 3-voice saw shimmer an OCTAVE UP
                            // whose gain tides in and out on LFO2, a triangle an OCTAVE
                            // DOWN for the floor. Glass jitter on top; pressure opens
                            // the filter, the wheel pushes the tine morph.
        {VCO1TYPE,99},{VCO1WTSEL,25},{VCO1WTPOS,.25f},{VCO1MORPHTO,.7f},
        {LFO1RATE,Lg(0.08f)},{LFO1DEPTH,.5f},{LFO1DEST,8},
        {VCO2COARSE,12},{VCO2TUNEEG,-1},{VCO2GAIN,-8},{VCO2UNIVOICES,3},{VCO2UNIDETUNE,.32f},
        {LFO2RATE,Lg(0.05f)},{LFO2DEPTH,.45f},{LFO2DEST,19},
        {VCO3NORMAL,1},{VCO3TYPE,4},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3GAIN,-5},{VCO3EG,0},
        {FILT_MODE,1},{FILT_CUT,.22f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.5f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.68f},
        {JITTERCENTS,7},
        {VEL_TO_AMP,.3f},
        {AT_TO_FILT,.35f},
        {MW_TO_MORPH,.6f},
        {POSTGAIN,-25} } },

    // ------------------------------------------------------------ BASS ---

    { "PM Sine Bass", {     // real PM — a SINE carrier, the only kind PM actually reaches.
                            // PM depth has no EG of its own, so the filter carries the motion.
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3PM,1},{VCO3PMDEPTH,.55f},{VCO3TYPE,-1},{VCO3COARSEST,12},{VCO3GAIN,0},
        {VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.4f},{FILTRES,.12f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.34f},{EG2SUSTAIN,.18f},{EG2RELEASE,.2f},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,.55f},{EG1RELEASE,.12f},
        {VEL_TO_AMP,.4f},
        {POSTGAIN,-23} } },

    { "Reso Ladder Bass", { // pulse width moving on EG2 while the ladder rides the same shape
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.15f},{VCO1EG,0},
        {VCO1PWMODSRC,2},{VCO1PWMODDEPTH,.5f},
        {EG2ATTACK,0},{EG2DECAY,.3f},{EG2SUSTAIN,.17f},{EG2RELEASE,.15f},
        {VCO2GAIN,-60},
        {FILT_MODE,7},{FILT_CUT,.22f},{FILTRES,.5f},{FILTEG,1},
        {EG1ATTACK,0},{EG1DECAY,.33f},{EG1SUSTAIN,.5f},{EG1RELEASE,.1f},
        {VEL_TO_FILT,.5f},
        {POSTGAIN,-23} } },

    { "Drive Bass", {       // DRIVE warp for the dirt — distortion inside the oscillator
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,0},{VCO1WTPOS,.2f},{VCO1MORPHTO,.2f},
        {VCO1WARPTYPE,8},{VCO1WARPAMT,.55f},{VCO1WARPTO,.55f},
        {VCO2GAIN,-60},
        {NOISEMODE,2},{NOISEGAIN,-38},{NOISEEG,0},
        {FILT_MODE,14},{FILT_CUT,.26f},{FILTRES,.35f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.28f},{EG2SUSTAIN,.15f},{EG2RELEASE,.15f},
        {EG1ATTACK,0},{EG1DECAY,.52f},{EG1SUSTAIN,.5f},{EG1RELEASE,.12f},
        {VEL_TO_FILT,.5f},{VEL_TO_AMP,.3f},
        {POSTGAIN,-19} } },

    { "AM Bass", {          // amplitude modulation from the sub, a fourth down
        {VCO1TYPE,0},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3AM,1},{VCO3AMDEPTH,.7f},{VCO3TYPE,-1},{VCO3COARSEST,-5},{VCO3GAIN,-1},
        {VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,10},{FILT_CUT,.3f},{FILTRES,.25f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.35f},{EG2SUSTAIN,.15f},{EG2RELEASE,.2f},
        {EG1ATTACK,0},{EG1DECAY,.4f},{EG1SUSTAIN,.55f},{EG1RELEASE,.12f},
        {VEL_TO_FILT,.45f},
        {POSTGAIN,-24} } },

    { "Rolling Reese", {    // Two saws beating at 11 cents through a dark ladder LP, 2-voice
                            // unison each for swirl - the beating IS the sound; MW opens the
                            // filter.
        {VCO1UNIVOICES,2},{VCO1UNIDETUNE,.12f},
        {VCO2FINE,11},{VCO2TUNEEG,-1},{VCO2UNIVOICES,2},{VCO2UNIDETUNE,.12f},
        {FILT_MODE,7},{FILT_CUT,.05f},{FILTRES,.2f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.40f},{EG1SUSTAIN,.85f},{EG1RELEASE,.25f},
        {VEL_TO_AMP,.3f},
        {MW_TO_FILT,.6f},
        {POSTGAIN,-20} } },

    { "Push Pull Wub", {    // 3.2 Hz sine wobble where filter and VCO1 gain pump 180 deg against
                            // each other while a sine sub holds the floor; MW speeds the wub.
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-4},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(3.2f)},{LFO1DEPTH,.85f},{LFO1DEST,2},
        {LFO2RATE,Lg(3.2f)},{LFO2DEPTH,.55f},{LFO2DEST,18},{LFO2PHASE,180},
        {FILT_MODE,7},{FILT_CUT,.10f},{FILTRES,.45f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.35f},{EG1SUSTAIN,.8f},{EG1RELEASE,.2f},
        {VEL_TO_FILT,.35f},
        {MW_TO_LFORATE,.7f},
        {POSTGAIN,-23} } },

    { "Breathing Sub", {    // Soft tri over a pulse sub an octave down whose width slowly
                            // breathes via LFO dest 6 - first factory PW3 use; MW deepens the
                            // breath.
        {VCO1TYPE,4},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,2},{VCO3GAIN,0},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3PW,.58f},
        {LFO1RATE,Lg(0.35f)},{LFO1DEPTH,.22f},{LFO1DEST,6},
        {FILT_MODE,1},{FILT_CUT,.08f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.30f},{EG1SUSTAIN,.9f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.35f},
        {AT_TO_FILT,.3f},
        {MW_TO_LFODEPTH,.6f},
        {POSTGAIN,-30} } },

    { "Grit Gate", {        // Steady pulse bass with a square LFO gating a pink-noise layer at
                            // dest 21; phase 180 starts the gate shut so each attack speaks
                            // clean, then EG4 fades the grit groove in as you hold.
        {VCO1TYPE,2},{VCO1PW,.5f},
        {VCO2GAIN,-60},
        {NOISEMODE,1},{NOISEGAIN,-8},{NOISEEG,3},
        {LFO1RATE,Lg(4.0f)},{LFO1DEPTH,1},{LFO1WAVE,3},{LFO1DEST,21},{LFO1PHASE,180},
        {FILT_CUT,.09f},{FILTRES,.2f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.50f},{EG1SUSTAIN,.7f},{EG1RELEASE,.2f},
        {EG4ATTACK,.55f},{EG4DECAY,.2f},{EG4SUSTAIN,1},{EG4RELEASE,.25f},
        {VEL_TO_FILT,.3f},
        {MW_TO_LFORATE,.6f},
        {POSTGAIN,-26} } },

    { "FM Knock", {         // DX-style knock: tri carrier PM'd by a sine two octaves up at depth
                            // .4, EG2 snaps the filter and a white-noise click on EG3 marks the
                            // strike.
        {VCO1TYPE,4},
        {VCO2GAIN,-60},
        {VCO3PM,1},{VCO3PMDEPTH,.4f},{VCO3TYPE,-1},{VCO3GAIN,0},{VCO3COARSEST,24},{VCO3TUNEEG,-1},
        {NOISEMODE,0},{NOISEGAIN,-14},{NOISEEG,2},
        {FILT_CUT,.06f},{FILTRES,.25f},{FILTEG,1},
        {EG1ATTACK,0},{EG1DECAY,.60f},{EG1SUSTAIN,.6f},{EG1RELEASE,.25f},
        {EG2ATTACK,0},{EG2DECAY,.28f},{EG2SUSTAIN,0},{EG2RELEASE,.2f},
        {EG3ATTACK,0},{EG3DECAY,.18f},{EG3SUSTAIN,0},{EG3RELEASE,.15f},
        {VEL_TO_FILT,.5f},{VEL_TO_AMP,.3f},
        {POSTGAIN,-15} } },

    { "Gutter Growl", {     // WT GROWL morph .15 to .85 rides EG2 with a 45 ms attack for a wah-
                            // bark on every note over a sine sub; MW pushes the morph further.
        {VCO1TYPE,99},{VCO1WTSEL,10},{VCO1WTPOS,.15f},{VCO1MORPHTO,.85f},{VCO1PWMODSRC,2},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-3},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.12f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.45f},{EG1SUSTAIN,.8f},{EG1RELEASE,.25f},
        {EG2ATTACK,.15f},{EG2DECAY,.45f},{EG2SUSTAIN,.25f},{EG2RELEASE,.25f},
        {VEL_TO_AMP,.3f},
        {AT_TO_FILT,.35f},
        {MW_TO_MORPH,.5f},
        {POSTGAIN,-30} } },

    { "Split Level", {      // HP-split bass: reedy pulse thinned through SEM-HP carries the bite
                            // while a unity sine sub an octave down carries the fundamental.
        {VCO1TYPE,2},{VCO1PW,.6f},{VCO1GAIN,-5},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,0},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,12},{FILT_CUT,.04f},{FILTRES,.3f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.70f},{EG1SUSTAIN,.5f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.35f},
        {MW_TO_FILT,.4f},
        {POSTGAIN,-19} } },

    { "Rubber Snap", {      // Ladder-BP squelch: saw snapped through LAD-BP on EG2 while a tri
                            // sub restores the low end the bandpass removes; velocity digs the
                            // filter.
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,4},{VCO3GAIN,-2},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,8},{FILT_CUT,.05f},{FILTRES,.6f},{FILTEG,1},
        {EG1ATTACK,0},{EG1DECAY,.55f},{EG1SUSTAIN,.55f},{EG1RELEASE,.2f},
        {EG2ATTACK,0},{EG2DECAY,.35f},{EG2SUSTAIN,.15f},{EG2RELEASE,.2f},
        {VEL_TO_FILT,.6f},
        {MW_TO_RES,.4f},
        {POSTGAIN,-11} } },

    // --------------------------------------------------- KEYS & ORGANS ---

    { "Tonewheel", {        // ORGAN table, filter open, percussive EG2 click on the upper sine
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,8},{VCO1WTPOS,.4f},{VCO1MORPHTO,.4f},
        {VCO2TYPE,-1},{VCO2GAIN,-4},{VCO2COARSE,19},{VCO2EG,1},{VCO2TUNEEG,-1},
        {EG2ATTACK,0},{EG2DECAY,.22f},{EG2SUSTAIN,0},{EG2RELEASE,.05f},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.05f},
        {POSTGAIN,-30} } },

    { "Tremolo Rhodes", {   // EPIANO with a SQUARE LFO on AMP — gated tremolo, no chorus
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,25},{VCO1WTPOS,.3f},{VCO1MORPHTO,.85f},
        {VCO1PWMODSRC,2},
        {EG2ATTACK,0},{EG2DECAY,.4f},{EG2SUSTAIN,.15f},{EG2RELEASE,.3f},
        {LFO1RATE,Lg(5.5f)},{LFO1DEPTH,.4f},{LFO1WAVE,3},{LFO1DEST,3},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.48f},{EG1SUSTAIN,.4f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.45f},{MW_TO_LFODEPTH,.6f},
        {POSTGAIN,-17} } },

    { "Rate Crush Keys", {  // RATE warp opening back up on EG2 — lofi that resolves as it decays
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,15},{VCO1WTPOS,.35f},{VCO1MORPHTO,.35f},
        {VCO1WARPTYPE,7},{VCO1WARPAMT,.5f},{VCO1WARPTO,.08f},{VCO1WARPEG,2},
        {EG2ATTACK,0},{EG2DECAY,.45f},{EG2SUSTAIN,.2f},{EG2RELEASE,.3f},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.55f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.36f},{EG1SUSTAIN,.45f},{EG1RELEASE,.3f},
        {VEL_TO_AMP,.4f},
        {POSTGAIN,-16} } },

    { "Chrome Keys", {      // DX-style tine key: FM table morph spikes bright on EG2 then
                            // settles to a mellow body, sine octave ting on top
        {VCO1TYPE,99},{VCO1WTSEL,13},{VCO1WTPOS,.1f},{VCO1MORPHTO,.8f},{VCO1PWMODSRC,2},
        {VCO2TYPE,-1},{VCO2GAIN,-12},{VCO2COARSE,12},{VCO2TUNEEG,-1},{VCO2EG,1},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,.0f},{EG1DECAY,.62f},{EG1SUSTAIN,.2f},{EG1RELEASE,.5f},
        {EG2ATTACK,.0f},{EG2DECAY,.32f},{EG2SUSTAIN,.0f},{EG2RELEASE,.32f},
        {VEL_TO_AMP,.7f},
        {AT_TO_VIBRATO,.35f},
        {POSTGAIN,-17} } },

    { "Acid Clav", {        // Snappy funk clav: COMB table through the 303-BP with an EG2 filter
                            // bark and a white-noise quill chiff
        {VCO1TYPE,99},{VCO1WTSEL,14},{VCO1WTPOS,.4f},{VCO1MORPHTO,.4f},
        {VCO2GAIN,-60},
        {NOISEMODE,0},{NOISEGAIN,-16},{NOISEEG,1},
        {FILT_MODE,15},{FILT_CUT,.12f},{FILTRES,.6f},{FILTEG,1},
        {EG1ATTACK,.0f},{EG1DECAY,.4f},{EG1SUSTAIN,.1f},{EG1RELEASE,.3f},
        {EG2ATTACK,.0f},{EG2DECAY,.28f},{EG2SUSTAIN,.0f},{EG2RELEASE,.28f},
        {VEL_TO_FILT,.6f},{VEL_TO_AMP,.4f},
        {MW_TO_FILT,.5f},
        {POSTGAIN,-2} } },

    { "Velvet Tines", {     // Struck tine piano: modal TINE rings over an EPIANO bed whose morph
                            // drifts on LFO MORPH2, aftertouch damps the ring
        {VCO1TYPE,97},{VCO1MODALCH,2},{VCO1MODALDEC,Lg(2.5f)},{VCO1MODALBRT,.55f},{VCO1MODALHRD,.45f},{VCO1MODALPOS,.25f},
        {VCO2TYPE,99},{VCO2WTSEL,25},{VCO2GAIN,-9},{VCO2WTPOS,.15f},{VCO2MORPHTO,.6f},{VCO2PWMODSRC,0},{VCO2EG,2},
        {LFO1RATE,Lg(0.25f)},{LFO1DEPTH,.5f},{LFO1DEST,9},{LFO1PHASE,90},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {KEYTRACK_TO_DECAY,.5f},
        {EG1ATTACK,.0f},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.3f},
        {EG3ATTACK,.0f},{EG3DECAY,.55f},{EG3SUSTAIN,.15f},{EG3RELEASE,.4f},
        {VEL_TO_AMP,.6f},
        {AT_TO_DECAY,.5f},
        {POSTGAIN,-22} } },

    { "Drawbar Drift", {    // Organ whose registration slowly rebalances: PULSE-to-ODD gain
                            // crossfade over a 16-foot sine sub, MW speeds the drift
        {VCO1TYPE,99},{VCO1WTSEL,6},{VCO1WTPOS,.5f},{VCO1MORPHTO,.5f},
        {VCO2TYPE,99},{VCO2WTSEL,2},{VCO2WTPOS,.5f},{VCO2MORPHTO,.5f},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-8},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.07f)},{LFO1DEPTH,.9f},{LFO1DEST,18},
        {LFO2RATE,Lg(0.07f)},{LFO2DEPTH,-.9f},{LFO2DEST,19},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,.0f},{EG1DECAY,.3f},{EG1SUSTAIN,1},{EG1RELEASE,.15f},
        {AT_TO_AMP,.35f},
        {MW_TO_LFORATE,.6f},
        {POSTGAIN,-25} } },

    { "Gilded Quill", {     // Harpsichord: PLUCK table with a 4-foot octave choir a few cents
                            // sharp and a fast quill chiff on EG2
        {VCO1TYPE,99},{VCO1WTSEL,22},{VCO1WTPOS,.3f},{VCO1MORPHTO,.3f},
        {VCO2TYPE,99},{VCO2WTSEL,22},{VCO2GAIN,-7},{VCO2WTPOS,.3f},{VCO2MORPHTO,.3f},{VCO2COARSE,12},{VCO2FINE,4},{VCO2TUNEEG,-1},
        {NOISEMODE,0},{NOISEGAIN,-22},{NOISEEG,1},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,.0f},{EG1DECAY,.48f},{EG1SUSTAIN,.0f},{EG1RELEASE,.3f},
        {EG2ATTACK,.0f},{EG2DECAY,.18f},{EG2SUSTAIN,.0f},{EG2RELEASE,.18f},
        {VEL_TO_AMP,.3f},
        {POSTGAIN,-16} } },

    { "Bellows Reed", {     // Harmonium: beating pair of CZ-RES reeds over a sine sub with a
                            // slow air attack, aftertouch is the bellows
        {VCO1TYPE,99},{VCO1WTSEL,20},{VCO1WTPOS,.35f},{VCO1MORPHTO,.35f},
        {VCO2TYPE,99},{VCO2WTSEL,20},{VCO2GAIN,-3},{VCO2WTPOS,.45f},{VCO2MORPHTO,.45f},{VCO2FINE,12},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-12},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.3f},{FILTEG,-1},
        {EG1ATTACK,.22f},{EG1DECAY,.3f},{EG1SUSTAIN,1},{EG1RELEASE,.25f},
        {AT_TO_AMP,.6f},
        {MW_TO_FILT,.4f},
        {POSTGAIN,-16} } },

    // ---------------------------------------------------------- PLUCKS ---

    { "Analog Pluck", {     // the textbook one: pulse, LP4, fast filter EG, sustain 0
        {VCO1TYPE,2},{VCO1GAIN,0},{VCO1PW,.4f},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3GAIN,-10},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,0},{FILT_CUT,.3f},{FILTRES,.28f},{FILTEG,1},
        {EG2ATTACK,0},{EG2DECAY,.32f},{EG2SUSTAIN,.15f},{EG2RELEASE,.15f},
        {EG1ATTACK,0},{EG1DECAY,.38f},{EG1SUSTAIN,0},{EG1RELEASE,.2f},
        {VEL_TO_FILT,.5f},{VEL_TO_AMP,.4f},
        {POSTGAIN,-12} } },

    { "Bit Pluck", {        // BITS warp collapsing on EG2 — a digital decay, no delay involved
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,12},{VCO1WTPOS,.45f},{VCO1MORPHTO,.45f},
        {VCO1WARPTYPE,6},{VCO1WARPAMT,.65f},{VCO1WARPTO,.05f},{VCO1WARPEG,2},
        {EG2ATTACK,0},{EG2DECAY,.4f},{EG2SUSTAIN,0},{EG2RELEASE,.2f},
        {VCO2GAIN,-60},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.28f},{EG1SUSTAIN,0},{EG1RELEASE,.25f},
        {VEL_TO_AMP,.45f},
        {POSTGAIN,-8} } },


    { "Struck String", {    // the other MODAL FILTER preset, and the opposite use of it: a 60 ms
                            // noise burst on EG2 excites the bank and then gets out of the way.
                            // No oscillator at all - the pitch is entirely the resonator.
        {VCO1GAIN,-60},{VCO2GAIN,-60},
        {NOISEMODE,0},{NOISEGAIN,10},{NOISEEG,1},
        {EG2ATTACK,0},{EG2DECAY,.06f},{EG2SUSTAIN,0},{EG2RELEASE,.04f},
        {FILT_MODE,17},{FILTMODALBODY,1},{FILTMODALPOS,.2f},
        {FILT_CUT,.6f},{FILTRES,.75f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.5f},
        {VEL_TO_AMP,.5f},
        {POSTGAIN,-9} } },

    { "Foldback Harp", {    // West-coast wavefolder pluck: SOFT table folded hard at note-on,
                            // fold collapses A-high to B-low on EG2; sine sub an octave down
                            // keeps it full, MW adds fold back in.
        {VCO1TYPE,99},{VCO1WTSEL,7},{VCO1WARPTYPE,4},{VCO1WARPAMT,.85f},{VCO1WARPTO,.05f},{VCO1WARPEG,2},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-7},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.30f},{FILTEG,-1},
        {EG1ATTACK,.0f},{EG1DECAY,.45f},{EG1SUSTAIN,.0f},{EG1RELEASE,.38f},
        {EG2ATTACK,.0f},{EG2DECAY,.42f},{EG2SUSTAIN,.0f},{EG2RELEASE,.38f},
        {VEL_TO_AMP,.65f},{VEL_TO_FILT,.50f},
        {MW_TO_WARP,.45f},
        {POSTGAIN,-14} } },

    { "Pixel Dust", {       // Clean ODD-table clav whose tail dissolves into coarse quantize
                            // steps: STEP warp (inverse amount, .12 clean to .58 crushed) creeps
                            // in on a slow-attack EG3; aftertouch re-crushes.
        {VCO1TYPE,99},{VCO1WTSEL,2},{VCO1WARPTYPE,10},{VCO1WARPAMT,.12f},{VCO1WARPTO,.58f},{VCO1WARPEG,3},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-8},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,10},{FILT_CUT,.28f},{FILTEG,-1},
        {EG1ATTACK,.0f},{EG1DECAY,.62f},{EG1SUSTAIN,.0f},{EG1RELEASE,.42f},
        {EG3ATTACK,.55f},{EG3DECAY,.50f},{EG3SUSTAIN,1},{EG3RELEASE,.30f},
        {VEL_TO_AMP,.60f},{VEL_TO_FILT,.50f},
        {AT_TO_WARP,.50f},
        {POSTGAIN,-16} } },

    { "Karplus Wire", {     // Karplus-flavoured string: WT PLUCK table darkens across the tail
                            // on an EG2 morph, white-noise pick chiff rides its own instant-
                            // decay EG4 (first factory EG4 use), sine sub underneath; MW palm-
                            // mutes.
        {VCO1TYPE,99},{VCO1WTSEL,22},{VCO1WTPOS,.05f},{VCO1MORPHTO,.80f},{VCO1PWMODSRC,2},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-10},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {NOISEMODE,0},{NOISEGAIN,-6},{NOISEEG,3},
        {FILT_MODE,0},{FILT_CUT,.25f},{FILTEG,-1},
        {EG1ATTACK,.0f},{EG1DECAY,.55f},{EG1SUSTAIN,.0f},{EG1RELEASE,.45f},
        {EG2ATTACK,.50f},{EG2DECAY,.30f},{EG2SUSTAIN,1},{EG2RELEASE,.30f},
        {EG4ATTACK,.0f},{EG4DECAY,.10f},{EG4SUSTAIN,.0f},{EG4RELEASE,.10f},
        {VEL_TO_AMP,.70f},{VEL_TO_FILT,.50f},
        {MW_TO_MORPH,.50f},
        {POSTGAIN,-14} } },

    { "Nylon Hush", {       // Palm-muted nylon: triangle plus soft sine an octave up through a
                            // low keytracked LAD-BP body, EG2 pops the filter open for the
                            // thump; aftertouch lifts the mute.
        {VCO1TYPE,4},
        {VCO2TYPE,-1},{VCO2GAIN,-14},{VCO2COARSE,12},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-8},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {FILT_MODE,8},{FILT_CUT,.02f},{FILTRES,.30f},{FILTEG,1},
        {EG1ATTACK,.0f},{EG1DECAY,.38f},{EG1SUSTAIN,.0f},{EG1RELEASE,.35f},
        {EG2ATTACK,.0f},{EG2DECAY,.28f},{EG2SUSTAIN,.0f},{EG2RELEASE,.30f},
        {VEL_TO_AMP,.70f},{VEL_TO_FILT,.60f},
        {AT_TO_FILT,.45f},
        {POSTGAIN,-1} } },

    { "Thumb Piano", {      // Kalimba: modal TINE struck patch (gate EG, bank decay is the
                            // sound) with a fast sine-tick attack on EG2 and a thumping EG3 sub;
                            // aftertouch damps, high notes die faster.
        {VCO1TYPE,97},{VCO1MODALCH,2},{VCO1MODALDEC,Lg(1.3f)},{VCO1MODALBRT,.55f},{VCO1MODALHRD,.60f},{VCO1MODALPOS,.33f},
        {VCO2TYPE,-1},{VCO2GAIN,-16},{VCO2COARSE,24},{VCO2TUNEEG,-1},{VCO2EG,1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-10},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3EG,2},
        {FILT_MODE,1},{FILT_CUT,.45f},{FILTEG,-1},
        {KEYTRACK_TO_DECAY,.50f},
        {EG1ATTACK,.0f},{EG1DECAY,.50f},{EG1SUSTAIN,1},{EG1RELEASE,.30f},
        {EG2ATTACK,.0f},{EG2DECAY,.22f},{EG2SUSTAIN,.0f},{EG2RELEASE,.20f},
        {EG3ATTACK,.0f},{EG3DECAY,.55f},{EG3SUSTAIN,.0f},{EG3RELEASE,.30f},
        {VEL_TO_AMP,.60f},{VEL_TO_FILT,.40f},
        {AT_TO_DECAY,.60f},
        {POSTGAIN,-17} } },

    { "Steel To Silk", {    // Digital FM pluck hands off to a warm detuned triangle across the
                            // tail: saw-LFO gain crossfade on dests 18/19 with phase-picked
                            // 180-deg start; MW speeds the swap into a flutter.
        {VCO1TYPE,99},{VCO1WTSEL,13},
        {VCO2TYPE,4},{VCO2GAIN,-3},{VCO2FINE,6},{VCO2TUNEEG,-1},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,-9},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1RATE,Lg(0.35f)},{LFO1DEPTH,.85f},{LFO1WAVE,2},{LFO1DEST,18},{LFO1PHASE,180},
        {LFO2RATE,Lg(0.35f)},{LFO2DEPTH,-.85f},{LFO2WAVE,2},{LFO2DEST,19},{LFO2PHASE,180},
        {FILT_MODE,1},{FILT_CUT,.35f},{FILTEG,-1},
        {EG1ATTACK,.0f},{EG1DECAY,.70f},{EG1SUSTAIN,.0f},{EG1RELEASE,.50f},
        {VEL_TO_AMP,.60f},{VEL_TO_FILT,.45f},
        {MW_TO_LFORATE,.60f},
        {POSTGAIN,-15} } },

    // ------------------------------------------------- BELLS & MALLETS ---

    { "Bell Choir", {       // two engines at once: a struck MODAL bell over a sustained PAD
        {VCO1TYPE,97},{VCO1GAIN,-2},{VCO1EG,0},
        {VCO1MODALCH,4},{VCO1MODALDEC,Lg(2.2f)},{VCO1MODALBRT,.6f},
        {VCO1MODALHRD,.4f},{VCO1MODALPOS,.16f},
        {VCO2TYPE,98},{VCO2GAIN,-9},{VCO2EG,2},
        // Likewise revoiced: 4.3 cents before, CHIME's half-cap ~19 now.
        {VCO2PADSEL,7},{VCO2PADPOS,.3f},{VCO2PADMTO,.3f}, // 7 = CHIME
        {EG3ATTACK,.55f},{EG3DECAY,.5f},{EG3SUSTAIN,.8f},{EG3RELEASE,.5f},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.6f},
        {VEL_TO_AMP,.45f},
        {POSTGAIN,-18} } },

    { "Ring Mod Bell", {    // kept from the old PocketAnalog bank, and it needed nothing
                            // dropped - no effects in it at all. Two SINES ring-modulated a
                            // fifth plus 15 cents apart: the detune is what makes the clang
                            // beat instead of sitting still. Filter EG on EG1 with SUSTAIN 0,
                            // so the strike closes as it decays.
        {VCO1TYPE,-1},{VCO1GAIN,0},{VCO1EG,0},
        {VCO2GAIN,-60},
        {VCO3RINGMOD,1},{VCO3TYPE,-1},{VCO3GAIN,0},{VCO3COARSEST,7},{VCO3FINE,15},
        {VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_CUT,.9f},{FILTRES,.2f},{FILTEG,0},
        {EG1ATTACK,0},{EG1DECAY,.55f},{EG1SUSTAIN,0},{EG1RELEASE,.2f},
        {POSTGAIN,-9} } },

    { "Deep Gong", {        // MODAL DEEP at the top of the decay range, struck off-centre
        {VCO1TYPE,97},{VCO1GAIN,0},{VCO1EG,0},
        {VCO1MODALCH,0},{VCO1MODALDEC,Lg(5.f)},{VCO1MODALBRT,.45f},
        {VCO1MODALHRD,.5f},{VCO1MODALPOS,.08f},
        {VCO2GAIN,-60},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.08f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.62f},
        {VEL_TO_AMP,.5f},{AT_TO_DECAY,.5f},{KEYTRACK_TO_DECAY,.25f},
        {POSTGAIN,-18} } },


    { "Metal Scrape", {     // RING warp INSIDE the oscillator riding EG2, ring mod OUTSIDE it.
                            // A static RING amount is a constant clang — it has to decay.
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,5},{VCO1WTPOS,.55f},{VCO1MORPHTO,.55f},
        {VCO1WARPTYPE,3},{VCO1WARPAMT,.25f},{VCO1WARPTO,.06f},{VCO1WARPEG,2},
        {EG2ATTACK,0},{EG2DECAY,.5f},{EG2SUSTAIN,.1f},{EG2RELEASE,.35f},
        {VCO2GAIN,-60},
        {VCO3RINGMOD,1},{VCO3TYPE,-1},{VCO3COARSEST,23},{VCO3GAIN,-3},{VCO3EG,0},{VCO3TUNEEG,-1},
        {FILT_MODE,13},{FILT_CUT,.55f},{FILTRES,.3f},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.7f},{EG1SUSTAIN,.3f},{EG1RELEASE,.4f},
        {VEL_TO_AMP,.45f},
        {POSTGAIN,-16} } },

    { "Music Box", {        // Tiny tine music box: MODAL TINE up an octave plus a fast-dying
                            // sine ping a twelfth above; keytrack decay kills high notes quick
                            // like the real comb.
        {VCO1TYPE,97},{VCO1MODALCH,2},{VCO1MODALDEC,Lg(1.8f)},{VCO1MODALBRT,.7f},{VCO1MODALHRD,.35f},{VCO1MODALPOS,.1f},{VCO1COARSE,12},{VCO1TUNEEG,-1},
        {VCO2TYPE,-1},{VCO2COARSE,31},{VCO2TUNEEG,-1},{VCO2GAIN,-16},{VCO2EG,1},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {KEYTRACK_TO_DECAY,.7f},
        {EG1ATTACK,0},{EG1DECAY,.3f},{EG1SUSTAIN,1},{EG1RELEASE,.3f},
        {EG2ATTACK,0},{EG2DECAY,.42f},{EG2SUSTAIN,0},{EG2RELEASE,.3f},
        {VEL_TO_AMP,.7f},
        {POSTGAIN,-19} } },

    { "Gamelan Wood", {     // Dark wooden gamelan bar: pink-noise mallet thwack layered on a
                            // short MODAL WOOD body; aftertouch palm-damps the ring.
        {VCO1TYPE,97},{VCO1MODALCH,3},{VCO1MODALDEC,Lg(1.1f)},{VCO1MODALBRT,.35f},{VCO1MODALHRD,.6f},{VCO1MODALPOS,.3f},
        {VCO2GAIN,-60},
        {NOISEMODE,1},{NOISEGAIN,-12},{NOISEEG,1},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {KEYTRACK_TO_DECAY,.4f},
        {EG1ATTACK,0},{EG1DECAY,.4f},{EG1SUSTAIN,1},{EG1RELEASE,.4f},
        {EG2ATTACK,0},{EG2DECAY,.16f},{EG2SUSTAIN,0},{EG2RELEASE,.2f},
        {VEL_TO_AMP,.6f},
        {AT_TO_DECAY,.8f},
        {POSTGAIN,-16} } },

    { "Ice Lattice", {      // Icy edge-struck MODAL GLASS over a hushed CHIME wavetable bed: a
                            // slow LFO drifts the chime morph while jitter frosts the tuning; MW
                            // deepens the shimmer.
        {VCO1TYPE,97},{VCO1MODALCH,5},{VCO1MODALDEC,Lg(2.8f)},{VCO1MODALBRT,.8f},{VCO1MODALHRD,.75f},{VCO1MODALPOS,.05f},
        {VCO2TYPE,99},{VCO2WTSEL,11},{VCO2WTPOS,.15f},{VCO2MORPHTO,.7f},{VCO2COARSE,12},{VCO2TUNEEG,-1},{VCO2GAIN,-18},
        {LFO1RATE,Lg(0.09f)},{LFO1DEPTH,.4f},{LFO1DEST,9},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {KEYTRACK_TO_DECAY,.6f},
        {EG1ATTACK,0},{EG1DECAY,.5f},{EG1SUSTAIN,1},{EG1RELEASE,.5f},
        {VEL_TO_AMP,.65f},
        {MW_TO_LFODEPTH,.5f},
        {JITTERCENTS,6},
        {POSTGAIN,-20} } },

    { "Tubular Gold", {     // Golden FM tubular bell: sine carrier +12 phase-modulated by a +19
                            // st sub at .5 depth, beating +31 sine overtone, brightness falls on
                            // an EG2 lowpass; velocity opens the strike.
        {VCO1TYPE,-1},{VCO1COARSE,12},{VCO1TUNEEG,-1},
        {VCO2TYPE,-1},{VCO2COARSE,31},{VCO2FINE,7},{VCO2TUNEEG,-1},{VCO2GAIN,-15},{VCO2EG,1},
        {VCO3PM,1},{VCO3TYPE,-1},{VCO3COARSEST,19},{VCO3TUNEEG,-1},{VCO3GAIN,0},{VCO3PMDEPTH,.5f},
        {FILT_MODE,1},{FILT_CUT,.12f},{FILTEG,1},
        {EG1ATTACK,0},{EG1DECAY,.78f},{EG1SUSTAIN,0},{EG1RELEASE,.55f},
        {EG2ATTACK,0},{EG2DECAY,.5f},{EG2SUSTAIN,0},{EG2RELEASE,.45f},
        {VEL_TO_FILT,.7f},{VEL_TO_AMP,.4f},
        {POSTGAIN,-22} } },

    { "Bell Harp", {        // Plucked bell-harp from the RESO wavetable: EG2 snaps the morph
                            // bright-to-dark on the strike, clean sine sub an octave down
                            // carries the body; MW re-brightens.
        {VCO1TYPE,99},{VCO1WTSEL,23},{VCO1WTPOS,.2f},{VCO1MORPHTO,.85f},{VCO1PWMODSRC,2},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},{VCO3GAIN,-9},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,0},{EG1DECAY,.58f},{EG1SUSTAIN,0},{EG1RELEASE,.4f},
        {EG2ATTACK,0},{EG2DECAY,.34f},{EG2SUSTAIN,0},{EG2RELEASE,.3f},
        {VEL_TO_AMP,.7f},
        {MW_TO_MORPH,.5f},
        {POSTGAIN,-20} } },

    { "Santur Hammer", {    // Hammered dulcimer: 3-voice unison MODAL STRING on VCO2 like
                            // detuned string courses, white-noise hammer click on EG4; MW widens
                            // the detune, keytrack shortens high strings.
        {VCO1GAIN,-60},
        {VCO2TYPE,97},{VCO2MODALCH,1},{VCO2MODALDEC,Lg(3.2f)},{VCO2MODALBRT,.6f},{VCO2MODALHRD,.85f},{VCO2MODALPOS,.13f},{VCO2UNIVOICES,3},{VCO2UNIDETUNE,.18f},
        {NOISEMODE,0},{NOISEGAIN,-16},{NOISEEG,3},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {KEYTRACK_TO_DECAY,.5f},
        {EG1ATTACK,0},{EG1DECAY,.45f},{EG1SUSTAIN,1},{EG1RELEASE,.45f},
        {EG4ATTACK,0},{EG4DECAY,.13f},{EG4SUSTAIN,0},{EG4RELEASE,.15f},
        {VEL_TO_AMP,.65f},
        {MW_TO_UNI,.6f},
        {POSTGAIN,-20} } },

    // -------------------------------------------------- WINDS & VOICES ---

    { "Breath Flute", {     // brown noise as breath, gated by its own EG3, over HOLLOW
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,17},{VCO1WTPOS,.35f},{VCO1MORPHTO,.35f},
        {VCO1FINE,3},{VCO1TUNEEG,-1},
        {VCO2GAIN,-60},
        {NOISEMODE,2},{NOISEGAIN,-14},{NOISEEG,2},
        {EG3ATTACK,.1f},{EG3DECAY,.3f},{EG3SUSTAIN,.25f},{EG3RELEASE,.2f},
        {FILT_MODE,1},{FILT_CUT,.6f},{FILTRES,.1f},{FILTEG,-1},
        {EG1ATTACK,.22f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.25f},
        {MW_TO_VIBRATO,.5f},{JITTERCENTS,7},
        {POSTGAIN,-22} } },

    { "Formant Vox", {      // FORMANT warp on LFO1 and MORPH on LFO2 — two independent movers
        {VCO1TYPE,99},{VCO1GAIN,0},{VCO1EG,0},{VCO1WTSEL,4},{VCO1WTPOS,.15f},{VCO1MORPHTO,.8f},
        {VCO1WARPTYPE,5},{VCO1WARPAMT,.06f},{VCO1WARPTO,.28f},
        {LFO1RATE,Lg(.35f)},{LFO1DEPTH,.55f},{LFO1WAVE,1},{LFO1DEST,11},
        {LFO2RATE,Lg(.22f)},{LFO2DEPTH,.5f},{LFO2WAVE,0},{LFO2DEST,8},
        {VCO2GAIN,-60},
        {FILT_MODE,11},{FILT_CUT,.2f},{FILTRES,.5f},{FILTEG,-1},
        {EG1ATTACK,.3f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.4f},
        {MW_TO_LFODEPTH,.5f},
        {POSTGAIN,-16} } },

    { "Vowel Tide", {       // Choir of two formant layers - WT FORMANT and PAD FORMANT - slowly
                            // tide-crossfaded by opposed gain LFOs; MW steers the WT vowel.
        {VCO1TYPE,99},{VCO1WTSEL,3},{VCO1WTPOS,.15f},{VCO1MORPHTO,.70f},
        {VCO2TYPE,98},{VCO2PADSEL,5},{VCO2PADPOS,.30f},{VCO2PADMTO,.30f},
        {LFO1RATE,Lg(0.07f)},{LFO1DEPTH,.70f},{LFO1DEST,18},
        {LFO2RATE,Lg(0.07f)},{LFO2DEPTH,-.70f},{LFO2DEST,19},
        {FILT_MODE,0},{FILT_CUT,1},{FILTEG,-1},
        {EG1ATTACK,.60f},{EG1DECAY,.50f},{EG1SUSTAIN,.85f},{EG1RELEASE,.60f},
        {VEL_TO_AMP,.35f},
        {MW_TO_MORPH,.80f},
        {JITTERCENTS,4},
        {POSTGAIN,-27} } },

    { "Velvet Brass", {     // Detuned-saw brass section - LP2 blooms open on a deliberately
                            // slower EG2 after the note speaks; aftertouch swells the
                            // brightness.
        {VCO2FINE,10},{VCO2TUNEEG,-1},
        {FILT_MODE,1},{FILT_CUT,.05f},{FILTRES,.15f},{FILTEG,1},
        {EG1ATTACK,.20f},{EG1DECAY,.40f},{EG1SUSTAIN,.80f},{EG1RELEASE,.35f},
        {EG2ATTACK,.30f},{EG2DECAY,.55f},{EG2SUSTAIN,.55f},{EG2RELEASE,.35f},
        {VEL_TO_FILT,.40f},
        {AT_TO_FILT,.50f},
        {MW_TO_VIBRATO,.40f},
        {POSTGAIN,-27} } },

    { "Clay Pipe", {        // Breathy pan-pipe: soft wavetable plus a pink-noise EG4 that is
                            // chiff AND low sustained breath in one envelope; MW vibrato enters
                            // mid-cycle via LFO1PHASE.
        {VCO1TYPE,99},{VCO1WTSEL,7},{VCO1WTPOS,.30f},{VCO1MORPHTO,.30f},
        {VCO2GAIN,-60},
        {NOISEMODE,1},{NOISEGAIN,-10},{NOISEEG,3},
        {LFO1RATE,Lg(5.2f)},{LFO1DEPTH,.05f},{LFO1DEST,1},{LFO1PHASE,90},
        {FILT_MODE,1},{FILT_CUT,.25f},{FILTEG,-1},
        {EG1ATTACK,.10f},{EG1DECAY,.30f},{EG1SUSTAIN,.90f},{EG1RELEASE,.30f},
        {EG4ATTACK,.0f},{EG4DECAY,.30f},{EG4SUSTAIN,.10f},{EG4RELEASE,.25f},
        {VEL_TO_AMP,.50f},
        {MW_TO_LFODEPTH,.90f},
        {POSTGAIN,-23} } },

    // --------------------------------------------------- TEXTURES & FX ---

    { "Wind Sweep", {       // noise only, through a bandpass - the one place a bandpass belongs,
                            // since noise is flat and any placement has something to pass. Noise
                            // has no pitch of its own, so the RESONANT BAND is the pitch, and
                            // KEYTRACK is left at its default 1.0.
                            //   THE 4-POLE BAND IS THE POINT, and so is the resonance. SEM BP and
                            // 303 BP were re-measured here and are far worse (flatness .090 and
                            // .492 against BP4's .014), so a 2-pole band still cannot get there.
                            //   RES .95, NOT .85. An earlier note here claimed .85 was a measured
                            // optimum and that BP4 "gets wider again past it - 1.09 octaves at
                            // .92, 1.23 at 1.0". THAT WAS WRONG, and wrong because the LFO was
                            // left running through the measurement: LFO1 sweeps this filter over
                            // most of its range, so an average taken across the sweep measures the
                            // SWEEP, not the band, and a more resonant filter follows the sweep
                            // more visibly. Defeat the LFO and the band narrows monotonically with
                            // resonance, as a 4-pole bandpass must:
                            //     RES    .75   .85   .90   .92   .95   .98
                            //     oct    0.48  0.39  0.32  0.28  0.24  0.17
                            //     flat  .0061 .0038 .0026 .0020 .0015 .0008
                            // Ear-reported at .85 as "still too much noise", which the numbers
                            // agree with. .95 cuts flatness from .0137 to .0050 with the LFO live.
                            // Stopping short of .98 leaves room before self-oscillation, where the
                            // band stops being wind and becomes a whistle.
                            //   CUT IS THE WHOLE BALANCE HERE, because the filter LFO's offset
                            // is scaled by rest = (0.5 - base) * cut. At CUT 0 the band sits
                            // exactly on the played note - perfect tracking - but rest is 0, so
                            // the LFO is completely dead. Big CUT gives a big sweep that ignores
                            // the keyboard. CUT .03 with a full-depth LFO is the useful corner:
                            // the FLOOR of the sweep tracks (174 Hz at C2 to 1143 Hz at C6) and
                            // it still sweeps ~8x up from there. Measured, not guessed.
                            //   TRIANGLE, not saw: the saw only ever rose and then jumped back,
                            // which is exactly why it read as one endlessly rising noise.
        {VCO1GAIN,-60},{VCO2GAIN,-60},
        {NOISEMODE,0},{NOISEGAIN,-2},{NOISEEG,0},
        {FILT_MODE,3},{FILT_CUT,.03f},{FILTRES,.95f},{FILTEG,-1},
        {LFO1RATE,Lg(.18f)},{LFO1DEPTH,1},{LFO1WAVE,1},{LFO1DEST,2},
        {EG1ATTACK,.6f},{EG1DECAY,.5f},{EG1SUSTAIN,.9f},{EG1RELEASE,.7f},
        {MW_TO_FILT,.5f},
        {POSTGAIN,-37} } },

    { "Breathing Halo", {   // Wide detuned saw bed whose unison width swells and recedes like
                            // slow breathing over a sine sub; HP2 keeps the top airy while the
                            // sub holds the floor.
        {VCO1GAIN,-6},{VCO1UNIVOICES,6},{VCO1UNIDETUNE,.6f},
        {VCO2GAIN,-60},
        {VCO3NORMAL,1},{VCO3TYPE,-1},{VCO3GAIN,0},{VCO3COARSEST,-12},{VCO3TUNEEG,-1},
        {LFO1DEST,14},{LFO1WAVE,1},{LFO1RATE,Lg(0.12f)},{LFO1DEPTH,.55f},
        {FILT_MODE,4},{FILT_CUT,.02f},{FILTRES,.3f},{FILTEG,-1},
        {EG1ATTACK,.6f},{EG1DECAY,.3f},{EG1SUSTAIN,1},{EG1RELEASE,.65f},
        {AT_TO_VIBRATO,.25f},
        {MW_TO_UNI,.7f},
        {POSTGAIN,-26} } },

    { "Glacier Scan", {     // A 33-second one-way trek through the SWEEP wavetable over a soft
                            // triangle pedal tone. The morph LFO is SPAN-RELATIVE (rest is the
                            // middle of A..B; depth .5 makes A and B the two extremes) and the
                            // engine's saw starts at -1, so depth .5 at phase 0 departs exactly
                            // at A and walks the full span once per cycle. The first cut used
                            // depth .8 + phase 180 from the absolute-offset mental model, which
                            // starts every note MID-trek and parks a fifth of each cycle
                            // clamped at either end.
        {VCO1TYPE,99},{VCO1WTSEL,16},{VCO1WTPOS,.0f},{VCO1MORPHTO,1},
        {VCO2TYPE,4},{VCO2GAIN,-7},
        {LFO1DEST,8},{LFO1WAVE,2},{LFO1RATE,Lg(0.03f)},{LFO1DEPTH,.5f},
        {FILT_MODE,1},{FILT_CUT,.4f},{FILTEG,-1},
        {EG1ATTACK,.35f},{EG1DECAY,.5f},{EG1SUSTAIN,.85f},{EG1RELEASE,.55f},
        {AT_TO_LFORATE,.4f},
        {MW_TO_MORPH,.5f},
        {POSTGAIN,-30} } },

    };

    // Section boundaries, in declaration order. These MUST sum to the bank size —
    // the static_assert below is what stops a preset being added without being
    // given a category (it would otherwise land silently in the wrong column).
    struct Section { int cat; int count; };
    static constexpr Section sections[] = {
        {CAT_LEADS, 13}, {CAT_PADS, 14}, {CAT_BASS, 12}, {CAT_KEYS, 9},
        {CAT_PLUCKS, 9}, {CAT_BELLS, 10}, {CAT_WINDS, 5}, {CAT_TEXTURES, 3},
    };
    constexpr int sectionTotal = [] {
        int n = 0; for (auto& s : sections) n += s.count; return n;
    }();
    static_assert(sectionTotal == (int)(sizeof(bank) / sizeof(bank[0])),
                  "sections[] must cover every preset — update it when adding one");

    // Dates still count down so anything that reads them (presetDate matching, the
    // isSystem/Default checks) keeps working; they no longer decide the UI order.
    int64_t date = LONG_MAX - 1;
    int si = 0, sn = 0, ord = 0;
    for (auto& p : bank) {
        if (sn >= sections[si].count) { si++; sn = 0; ord = 0; }
        Preset fp{};
        fp.name = p.name; fp.path = "null"; fp.date = date--; fp.isSystem = true;
        fp.category = sections[si].cat;
        fp.order = ord++;
        fp.uid = uidOf(p.name);
        fp.values = p.vals;
        _DATA->presets.push_back(std::move(fp));
        sn++;
    }
};

// A state written before the matrix existed cannot contain matrix ids, and
// loadPreset/unserializeState reset every param to initvalue (0 for the whole
// matrix) before applying — so "matrix all zero but legacy DEPTH set" can ONLY
// be a legacy state: a new save stores DEPTH as the window, equal to its matrix
// slot, so the two can never disagree in a file this code wrote.
//
// destStoredMask (bit n = LFOn) says whether the FILE actually stored LFOnDEST.
// The fold requires it: old saves are sparse against an init of 0 (OFF), so a
// user preset could store an ORPHANED depth — knob turned up while DEST sat on
// OFF, silent by the old semantics. DEST's init is now 1 (PITCH, since the OFF
// row left the selector), and folding such an orphan against the new init would
// manufacture a vibrato route the preset never had. Stored dest + stored depth
// is a real legacy route; anything less folds nothing.
const int Preset::kLfoDestIds[4]  = {LFO1DEST,  LFO2DEST,  LFO3DEST,  LFO4DEST};
const int Preset::kLfoDepthIds[4] = {LFO1DEPTH, LFO2DEPTH, LFO3DEPTH, LFO4DEPTH};

void Preset::foldLegacyLfoRoutes(tsl::AppState* _appState, unsigned destStoredMask) {
    for (int n = 0; n < 4; n++) {
        bool anyMatrix = false;
        for (int d = 1; d <= LFO_MD_NDEST; d++)
            if (_STATE->params[0][lfoMdId(n, d)].load() != 0.f) { anyMatrix = true; break; }
        int dest          = (int)_STATE->params[0][kLfoDestIds[n]].load();
        const float depth = _STATE->params[0][kLfoDepthIds[n]].load();
        if (!anyMatrix && (destStoredMask >> n & 1u) && dest >= 1 && depth != 0.f)
            _STATE->params[0][lfoMdId(n, dest)].store(depth);
        // The selector has no OFF row any more: a legacy stored 0 becomes a cursor
        // on PITCH. Purely cosmetic — a cursor carries no route.
        if (dest < 1) {
            dest = 1;
            _STATE->params[0][kLfoDestIds[n]].store(1.f);
        }
        // re-seat the window on the (possibly just-folded) slot the cursor points at
        _STATE->params[0][kLfoDepthIds[n]].store(_STATE->params[0][lfoMdId(n, dest)].load());
    }
}

void Preset::loadPreset(tsl::AppState* _appState, int num, tsl::FastQueue<VcoPreNote>& notes) {
    if (num >= (int)_DATA->presets.size())
        return;
    Preset& preset = _DATA->presets.at(num);

    for (int i = 0; i < NUM_PARAMS; i++)
        _STATE->params[0][i].store(static_cast<float>(_STATE->parameters[i].initvalue));
    unsigned destStored = 0;
    for (auto& val : preset.values) {
        _STATE->params[0][val.num].store(val.val);
        for (int n = 0; n < 4; n++)
            if (val.num == kLfoDestIds[n]) destStored |= 1u << n;
    }
    foldLegacyLfoRoutes(_appState, destStored);
    _DATA->presetDate = preset.date;

    std::lock_guard lk(_STATE->mutex_midi);
    for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
        for (int i = 0; i < NUM_MIDI_CONTROL; i++) {
            _STATE->midicontrolevents[ch][i] = tsl::parameters::Event{};
            _STATE->midicontrolevents[ch][i].eventType = tsl::parameters::Eventtype::NoParam;
        }
        for (int i = 0; i < NUM_MIDI_NOTEON; i++) {
            _STATE->midinoteevents[ch][i] = tsl::parameters::Event{};
            _STATE->midinoteevents[ch][i].eventType = tsl::parameters::Eventtype::NoParam;
        }
    }
    for (const auto& e : preset.midiEvents) {
        const int ch = e.midiState.channel;
        const int num = e.midiState.num;
        if (e.midiState.type == ParameterType_double) {
            if (ch < NUM_MIDICHANNELS && num < NUM_MIDI_CONTROL)
                _STATE->midicontrolevents[ch][num] = e;
        } else {
            if (ch < NUM_MIDICHANNELS && num < NUM_MIDI_NOTEON)
                _STATE->midinoteevents[ch][num] = e;
        }
    }

    notes.flush();
    for (auto& n : preset.notes)
        if (auto* p = notes.push()) *p = n;

}

// Returns whether the preset file was actually written — deleteOverwriteFunc only
// removes a replaced preset's old file after a successful write.
static bool savePresetToFile(tsl::AppState* _appState, const std::string& presetname,
                             const std::vector<Preset::PresetParam>& vals,
                             const std::vector<VcoPreNote>& notes) {
    PresetHeader header;
    strcpy(header.header, headername);
    struct timespec spec{};
    clock_gettime(CLOCK_REALTIME, &spec);
    header.date = spec.tv_sec;
    header.namelen = (int)presetname.size();
    header.numparams = (int)vals.size();

    // `handle` is what Preset::path becomes: a filesystem path, or a content://
    // URI when the user has pointed Android at their own folder. The delete path
    // (deleteOverwriteFunc) takes it back, so it has to survive the write.
    std::string handle;
    FILE* fd = nullptr;
#ifdef __ANDROID__
    if (::Preset::customFolder()) {
        const std::string name = std::to_string(tsl::time::nanosecondsSinceEpoch());
        fd = fromPipeResult(jStaticString("createStorageFile",
                                          "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                                          "presets", name),
                            "wb", &handle);
        if (!fd) {
            showToast(_appState, "Save Preset: the preset folder is not writable.");
            return false;
        }
    }
#endif
    if (!fd) {
        std::string dir = tsl::app::getStoragePath("presets");
        if (dir.empty()) { showToast(_appState, "Save Preset: No storage path."); return false; }
        if (!fs::exists(dir)) fs::create_directories(dir);

        std::stringstream filename;
        filename << dir << "/" << tsl::time::nanosecondsSinceEpoch();
        handle = filename.str();

        fd = fopen(handle.c_str(), "wb");
        if (!fd) { showToast(_appState, "Save Preset: Could not open output file."); return false; }
    }

    bool ok = fwrite(&header, 1, sizeof(PresetHeader), fd) == sizeof(PresetHeader)
           && fwrite(presetname.data(), 1, presetname.size(), fd) == presetname.size()
           && fwrite(vals.data(), sizeof(Preset::PresetParam), vals.size(), fd) == vals.size();

    if (ok && !notes.empty()) {
        int s = (int)notes.size();
        ok = fwrite(&hasNotesCode, sizeof(int), 1, fd) == 1
          && fwrite(&s, sizeof(int), 1, fd) == 1
          && fwrite(notes.data(), sizeof(VcoPreNote), s, fd) == (size_t)s;
    }

    if (ok) {
        std::vector<tsl::parameters::Event> midiEvents;
        std::lock_guard lk(_STATE->mutex_midi);
        for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
            for (int i = 0; i < NUM_MIDI_CONTROL; i++) {
                const auto& e = _STATE->midicontrolevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) midiEvents.push_back(e);
            }
            for (int i = 0; i < NUM_MIDI_NOTEON; i++) {
                const auto& e = _STATE->midinoteevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) midiEvents.push_back(e);
            }
        }
        if (!midiEvents.empty()) {
            int marker = hasMidiEventsCode;
            int count = (int)midiEvents.size();
            ok = fwrite(&marker, sizeof(int), 1, fd) == 1
              && fwrite(&count, sizeof(int), 1, fd) == 1
              && fwrite(midiEvents.data(), sizeof(tsl::parameters::Event), count, fd) == (size_t)count;
        }
    }

    fclose(fd);
    if (!ok) { ::Preset::removePreset(handle); showToast(_appState, "Save Preset: Write error."); return false; }

    {
        ::Preset::Preset np{};
        np.name = presetname; np.path = handle; np.date = header.date;
        np.isSystem = false;
        np.category = ::Preset::CAT_USER;
        np.uid = ::Preset::uidOf(std::to_string(header.date).c_str());
        np.values = vals;
        np.notes = notes;
        // A freshly saved preset goes to the TOP of USER: push every existing user
        // entry down one rather than sorting by date, or a user who has dragged
        // their list into an order would have this land wherever its timestamp fell.
        np.order = 0;
        for (auto& p : _DATA->presets)
            if (p.category == ::Preset::CAT_USER) p.order++;
        _DATA->presets.push_back(std::move(np));
        ::Preset::sortAll(_appState);
    }
    showToast(_appState, "Preset Saved.");
    return true;
}

void Preset::savePreset(tsl::AppState* _appState, std::vector<PresetParam> vals, std::vector<VcoPreNote> notes) {
    using PAPreset = std::shared_ptr<::Preset::Preset>;
    tsl::QueueUnsafe<PAPreset, 100> queue;
    auto savedVals  = std::make_shared<std::vector<PresetParam>>(std::move(vals));
    auto savedNotes = std::make_shared<std::vector<VcoPreNote>>(std::move(notes));

    tsl::app::deleteOverwriteFunc<PAPreset>(
        _appState, queue, "Save Preset",
        [_appState]() -> std::vector<PAPreset> {
            std::vector<PAPreset> items;
            for (auto& p : _DATA->presets)
                if (!p.isSystem)
                    items.push_back(std::make_shared<::Preset::Preset>(p));
            return items;
        },
        [_appState, savedVals, savedNotes](tsl::AppState* app, std::string& name) {
            return savePresetToFile(app, name, *savedVals, *savedNotes);
        });

    // Refresh preset list (handles any deletions that happened in the window).
    // This must rebuild it exactly as startup does -- setupDefault, then setupFactory,
    // then readPresets (setup.cpp calls the first two, gui.cpp the third). Omitting
    // setupFactory here dropped the entire baked bank from the selector the moment the
    // user saved anything, leaving only Default and their own presets, and it looks
    // like the presets "suddenly disappeared" because nothing is logged and the next
    // launch puts them all back.
    long currentDate = _DATA->presetDate;
    _DATA->presets.clear();
    setupDefault(_appState);
    setupFactory(_appState);
    readPresets(_appState);
    _DATA->presetDate = currentDate;

    // Refresh the preset selector UI
    _appState->toUiThreadQueue.try_push([_appState]() {
        auto* sel = _DATA->views.presetSelector;
        if (sel && sel->visible_) {
            sel->addRecursiveDraw();
            sel->redraw();
        }
    });
}


// The preset folder changed under us (Settings > Presets > Preset Folder). The
// files have already been moved by the Java side; this re-reads the bank from
// wherever they now are. Queued rather than done inline: this arrives on a Java
// worker thread and _DATA->presets belongs to the UI thread. Rebuilt exactly as
// startup does -- setupDefault, setupFactory, readPresets -- because omitting
// setupFactory drops the whole baked bank from the selector.
void Preset::reload(tsl::AppState* _appState) {
    _appState->toUiThreadQueue.try_push([_appState]() {
        const long currentDate = _DATA->presetDate;
        _DATA->presets.clear();
        setupDefault(_appState);
        setupFactory(_appState);
        readPresets(_appState);
        _DATA->presetDate = currentDate;
        auto* sel = _DATA->views.presetSelector;
        if (sel && sel->visible_) {
            sel->addRecursiveDraw();
            sel->redraw();
        }
    });
}

static Preset::Preset captureCurrentPreset(tsl::AppState* _appState) {
    Preset::Preset p;
    p.date = _DATA->presetDate;

    for (int i = 0; i < NUM_PARAMS; i++) {
        float val = _STATE->params[0][i].load();
        if (val != static_cast<float>(_STATE->parameters[i].initvalue) &&
            !_STATE->parameters[i].getFlag(Param::NoAssignment))
            p.values.push_back({i, val});
    }
    for (auto* n = _DATA->seq_noteon._first; n; n = n->next)
        p.notes.push_back(n->data);
    {
        std::lock_guard lk(_STATE->mutex_midi);
        for (int ch = 0; ch < NUM_MIDICHANNELS; ch++) {
            for (int i = 0; i < NUM_MIDI_CONTROL; i++) {
                const auto& e = _STATE->midicontrolevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) p.midiEvents.push_back(e);
            }
            for (int i = 0; i < NUM_MIDI_NOTEON; i++) {
                const auto& e = _STATE->midinoteevents[ch][i];
                if (e.eventType != tsl::parameters::Eventtype::NoParam) p.midiEvents.push_back(e);
            }
        }
    }
    return p;
}

static std::vector<uint8_t> presetToBytes(const Preset::Preset& p) {
    std::vector<uint8_t> buf;
    auto append = [&](const void* data, size_t n) {
        const auto* b = static_cast<const uint8_t*>(data);
        buf.insert(buf.end(), b, b + n);
    };

    PresetHeader header;
    strcpy(header.header, headername);
    header.date = p.date;
    header.namelen = (int)p.name.size();
    header.numparams = (int)p.values.size();
    append(&header, sizeof(header));
    if (!p.name.empty())
        append(p.name.data(), p.name.size());
    if (!p.values.empty())
        append(p.values.data(), p.values.size() * sizeof(Preset::PresetParam));
    if (!p.notes.empty()) {
        int s = (int)p.notes.size();
        append(&hasNotesCode, sizeof(int));
        append(&s, sizeof(int));
        append(p.notes.data(), p.notes.size() * sizeof(VcoPreNote));
    }
    if (!p.midiEvents.empty()) {
        int marker = hasMidiEventsCode;
        int count = (int)p.midiEvents.size();
        append(&marker, sizeof(int));
        append(&count, sizeof(int));
        append(p.midiEvents.data(), p.midiEvents.size() * sizeof(tsl::parameters::Event));
    }
    return buf;
}

static bool presetFromBytes(const uint8_t* data, size_t size, Preset::Preset& p) {
    size_t pos = 0;
    auto read = [&](void* dst, size_t n) -> bool {
        if (pos + n > size) return false;
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    };

    PresetHeader header{};
    if (!read(&header, sizeof(header))) return false;
    if (strcmp(header.header, headername) != 0) return false;

    p.date = header.date;
    if (header.namelen > 0) {
        std::vector<char> namebuf(header.namelen + 1, '\0');
        if (!read(namebuf.data(), header.namelen)) return false;
        p.name = namebuf.data();
    }

    p.values.resize(header.numparams);
    if (header.numparams > 0 && !read(p.values.data(), p.values.size() * sizeof(Preset::PresetParam)))
        return false;

    {
        int marker{};
        size_t saved = pos;
        if (read(&marker, sizeof(int)) && marker == hasNotesCode) {
            int count{};
            if (read(&count, sizeof(int)) && count > 0 && count < 10000) {
                p.notes.resize(count);
                if (!read(p.notes.data(), count * sizeof(VcoPreNote))) p.notes.clear();
            }
        } else {
            pos = saved;
        }
    }
    {
        int marker{};
        size_t saved = pos;
        if (read(&marker, sizeof(int)) && marker == hasMidiEventsCode) {
            int count{};
            if (read(&count, sizeof(int)) && count > 0 && count < 10000) {
                for (int i = 0; i < count; i++) {
                    tsl::parameters::Event e{};
                    if (!read(&e, sizeof(tsl::parameters::Event))) break;
                    p.midiEvents.push_back(e);
                }
            }
        } else {
            pos = saved;
        }
    }
    return true;
}

std::vector<uint8_t> Preset::serializeCurrentState(tsl::AppState* _appState) {
    // Only serialize params — notes/MIDI events contain raw pointers, unsafe across sessions
    std::vector<uint8_t> buf;
    auto append = [&](const void* d, size_t n) {
        const auto* b = static_cast<const uint8_t*>(d);
        buf.insert(buf.end(), b, b + n);
    };
    for (int i = 0; i < NUM_PARAMS; i++) {
        float val = _STATE->params[0][i].load();
        if (val != static_cast<float>(_STATE->parameters[i].initvalue) &&
            !_STATE->parameters[i].getFlag(Param::NoAssignment)) {
            PresetParam pp{(uint16_t)i, val};
            append(&pp, sizeof(pp));
        }
    }
    return buf;
}

void Preset::unserializeState(tsl::AppState* _appState, const uint8_t* data, size_t size) {
    // Reset all params to init, then apply saved values
    for (int i = 0; i < NUM_PARAMS; i++)
        _STATE->params[0][i].store(static_cast<float>(_STATE->parameters[i].initvalue));

    const size_t stride = sizeof(PresetParam);
    unsigned destStored = 0;
    for (size_t pos = 0; pos + stride <= size; pos += stride) {
        PresetParam pp{};
        memcpy(&pp, data + pos, stride);
        if (pp.num > 0 && pp.num < NUM_PARAMS &&
            !_STATE->parameters[pp.num].getFlag(Param::NoAssignment)) {
            _STATE->params[0][pp.num].store(pp.val);
            for (int n = 0; n < 4; n++)
                if (pp.num == kLfoDestIds[n]) destStored |= 1u << n;
        }
    }
    foldLegacyLfoRoutes(_appState, destStored);

    // (notes and MIDI learn not restored — unsafe to serialize raw structs)
    // Apply restored params to running state
    for (int i = 1; i < NUM_PARAMS; i++) {
        tsl::parameters::Event e;
        e.setup(_STATE, 0, i);
        e.value = _STATE->params[0][i].load();
        e.flags |= tsl::parameters::Event::FromDaw;
        e.applyFromExt(_STATE, tsl::parameters::None);
    }
}

