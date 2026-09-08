#include "logger.h"
#include <cstdlib>
#include <cstring>
#include "defines.h"
#include "file.h"
#include <app.h>

#define SPEAKER_FRONT_LEFT             0x1
#define SPEAKER_FRONT_RIGHT            0x2
#define SPEAKER_FRONT_CENTER           0x4
#define SPEAKER_LOW_FREQUENCY          0x8
#define SPEAKER_BACK_LEFT              0x10
#define SPEAKER_BACK_RIGHT             0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER   0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER  0x80
#define SPEAKER_BACK_CENTER            0x100
#define SPEAKER_SIDE_LEFT              0x200
#define SPEAKER_SIDE_RIGHT             0x400
#define SPEAKER_TOP_CENTER             0x800
#define SPEAKER_TOP_FRONT_LEFT         0x1000
#define SPEAKER_TOP_FRONT_CENTER       0x2000
#define SPEAKER_TOP_FRONT_RIGHT        0x4000
#define SPEAKER_TOP_BACK_LEFT          0x8000
#define SPEAKER_TOP_BACK_CENTER        0x10000
#define SPEAKER_TOP_BACK_RIGHT         0x20000
#define SPEAKER_RESERVED               0x80000000

struct wav_h {
    char ChunkID[4];
    int32_t ChunkSize;
    char Format[4];

    char Subchunk1ID[4];
    int32_t Subchunk1Size;
    int16_t AudioFormat;
    int16_t NumChannels;
    int32_t SampleRate;
    int32_t ByteRate;
    int16_t BlockAlign;
    int16_t BitsPerSample;

    char Subchunk2ID[4];
    int32_t Subchunk2Size;
};


static size_t waf_write(WAF *waf, void *pointer, size_t samples);

static int waf_close(WAF *waf);

static void waf_destroy(WAF *waf);

static int write_wav_header(FILE *file, long samples, int sr, int16_t channels, int16_t bitdepth,
                            int16_t audioformat);


static size_t waf_write(WAF *waf, void *pointer, size_t samples) {
    if (samples &&
        (fseek(waf->f, sizeof(struct wav_h) + waf->offset * waf->bytedepth, SEEK_SET)) != 0) {
        LOGE("Error: wav_write: fseek (%d).", ferror(waf->f));
        waf->close(waf);
        return 0;
    }
    if (samples && fwrite(pointer, samples * waf->bytedepth, 1, waf->f) !=
                   1) {
        LOGE("Error: waf_write: fwrite(%d)", ferror(waf->f));
        return 0;
    }
    waf->offset += samples;
    return samples;
}

static int waf_close(WAF *waf) {
    if (waf->f == nullptr)
        return -1;
    rewind(waf->f);
    if ((write_wav_header(waf->f, waf->offset, waf->sr, waf->channels, waf->bitdepth,
                          waf->audioformat)) != sizeof(struct wav_h)) {
        LOGE("Error writing wav header.");
        fclose(waf->f);
        waf->f = nullptr;
        return 1;
    }
    fclose(waf->f);
    waf->f = nullptr;
    return 0;
}

WAF *waf_open(const char *path, int sr, int16_t channels, int16_t bitdepth, int16_t audioformat) {
    //LOGE("%d %d %d %d", sr, channels, bitdepth, audioformat);
    FILE *f = fopen(path, "wb");
    if (f == nullptr)
        return nullptr;
    struct wav_h w;
    if (fwrite(&w, sizeof(struct wav_h), 1, f) != 1) {
        LOGE("Unable to write wav header.");
        return nullptr;
    }
    WAF *waf = new WAF;
    waf->f = f;
    waf->sr = sr;
    waf->channels = channels;
    waf->bitdepth = bitdepth;
    waf->bytedepth = channels * bitdepth / (short int) 8;
    waf->audioformat = audioformat;
    waf->write = waf_write;
    waf->close = waf_close;
    return waf;
}


