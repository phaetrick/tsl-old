#pragma once
#include <functional>
#include <string>
#include <thread>

namespace tsl {

	struct AppState;
	class Lc {
	public:
		Lc(tsl::AppState* appState, std::string appName) : _appState(appState), _appName(appName) {
			thr = std::thread(&Lc::threadFunc, this, appState);
		}
		~Lc() { if (thr.joinable())thr.join(); }

	private:
		tsl::AppState* _appState;
		std::string _appName;

		std::thread thr;
		void threadFunc(tsl::AppState* _appState);
	};

	// namespace graphics
} // namespace tsl
