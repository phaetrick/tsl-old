
// Created by pr on 15.11.17.


#include "logger.h"
#include <utility>
#include "DecoderAndroid.h"


#include "DecoderView.h"
#include "button.h"
#include "Presets/preset.h"
#include "infopanel.h"
#include "waveform.h"
#include "grainstorm.h"
#include "view.h"
#include "infopanel.h"
#include "tools.h"
#include "track.h"
#include "tools/queuetsl.h"
#include "colours.h"
#include <Input.h>
#include <keyboard.h>
#include "app.h"

#include <cstddef>
#include <unistd.h>

using namespace tsl::graphics;

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include<libswresample/swresample.h>
#include <libavutil/opt.h>
}


#define RAW_OUT_ON_PLANAR true

#ifdef __cplusplus
#define REINTERPRET_CAST(type, variable) reinterpret_cast<type>(variable)
#define STATIC_CAST(type, variable) static_cast<type>(variable)
#else
#define C_CAST(type, variable) ((type)variable)
#define REINTERPRET_CAST(type, variable) C_CAST(type, variable)
#define STATIC_CAST(type, variable) C_CAST(type, variable)
#endif


#define INSUFFICIENT_MEMORY -1000


static void decoding_stop(struct android_app *app, InputEvent &event);


int32_t printError(const char *prefix, int errorCode) {
    if (errorCode == 0) {
        return 0;
    } else {
        const size_t bufsize = 64;
        char buf[bufsize];

        if (av_strerror(errorCode, buf, bufsize) != 0) {
            strcpy(buf, "UNKNOWN ERROR");
        }
        LOGE("%s (%d: %s)", prefix, errorCode, buf);

        return errorCode;
    }
}


int32_t findAudioStream(const AVFormatContext *formatCtx) {
    int32_t audioStreamIndex = -1;
    for (size_t i = 0; i < formatCtx->nb_streams; ++i) {
        // Use the first audio stream we can find.
        // NOTE: There may be more than one, depending on the file.
        if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
            break;
        }
    }
    return audioStreamIndex;
}

void
printStreamInformation(const AVCodec *codec, const AVCodecContext *codecCtx,
                       int32_t audioStreamIndex) {
    LOGD("Codec: %s", codec->long_name);
    if (codec->sample_fmts != nullptr) {
        LOGD("Supported sample formats: ");
        for (int32_t i = 0; codec->sample_fmts[i] != -1; ++i) {
            LOGD("%s", av_get_sample_fmt_name(codec->sample_fmts[i]));
            //if (codec->sample_fmts[i + 1] != -1) {
            //    LOGD(", ");
            //}
        }
    }
    //LOGD("---------");
    LOGD("Stream:        %7d", audioStreamIndex);
    LOGD("Sample Format: %7s", av_get_sample_fmt_name(codecCtx->sample_fmt));
    LOGD("Sample Rate:   %7d", codecCtx->sample_rate);
    LOGD("Sample Size:   %7d", av_get_bytes_per_sample(codecCtx->sample_fmt));
    LOGD("Channels:      %7d", codecCtx->channels);
    //LOGD("Float Output:  %7s",
    //     !RAW_OUT_ON_PLANAR || av_sample_fmt_is_planar(codecCtx->sample_fmt) ? "yes" : "no");
}

/**
 * Extract a single sample and convert to float.
 */
