#pragma once
#include <deque>
#include <string>
#include <vector>
#include "view.h"
#include "Input.h"
#include "RecyclerView.h"
#include "preset.h"

namespace tsl {
    struct AppState;
    namespace graphics {

        // Full-window preset browser: categories down the left, the selected
        // category's presets in one full-width list on the right. A tap loads;
        // a drag on a row's grip reorders it, or recategorises it if dropped on
        // the category column; a drag anywhere else scrolls the list.
        //
        // Ported from grainstorm's EffectOrderView rather than shared with it: the
        // shared-library rule says copy into va/ instead of editing grainstorm.
        // Two things changed on the way over.
        //
        // Layout: that view puts three FX queues side by side, and at ten preset
        // categories side-by-side columns would be too narrow to read a preset
        // name in. So categories are a column and the list gets the full width.
        //
        // Gesture: that view has no scrolling to speak of (its queues fit), so a
        // drag there can only mean "reorder", and it drags the item and scrolls
        // the list at the same time. Here the USER category grows without bound
        // and the list must scroll, so drag has to mean two different things —
        // hence the grip. Drag-anywhere-reorders would turn every attempt to
        // scroll into an accidental rearrangement of the bank.
        class PresetBrowserView : public View, protected ScrollViewBase {
        public:
            explicit PresetBrowserView(tsl::AppState* appState);

            void computeSize() override;
            // Called by the resize path for every window owner after the root
            // has been re-laid-out. Without it the browser kept the geometry --
            // and the text sizes -- of the orientation it was opened in.
            void init() override;
            void callback(const InputEvent& event) override;
            void render(void* ctx) override;

            // Builds (or reuses) the browser and shows it. UI thread.
            static void activate(tsl::AppState* appState);
            static void hideIfOpen(tsl::AppState* appState);

            void rebuild();          // refresh the row list from _DATA->presets
            void reset();

        protected:
            void addRecursiveDraw() override;
            void delRecursiveDraw() override;
            void addRecursiveCB() override;
            void delRecursiveCB() override;

        private:
            struct Row {
                std::string name;
                std::string display; // `name` cut to the row width, computeSize's job
                int index{};         // index into _DATA->presets
                uint32_t uid{};
            };
            // The dragged row. Atomic for the same reason EffectOrderView's is:
            // render() reads it while the input callback writes it. row < 0 while
            // nothing is being dragged, which is also how MOVING tells a list
            // scroll (row < 0) from a row reorder (row >= 0).
            struct Drag {
                Drag() = default;
                Drag(int row_, float x_, float y_) : row(row_), x(x_), y(y_) {}
                int row{-1};
                float x{};
                float y{};
            };

            void commitDrop(int from, int to);
            void moveToCategory(int row, int cat);
            void resetSynth();
            void loadRow(int row);
            void clampOffset();
            int rowAt(float ypos) const;
            int catAt(float ypos) const;

            // Scroll position kept per category as a FRACTIONAL TOP ROW INDEX
            // (-offset / itemheight), not as pixels. Rows are the only unit that
            // survives a resize: itemheight is derived from the window width here,
            // so a pixel offset restored into a different window size lands
            // somewhere arbitrary. render() keeps this current; computeSize()
            // converts it back at whatever itemheight is then in force.
            float catTop[Preset::CAT_COUNT]{};

            std::vector<Row> rows;
            std::atomic<Drag> drag{Drag()};
            // INIT is NOT one of the browser's columns: it holds Default alone, and
            // Default is reached by the RESET SYNTH button in the bar instead. The
            // enum value stays where it is — presetorder.conf stores categories as
            // integers, so renumbering the enum would refile every saved preset.
            static constexpr int kCatFirst = Preset::CAT_LEADS;
            static constexpr int kCatRows = Preset::CAT_COUNT - kCatFirst;

            int activeCat{Preset::CAT_LEADS};
            int pressRow{-1};        // row the finger went down on, -1 for none
            bool pressGrip{};        // ...and whether it went down on its grip
            int pressCat{-1};        // category the finger is holding, -1 for none
            bool closeHot{};         // the X is pressed (settings2's feedback)
            bool resetHot{};         // ...and the same for RESET SYNTH
            float resetW{};          // width of the RESET SYNTH hit zone in the bar
            float catWidth{};
            float catTextSize{};
            float catItemH{};        // categories get their own height: the box is
                                     // fixed now and all ten have to fit inside it
            float listX{}, listW{};
            float gripX{}, gripW{};
            float nameW{};
            float barH{};
            int windowindex{-1};
        };
    }
}
