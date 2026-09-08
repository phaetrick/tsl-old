#include "app.h"
#include "params.h"
#include "grainstorm.h"
#include "infopanel.h"
#include "track.h"
#include "waveform.h"
#include <cstdint>
#include "types.h"
#include "defines.h"
#include "button.h"
#include "ControlItem.h"
#include "view.h"
#include "synth.h"
#include "lfo.h"
#include "Presets/preset.h"
#include "Input.h"
#include "TrackSettings.h"
#include "Editor.h"
#include "DynamicDialog.h"
#include "player.h"
#include <iostream>
#include <memory>
#include <tools/StackVector.h>

#include "tools/PlatformPaths.h"

#if defined PLUGIN_MODE || defined STANDALONE_MODE
#include <SettingsView.h>
#ifndef PLATFORM_MOBILE
#include <OSCredentialStore.h>
#endif
#endif

#if defined PLUGIN_MODE
#include "IPlugParamDefs.h"
#endif
using namespace tsl::graphics;

using namespace tsl::parameters;

void Event::applyFromExt(tsl::AppState *_appState, tsl::parameters::SenderFlags from) {
    TRACK *track = _DATA->tracks[trackIndex];
    TRACK *distr = _DATA->tracks[(int) _STATE->params[track->index][DISTRSOURCE].load()];
    auto info = _DATA->views.infopanel;

    switch (paramIndex) {
        case REDOBUTTON: {
            if (_DATA->snapShot.hasRedos[trackIndex].load()) {
                int32_t res = _DATA->snapShot.add_taskInt([_STATE, tindex = trackIndex] {
                    _DATA->snapShot.redo(tindex);
                });
                if (res > 0) {
                    char text[100];
                    snprintf(text, 100, "Redo queued. Pos %d.", res + 1);
                    showToast(_STATE, text);
                }
            }
            break;
        }
        case UNDOBUTTON: {
            if (_DATA->snapShot.hasUndos[trackIndex].load()) {
                int32_t res = _DATA->snapShot.add_taskInt([_STATE, tindex = trackIndex] {
                    _DATA->snapShot.undo(tindex);
                });
                if (res > 0) {
                    char text[100];
                    snprintf(text, 100, "Undo queued. Pos %d.", res + 1);
                    showToast(_STATE, text);
                }
            }
            break;
        }
        case MICROPHONEButton: {
            Recorder &recorder = _DATA->recorder;
            if (_STATE->params[track->index][MICROPHONEButton].load() == 0.) {
                int32_t res = _DATA->snapShot.add_taskInt(startrecfunc, track);
                if (res > 0) {
                    char text[100];
                    snprintf(text, 100, "%s Microphone Recording Queued. Pos %d.", track->name, res + 1);
                    showToast(_STATE, text);
                }
            } else {
                recorder._isRecording.store(false);
                _STATE->waitNotify.wake_thread(recorder.slot);
            }
            break;
        }

#if defined STANDALONE_MODE || defined PLUGIN_MODE
        case EDITORSAVE: {
            if (auto s = _STATE->WorkerQueue.add_taskInt([_appState, track]() {
                save_loop(_STATE, track);
            }) > 0) {
                char text[100];
                snprintf(text, 100, "SAVE LOOP Queued. Pos %d.", s + 1);
                showToast(_STATE, text);
            }
        }
        break;

        case SETTINGSBUTTON:
            _STATE->UiTasksQueue.add_task([_appState]() {
                auto sp = _STATE->settingsView.load();
                if (sp != nullptr) {
                    auto settings = std::static_pointer_cast<tsl::graphics::Settings>(sp);
                    settings->hide();
                    settings->show();
                } else {
                    std::string xmlPrefs = R"(
        <Settings>)";
#if defined(OS_IOS) && defined(STANDALONE_MODE)
                    if (!_DATA->isRunningAsPlugin) {
                        xmlPrefs += R"(
            <PrefCategory name="Audio">
                <Pref name="Buffer Size" key="buffer_size" type="dropdown" value="256" displayOptions="64 samples,128 samples,256 samples,512 samples,1024 samples, 2048 samples, 4096 samples" optionValues="64,128,256,512,1024,2048,4096"/>
            </PrefCategory>)";
                    }
#endif
                    xmlPrefs += R"(
            <PrefCategory name="Recording">
                <Pref name="Audio Format" key="audio_format" type="dropdown" value="0" displayOptions="WAV 16bit,WAV 32bit,FLAC 16bit, FLAC 24bit, MP3 CBR 320 kbps" optionValues="0,12,10,11,8"/>
                <Pref name="Ask for Filename on Finish" key="ask_name" type="bool" value="false"/>
            </PrefCategory>)";
#if defined(OS_IOS) && defined(STANDALONE_MODE)
                    if (!_DATA->isRunningAsPlugin) {
                        xmlPrefs += R"(
            <PrefCategory name="Microphone">
                <Pref name="Pause Playback when Recording" key="mic_pause_playback" type="bool" value="true"/>
            </PrefCategory>)";
                    }
#endif
                    xmlPrefs += R"(
            <PrefCategory name="Presets">
                <Pref name="Save Audio" description="Saves audio on track with preset in FLAC 16-bit format. Useful if you want to reuse presets after modifying or deleting the audio source file, on other devices or even share presets with other users. This will grow the preset size. The maximum preset size with 7 minutes stereo sound loaded on all four tracks is 100-200mb approx." key="preset_save_audio" type="bool" value="false"/>
            </PrefCategory>
        )";

