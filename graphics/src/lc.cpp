#include "lc.h"

#include <LicenseChecker.h>
#include "DynamicDialog.h"
#include "AnimatedSpinner.h"
#include "app.h"
#include <IconsMaterialDesignReduced.h>
#include "StringObfuscation.h"
#include <chrono>
#include <deque>
#include <vector>
#include <string>
#include <algorithm>
#include <random>

#if defined GRAINSTORM
#ifdef STANDALONE_MODE
static constexpr int productNumber = 0;
#elif defined PLUGIN_MODE
static constexpr int productNumber = 1;
#else
static constexpr int productNumber = 999;
#endif
#elif defined POCKET_ANALOG
static constexpr int productNumber = 0;

#else
static constexpr int productNumber = 999;
#endif


class RateLimiter {
public:
	struct Limit {
		int maxEvents;
		std::chrono::seconds timeWindow;
		std::string description;

		Limit(int max, int seconds, const std::string& desc = "")
			: maxEvents(max), timeWindow(seconds), description(desc) {
		}
	};

	// Constructor with single limit
	RateLimiter(int maxEvents = 5, int timeWindowSeconds = 60)
		: blocked_(false)
		, blockUntil_()
		, blockCount_(0)
		, useProgressivePenalty_(false)
		, penaltyMultiplier_(2.0f)
		, maxPenaltySeconds_(3600)
	{
		addLimit(maxEvents, timeWindowSeconds);
	}

	// Constructor with multiple limits
	RateLimiter(const std::vector<Limit>& limits)
		: limits_(limits)
		, blocked_(false)
		, blockUntil_()
		, blockCount_(0)
		, useProgressivePenalty_(false)
		, penaltyMultiplier_(2.0f)
		, maxPenaltySeconds_(3600)
	{
	}

	// Add a new limit
	void addLimit(int maxEvents, int timeWindowSeconds, const std::string& description = "") {
		limits_.push_back(Limit(maxEvents, timeWindowSeconds, description));
	}

	// Enable progressive penalty (each block increases wait time)
	// multiplier: how much to multiply wait time (default 2.0 = double each time)
	// maxSeconds: maximum wait time cap (default 3600 = 1 hour)
	void enableProgressivePenalty(float multiplier = 2.0f, int maxSeconds = 3600) {
		useProgressivePenalty_ = true;
		penaltyMultiplier_ = multiplier;
		maxPenaltySeconds_ = maxSeconds;
	}

	void disableProgressivePenalty() {
		useProgressivePenalty_ = false;
		blockCount_ = 0;
	}

	// Check if a new event is allowed
	bool newEvent() {
		auto now = std::chrono::steady_clock::now();

		// Check if currently blocked
		if (blocked_) {
			if (now < blockUntil_) {
				return false; // Still blocked
			}
			else {
				// Block period expired, unblock but keep block count for progressive penalty
				blocked_ = false;
				blockUntil_ = std::chrono::steady_clock::time_point();
				violatedLimit_ = nullptr;
			}
		}

		// Remove events outside the largest time window
		cleanupOldEvents(now);

		// Check all limits (scan backwards - longer time windows last)
		for (auto it = limits_.rbegin(); it != limits_.rend(); ++it) {
			const auto& limit = *it;
			int eventCount = countEventsInWindow(now, limit.timeWindow);

			if (eventCount >= limit.maxEvents) {
				// Rate limit exceeded - calculate block time
				blocked_ = true;
				blockCount_++;
				lastBlockTime_ = now;

				int baseBlockSeconds = static_cast<int>(limit.timeWindow.count());
				int actualBlockSeconds = baseBlockSeconds;

				if (useProgressivePenalty_ && blockCount_ > 1) {
					// Apply progressive penalty: base * multiplier^(blockCount-1)
					float penalty = std::pow(penaltyMultiplier_, blockCount_ - 1);
					actualBlockSeconds = static_cast<int>(baseBlockSeconds * penalty);

					// Cap at maximum
					actualBlockSeconds = std::min(actualBlockSeconds, maxPenaltySeconds_);
				}

				blockUntil_ = now + std::chrono::seconds(actualBlockSeconds);
				violatedLimit_ = &limit;
				return false;
			}
		}

		// Event allowed - record it
		events_.push_back(now);

		// If enough time has passed since last violation, reduce block count
		if (useProgressivePenalty_ && blockCount_ > 0) {
			// Decay block count if user behaves well
			auto timeSinceLastBlock = now - lastBlockTime_;
			if (timeSinceLastBlock > std::chrono::minutes(5)) {
				blockCount_ = std::max(0, blockCount_ - 1);
			}
		}

		return true;
	}