WAF *waf_open_fd(int fd, int sr, int16_t channels, int16_t bitdepth, int16_t audioformat) {
    //LOGE("%d %d %d %d", sr, channels, bitdepth, audioformat);
    FILE *f = nullptr;
    f = fdopen(fd, "wb");
    if (f == nullptr)
        return nullptr;
    struct wav_h w{};
    if (fwrite(&w, sizeof(struct wav_h), 1, f) != 1) {
        LOGE("Unable to write wav header.");
        return nullptr;
    }
    WAF *waf = new WAF;
    waf->f = f;
    waf->sr = sr;
    waf->channels = channels;
    waf->bitdepth = bitdepth;
    waf->bytedepth = channels * bitdepth / (short int) 8;
    waf->audioformat = audioformat;
    waf->write = waf_write;
    waf->close = waf_close;
    return waf;
}

static size_t waf_write_buffer(WAF *waf, void *pointer, size_t samples) {
    if (samples) {
        auto *in = static_cast<short *>(pointer);
        for (int smpl = 0; smpl < samples; smpl++) {
            waf->buf[0].push_back(in[smpl * 2]);
            waf->buf[1].push_back(in[smpl * 2 + 1]);

        }
    }
    waf->offset += samples;
    return samples;
}

WAF *waf_open_buffer(int sr, uint8_t channels){
    WAF *waf = new WAF;
    waf->sr = sr;
    waf->channels = channels;
    waf->write = waf_write_buffer;
    waf->close = nullptr;
    return waf;
};



int write_wav_header(FILE *file, long samples, int sr, int16_t channels, int16_t bitdepth,
                     int16_t audioformat) {

/* Create WAV file header */

    struct wav_h wav_header{};

    wav_header.ChunkID[0] = 'R';
    wav_header.ChunkID[1] = 'I';
    wav_header.ChunkID[2] = 'F';
    wav_header.ChunkID[3] = 'F';
    wav_header.ChunkSize = 36 + samples * channels * bitdepth / (short int) 8;

    wav_header.Format[0] = 'W';
    wav_header.Format[1] = 'A';
    wav_header.Format[2] = 'V';
    wav_header.Format[3] = 'E';

    wav_header.Subchunk1ID[0] = 'f';
    wav_header.Subchunk1ID[1] = 'm';
    wav_header.Subchunk1ID[2] = 't';
    wav_header.Subchunk1ID[3] = ' ';

    wav_header.Subchunk1Size = 16;
    wav_header.AudioFormat = audioformat;
    wav_header.NumChannels = channels;
    wav_header.SampleRate = sr;
    wav_header.ByteRate = sr * channels *
                          bitdepth / 8; /* sample rate * number of channels * bits per sample / 8 */
    wav_header.BlockAlign =
            channels * bitdepth / (short int) 8; /* number of channels / bits per sample / 8 */
    wav_header.BitsPerSample = bitdepth;

    wav_header.Subchunk2ID[0] = 'd';
    wav_header.Subchunk2ID[1] = 'a';
    wav_header.Subchunk2ID[2] = 't';
    wav_header.Subchunk2ID[3] = 'a';
    wav_header.Subchunk2Size = samples * channels * bitdepth /
                               8; /* frame count * number of channels * bits per sample / 8 */

    if (fwrite(&wav_header, sizeof(struct wav_h), 1, file) != 1)
        return 0;
    else
        return sizeof(struct wav_h);
}
#include <lame.h>


static size_t waf_write_mp3(WAF *waf, void *pointer, size_t samples) {
    waf->mp3buffer.resize(1.25 * (samples < 48000 ? 48000 : samples) + 7200);
    int encoded = lame_encode_buffer(waf->gfp, (const short *) pointer, nullptr,
                                     (const int) samples, waf->mp3buffer.data(),
                                     waf->mp3buffer.size());
    if (encoded && fwrite(waf->mp3buffer.data(), (size_t) encoded, 1, waf->f) != 1) {
        LOGE("Write error. %d %d %d", (int) waf->mp3buffer.size(), encoded, samples);
        return 0;
    }
    waf->offset += samples;
    return samples;
}

static size_t waf_write_mp3_interleaved(WAF *waf, void *pointer, size_t samples) {
    waf->mp3buffer.resize(1.25 * (samples < 48000 ? 48000 : samples) * 2 + 7200);
    int encoded = lame_encode_buffer_interleaved(waf->gfp, (short *) pointer, (const int) samples,
                                                 waf->mp3buffer.data(),
                                                 (int) waf->mp3buffer.size());
    if (encoded && fwrite(waf->mp3buffer.data(), (size_t) encoded, 1, waf->f) != 1) {
        LOGE("Write error. %d %d %d", (int) waf->mp3buffer.size(), encoded, samples);
        return 0;
    }
    waf->offset += samples;
    return samples;
}