#ifndef PLATFORM_MOBILE
                    xmlPrefs += R"(
            <PrefCategory name="Misc">
                <Pref name="Delete saved credentials" description="You will need to sign in again on next start." type="action" key="credentials"/>
            </PrefCategory>
        )";
#endif

                    xmlPrefs += R"(        </Settings>
        )";
#if defined(OS_IOS) && defined(STANDALONE_MODE)
                    xmlPrefs.insert(xmlPrefs.rfind("</Settings>"),
                        R"(<PrefCategory name="Grainstorm Pro">
                <Pref name="Unlock Pro" description="Purchase Grainstorm Pro to unlock all features." type="action" key="purchase_pro"/>
                <Pref name="Restore Purchase" type="action" key="restore_purchases"/>
            </PrefCategory>
            <PrefCategory name="Grainstorm AUv3">
                <Pref name="Unlock AUv3 Plugin" description="Purchase the AUv3 plugin to use Grainstorm in your DAW." type="action" key="purchase_au"/>
            </PrefCategory>)");
#endif
#if defined(OS_IOS)
                    const std::string privacyUrl = "https://thesecretlaboratory.com/apps/grainstorm/policy#ios";
#elif defined(__ANDROID__) || defined(OS_ANDROID)
                    const std::string privacyUrl = "https://thesecretlaboratory.com/apps/grainstorm/policy#android";
#else
                    const std::string privacyUrl = "https://thesecretlaboratory.com/apps/grainstorm/policy#desktop";
#endif
                    xmlPrefs.insert(xmlPrefs.rfind("</Settings>"),
                        std::string(R"(<PrefCategory name="Links">
                <Pref name="Privacy Policy" type="url" url=")")
                        + privacyUrl +
                        R"("/>
                <Pref name="Visit Our Website" type="url" url="https://thesecretlaboratory.com"/>
                <Pref name="Open Source Software" type="url" url="https://thesecretlaboratory.com/apps/grainstorm/oss"/>
            </PrefCategory>)"
                    );
                    std::function<void(const std::string &key, int value)> intChangeCallback = [_appState
                            ](const std::string &key, int value) {
                        if (key == "audio_format")
                            _STATE->format = value;
                        else if (key == "ask_name")
                            _STATE->askName = value == 0 ? false : true;
                        else if (key == "preset_save_audio")
                            _DATA->saveAudioWithPreset = value == 0 ? false : true;
                        else if (key == "mic_pause_playback")
                            _DATA->micPausePlayback = value != 0;
#if defined(OS_IOS) && defined(STANDALONE_MODE)
                        else if (key == "buffer_size") {
                            auto res2 = _DATA->snapShot.add_taskInt([_STATE, value] {
                                _STATE->player.preferredBufferSize = value;
                                if (_STATE->player.setBufferSizeFunc)
                                    _STATE->player.setBufferSizeFunc(value);
                            });
                            if (res2 > 0) {
                                char text[100];
                                snprintf(text, 100, "Change Buffer Size Queued. Pos %d.",
                                         res2 + 1);
                                showToast(_STATE, text);
                            }
                        }
#endif
                    };
                    auto settings = std::make_shared<tsl::graphics::Settings>(
                        _appState, "Grainstorm", intChangeCallback);
                    settings->loadFromXml(xmlPrefs);