float getSample(const AVCodecContext *codecCtx, uint8_t *buffer, int32_t sampleIndex) {
    int64_t val = 0;
    float ret = 0;
    int32_t sampleSize = av_get_bytes_per_sample(codecCtx->sample_fmt);
    switch (sampleSize) {
        case 1:
            // 8bit samples are always unsigned
            val = REINTERPRET_CAST(uint8_t*, buffer)[sampleIndex];
            // make signed
            val -= 127;
            break;

        case 2:
            val = REINTERPRET_CAST(int16_t*, buffer)[sampleIndex];
            break;

        case 4:
            val = REINTERPRET_CAST(int32_t*, buffer)[sampleIndex];
            break;

        case 8:
            val = REINTERPRET_CAST(int64_t*, buffer)[sampleIndex];
            break;

        default:
            LOGE("Invalid sample size %d.", sampleSize);
            return 0;
    }

    switch (codecCtx->sample_fmt) {
        case AV_SAMPLE_FMT_U8:
        case AV_SAMPLE_FMT_S16:
        case AV_SAMPLE_FMT_S32:
        case AV_SAMPLE_FMT_U8P:
        case AV_SAMPLE_FMT_S16P:
        case AV_SAMPLE_FMT_S32P:
            // integer => Scale to [-1, 1] and convert to float.
            ret = val / STATIC_CAST(float, ((1 << (sampleSize * 8 - 1)) - 1));
            break;

        case AV_SAMPLE_FMT_FLT:
        case AV_SAMPLE_FMT_FLTP:
            // float => reinterpret
            ret = *REINTERPRET_CAST(float*, &val);
            break;

        case AV_SAMPLE_FMT_DBL:
        case AV_SAMPLE_FMT_DBLP:
            // double => reinterpret and then static cast down
            ret = STATIC_CAST(float, *REINTERPRET_CAST(double * , &val));
            break;

        default:
            LOGE("Invalid sample format %s.",
                 av_get_sample_fmt_name(codecCtx->sample_fmt));
            return 0;
    }

    return ret;
    // ...
}

int32_t handleFrame(tsl::Decdata &decdata, const AVFrame *resampled) {
    int32_t channels = decdata.channels;
    int32_t processed = resampled->nb_samples;
    if (processed == 0)
        return 0;
    auto offset = decdata.offset.load();
    auto new_size = offset + processed;
    std::lock_guard lk(decdata.mtx);
    for (int32_t i = 0; i < channels; i++) {
        try {
            decdata.buffer[i].resize(new_size, 0);
        } catch (std::bad_alloc&) {
            LOGE("Out of memory.");
            return INSUFFICIENT_MEMORY;
        }
        memcpy(decdata.buffer[i].data() + offset, resampled->extended_data[i],
               (size_t) processed * sizeof(short));
    }
    decdata.offset.store(offset + processed);
    return 0;
}

int
receiveAndHandle(tsl::Decdata &decdata, AVCodecContext *codecCtx, SwrContext *swrCtx, AVFrame *frame,
                 AVFrame *resampled) {
    int32_t err = 0;
    while ((err = avcodec_receive_frame(codecCtx, frame)) == 0) {
        if ((err = swr_convert_frame(swrCtx, resampled, frame)) < 0) {
            printError("Error resampling: ", err);
        } else {
            if ((err = handleFrame(decdata, resampled)) != 0)
                return err;
        }
        av_frame_unref(frame);
    }
    return err;
}

void
drainDecoder(tsl::Decdata &decdata, AVCodecContext *codecCtx, SwrContext *swrCtx, AVFrame *frame,
             AVFrame *resampled) {
    int32_t err = 0;
    if ((err = avcodec_send_packet(codecCtx, nullptr)) == 0) {
        err = receiveAndHandle(decdata, codecCtx, swrCtx, frame, resampled);
        if (err != AVERROR(EAGAIN) && err != AVERROR_EOF)
            printError("Receive error.", err);
    } else {
        printError("Send error.", err);
    }
}


void showerror(TRACK *t, int32_t errorCode) {
    const size_t bufsize = 64;
    char buf[bufsize];
    if (av_strerror(errorCode, buf, bufsize) != 0) {
        strcpy(buf, "Unknown Error.");
    }
    std::string str = t->name;
    str.append(": Audio Import Fail. ");
    str.append(buf);
    showToast(t->_appState, str.data());
}

#ifdef __ANDROID__


static void getFilePathFromUri(std::string &path, std::string &res) {
    ATTACH
    mid = env->GetStaticMethodID(tsl::android::activityclass, "getDecodingPath",
                                 "(Ljava/lang/String;)Ljava/lang/String;");
    if (mid == nullptr)
        return;
    auto jstring1 = (jstring) env->CallStaticObjectMethod(tsl::android::activityclass, mid,
                                                          env->NewStringUTF(path.data()));
    if (jstring1 == nullptr)
        return;
    jboolean isCopy;
    const char *tmp = env->GetStringUTFChars(jstring1, &isCopy);
    if (tmp == nullptr)
        return;
    res = tmp;
    env->ReleaseStringUTFChars(jstring1, tmp);
    DETACH
}

