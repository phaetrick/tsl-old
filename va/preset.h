#ifndef _PRESET_H_
#define _PRESET_H_

#include <utility>
#include "va_types.h"
#include <tools/queuetsl.h>
#include <params.h>
#include <vector>
#include <deque>
#include <string>
#include <mutex>
#include <cstdint>

namespace tsl { struct AppState; }

namespace Preset {

    struct PresetParam {
        int num{};
        float val{};
    };

    // Browser categories. THE ORDER HERE IS THE CATEGORY ORDER IN THE BROWSER and
    // the primary sort key of _DATA->presets, so it is also the order the LOAD
    // PRESET selector lists. INIT holds Default alone (it is a starting point, not
    // a sound) and is the one category with NO column in the browser — Default is
    // reached by the RESET SYNTH button in its title bar. USER collects everything
    // the user saved, newest first by default.
    // These values are PERSISTED as integers in presetorder.conf: append only,
    // never renumber, or every saved preset is refiled into the wrong category.
    enum Category {
        CAT_INIT = 0, CAT_LEADS, CAT_PADS, CAT_BASS, CAT_KEYS, CAT_PLUCKS,
        CAT_BELLS, CAT_WINDS, CAT_TEXTURES, CAT_USER, CAT_COUNT
    };
    extern const char* const categoryNames[CAT_COUNT];

    struct Preset {
        std::string name;
        std::string path;
        int64_t date{};
        bool isSystem{};
        // Browser placement. `category` is baked for factory presets and CAT_USER
        // for saved ones; `order` is the position WITHIN that category and is what
        // a drag in the browser rewrites. The deque is sorted by (category, order),
        // not by date, so browser order == selector order by construction.
        int category{CAT_USER};
        int order{};
        // STABLE IDENTITY, and the reason a saved ordering survives a bank edit.
        // A factory preset's `date` is positional (LONG_MAX-1 minus its declaration
        // index), so it is renumbered by any insertion or reorder — useless as a key.
        // uid is an FNV-1a hash of the NAME for factory presets (stable across bank
        // edits; a rename orphans that one entry, which self-heals to its baked
        // position) and of the date for user presets.
        uint32_t uid{};
        std::vector<PresetParam> values;
        std::vector<VcoPreNote> notes;
        std::vector<tsl::parameters::Event> midiEvents;
    };

    // FNV-1a. constexpr so the factory bake can hash names at compile time.
    constexpr uint32_t uidOf(const char* s) {
        uint32_t h = 2166136261u;
        for (; *s; ++s) { h ^= (uint32_t)(unsigned char)*s; h *= 16777619u; }
        return h;
    }

    void setupDefault(tsl::AppState* _appState);
    void setupFactory(tsl::AppState* _appState);
    void readPresets(tsl::AppState* _appState);
    bool contains(tsl::AppState* _appState, int64_t date);
    bool get(tsl::AppState* _appState, Preset& preset, int64_t date);
    void loadPreset(tsl::AppState* _appState, int num, tsl::FastQueue<VcoPreNote>& notes);
    // LFO multi-dest compatibility: fold a legacy LFOnDEST/LFOnDEPTH pair into the
    // matrix when a loaded state carries no matrix values (old preset / old DAW
    // project), then re-seat the DEPTH windows. destStoredMask bit n = the file
    // actually stored LFOnDEST — required, or an orphaned depth saved against the
    // old OFF init would fold into a route that never existed. Called by
    // loadPreset and unserializeState after params are applied.
    extern const int kLfoDestIds[4];
    extern const int kLfoDepthIds[4];
    void foldLegacyLfoRoutes(tsl::AppState* _appState, unsigned destStoredMask);
    void savePreset(tsl::AppState* _appState, std::vector<PresetParam> vals, std::vector<VcoPreNote> notes);
    std::vector<uint8_t> serializeCurrentState(tsl::AppState* _appState);
    void unserializeState(tsl::AppState* _appState, const uint8_t* data, size_t size);
    // Browser order persistence: `uid=order` lines in presetorder.conf next to
    // settings.conf. loadOrder() applies saved positions and re-sorts; saveOrder()
    // writes the current arrangement. sortAll() is the (category, order) sort every
    // list rebuild must end with.
    void sortAll(tsl::AppState* _appState);
    void loadOrder(tsl::AppState* _appState);
    void saveOrder(tsl::AppState* _appState);

    // ── Where preset files live ──────────────────────────────────────────────
    // Everywhere except Android: a directory, opened with fopen/std::filesystem,
    // and Preset::path is a filesystem path. On Android the user may point the
    // app at a folder of their own (Settings > Presets > Preset Folder), which
    // is a SAF tree with no filesystem path at all -- files are reached through
    // a ContentResolver fd and Preset::path is a content:// URI.
    //
    // customFolder() is what every I/O site branches on, and it is false unless
    // the user has chosen a folder. A user who never opens that setting runs
    // exactly the POSIX code that shipped.
    bool customFolder();
    FILE* openPreset(const std::string& handle, const char* mode);
    bool removePreset(const std::string& handle);
    // Re-reads the whole bank from wherever it now lives. Called from Java after
    // the preset folder changed.
    void reload(tsl::AppState* _appState);
};

#endif
