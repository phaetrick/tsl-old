//
// Created by pr on 06.10.19.
//

#include <cstdint>
#include <app.h>
#include <SettingsView.h>
#ifndef __ANDROID__
#include <OSCredentialStore.h>
#endif
#include <settings.h>
#include <tools/PlatformPaths.h>
#include <tools.h>
#include <keyboard.h>
#include "callbacks_loop_controls.h"
#include "types.h"
#include "defines.h"
#include "button.h"
#include "grainstorm.h"
#include "view.h"
#include "infopanel.h"
#include "queue.h"
#include "synth.h"
#include "player.h"
#include "preset.h"
#include "Input.h"
#include "gui.h"
#include "sequencer.h"

bool applySpecialAction(tsl::AppState* _appState, int viewid) {

    switch (viewid) {
        case RECORDButton:
            if (!_appState->player.isrecording.load()) {
#ifdef __ANDROID__
                _appState->WorkerQueue.add_task([mode = RECORD_LIVE, track = 0](){
                ATTACH
                if (env && tsl::android::activityclass) {
                    jmethodID jmid = env->GetStaticMethodID(tsl::android::activityclass, "recFile", "(IJ)V");
                    if (jmid) {
                        env->CallStaticVoidMethod(tsl::android::activityclass, jmid, (jint) mode, (jlong) track);
                    } else {
                        env->ExceptionClear();
                    }
                }
                DETACH
                });
#else
                _STATE->RecordingQueue.add_task([_appState]() {
                    // Desktop-only path (Android records via the Java recFile() above),
                    // and no desktop build has shipped, so this follows the rename.
                    auto folder = tsl::app::getStoragePath("Voltaic/Recordings");
                    if (folder.empty()) {
                        showToast(_STATE, "Failed to get storage path for recordings.");
                        return;
                    }
                    // Filename prefix for exported recordings, e.g.
                    // voltaic_2026-08-11_12-30-00.wav. Same desktop-only block as the
                    // folder above; existing files keep the names they were written with.
                    auto recName = tsl::generateTimestampedName("voltaic");
                    std::string extension = _STATE->getAudioFormat();
                    std::string fullPath = folder + "/" + recName + extension;
                    FILE* fd = std::fopen(fullPath.c_str(), "wb");
                    if (!fd) {
                        showToast(_STATE, "Failed to open file for recording.");
                        return;
                    }
                    auto w = tsl::Player::setupRecording(_STATE, fd, "Live Recording ");
                    if (w != nullptr && _STATE->player.record_live_thread(w) != -1) {
                        if (_STATE->askName) {
                            tsl::graphics::AlphaPopUp kbd(_STATE);
                            kbd.setSlotId(_STATE->waitNotify.acquire_slot());
                            kbd.setTitle("Save As:");
                            tsl::graphics::AlphaPopUp::InputResult result{};
                            kbd.onCompleteCallback = [&result](const tsl::graphics::AlphaPopUp::InputResult& r) {
                                if (r.confirmed) { result = r; return 1; }
                                return 0;
                            };
                            kbd.setText(recName);
                            kbd.init();
                            kbd.addDraw();
                            kbd.addCB();
                            _STATE->waitNotify.wait_for_signal();
                            if (_STATE->destroyRequested.load()) return;
                            kbd.deldraw();
                            kbd.delCB();
                            if (result.confirmed) {
                                std::string trimmed = tsl::trimToValidFilename(result.text);
                                if (!trimmed.empty()) {
                                    std::string newPath = folder + "/" + trimmed + extension;
                                    if (std::rename(fullPath.c_str(), newPath.c_str()) == 0)
                                        showToast(_STATE, ("Recording saved as: " + newPath).c_str());
                                    else
                                        showToast(_STATE, "Rename failed.");
                                } else {
                                    std::remove(fullPath.c_str());
                                    showToast(_STATE, "Recording name empty. File deleted.");
                                }
                            } else {
                                std::remove(fullPath.c_str());
                                showToast(_STATE, "Recording cancelled. File deleted.");
                            }
                        } else {
                            showToast(_STATE, ("Recording saved as: " + fullPath).c_str());
                        }
                    } else {
                        showToast(_STATE, "Failed to start recording.");
                    }
                });
#endif
            } else {
                _appState->player.recStop();
            }
            break;
        case POWERButton: {
            int s;
            if ((s = _appState->WorkerQueue.add_taskInt([_appState](){
                if (!_appState->player.isPlaying()) {
                    _appState->player.flush();
                    _appState->player.play();
                    _appState->data->views.loadInfo->redraw();
                } else {
                    _appState->player.stop();
                }
            })) > 0) {
                char text[100];
                snprintf(text, 100, "POWER On/Off Queued. Pos %d.", s + 1);
                showToast(_appState, text);
            }
            break;
        }
        case SETTINGSBUTTON:
#ifdef __ANDROID__
            _appState->WorkerQueue.add_task([](){
                ATTACH
                if (env && tsl::android::activityclass) {
                    jmethodID jmid = env->GetStaticMethodID(tsl::android::activityclass, "invokeSettings", "()V");
                    if (jmid) {
                        env->CallStaticVoidMethod(tsl::android::activityclass, jmid);
                    } else {
                        env->ExceptionClear();
                    }
                }
                DETACH
            });
#else
            _STATE->UiTasksQueue.add_task([_appState]() {
                auto sp = _STATE->settingsView.load();
                if (sp != nullptr) {
                    auto settings = std::static_pointer_cast<tsl::graphics::Settings>(sp);
                    settings->hide();
                    settings->show();
                } else {
                    std::string xmlPrefs = R"(
        <Settings>
            <PrefCategory name="Audio">
                <Pref name="Audio Format" key="audio_format" type="dropdown" value="0" displayOptions="WAV 16bit,WAV 32bit,FLAC 16bit,FLAC 24bit,MP3 CBR 320 kbps" optionValues="0,12,10,11,8"/>
                <Pref name="Ask for Filename on Finish" key="ask_name" type="bool" value="false"/>
                <Pref name="HQ Resampling" key="hq_resampling" type="bool" value="false"/>
            </PrefCategory>
            <PrefCategory name="Misc">
                <Pref name="Delete saved credentials" description="You will need to sign in again on next start." type="action" key="credentials"/>
            </PrefCategory>
            <PrefCategory name="Links">
                <Pref name="Changelog" type="url" url="https://pocketanalog.rocks.me/changes.html"/>
                <Pref name="EULA" type="url" url="https://pocketanalog.rocks.me/eula.html"/>
                <Pref name="Open Source Software" type="url" url="https://pocketanalog.rocks.me/oss.html"/>
            </PrefCategory>
        </Settings>
        )";
                    std::function<void(const std::string&, int)> intChangeCallback = [_appState](const std::string& key, int value) {
                        if (key == "audio_format")
                            _STATE->format = (uint8_t)value;
                        else if (key == "ask_name")
                            _STATE->askName = value != 0;
                        else if (key == "hq_resampling")
                            _DATA->hqresampling.store(value != 0);
                    };
                    // This is the on-disk settings directory (Application Support/<name>
                    // on macOS, APPDATA\<name> on Windows) and it follows the rename,
                    // because this branch is DESKTOP-ONLY and no desktop build has ever
                    // shipped. Renaming a published one would not fail loudly — it would
                    // silently point at a fresh empty directory and read as a factory
                    // reset — but there is nothing published here to strand.
                    //
                    // Android never reaches this code (see the #ifdef above: settings go
                    // through invokeSettings() and Java SharedPreferences), and its
                    // storage root comes from getExternalFilesDir, i.e. from the frozen
                    // applicationId. Nothing on Android is named after the product.
                    auto settings = std::make_shared<tsl::graphics::Settings>(
                        _appState, tsl::app::appName, intChangeCallback);
                    settings->loadFromXml(xmlPrefs);

                    // Apply persisted values to app state — SettingsManager only fires
                    // intChangeCallback on Set(), not on initial load, so we read them here.
                    {
                        tsl::settings::SettingsManager savedPrefs(tsl::app::getStoragePath(tsl::app::appName));
                        _STATE->format    = (uint8_t)savedPrefs.Get("audio_format", 0);
                        _STATE->askName   = savedPrefs.Get("ask_name", false);
                        _DATA->hqresampling.store(savedPrefs.Get("hq_resampling", false));
                    }

                    settings->getByKey("credentials")->setAction([_appState]() {
                        tsl::OSCredentialStore store(tsl::app::lsName);
                        store.clearAll();
                        showToast(_STATE, "Credentials cleared");
                    });
                    _STATE->settingsView.store(settings, std::memory_order_release);
                    settings->show();
                }
            });
#endif
            break;
        case MIDILEARNBUTTON: {
            auto x = _appState->params[0][viewid].load();
            auto des = x == 1.0 ? 0 : 1.0;
            while (!_appState->params[0][viewid].compare_exchange_weak(x, des,
                                                                        std::memory_order_release,
                                                                        std::memory_order_relaxed));
            _appState->midilearning.store(des == 1.0);
            callbackFXSwitch(_appState, GUISPACE, (int)_appState->params[0][GUISPACE].load());
            break;
        }
        case CLEARTASKS: {
            std::function<void()> func;
            _appState->data->toAudioThreadQueue.pop(func);
            break;
        }
        case ZERONOTE: {
            VCOPreEvent preEvent{0, 0, -1};
            _DATA->preQueue.push(preEvent);
            auto b = _appState->parameters[viewid].view;
            if (b != nullptr && b->visible_.load())
                b->redraw();
            break;
        }
        default:
            // Not a special action — let the caller apply the normal param toggle.
            return false;
    }
    return true;
}