#endif

std::shared_ptr<tsl::Recording> dec(TRACK *track, std::string name, long gap, bool showMsg) {
    auto _appState = track->_appState;
    std::string file;
#ifdef __ANDROID__
    while (!track->_STATE->initdone.load())usleep(10 * 1000);
    getFilePathFromUri(name, file);
#else
    file = name;
#endif
    if (file.empty()) {
        return nullptr;
    }
    int32_t fd = 0;
    if (file.find("pipe:", 0, 5) != std::string::npos) {
        fd = std::stoi(file.substr(5));
    }

    auto decViewS = _DATA->views.decoderView;
    auto decoderView = decViewS ? dynamic_cast<tsl::graphics::DecoderView*>(decViewS.get()) : nullptr;
    tsl::Decdata localDecData(_appState);
    tsl::Decdata& decData = decoderView ? decoderView->decData : localDecData;
    decData.channels = _STATE->channels;
    decData.fileName = name;
    decData.sr = _STATE->sr;
    decData.off = gap;
    decData.offset.store(0);



    int32_t ret = 0;

    int32_t channels = _STATE->channels;
    AVFormatContext *formatCtx = nullptr;
    SwrContext *swrCtx = nullptr;
    AVDictionaryEntry *tag = nullptr;
    if ((ret = avformat_open_input(&formatCtx, file.c_str(), nullptr, 0)) != 0) {
        printError("Error opening file.", ret);
        showerror(track, ret);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }

    avformat_find_stream_info(formatCtx, nullptr);


    while ((tag = av_dict_get(formatCtx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX)))
        LOGD("%s=%s", tag->key, tag->value);


    int32_t audioStreamIndex = findAudioStream(formatCtx);
    if (audioStreamIndex == -1) {
        // No audio stream was found.
        LOGE("None of the available %d streams are audio streams.", formatCtx->nb_streams);
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        showerror(track, AVERROR_DECODER_NOT_FOUND);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }

    const AVCodec *codec = avcodec_find_decoder(
            formatCtx->streams[audioStreamIndex]->codecpar->codec_id);
    if (codec == nullptr) {
        // Decoder not found.
        avformat_close_input(&formatCtx);
        LOGE("Decoder not found. The codec is not supported.");
        showerror(track, AVERROR_DECODER_NOT_FOUND);
        if (fd)
            close(fd);
        return nullptr;
    }
    AVCodecContext *codecCtx = avcodec_alloc_context3(codec);
    if (codecCtx == nullptr) {
        // Something went wrong. Cleaning up...
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);

        LOGE("Could not allocate a decoding context.");
        showerror(track, AVERROR_UNKNOWN);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }

    // Fill the codecCtx with the parameters of the codec used in the read file.
    if ((ret = avcodec_parameters_to_context(codecCtx,
                                             formatCtx->streams[audioStreamIndex]->codecpar)) !=
        0) {
        // Something went wrong. Cleaning up...
        avcodec_close(codecCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);

        printError("Error setting codec context parameters.", ret);
        showerror(track, ret);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }

    codecCtx->request_sample_fmt = av_get_alt_sample_fmt(codecCtx->sample_fmt,
                                                         0);

    if ((ret = avcodec_open2(codecCtx, codec, nullptr)) != 0) {
        avcodec_close(codecCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        showerror(track, ret);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }

    printStreamInformation(codec, codecCtx, audioStreamIndex);

    if (codecCtx->channel_layout == 0)
        codecCtx->channel_layout = (uint64_t) av_get_default_channel_layout(
                codecCtx->channels);
    int64_t ch_layout = av_get_default_channel_layout(channels);

    swrCtx = swr_alloc_set_opts(nullptr,
                                ch_layout,
                                AV_SAMPLE_FMT_S16P,
                                _STATE->sr,
                                codecCtx->channel_layout,
                                codecCtx->sample_fmt,
                                codecCtx->sample_rate,
                                0, nullptr);
    if (!(swrCtx)) {
        LOGE("Unable to allocate resampler context.");
        avcodec_close(codecCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        showerror(track, AVERROR_UNKNOWN);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }

    // Open the resampler
    if ((ret = swr_init(swrCtx)) < 0) {
        printError("Unable to init resampler context.", ret);
        swr_free(&swrCtx);
        avcodec_close(codecCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        showerror(track, ret);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }


    AVFrame *frame = nullptr;
    AVFrame *resampled = nullptr;
    if ((frame = av_frame_alloc()) == nullptr) {
        avcodec_close(codecCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        swr_free(&swrCtx);
        showerror(track, AVERROR_UNKNOWN);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }
    if ((resampled = av_frame_alloc()) == nullptr) {
        avcodec_close(codecCtx);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
        avformat_free_context(formatCtx);
        swr_free(&swrCtx);
        av_frame_free(&frame);
        showerror(track, AVERROR_UNKNOWN);
        if (fd)
            close(fd);
        if (file.find("grainstormtmpdecoding") != std::string::npos) {
            std::remove(file.data() + 5);
        }
        return nullptr;
    }

    _DATA->decodingStop = false;

    AVPacket packet;
    av_init_packet(&packet);

    auto info = _DATA->views.infopanel;

    bool progressupdate = false;
    double dursecs = 1.;
    if (decoderView && info) {
        info->dec_offset = 0;
        info->bytes = 0;
        if (formatCtx->duration != AV_NOPTS_VALUE) {
            progressupdate = true;
            dursecs = formatCtx->duration / (double) AV_TIME_BASE;
            if (dursecs > 420)
                dursecs = 420;
        } else {
            info->progress = -1;
        }
        info->text.store(track->name);
        info->text2.store(" DECODING");
        info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);
        decoderView->addCB();
        decoderView->addDraw();
    }

    while (!_DATA->decodingStop.load() && decData.offset < decData.off &&
           (ret = av_read_frame(formatCtx, &packet)) >= 0) {

        resampled->channel_layout = (uint64_t) ch_layout;
        resampled->sample_rate = _STATE->sr;
        resampled->format = AV_SAMPLE_FMT_S16P;

        if (ret != 0) {
            // Something went wrong.
            printError("Read error.", ret);
            continue; // Don't return, so we can clean up nicely.
        }
        if (packet.stream_index != audioStreamIndex) {
            // Free the buffers used by the frame and reset all fields.
            av_packet_unref(&packet);
            continue;
        }
        if ((ret = avcodec_send_packet(codecCtx,
                                       &packet)) == 0) {
            // The packet was sent successfully. We don't need it anymore.
            // => Free the buffers used by the frame and reset all fields.
            av_packet_unref(&packet);
        } else {
            // Something went wrong.
            // EAGAIN is technically no error here but if it occurs we would need to buffer
            // the packet and send it again after receiving more frames. Thus we handle it as an error here.
            av_packet_unref(&packet);
            printError("Send error.", ret);
            //continue; // Don't return, so we can clean up nicely.
        }
        if ((ret = receiveAndHandle(decData, codecCtx,
                                    swrCtx, frame,
                                    resampled)) !=
            AVERROR(EAGAIN)) {
            // Not EAGAIN => Something went wrong.
            printError("Receive error.", ret);
            if (ret == INSUFFICIENT_MEMORY)
                break;
            else
                continue; // Don't return, so we can clean up nicely.
        }

        av_frame_unref(resampled);
        av_frame_unref(frame);
        // ...
        if (decoderView && info) {
            if (progressupdate)
                info->progress.store(decData.offset / (double) _STATE->sr / dursecs);
            info->dec_offset.store(static_cast<long>(decData.offset.load()));
            info->bytes.store(static_cast<long>(decData.offset.load()) * _STATE->channels * sizeof(short));
        }

    }

    std::string message = track->name;
    message.append(" Audio Import ");
    if (decData.offset >= _STATE->sr * 420)
        message.append("(7 Minutes Max Reached.) ");


    drainDecoder(decData, codecCtx, swrCtx, frame,
                 resampled);

    // Free all data used by the frame.
    av_frame_free(&frame);
    av_frame_free(&resampled);

    // Close the context and free all data associated to it, but not the context itself.
    avcodec_close(codecCtx);

    // Free the context itself.
    avcodec_free_context(&codecCtx);

    // We are done here. Close the input.
    avformat_close_input(&formatCtx);
    avformat_free_context(formatCtx);

    swr_free(&swrCtx);

    if (decoderView) {
        decoderView->delCB();
        decoderView->deldraw();
        if (info)
            info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);
    }

    std::shared_ptr<tsl::Recording> rec;
    if (!decData.buffer[0].empty()) {
        {
            /* The Recording constructor MOVES these buffers out, and
               DecoderView::render may still be reading them for one more frame
               -- deldraw() above only queues the removal now. */
            std::lock_guard lk(decData.mtx);
            rec = std::make_shared<tsl::Recording>(_appState, decData.buffer, _STATE->channels, name);
        }
        rec->sr = _STATE->sr;
        if (rec->off >= gap)
            message.append("(7 Minutes Max Reached.) ");
        message.append("Finished.");
    } else
        message.append("No samples decoded.");
    /* Stored on BOTH paths, not just success: with showMsg false the caller is
       the only thing that can report, and it needs the failure text too. */
    if (rec)
        rec->statusMessage = message;

    if (fd)
        close(fd);
    if (file.find("grainstormtmpdecoding") != std::string::npos) {
        std::remove(file.data() + 5);
    }
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        ret = 0;
    if (showMsg)
        showToast(_STATE, message.data());
    return rec;
}

#ifdef __ANDROID__

jstring java_ffmpegerrortostring(JNIEnv *env, jclass obj, int32_t errorCode) {
    if (errorCode == 0) {
        return 0;
    } else {
        const size_t bufsize = 64;
        char buf[bufsize];

        if (av_strerror(errorCode, buf, bufsize) != 0) {
            strcpy(buf, "UNKNOWN ERROR");
        }

        return env->NewStringUTF(buf);
    }
}

jint filebrowsercallback(JNIEnv *env, jclass thiz, jstring name) {
    if (name == nullptr)
        return -1;
    jboolean isCopy;
    const char *_name = env->GetStringUTFChars(name, &isCopy);
    std::string _filename = _name;
    env->ReleaseStringUTFChars(name, _name);
    tsl::AppState *_appState = __STATE;
    if(_appState == nullptr || _STATE->initdone.load(std::memory_order_relaxed) == false)return 1;

#else
//#include <Windows.h>

void usleep(__int64 usec)
{
}
    int32_t filebrowsercallback(tsl::AppState *_appState, const std::string & filename1) {
        std::string _filename = filename1;
#endif
    if (_filename.empty())
        return -1;
    TRACK *track = _DATA->tracks[_STATE->active_track.load()];
    int32_t s;
    if ((s = _DATA->snapShot.add_taskInt([_STATE, track, _filename]() {
        while (!_STATE->initdone.load())usleep(10 * 1000);
        /* showMsg false: dec() would announce "Finished." the moment the last
           frame is decoded, but the import is not done then -- loadAudio still
           has to archive whatever the track was holding, which is the slow
           part. The toast moves to the end of that task instead. */
        auto rec = dec(track, _filename, _STATE->sr * 420, false);
        if (rec && rec->off > 0) {
            // No one waits on this task; the old wake_thread here only poisoned the slot.
            _DATA->snapShot.add_task([_STATE, track, rec]() mutable {
                auto e = track->loadAudio(rec);
                if (e.poolHandle < tsl::INVALID_POOL_HANDLE) {
                    _DATA->snapShot.addEvent(e);
                }
                if (!rec->statusMessage.empty())
                    showToast(_STATE, rec->statusMessage.c_str());
            });
            track->waveform->setup(rec);
        } else if (rec && !rec->statusMessage.empty())
            showToast(_STATE, rec->statusMessage.c_str());
        else
            showToast(_STATE, "Failed to decode audio.");
    })) > 0) {
        char text[100];
        snprintf(text, 100, "%s Import Audio Queued. Pos %d .", track->name, s + 1);
        showToast(_STATE, text);
    }
    return 0;
}