static int waf_close_mp3(WAF *waf) {
    if (waf->offset > 0 && waf->mp3buffer.data() != nullptr) {
        auto encoded = (size_t) lame_encode_flush(waf->gfp, waf->mp3buffer.data(),
                                                        waf->mp3buffer.size());
        if (encoded > 0) {
            size_t byteswritten = fwrite(waf->mp3buffer.data(), 1, encoded, waf->f);
            if (byteswritten != encoded) {
                LOGE("Write error.");
            }
        }
        lame_mp3_tags_fid(waf->gfp, waf->f);
    }
    lame_close(waf->gfp);

    waf->gfp = nullptr;
    fclose(waf->f);
    waf->f = nullptr;
    waf->mp3buffer.clear();
    return 0;
}

//mp3buffer_size (in bytes) = 1.25*num_samples + 7200

WAF *waf_open_fd_mp3(int fd, int sr, uint8_t channels, int qual) {
    FILE *f = nullptr;
    f = fdopen(fd, "wb");
    if (f == nullptr)
        return nullptr;
    WAF *waf = new WAF;
    waf->f = f;
    waf->sr = sr;
    waf->channels = channels;
    waf->write = channels == 1 ? waf_write_mp3 : waf_write_mp3_interleaved;
    waf->close = waf_close_mp3;

    lame_global_flags *gfp;
    gfp = lame_init();
    id3tag_set_comment(gfp, "Made with Grainstorm for Android");
    id3tag_set_album(gfp, "Grainstorm");
    id3tag_set_albumart(gfp, (const char *) tsl::app::icon_data, tsl::app::icon_size);
    lame_set_num_channels(gfp, channels);
    lame_set_in_samplerate(gfp, sr);
    lame_set_out_samplerate(gfp, sr);
    lame_set_quality(gfp, 5);
    lame_set_VBR_quality(gfp, 5); /* 2=high  5 = medium  7=low */
    if (qual == 1) {
        lame_set_out_samplerate(gfp, sr);
        lame_set_VBR(gfp, vbr_off);
        lame_set_brate(gfp, 320);
    } else {
        int setting;
        if (qual == 6)
            setting = 96;
        else if (qual == 4)
            setting = 160;
        else if (qual == 2)
            setting = 240;
        else
            setting = 160;

        lame_set_VBR(gfp, vbr_abr);
        lame_set_VBR_mean_bitrate_kbps(gfp, setting);
        //lame_set_VBR(gfp, vbr_default);
        //lame_set_VBR_quality(gfp, qual - 2); /* 2=high  5 = medium  7=low */
    }

    //lame_set_VBR_q(gfp, 3);
    lame_init_params(gfp);
    waf->gfp = gfp;
    return waf;

}

void progress_callback(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written,
                       FLAC__uint64 samples_written, unsigned frames_written,
                       unsigned total_frames_estimate, void *client_data) {
    (void) encoder, (void) client_data;

    ;// LOGD( "wrote %ld bytes, %ld samples, %d frames", bytes_written, samples_written,  frames_written);
}