#if defined(OS_IOS) && defined(STANDALONE_MODE)
                    settings->getByKey("purchase_pro")->setAction([_appState]() {
                        _appState->purchasePro();
                    });
                    settings->getByKey("purchase_au")->setAction([_appState]() {
                        _appState->purchaseAU();
                    });
                    settings->getByKey("restore_purchases")->setAction([_appState]() {
                        _appState->restorePurchases();
                    });
#endif
#ifndef PLATFORM_MOBILE
                    settings->getByKey("credentials")->setAction([_appState]() {
                        tsl::OSCredentialStore store(tsl::app::lsName);
                        store.clearAll();
                        showToast(_STATE, "Ok");
                    });
#endif

                    _STATE->settingsView.store(settings, std::memory_order_release);
                    settings->show();
                }
            });
            break;

        case RECLOOPButton: {
            if (_STATE->dofastrender.load()) {
                auto orig = _DATA->tracks[trackIndex];
                if (auto res = _DATA->snapShot.add_taskInt([_appState, orig]() {
                    auto folder = tsl::app::getStoragePath("Grainstorm/Recordings");
                    if (folder.empty()) {
                        showToast(_STATE, "Failed to get storage path for recordings.");
                        return;
                    }
                    auto recName = tsl::generateTimestampedName("grainstorm-loop");
                    std::string extension = _STATE->getAudioFormat();
                    std::string fullPath = folder + "/" + recName + extension;

                    FILE *fd = std::fopen(fullPath.c_str(), "wb");
                    if (!fd) {
                        showToast(_STATE, "Failed to open file for writing.");
                        return;
                    }
                    auto w = tsl::Player::setupRecording(_STATE, fd, "Loop Recording ");
                    if (w != nullptr && record_loop(orig, w) == 0 && w->wrapper.size() != 0) {
                        w->wrapper.close();
                        w->finished.store(true, std::memory_order_release);

                        if (_STATE->askName) {
                            tsl::graphics::AlphaPopUp kbd(_STATE);
                            kbd.setTitle("Save As:");
                            kbd.setToken(_STATE->waitNotify.begin_wait());
                            tsl::graphics::AlphaPopUp::InputResult result{};
                            kbd.onCompleteCallback = [&result](const tsl::graphics::AlphaPopUp::InputResult &r) {
                                if (r.confirmed) {
                                    result = r;
                                    return 1;
                                }
                                return 0;
                            };
                            kbd.setText(recName);
                            kbd.init();
                            kbd.addDraw();
                            kbd.addCB();
                            _STATE->waitNotify.wait_for_signal(kbd.token());
                            if (_STATE->destroyRequested.load()) return;
                            kbd.deldraw();
                            kbd.delCB();

                            if (result.confirmed) {
                                std::string trimmed = tsl::trimToValidFilename(result.text);
                                if (!trimmed.empty()) {
                                    std::string newPath = folder + "/" + trimmed + extension;
                                    if (std::rename(fullPath.c_str(), newPath.c_str()) == 0) {
                                        showToast(_STATE, ("Recording saved as: " + newPath).c_str());
                                    } else {
                                        showToast(_STATE, "Rename failed.");
                                    }
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
                        w->wrapper.close();
                        w->finished.store(true, std::memory_order_release);
                        showToast(_STATE, "Failed to start recording.");
                    }
                })) {
                    char text[100];
                    snprintf(text, 100, "%s Loop Recording Queued. Pos %d.", track->name, res + 1);
                    showToast(_STATE, text);
                }
            }
            break;
        }

        case RECORDButton:
            if (!_STATE->player.isrecording.load()) {
                if (_STATE->dofastrender.load()) {
                    _STATE->RecordingQueue.add_task([_appState]() {
                        auto folder = tsl::app::getStoragePath("Grainstorm/Recordings");
                        if (folder.empty()) {
                            showToast(_STATE, "Failed to get storage path for recordings.");
                            return;
                        }
                        auto recName = tsl::generateTimestampedName("grainstorm");
                        std::string extension = _STATE->getAudioFormat();
                        std::string fullPath = folder + "/" + recName + extension;

                        FILE *fd = std::fopen(fullPath.c_str(), "wb");
                        if (!fd) {
                            showToast(_STATE, "Failed to open file for writing.");
                            return;
                        }
                        auto w = tsl::Player::setupRecording(_STATE, fd, "Live Recording ");
                        if (w != nullptr && _STATE->player.record_live_thread(w) != -1) {
                            if (_STATE->askName) {
                                tsl::graphics::AlphaPopUp kbd(_STATE);
                                kbd.setToken(_STATE->waitNotify.begin_wait());
                                kbd.setTitle("Save As:");
                                tsl::graphics::AlphaPopUp::InputResult result{};
                                kbd.onCompleteCallback = [&result](const tsl::graphics::AlphaPopUp::InputResult &r) {
                                    if (r.confirmed) {
                                        result = r;
                                        return 1;
                                    }
                                    return 0;
                                };
                                kbd.setText(recName);
                                kbd.init();
                                kbd.addDraw();
                                kbd.addCB();
                                _STATE->waitNotify.wait_for_signal(kbd.token());
                                if (_STATE->destroyRequested.load()) return;
                                kbd.deldraw();
                                kbd.delCB();

                                if (result.confirmed) {
                                    std::string trimmed = tsl::trimToValidFilename(result.text);
                                    if (!trimmed.empty()) {
                                        std::string newPath = folder + "/" + trimmed + extension;
                                        if (std::rename(fullPath.c_str(), newPath.c_str()) == 0) {
                                            showToast(_STATE, ("Recording saved as: " + newPath).c_str());
                                        } else {
                                            showToast(_STATE, "Rename failed.");
                                        }
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
                }
            } else {
                _STATE->player.recStop();
            }
            break;

#elif defined(__ANDROID__)
        case EDITORSAVE: {
            int32_t s;
            if ((s = _STATE->WorkerQueue.add_taskInt([track]() {
                ATTACH
				jmethodID jmid;
                jmid = env->GetStaticMethodID(tsl::android::activityclass, "recFile", "(IJ)V");
                env->CallStaticVoidMethod(tsl::android::activityclass, jmid, (jint) SAVE_LOOP,
                                          (jlong) track);
                DETACH
            })) > 0) {
                char text[100];
                snprintf(text, 100, "SAVE LOOP Queued. Pos %d.", s + 1);
                showToast(_STATE, text);
            }
        }
        break;

        case RECORDButton:
            if (!_STATE->player.isrecording.load()) {
                if (_STATE->dofastrender.load()) {
                    _STATE->RecordingQueue.add_task([track]() {
                        ATTACH
						jmethodID jmid;
                        jmid = env->GetStaticMethodID(tsl::android::activityclass, "recFile",
                                                      "(IJ)V");
                        env->CallStaticVoidMethod(tsl::android::activityclass, jmid,
                                                  (jint) RECORD_LIVE, (jlong) track);
                        DETACH
                    });
                }
            } else {
                _STATE->player.recStop();
            }
            break;

        case RECLOOPButton: {
            if (_STATE->dofastrender.load()) {
                int32_t res;
                if ((res = _DATA->snapShot.add_taskInt([track]() {
                    ATTACH
					jmethodID jmid;
                    jmid = env->GetStaticMethodID(tsl::android::activityclass, "recFile", "(IJ)V");
                    env->CallStaticVoidMethod(tsl::android::activityclass, jmid, (jint) RECORD_LOOP,
                                              (jlong) track);
                    DETACH
                })) > 0) {
                    char text[100];
                    snprintf(text, 100, "%s Loop Recording Queued. Pos %d.", track->name, res + 1);
                    showToast(_STATE, text);
                }
            }
            break;
        }

        case EJECTBUTTON:
            _STATE->UiTasksQueue.add_task([index = _STATE->active_track.load()]() {
                ATTACH
                        mid = env->GetStaticMethodID(tsl::android::activityclass, "filebrowser", "(I)V");
                env->CallStaticVoidMethod(tsl::android::activityclass, mid, index);
                DETACH
            });
            break;

        case SETTINGSBUTTON:
            _STATE->UiTasksQueue.add_task([]() {
                ATTACH
				jmethodID jmid;
                jmid = env->GetStaticMethodID(tsl::android::activityclass, "invokeSettings", "()V");
                env->CallStaticVoidMethod(tsl::android::activityclass, jmid);
                DETACH
            });
            break;
#endif
#ifdef OS_IOS
        case EJECTBUTTON:
            _STATE->UiTasksQueue.add_task([_STATE, index = _STATE->active_track.load()]() {
                _STATE->openFileBrowser();
            });
            break;
#endif
        case EDITORCOPY:
        case EDITORCUT:
        case EDITORINSERT:
        case EDITORFADEIN:
        case EDITORFADEOUT:
        case EDITORPASTE:
        case EDITORUNDO:
        case EDITORREVERSE:
        case EDITORMAXIMIZE:
        case EDITORINSERTSILENCE:
        case EDITORGRAINENV: {
            int32_t res = _STATE->WorkerQueue.add_taskInt([_STATE, track, event = paramIndex] {
                fadeinout(_STATE, track->index, (ParameterNum) event);
            });
            if (res > 0) {
                char text[100];
                snprintf(text, 100, "%s %s Queued. Pos %d.", track->name,
                         _STATE->parameters[(uint16_t) paramIndex].name, res + 1);
                showToast(_STATE, text);
            }
        }
        break;

        case EDITORREC1:
        case EDITORREC2:
        case EDITORREC3:
        case EDITORREC4: {
            int32_t res = _STATE->RecordingQueue.add_taskInt([event = paramIndex, _appState]() {
                auto w = tsl::Player::setupRecording(_STATE, nullptr);
                std::vector<unsigned char> vec;
                if (w != nullptr && _STATE->player.record_live_thread(w) != -1 &&
                    w->wrapper.size() != 0 && w->wrapper.moveClose(vec)) {
                    auto track = _DATA->tracks[event - EDITORREC1];
                    int32_t res = _DATA->snapShot.add_taskInt(
                        [track, mem = std::move(vec), _appState]() mutable {
                            auto current = track->getAudioCopy();
                            if (current == nullptr) {
                                showToast(_STATE, "Out of memory.");
                                return;
                            }
                            current->numEdits++;
                            current->lastEdit = EDITORINSERT;
                            tsl::RecordingDiff diff;
                            if (current->push<int16_t>(mem, 2, diff)) {
                                auto e = track->loadAudio(current, diff);
                                if (e.poolHandle < tsl::INVALID_POOL_HANDLE) {
                                    _DATA->snapShot.addEvent(e);
                                }

                                _STATE->WorkerQueue.add_task([_STATE, track, current = std::move(current)]mutable {
                                    track->waveform->setup(current);
                                    showToast(_STATE, (std::string("Recording pushed to ") +
                                                       track->name).c_str());
                                });
                            } else showToast(_STATE, "Out of memory.");
                        });
                    if (res > 0) {
                        char text[100];
                        snprintf(text, 100, "%s Load Recording Queued. Pos %d.", track->name,
                                 res + 1);
                        showToast(_STATE, text);
                    }
                }
            });
            if (res > 0) {
                char text[100];
                snprintf(text, 100, "%s Queued. Pos %d.", _STATE->parameters[paramIndex].name, res + 1);
                showToast(_STATE, text);
            }
        }
        break;

        case EDITORLOOP1:
        case EDITORLOOP2:
        case EDITORLOOP3:
        case EDITORLOOP4: {
            auto orig = _DATA->tracks[_STATE->active_track.load()];
            int32_t res = _DATA->snapShot.add_taskInt([_appState, orig, event = paramIndex]() {
                auto w = tsl::Player::setupRecording(_STATE, nullptr, "Loop recording ");
                std::vector<unsigned char> vec;
                if (w != nullptr && record_loop(orig, w) == 0 && w->wrapper.size() != 0 &&
                    w->wrapper.moveClose(vec)) {
                    auto track = _DATA->tracks[event - EDITORLOOP1];
                    auto current = track->getAudioCopy();
                    if (current == nullptr) {
                        showToast(_STATE, "Out of memory.");
                        return;
                    }
                    current->numEdits++;
                    current->lastEdit = EDITORINSERT;
                    tsl::RecordingDiff diff;
                    if (current->push<int16_t>(vec, 2, diff)) {
                        auto e = track->loadAudio(current, diff);
                        if (e.poolHandle < tsl::INVALID_POOL_HANDLE) {
                            _DATA->snapShot.addEvent(e);
                        }
                        {
                            _STATE->WorkerQueue.add_task([_STATE, track, orig, current = std::move(current)] mutable {
                                track->waveform->setup(current);
                                std::string s = std::string("Loop from ") + orig->name + " pushed to " +
                                                track->name + ".";
                                showToast(_STATE, s.c_str());
                            });
                        }
                    } else showToast(_STATE, "Out of memory.");
                }
            });
            if (res > 0) {
                char text[100];
                snprintf(text, 100, "%s %s Queued. Pos %d.", orig->name,
                         _STATE->parameters[paramIndex].name, res + 1);
                showToast(_STATE, text);
            }
        }
        break;

        case POWERButton: {
            if (auto s = _DATA->snapShot.add_taskInt([_appState]() {
                if (!_STATE->player._isplaying.load()) {
                    for (auto t: _DATA->tracks) {
                        t->gainTask.setTarget(-120, 0);
                    }
                    {
                        std::lock_guard lk(_STATE->player);
                        _STATE->player.flush();
                        _STATE->player.play();
                    }
                } else {
                    for (auto t: _DATA->tracks) {
                        t->gainTask.setTarget(0, -120);
                    };
                    _STATE->waitNotify.sleep_for(70);
                    {
                        std::lock_guard lk(_STATE->player);
                        _STATE->player.stop();
                    }
                }
            }) > 0) {
                char text[100];
                snprintf(text, 100, "POWER On/Off Queued. Pos %d.", s + 1);
                showToast(_STATE, text);
            }
        }
        break;

        case MENUBUTTON:
            _STATE->UiTasksQueue.add_task(showTrackSettings,
                                          _DATA->tracks[_STATE->active_track.load()]);
            break;

        case STEPBACK:
        case STEPFORW:
        case STOPButton:
        case PLAYButton:
        case DIRButton:
        case SLOWButton:
        case FASTButton:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                distr->controlAll(paramIndex);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([_appState, distr, event = paramIndex, tindex = trackIndex]() {
                    distr->controlAll(event);

                    if (event == STOPButton || event == PLAYButton)
                        _STATE->toUiThreadQueue.try_push([_STATE] {
                            if (_STATE->parameters[PLAYButton].view->visible_)
                                _STATE->parameters[PLAYButton].view->redraw();
                        });
                });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case SYNCButton:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                distr->sync();
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([distr]() { distr->sync(); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case LFO1SYNC:
        case LFO2SYNC:
        case LFO3SYNC: {
            auto lfo = track->lfos[paramIndex - LFO1SYNC];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->sync();
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([lfo]() { lfo->sync(); });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case LFO1BACKW:
        case LFO2BACKW:
        case LFO3BACKW: {
            auto lfo = track->lfos[paramIndex - LFO1BACKW];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->controlAll(STEPBACK);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([lfo]() { lfo->controlAll(STEPBACK); });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case LFO1STOP:
        case LFO2STOP:
        case LFO3STOP: {
            auto lfo = track->lfos[paramIndex - LFO1STOP];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->controlAll(STOPButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([_appState, lfo]() {
                    lfo->controlAll(STOPButton);
                    _STATE->toUiThreadQueue.try_push([_STATE] {
                        if (_STATE->parameters[LFO1PLAY].view->visible_)
                            _STATE->parameters[LFO1PLAY].view->redraw();
                    });
                });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case LFO1PLAY:
        case LFO2PLAY:
        case LFO3PLAY: {
            auto lfo = track->lfos[paramIndex - LFO1PLAY];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->controlAll(PLAYButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([_appState, lfo]() {
                    lfo->controlAll(PLAYButton);
                    _STATE->toUiThreadQueue.try_push([_STATE] {
                        if (_STATE->parameters[LFO1PLAY].view->visible_)
                            _STATE->parameters[LFO1PLAY].view->redraw();
                    });
                });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case LFO1FORW:
        case LFO2FORW:
        case LFO3FORW: {
            auto lfo = track->lfos[paramIndex - LFO1FORW];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->controlAll(STEPFORW);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([lfo]() { lfo->controlAll(STEPFORW); });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case LFO1DIR:
        case LFO2DIR:
        case LFO3DIR: {
            auto lfo = track->lfos[(paramIndex - LFO1DIR) / LFONUMPARAMS];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->controlAll(DIRButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([lfo]() { lfo->controlAll(DIRButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }
        case LFO1FAST:
        case LFO2FAST:
        case LFO3FAST: {
            auto lfo = track->lfos[paramIndex - LFO1FAST];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->controlAll(FASTButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([lfo]() { lfo->controlAll(FASTButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }
        case LFO1SLOW:
        case LFO2SLOW:
        case LFO3SLOW: {
            auto lfo = track->lfos[paramIndex - LFO1SLOW];
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                lfo->controlAll(SLOWButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([lfo]() { lfo->controlAll(SLOWButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case PP_SLOW:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[9].controlAll(SLOWButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->delaySyncTargets[9].controlAll(SLOWButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case PP_FAST:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[9].controlAll(FASTButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->delaySyncTargets[9].controlAll(FASTButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case PP_SYNC:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[9].sync();
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->delaySyncTargets[9].sync(); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case DELAY_SLOW:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[0].controlAll(SLOWButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->delaySyncTargets[0].controlAll(SLOWButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case DELAY_FAST:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[0].controlAll(FASTButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->delaySyncTargets[0].controlAll(FASTButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case DELAY_SYNC:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[0].sync();
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->delaySyncTargets[0].sync(); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case MDELAY1_SLOW:
        case MDELAY2_SLOW:
        case MDELAY3_SLOW:
        case MDELAY4_SLOW:
        case MDELAY5_SLOW:
        case MDELAY6_SLOW:
        case MDELAY7_SLOW:
        case MDELAY8_SLOW: {
            auto mdelayindex = paramIndex - MDELAY1_SLOW;
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[1 + mdelayindex].controlAll(SLOWButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([track, mdelayindex]() {
                    track->delaySyncTargets[1 + mdelayindex].controlAll(SLOWButton);
                });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case MDELAY1_FAST:
        case MDELAY2_FAST:
        case MDELAY3_FAST:
        case MDELAY4_FAST:
        case MDELAY5_FAST:
        case MDELAY6_FAST:
        case MDELAY7_FAST:
        case MDELAY8_FAST: {
            auto mdelayindex = paramIndex - MDELAY1_FAST;
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[1 + mdelayindex].controlAll(FASTButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([track, mdelayindex]() {
                    track->delaySyncTargets[1 + mdelayindex].controlAll(FASTButton);
                });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }
        case MDELAY1_SYNC:
        case MDELAY2_SYNC:
        case MDELAY3_SYNC:
        case MDELAY4_SYNC:
        case MDELAY5_SYNC:
        case MDELAY6_SYNC:
        case MDELAY7_SYNC:
        case MDELAY8_SYNC: {
            auto mdelayindex = paramIndex - MDELAY1_SYNC;
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->delaySyncTargets[1 + mdelayindex].sync();
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([track, mdelayindex]() {
                    track->delaySyncTargets[1 + mdelayindex].sync();
                });
            } else showToast(_STATE, "Synth has to be running.");
            break;
        }

        case GRAINSEQSYNC:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->grainsequencer.sync();
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([track]() { track->grainsequencer.sync(); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case GRAINSEQBACKW:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->grainsequencer.controlAll(STEPBACK);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->grainsequencer.controlAll(STEPBACK); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case GRAINSEQFORW:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->grainsequencer.controlAll(STEPFORW);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->grainsequencer.controlAll(STEPFORW); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case GRAINSEQDIR:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->grainsequencer.controlAll(DIRButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->grainsequencer.controlAll(DIRButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case GRAINSEQFAST:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->grainsequencer.controlAll(FASTButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->grainsequencer.controlAll(FASTButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case GRAINSEQSLOW:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->grainsequencer.controlAll(SLOWButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->grainsequencer.controlAll(SLOWButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case BPMSYNCSYNC:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->bpmSyncer.sync();
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push([track]() { track->bpmSyncer.sync(); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case BPMSYNCFAST:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->bpmSyncer.controlAll(FASTButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->bpmSyncer.controlAll(FASTButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case BPMSYNCSLOW:
            if (_DATA->inputdisabled.load(std::memory_order_acquire))
                track->bpmSyncer.controlAll(SLOWButton);
            else if (_STATE->player.isPlaying()) {
                _DATA->toAudioThreadQueue.try_push(
                    [track]() { track->bpmSyncer.controlAll(SLOWButton); });
            } else showToast(_STATE, "Synth has to be running.");
            break;

        case LOADSEQUENCE1:
        case LOADSEQUENCE2:
        case LOADSEQUENCE3:
        case LOADSEQUENCE4:
            track->grainsequencer.pushLoad(DISTANCE(LOADSEQUENCE1, paramIndex));
            break;

        case SAVESEQUENCE1:
        case SAVESEQUENCE2:
        case SAVESEQUENCE3:
        case SAVESEQUENCE4:
            track->grainsequencer.pushSave(DISTANCE(SAVESEQUENCE1, paramIndex));
            break;
        case LOOPLOAD0:
        case LOOPLOAD1:
        case LOOPLOAD2:
        case LOOPLOAD3:
        case LOOPLOAD4:
        case LOOPLOAD5:
        case LOOPLOAD6:
        case LOOPLOAD7: {
            auto rec = track->filebuffer.load();
            if (rec == nullptr || rec->off == 0) {
                showToast(_STATE, "Empty Track.");
                return;
            }
            auto tindex = track->index;
            tsl::StackVector<TRACK *, 4> vec{};
            for (auto &t: _DATA->tracks) {
                if (t->source == track && t->filebuffer.load() != nullptr && _STATE->params[t->index][POWERTRACK].load()
                    == 1.0) {
                    vec.push_back(t);
                }
            };
            int32_t act = DISTANCE(LOOPLOAD0, paramIndex);
            if (_STATE->player.isPlaying()) {
                if (!vec.empty()) {
                    for (auto t: vec)t->gainTask.setTarget(tsl::gaintask::GainDown, false);
                    _STATE->waitNotify.sleep_for(tsl::gainTaskFadeMs);
                }


                auto token = _STATE->waitNotify.begin_wait();
                auto vecPtr = std::make_shared<tsl::StackVector<TRACK*, 4>>(std::move(vec));
                _DATA->toAudioThreadQueue.try_push([_appState, tindex, act, token, vecPtr]() {
                    auto t = _DATA->tracks[tindex];
                    auto rec = t->filebuffer.load();
                    if (rec == nullptr || rec->off == 0) {
                        showToast(_STATE, "Empty Track.");
                        return;
                    }
                    rec->loadPositions(act);
                    rec->dorendering.store(true);
                    auto state = rec->state.load();
                    if (state) {
                        auto dist = state->off_stop - state->off_start;
                        t->play_dur.store(dist > 0 ? dist : 0);
                        t->computeLoopTime();
                    }
                    for (auto t: *vecPtr)t->gainTask.setTarget(tsl::gaintask::GainUp, false);
                    _STATE->waitNotify.complete(token);
                });
            } else {
                rec->loadPositions(act);
                rec->dorendering.store(true);
                auto state = rec->state.load();
                if (state) {
                    auto dist = state->off_stop - state->off_start;
                    track->play_dur.store(dist > 0 ? dist : 0);
                    track->computeLoopTime();
                }
                for (auto t: vec)t->gainTask.setTarget(tsl::gaintask::GainUp, false);
            };
            break;
        }

        case LOOPSAVE0:
        case LOOPSAVE1:
        case LOOPSAVE2:
        case LOOPSAVE3:
        case LOOPSAVE4:
        case LOOPSAVE5:
        case LOOPSAVE6:
        case LOOPSAVE7: {
            auto t = _DATA->tracks[track->index];
            auto rec = t->filebuffer.load();
            if (rec == nullptr || rec->off == 0) {
                showToast(_STATE, "Empty Track.");
                return;
            }
            int32_t act = DISTANCE(LOOPSAVE0, paramIndex);
            rec->savePositions(act);
            break;
        }

#if defined(PLUGIN_MODE) || defined(OS_IOS)
        case LOOPSYNCDAWTRANSPORT: {
            auto x = _STATE->params[track->index][LOOPSYNCDAWTRANSPORT].load();
            auto des = x == 1.0 ? 0 : 1.0;
            while (!_STATE->params[track->index][LOOPSYNCDAWTRANSPORT].compare_exchange_weak(x, des,
                std::memory_order_release, std::memory_order_relaxed));
            track->computeLoopTime();
            _STATE->toUiThreadQueue.try_push([_STATE, track]() {
                auto x = _STATE->params[track->index][LOOPSYNCDAWTRANSPORT].load();
                if (_STATE->active_track.load() == track->index) {
                    auto top = _DATA->views.controlpanel;
                    _STATE->queue_draw.lock();
                    auto ltop = _DATA->views.lfocontrolpanel;
                    if (ltop->visible_) {
                        ltop->redrawDirect();
                        ltop->addRecursiveDraw();
                    }
                }
            });
            break;
        }
#endif

        default: {
            if (flags & tsl::parameters::Event::ToWorkerThread) {
                if (auto s = _DATA->snapShot.add_taskInt([_STATE, e = *this]() mutable {
                                 e.apply(_STATE, tsl::parameters::FromHistory);
                             })
                             > 0) {
                    char text[100];
                    int pos = 0;
                    toString(_STATE, pos, text, 100, true);
                    pos = std::min(pos, 100);

                    pos = snprintf(text + pos, 100 - pos, " queued. Pos %d.", s + 1);
                    if (!(flags & tsl::parameters::Event::FromDaw))
                        showToast(_STATE, text);
                }
                return;
            } else if (flags & tsl::parameters::Event::ToAudioThread) {
                if (_STATE->player.isPlaying()) {
                    _DATA->toAudioThreadQueue.try_push(
                        [_STATE, e = *this]() mutable {
                            e.apply(_STATE, tsl::parameters::None);
                        });
                } else showToast(_STATE, "Synth has to be running.");
                return;
            } else {
                apply(_STATE, from);
            }
            break;
        }
    }
}