	// Get remaining wait time in seconds
	int getWaitTimeSeconds() const {
		if (!blocked_) {
			return 0;
		}

		auto now = std::chrono::steady_clock::now();
		if (now >= blockUntil_) {
			return 0;
		}

		auto remaining = std::chrono::duration_cast<std::chrono::seconds>(blockUntil_ - now);
		return static_cast<int>(remaining.count());
	}

	// Get remaining wait time in milliseconds
	int getWaitTimeMilliseconds() const {
		if (!blocked_) {
			return 0;
		}

		auto now = std::chrono::steady_clock::now();
		if (now >= blockUntil_) {
			return 0;
		}

		auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(blockUntil_ - now);
		return static_cast<int>(remaining.count());
	}

	// Get formatted wait time
	std::string getWaitTimeFormatted() const {
		int seconds = getWaitTimeSeconds();
		if (seconds == 0) {
			return "0s";
		}

		int hours = seconds / 3600;
		int minutes = (seconds % 3600) / 60;
		int remainingSeconds = seconds % 60;

		std::string result;
		if (hours > 0) {
			result = std::to_string(hours) + "h ";
		}
		if (minutes > 0 || hours > 0) {
			result += std::to_string(minutes) + "m ";
		}
		result += std::to_string(remainingSeconds) + "s";

		return result;
	}

	// Get formatted wait time with milliseconds
	std::string getWaitTimeFormattedPrecise() const {
		int milliseconds = getWaitTimeMilliseconds();
		if (milliseconds == 0) {
			return "0s";
		}

		int totalSeconds = milliseconds / 1000;
		int ms = milliseconds % 1000;
		int hours = totalSeconds / 3600;
		int minutes = (totalSeconds % 3600) / 60;
		int seconds = totalSeconds % 60;

		std::string result;
		if (hours > 0) {
			result = std::to_string(hours) + "h " + std::to_string(minutes) + "m ";
		}
		else if (minutes > 0) {
			result = std::to_string(minutes) + "m ";
		}

		result += std::to_string(seconds);
		if (ms > 0 && hours == 0) { // Only show ms if less than 1 hour
			result += "." + std::to_string(ms);
		}
		result += "s";

		return result;
	}

	// Get which limit was violated
	std::string getViolatedLimitDescription() const {
		if (violatedLimit_ && !violatedLimit_->description.empty()) {
			return violatedLimit_->description;
		}
		return "Rate limit exceeded";
	}

	// Check if currently blocked
	bool isBlocked() const {
		if (!blocked_) {
			return false;
		}

		auto now = std::chrono::steady_clock::now();
		return now < blockUntil_;
	}

	// Get current block count (how many times user has been blocked)
	int getBlockCount() const {
		return blockCount_;
	}

	// Get event count
	int getEventCount(int timeWindowSeconds = -1) const {
		auto now = std::chrono::steady_clock::now();

		if (timeWindowSeconds < 0) {
			return static_cast<int>(events_.size());
		}

		return countEventsInWindow(now, std::chrono::seconds(timeWindowSeconds));
	}

	// Get remaining attempts
	int getRemainingAttempts() const {
		if (blocked_) {
			return 0;
		}

		auto now = std::chrono::steady_clock::now();
		int minRemaining = INT_MAX;

		for (const auto& limit : limits_) {
			int eventCount = countEventsInWindow(now, limit.timeWindow);
			int remaining = limit.maxEvents - eventCount;
			if (remaining < minRemaining) {
				minRemaining = remaining;
			}
		}

		return minRemaining == INT_MAX ? 0 : minRemaining;
	}