static FLAC__StreamEncoder *init_flac_encoder(WAF *waf, int bps = 16) {
    FLAC__bool ok = true;
    FLAC__StreamEncoder *encoder = 0;
    FLAC__StreamEncoderInitStatus init_status;
    FLAC__StreamMetadata_VorbisComment_Entry entry;
    /* allocate the encoder */
    if ((encoder = FLAC__stream_encoder_new()) == nullptr) {
        LOGE("ERROR: allocating encoder.");
        return nullptr;
    }


    ok &= FLAC__stream_encoder_set_verify(encoder, true);
    ok &= FLAC__stream_encoder_set_compression_level(encoder, 5);
    ok &= FLAC__stream_encoder_set_channels(encoder, waf->channels);
    ok &= FLAC__stream_encoder_set_bits_per_sample(encoder, bps);
    ok &= FLAC__stream_encoder_set_sample_rate(encoder, waf->sr);
    // ok &= FLAC__stream_encoder_set_total_samples_estimate(encoder, total_samples);

    /* now add some metadata; we'll add some tags and a padding block */
    if (ok) {
        if (
                (waf->metadata[0] = FLAC__metadata_object_new(
                        FLAC__METADATA_TYPE_VORBIS_COMMENT)) ==
                nullptr ||
                (waf->metadata[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PICTURE)) ==
                nullptr ||
                (waf->metadata[2] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PADDING)) ==
                nullptr ||
                /* there are many tag (vorbiscomment) functions but these are convenient for this particular use: */
                !FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "ALBUM",
                                                                                "Grainstorm") ||
                !FLAC__metadata_object_vorbiscomment_append_comment(waf->metadata[0],
                                                                    entry, /*copy=*/
                                                                    false) || /* copy=false: let metadata object take c
ontrol of entry's allocated string */
                !FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry, "COMMENT",
                                                                                "Made with Grainstorm for Android") ||
                !FLAC__metadata_object_vorbiscomment_append_comment(waf->metadata[0],
                                                                    entry, /*copy=*/
                                                                    false)
                ) {
            LOGE("ERROR: out of memory or tag error");
            ok = false;
        } else {
            ok &= FLAC__metadata_object_picture_set_data(waf->metadata[1],
                                                         (FLAC__byte *) tsl::app::icon_data,
                                                         tsl::app::icon_size, true);

            waf->metadata[2]->length = 1234; /* set the padding length */

            ok &= FLAC__stream_encoder_set_metadata(encoder, waf->metadata, 3);
        }
    }
    /* initialize encoder */
    if (ok) {
        init_status = FLAC__stream_encoder_init_FILE(encoder, waf->f,
                                                     progress_callback, /*client_data=*/nullptr);
        if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
            LOGE("ERROR: initializing encoder: %s",
                 FLAC__StreamEncoderInitStatusString[init_status]);
            ok = false;
        }
    }
    if (ok)
        return encoder;
    else
        return nullptr;

}

static size_t waf_write_flac(WAF *waf, void *pointer, size_t samples) {
    FLAC__bool ok = true;
    size_t i;

    if (waf->bitdepth == 16) {
        waf->mp3buffer.resize(samples * sizeof(FLAC__int32) * waf->channels);
        auto *pcm = (FLAC__int32 *) waf->mp3buffer.data();

        for (i = 0; i < samples * waf->channels; i++) {
            pcm[i] = (FLAC__int32) *((short *) pointer + i);
            /* inefficient but simple and works on big- or little-endian machines */
            //pcm[i] = (FLAC__int32)(((FLAC__int16)(FLAC__int8)pointer[2*i+1] << 8) | (FLAC__int16)pointer[2*i]);
        }
        ok = FLAC__stream_encoder_process_interleaved(waf->encoder, pcm, samples);
    } else {
        ok = FLAC__stream_encoder_process_interleaved(waf->encoder, (FLAC__int32 *) pointer,
                                                      samples);
    }

    if (ok) {
        waf->offset += samples;
        return samples;
    } else return 0;
}


static int waf_close_flac(WAF *waf) {
    FLAC__stream_encoder_finish(waf->encoder);
    /* now that encoding is finished, the metadata can be freed */
    FLAC__metadata_object_delete(waf->metadata[0]);
    FLAC__metadata_object_delete(waf->metadata[1]);
    FLAC__metadata_object_delete(waf->metadata[2]);
    FLAC__stream_encoder_delete(waf->encoder);
    fclose(waf->f);
    waf->f = nullptr;
    return 0;
}

WAF *waf_open_fd_flac(int fd, int sr, uint8_t channels, int bps) {
    FILE *f = fdopen(fd, "wb");
    if (f == nullptr)
        return nullptr;
    WAF *waf = new WAF;
    waf->f = f;
    waf->sr = sr;
    waf->bitdepth = bps;
    waf->channels = channels;
    waf->write = waf_write_flac;
    waf->close = waf_close_flac;
    if ((waf->encoder = init_flac_encoder(waf, bps)) == nullptr)
        return nullptr;
    else
        return waf;
}



std::map<int, std::string>AudioFile::errorToString{{-1, "Could not open output file."}, {-2, "Write Error."}, {0, "OK"}};