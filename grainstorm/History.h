#pragma once
#include <vector>
#include <string>
#include <mutex>
#include "tools/AtomicSharedPtr.h"
#include "params.h"
#include "tools/FunctionQueueWorker.h"
#include "tools/RingBufferQueue.h"
#include "audio/Recording.h"
#include <FloatingView.h>

using EventQueue = tsl::RingBufferMPSCQueue<tsl::parameters::Event, 4096>;

namespace tsl {
	struct AppState;
	namespace parameters {
	
		extern constexpr int supposeSnapSize(int s);
		struct CurrentSnapShot
		{
			CurrentSnapShot(tsl::AppState* appState) : _appState(appState), mem(nullptr), size(0) {}
			CurrentSnapShot(tsl::AppState* appState, unsigned char *mem_, int s) : _appState(appState), mem(mem_), size(s) {}
			CurrentSnapShot(tsl::AppState*, int size);
			~CurrentSnapShot();
			unsigned char* mem;
			int size;
			tsl::AppState* _appState;
		};
        struct PresetUndoRedo {
			std::vector<Event> events{};
			Event fileEvent{};
			std::string fileName{};
		};

		class Snapshot : public tsl::MPSCWorker<256>, public tsl::graphics::FloatingView
		{
			friend class NormalButton;
			friend struct Event;

		public:
			Snapshot() = default;
			explicit Snapshot(tsl::AppState* appState);
			void addEvent(const Event &e);
			std::shared_ptr<CurrentSnapShot> get();
			bool load(unsigned char *in, int size, bool fromDaw = false);
			static void deserialize(unsigned char* in, int size, std::vector<Event>& events, std::string &name, int trackIndex);
			static void deserialize(unsigned char* in, int size, std::vector<Event>& events, std::string(&names)[4]);
			static void apply(tsl::AppState* _appState, std::shared_ptr<CurrentSnapShot> css, std::vector<Event>& events, std::string(&names)[4], bool fromDaw = false);
			void addRecording(int trackIndex, std::shared_ptr<tsl::Recording> rec);
			void clear(int trackIndex);
			void OnPresetLoaded(std::shared_ptr<CurrentSnapShot> oldsnap, std::shared_ptr<CurrentSnapShot> nsnap, std::vector<Event>&);
			bool undo(int trackIndex);
			bool redo(int trackIndex);
			std::shared_ptr<CurrentSnapShot>get(int trackIndex);
			uint16_t nextGroupId();
			EventQueue queue;
			std::vector<PresetUndoRedo> presetUndoRedos[4]{};
			std::recursive_mutex presetMutex{};
			void loadFromPresetEvent(Event&);
			std::atomic<bool> hasRedos[4]{}, hasUndos[4]{};
			void getEvents(std::vector<Event>& events, int trackIndex);
			
			void lock() {
				mutex.lock();
			}
			void unlock() {
				mutex.unlock();
			}
			bool try_lock() {
				return mutex.try_lock();
			}

		protected:
			int cb(float xpos, float ypos, int action, int pid)override;
			void renderContent(void*)override;
			void computeContent(int maxWidth, int maxHeight) override;
		private:
			tsl::AppState* _appState;
			std::vector<Event>  events_;
			std::string names_[4];
			tsl::AtomicSharedPtr<CurrentSnapShot> currentSnapShot;
			std::shared_ptr<std::string> redoName[4]{};
			std::shared_ptr<std::string> undoName[4]{};
			std::atomic<int> currentTindex{};
			std::atomic<int> hot{-1};
			float xposUndo{}, xposRedo{}, yposUndo{}, yposRedo{};
			float xopsTrackNumber[4]{}, yposTrackNumber[4]{};
			std::vector<Event> history[4]{};
			std::vector<Event> redoEvents[4]{};
			int undoredoPos[4]{};
			std::atomic<uint16_t> groupIdCounter{ 0 };
			Event lastEvent{};
			tsl::time historyTimer;
			int posPreset[4]{};
			std::recursive_mutex mutex{};
			void clear();
			void publish_();

		};
	}
}