	// Get status report
	std::string getStatusReport() const {
		auto now = std::chrono::steady_clock::now();
		std::string report;

		for (size_t i = 0; i < limits_.size(); ++i) {
			const auto& limit = limits_[i];
			int eventCount = countEventsInWindow(now, limit.timeWindow);
			int remaining = limit.maxEvents - eventCount;

			if (i > 0) report += "\n";

			report += "Limit " + std::to_string(i + 1);
			if (!limit.description.empty()) {
				report += " (" + limit.description + ")";
			}
			report += ": " + std::to_string(eventCount) + "/" + std::to_string(limit.maxEvents);
			report += " (remaining: " + std::to_string(remaining) + ")";
		}

		if (blocked_) {
			report += "\nBLOCKED for " + getWaitTimeFormatted();
			if (useProgressivePenalty_) {
				report += " (violation #" + std::to_string(blockCount_) + ")";
			}
		}

		return report;
	}

	// Reset completely (clears history and block count)
	void reset() {
		events_.clear();
		blocked_ = false;
		blockUntil_ = std::chrono::steady_clock::time_point();
		violatedLimit_ = nullptr;
		blockCount_ = 0;
		lastBlockTime_ = std::chrono::steady_clock::time_point();
	}

	// Clear all limits
	void clearLimits() {
		limits_.clear();
		reset();
	}

private:
	int countEventsInWindow(const std::chrono::steady_clock::time_point& now,
		const std::chrono::seconds& window) const {
		auto cutoff = now - window;
		int count = 0;

		for (const auto& eventTime : events_) {
			if (eventTime >= cutoff) {
				count++;
			}
		}

		return count;
	}

	void cleanupOldEvents(const std::chrono::steady_clock::time_point& now) {
		std::chrono::seconds maxWindow(0);
		for (const auto& limit : limits_) {
			if (limit.timeWindow > maxWindow) {
				maxWindow = limit.timeWindow;
			}
		}

		if (maxWindow.count() == 0) return;

		auto cutoff = now - maxWindow;

		while (!events_.empty() && events_.front() < cutoff) {
			events_.pop_front();
		}
	}

	std::vector<Limit> limits_;
	std::deque<std::chrono::steady_clock::time_point> events_;
	bool blocked_;
	std::chrono::steady_clock::time_point blockUntil_;
	std::chrono::steady_clock::time_point lastBlockTime_;
	const Limit* violatedLimit_ = nullptr;

	// Progressive penalty settings
	int blockCount_;
	bool useProgressivePenalty_;
	float penaltyMultiplier_;
	int maxPenaltySeconds_;
};


void tsl::Lc::threadFunc(tsl::AppState* _appState) {
	auto slot = _STATE->waitNotify.acquire_slot();
	tsl::graphics::AnimatedSpinner spinner(_STATE);
	tsl::LicenseChecker ls(OBF_STR("https://thesecretlaboratory.com"), 443, true, productNumber, tsl::app::lsName, [](const std::string& msg) {LOGE("%s", msg.c_str());});
	auto wait = rand() % 300;
	_STATE->waitNotify.wait_for_signal((wait + 60) * 1000); // blocks until user interacts

	// Consecutive checks where something answered on our behalf but it wasn't
	// us. See the interception branch below for why that is tracked separately.
	int interceptedStreak = 0;
	constexpr int kInterceptedBeforePrompt = 5;

	// Consecutive "signin" verdicts. The first one is ordinary — this machine's
	// key can legitimately go stale — and clearing it to sign in again is the
	// designed recovery. A second one means that recovery has already been
	// tried and the server refused anyway, so repeating it can only produce an
	// endless credential prompt. See the branch below.
	int signinStreak = 0;
	constexpr int kSigninBeforeGivingUp = 2;

	// Every terminal verdict — no licence for this product, device limit reached,
	// trial over, account unverified, payment not yet settled — ends here.
	//
	// DELIBERATELY BUTTONLESS, and it must stay that way. Lc reports nothing back
	// to the app: no flag on AppState, no callback, threadFunc returns void. This
	// dialog IS the enforcement — the moment it can be dismissed, or this thread
	// returns with the window gone, the app is fully usable with no licence at
	// all. A dismiss button here reads as a UX improvement and is actually a
	// bypass. Anything that lets the user proceed has to gate the app for real
	// first.
	//
	// What DID change is the message: the server now distinguishes "your payment
	// has not cleared yet" from "you do not own this", so a buyer waiting on a
	// bank transfer is told to start the app again later instead of being told
	// they have no licence. The remedy for every case here is a restart.
	auto showTerminal = [&](const std::string& message) {
		auto dialog = std::make_unique<tsl::graphics::DynamicDialog>(_STATE);
		dialog->setTitle(message);
		dialog->setStaticMode(true);
		dialog->setButtonMode(tsl::graphics::DynamicDialog::ButtonMode::NO_BUTTONS);
		dialog->init();
		dialog->addDraw();
		dialog->addCB();
		_STATE->waitNotify.wait_for_signal(); // blocks; only shutdown releases it
		dialog->deldraw();
		dialog->delCB();
		};

	while (!_STATE->destroyRequested.load()) {
		// --- Offline licence token, checked before any network activity ------
		//
		// An activated machine holds a server-signed token, so it can prove its
		// own licence with no connection at all. Deliberately evaluated BEFORE
		// checkConnectivity(), which opens sockets: a user working offline
		// should generate no network traffic and no waiting whatsoever.
		// Gated on the device private key. clearAll() — which the "signin" verdict
		// below calls — is a misnomer: it deletes only the device key, NOT the
		// token. Trusting a token on its own would therefore let a device the
		// server has just DE-REGISTERED sail straight through this fast path,
		// never reach the activation flow, and keep running offline until the
		// token expired. A token without its device key is incoherent however it
		// arose, so discard it rather than reason about how it got there.
		const bool activated = ls.hasPrivateKey();
		tsl::TokenStatus tok;
		if (activated) {
			tok = ls.checkStoredToken(productNumber);
		}
		else {
			ls.clearLicenseToken();
		}

		if (tok.valid && !tok.refreshDue) {
			spinner.deldraw();
			return;
		}
		if (tok.tampered) {
			// Forged, copied from another machine, or the clock was wound back.
			// Drop it and fall through to a normal online activation.
			ls.clearLicenseToken();
		}

		auto inetConnect = ls.checkConnectivity();

		// Token still usable but we cannot reach the server: let the user work.
		// Interrupting somebody who has already paid because WE are unreachable
		// is precisely the failure the token exists to prevent. Only reached
		// when the token is valid-but-due-for-refresh, or inside its grace
		// period; once the grace runs out tok.usable() is false and the normal
		// activation path below takes over.
		if (tok.usable() && !(inetConnect.internetAvailable && inetConnect.serverReachable)) {
			if (tok.inGrace) {
				const std::string msg = OBF_STR("Licence could not be re-checked. ") +
					std::to_string(tok.graceDaysRemaining) +
					OBF_STR(" days of offline use remaining.");
				showToast(_STATE, msg.c_str());
			}
			spinner.deldraw();
			return;
		}
		 
		// Reuses the value sampled above rather than re-reading the credential
		// store, which is a Keychain / Credential Manager round trip. Equivalent:
		// nothing between there and here can create the key.
		if (!activated) {
			if (!inetConnect.internetAvailable) {
				auto loginDialog = std::make_unique<tsl::graphics::DynamicDialog>(_STATE);
				loginDialog->setTitle(OBF_STR("No internet connection detected and the software is not activated. Please verify your network settings to continue."));
				loginDialog->onCompleteCallback = [&](const tsl::graphics::DynamicDialog::DialogResult& result) {
					_STATE->waitNotify.wake_thread(slot); // unblocks
					};
				loginDialog->setCustomButton(OBF_STR("Try again"), [_STATE, slot]() {
					_STATE->waitNotify.wake_thread(slot); // unblocks
					});
				spinner.deldraw();
				spinner.delCB();

				loginDialog->init();
				loginDialog->addDraw();
				loginDialog->addCB();
				_STATE->waitNotify.wait_for_signal(); // blocks until user interacts
				loginDialog->delCB();
				loginDialog->deldraw();
				if (_STATE->destroyRequested.load()) {
					return;
				}
				spinner.init();
				spinner.addDraw();
				spinner.addCB();
				continue;

			}
			else if (!inetConnect.serverReachable) {
				// Two very different conditions land here and used to be treated
				// alike:
				//
				//   a) nothing answered at all — genuinely offline, a legitimate
				//      state we must stay lenient about.
				//   b) SOMETHING answered, but it wasn't us — a Cloudflare bot
				//      challenge, a captive portal, corporate TLS interception,
				//      or an ISP hijacking NXDOMAIN.
				//
				// (b) silently produced the same unlimited grace as (a), so an
				// intercepted licence check was indistinguishable from being
				// offline and simply handed out the unactivated grace period
				// forever, with no error anywhere. testServerReachability()
				// already separates the two — it reports "Server responded but
				// ping failed" and records the HTTP status — the result just was
				// never acted on.
				//
				// We stay quiet for a few rounds (a portal may still be pending
				// sign-in, a challenge may clear) and retry far more often than
				// the offline path, but we do not stay silent indefinitely.
				// >= 300 rather than >= 400 on purpose: a captive portal answers
				// with a 302 to its sign-in page, and /api/ping never legitimately
				// redirects — the client speaks HTTPS to the apex directly, so it
				// never meets the http->https or www->apex rules. A 403 challenge
				// keeps its status code here because the library returns the body
				// for >= 400 rather than an empty string, while a challenge served
				// as 200 is caught by the message instead.
				const bool intercepted =
					ls.lastHttpStatusCode >= 300 ||
					ls.lastStatusMessage.find(OBF_STR("responded")) != std::string::npos;

				if (!intercepted) {
					interceptedStreak = 0;
					_STATE->waitNotify.wait_for_signal(5 * 60 * 1000); // blocks until user interacts
					continue;
				}

				if (++interceptedStreak < kInterceptedBeforePrompt) {
					_STATE->waitNotify.wait_for_signal(30 * 1000); // retry soon; this is not an outage
					continue;
				}

				interceptedStreak = 0;
				auto loginDialog = std::make_unique<tsl::graphics::DynamicDialog>(_STATE);
				loginDialog->setTitle(OBF_STR("The connection to our activation server is being intercepted, so the software cannot be activated. This is usually caused by a captive portal, a company firewall or a security product. Please try a different network."));
				loginDialog->onCompleteCallback = [&](const tsl::graphics::DynamicDialog::DialogResult& result) {
					_STATE->waitNotify.wake_thread(slot); // unblocks
					};
				loginDialog->setCustomButton(OBF_STR("Try again"), [_STATE, slot]() {
					_STATE->waitNotify.wake_thread(slot); // unblocks
					});
				spinner.deldraw();
				spinner.delCB();

				loginDialog->init();
				loginDialog->addDraw();
				loginDialog->addCB();
				_STATE->waitNotify.wait_for_signal(); // blocks until user interacts
				loginDialog->delCB();
				loginDialog->deldraw();
				if (_STATE->destroyRequested.load()) {
					return;
				}
				spinner.init();
				spinner.addDraw();
				spinner.addCB();
				continue;
			}
			else {
				std::string title = OBF_STR("Please activate the software using the same credentials from our website. Your information is protected with a secure connection.");
				auto connected = false;
				std::string email, password;
				while (!connected && !_STATE->destroyRequested.load()) {
					// When user tries to login:
					auto loginDialog = std::make_unique<tsl::graphics::DynamicDialog>(_STATE);
					loginDialog->setTitle(title);
					loginDialog->addInputField(OBF_STR("Email:"));
					loginDialog->setFieldValue(0, email);
					loginDialog->addInputField(OBF_STR("Password:")); // password field
					loginDialog->setFieldValue(1, password);
					loginDialog->setButtonMode(tsl::graphics::DynamicDialog::ButtonMode::OK_ONLY);
					loginDialog->onCompleteCallback = [&](const tsl::graphics::DynamicDialog::DialogResult& result) {
						if (result.confirmed) {
							email = result.namedValues.at(OBF_STR("Email:"));
							password = result.namedValues.at(OBF_STR("Password:"));
							if (!password.empty() && !email.empty())
								_STATE->waitNotify.wake_thread(slot); // unblocks
						}
						};
					loginDialog->init();
					loginDialog->addDraw();
					loginDialog->addCB();
					_STATE->waitNotify.wait_for_signal(); // blocks until user interacts
					loginDialog->deldraw();
					loginDialog->delCB();
					if (_STATE->destroyRequested.load()) {
						return;
					}

					spinner.init();
					spinner.addDraw();
					auto res = ls.login(email, password);
					spinner.deldraw();
					if (res == 0) {
						// The server refused for a reason no retry can change with
						// these credentials: not this account's product, device
						// limit reached, account unverified, payment still
						// clearing. Re-opening the credential prompt would blame
						// the user for something that is not a typo.
						showTerminal(ls.lastStatusMessage);
						return;
					}
					else if (res == 1) {
						title = ls.lastStatusMessage + OBF_STR(" Please try again.");
						continue;
					}
					else if (res == 3) {
						auto loginDialog = std::make_unique<tsl::graphics::DynamicDialog>(_STATE);
						loginDialog->setTitle(ls.lastStatusMessage);
						loginDialog->setStaticMode(true);
						loginDialog->setButtonMode(tsl::graphics::DynamicDialog::ButtonMode::NO_BUTTONS);
						loginDialog->init();
						loginDialog->addDraw();
						loginDialog->addCB();
						_STATE->waitNotify.wait_for_signal(ls.waitTimeMs); // blocks until user interacts
						loginDialog->deldraw();
						loginDialog->delCB();
						if (_STATE->destroyRequested.load()) {
							return;
						}
						continue;
					}
					else {
						connected = true;
					}
				}

			}

		}

		if (inetConnect.internetAvailable && inetConnect.serverReachable) {
			auto v = ls.validateProductDetailed(productNumber, [](const std::string& result) {
				// Deliberately empty. The callback receives the raw activation
				// response, and every log macro here reaches an attacker: on
				// Android __android_log_write lands in logcat, on Apple NSLog
				// in Console, and stringf() copies EVERY message into
				// tsl::logRingBuffer regardless of platform. Handing out the
				// verbatim protocol — and the signed licence token with it —
				// would undo the point of obfuscating these strings at all.
				});
			spinner.deldraw();
			if (v.action != OBF_STR("signin")) signinStreak = 0;
			if (v.success) {
				if (v.hasTrial)
					showToast(_STATE, v.status.c_str());
				return;
			}
			else {
				if (v.statusCode == 200) {
					if (v.action == OBF_STR("signin")) {
						// Reached twice in a row means the sign-in that ran on
						// the previous pass succeeded — the server accepted the
						// credentials and registered this device — and the very
						// next validation still did not recognise the device.
						// The credentials are demonstrably not the problem, so
						// wiping the key and asking for them again cannot fix
						// it; it just re-opens the same dialog forever, with no
						// message, which is indistinguishable from the app being
						// broken. Surface what the server actually said instead.
						if (++signinStreak >= kSigninBeforeGivingUp) {
							showTerminal(v.status + OBF_STR(" This device could not be registered even though the sign-in succeeded. Please contact support."));
							return;
						}
						ls.clearAll();
						continue;
					}
					else if (v.action == OBF_STR("tryagain")) {
						auto loginDialog = std::make_unique<tsl::graphics::DynamicDialog>(_STATE);
						loginDialog->setTitle(v.status);
						loginDialog->setButtonMode(tsl::graphics::DynamicDialog::ButtonMode::CUSTOM_BUTTON);
						loginDialog->setCustomButton(OBF_STR("Try again"), [&]() {
							_STATE->waitNotify.wake_thread(slot); // unblocks
							});
						loginDialog->onCompleteCallback = [&](const tsl::graphics::DynamicDialog::DialogResult& result) {
							if (result.confirmed) {
								_STATE->waitNotify.wake_thread(slot); // unblocks
							}
							};

						loginDialog->init();
						loginDialog->addDraw();
						loginDialog->addCB();
						_STATE->waitNotify.wait_for_signal(); // blocks until user interacts
						loginDialog->deldraw();
						loginDialog->delCB();
						spinner.init();
						spinner.addDraw();
						continue;

					}
					else {
						// "stop" and anything unrecognised: revoked, refunded,
						// trial over. Nothing this session can do about it.
						showTerminal(v.status);
						return;
					}

				}
			}

		}
		auto wait = rand() % 300;
		_STATE->waitNotify.wait_for_signal((wait + 60) * 1000); // blocks until user interacts

	}
	spinner.deldraw();
};
