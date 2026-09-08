#include "track.h"
#include "infopanel.h"
#include "grainstorm.h"

#include "flac.h"
#include <FLAC/stream_decoder.h>
#include <FLAC/stream_encoder.h>
// ═════════════════════════════════════════════════════════════
//  FlacDecoder
// ═════════════════════════════════════════════════════════════

FlacDecoder::~FlacDecoder()
{
    if (dec) FLAC__stream_decoder_delete(dec);
}

std::shared_ptr<tsl::Recording> FlacDecoder::init(tsl::FileWrapper* _fd, TRACK* track, long _end)
{
    _appState = track->_appState;
    fd = _fd;
    end = _end;
    if ((start = fd->tell()) < 0)
        return nullptr;

    dec = FLAC__stream_decoder_new();
    if (!dec) return nullptr;

    FLAC__stream_decoder_set_md5_checking(dec, false);

    FLAC__stream_decoder_init_stream(
        dec,
        s_read, s_seek, s_tell, s_length, s_eof,
        s_write, s_metadata, s_error,
        this
    );

    FLAC__stream_decoder_process_until_end_of_metadata(dec);
    totalSamples = FLAC__stream_decoder_get_total_samples(dec);

    auto info = _DATA->views.infopanel;
    if (info) {
        info->dec_offset.store(0);
        info->bytes.store(0);
        if (totalSamples != 0) info->progress.store(0);
        else                   info->progress = -1;
        info->text.store(track->name);
        info->text2.store(" DECODING");
        info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);
    }

    FLAC__stream_decoder_process_until_end_of_stream(dec);
    if (info) info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);

    FLAC__stream_decoder_finish(dec);
    FLAC__stream_decoder_delete(dec);
    dec = nullptr;

    if (off > 0) {
        auto rec = std::make_shared<tsl::Recording>(_STATE, audio, _STATE->channels, "From Preset");
        rec->fileName = presetHasAudio;
        return rec;
    }
    return nullptr;
}

// ── static trampolines ────────────────────────────────────────

FLAC__StreamDecoderReadStatus FlacDecoder::s_read(
    const FLAC__StreamDecoder*, FLAC__byte buf[], size_t* bytes, void* cd)
{
    return static_cast<FlacDecoder*>(cd)->on_read(buf, bytes);
}

FLAC__StreamDecoderSeekStatus FlacDecoder::s_seek(
    const FLAC__StreamDecoder*, FLAC__uint64 offset, void* cd)
{
    return static_cast<FlacDecoder*>(cd)->on_seek(offset);
}

FLAC__StreamDecoderTellStatus FlacDecoder::s_tell(
    const FLAC__StreamDecoder*, FLAC__uint64* offset, void* cd)
{
    return static_cast<FlacDecoder*>(cd)->on_tell(offset);
}

FLAC__StreamDecoderLengthStatus FlacDecoder::s_length(
    const FLAC__StreamDecoder*, FLAC__uint64*, void*)
{
    return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
}

FLAC__bool FlacDecoder::s_eof(const FLAC__StreamDecoder*, void* cd)
{
    auto* self = static_cast<FlacDecoder*>(cd);
    off_t pos = self->fd->tell();
    return (pos < 0 || pos >= self->end) ? true : false;
}

FLAC__StreamDecoderWriteStatus FlacDecoder::s_write(
    const FLAC__StreamDecoder*, const FLAC__Frame* frame,
    const FLAC__int32* const buf[], void* cd)
{
    return static_cast<FlacDecoder*>(cd)->on_write(frame, buf);
}

void FlacDecoder::s_metadata(const FLAC__StreamDecoder*, const FLAC__StreamMetadata*, void*) {}

void FlacDecoder::s_error(
    const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus, void*)
{
    LOGE("Error");
}

// ── instance callbacks ────────────────────────────────────────

FLAC__StreamDecoderReadStatus FlacDecoder::on_read(FLAC__byte* buf, size_t* bytes)
{
    if (*bytes == 0)
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    off_t offset = fd->tell();
    if (offset < 0)
        return FLAC__STREAM_DECODER_READ_STATUS_ABORT;

    if (end - offset < (long)*bytes) {
        if (offset >= end) { *bytes = 0; return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM; }
        *bytes = fd->read(buf, sizeof(FLAC__byte), end - offset);
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }
    *bytes = fd->read(buf, sizeof(FLAC__byte), *bytes);
    if (*bytes == 0)
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderSeekStatus FlacDecoder::on_seek(FLAC__uint64 absolute_byte_offset)
{
    if (fd->seek((off_t)(absolute_byte_offset + start), SEEK_SET) < 0)
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}

FLAC__StreamDecoderTellStatus FlacDecoder::on_tell(FLAC__uint64* absolute_byte_offset)
{
    off_t pos = fd->tell();
    if (pos < 0) return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = (FLAC__uint64)(pos - start);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

FLAC__StreamDecoderWriteStatus FlacDecoder::on_write(
    const FLAC__Frame* frame, const FLAC__int32* const buffer[])
{
    if (_STATE->channels == 1 && frame->header.channels == 2) {
        if (_STATE->sr == frame->header.sample_rate) {
            int32_t smpl = 0;
            while (smpl < (int32_t)frame->header.blocksize)
                audio[0].push_back((buffer[0][smpl] + buffer[1][smpl++]) * .5);
        }
        else {
            rs[0].init(frame->header.sample_rate * _STATE->onedsr);
            int32_t smpl = 0;
            while (smpl < (int32_t)frame->header.blocksize) {
                if (rs[0].isWriteNeeded()) rs[0].writeNextFrame((buffer[0][smpl] + buffer[1][smpl]) * .5), ++smpl;
                else audio[0].push_back(rs[0].readNextFrame());
            }
            while (!rs[0].isWriteNeeded())
                audio[0].push_back(rs[0].readNextFrame());
        }
    }
    else if (_STATE->channels == 2 && frame->header.channels == 1) {
        if (_STATE->sr == frame->header.sample_rate) {
            int32_t smpl = 0;
            while (smpl < (int32_t)frame->header.blocksize) {
                audio[0].push_back(buffer[0][smpl]);
                audio[1].push_back(buffer[0][smpl++]);
            }
        }
        else {
            rs[0].init(frame->header.sample_rate * _STATE->onedsr);
            int32_t smpl = 0;
            while (smpl < (int32_t)frame->header.blocksize) {
                if (rs[0].isWriteNeeded()) rs[0].writeNextFrame(buffer[0][smpl++]);
                else { auto s = rs[0].readNextFrame(); audio[0].push_back(s); audio[1].push_back(s); }
            }
            while (!rs[0].isWriteNeeded()) {
                auto s = rs[0].readNextFrame();
                audio[0].push_back(s); audio[1].push_back(s);
            }
        }
    }
    else {
        if (_STATE->sr == frame->header.sample_rate) {
            for (int32_t chan = 0; chan < (int32_t)frame->header.channels; chan++) {
                int32_t smpl = 0;
                while (smpl < (int32_t)frame->header.blocksize)
                    audio[chan].push_back(buffer[chan][smpl++]);
            }
        }
        else {
            for (int32_t chan = 0; chan < (int32_t)frame->header.channels; chan++) {
                rs[chan].init(frame->header.sample_rate * _STATE->onedsr);
                int32_t smpl = 0;
                while (smpl < (int32_t)frame->header.blocksize) {
                    if (rs[chan].isWriteNeeded()) rs[chan].writeNextFrame(buffer[chan][smpl++]);
                    else audio[chan].push_back(rs[chan].readNextFrame());
                }
                while (!rs[chan].isWriteNeeded())
                    audio[chan].push_back(rs[chan].readNextFrame());
            }
        }
    }

    off += frame->header.blocksize;
    if (auto info = _DATA->views.infopanel) {
        info->dec_offset.store(off);
        info->bytes.store(off * _STATE->channels * sizeof(short));
        if (totalSamples != 0)
            info->progress.store((long double)off / totalSamples);
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

// ═════════════════════════════════════════════════════════════
//  FlacEncoder
// ═════════════════════════════════════════════════════════════

FlacEncoder::~FlacEncoder()
{
    if (enc) FLAC__stream_encoder_delete(enc);
}

size_t FlacEncoder::init(tsl::FileWrapper* _wrapper, TRACK* track)
{
    _appState = track->_appState;
    auto rec = track->filebuffer.load();
    if (rec == nullptr || rec->off == 0) return 0;
    auto off = rec->off;
    wrapper = _wrapper;
    if ((start = wrapper->tell()) < 0) return 0;

    enc = FLAC__stream_encoder_new();
    if (!enc) return 0;

    FLAC__stream_encoder_set_channels(enc, rec->channels);
    FLAC__stream_encoder_set_bits_per_sample(enc, 16);
    FLAC__stream_encoder_set_sample_rate(enc, rec->sr);
    FLAC__stream_encoder_set_total_samples_estimate(enc, off);

    FLAC__stream_encoder_init_stream(
        enc,
        s_write, s_seek, s_tell, s_metadata,
        this
    );

    int32_t framesread{};
    auto info = _DATA->views.infopanel;
    if (info) {
        info->progress.store(0);
        info->bytes.store(0);
        info->dec_offset.store(framesread);
        info->text.store(track->name);
        info->text2.store(" ENCODING");
        info->setRenderFunc(tsl::graphics::InfoPanel::RenderProg);
    }

    while (framesread < off) {
        uint32_t samples = 0;
        while (framesread < off && samples < (uint32_t)(bufsize * rec->channels)) {
            for (int32_t i = 0; i < rec->channels; i++)
                tmp[samples++] = rec->buffer[i].at(framesread);
            framesread++;
        }
        FLAC__stream_encoder_process_interleaved(enc, tmp, samples / _STATE->channels);
        if (info) {
            info->dec_offset.store(framesread);
            info->progress.store(framesread / (double)off);
        }
    }

    FLAC__stream_encoder_finish(enc);
    FLAC__stream_encoder_delete(enc);
    enc = nullptr;

    if (info) info->setRenderFunc(tsl::graphics::InfoPanel::RenderStand);
    return byteswritten;
}

// ── static trampolines ────────────────────────────────────────

FLAC__StreamEncoderWriteStatus FlacEncoder::s_write(
    const FLAC__StreamEncoder*, const FLAC__byte* buf, size_t bytes,
    uint32_t, uint32_t, void* cd)
{
    return static_cast<FlacEncoder*>(cd)->on_write(buf, bytes);
}

FLAC__StreamEncoderSeekStatus FlacEncoder::s_seek(
    const FLAC__StreamEncoder*, FLAC__uint64 offset, void* cd)
{
    return static_cast<FlacEncoder*>(cd)->on_seek(offset);
}

FLAC__StreamEncoderTellStatus FlacEncoder::s_tell(
    const FLAC__StreamEncoder*, FLAC__uint64* offset, void* cd)
{
    return static_cast<FlacEncoder*>(cd)->on_tell(offset);
}

void FlacEncoder::s_metadata(
    const FLAC__StreamEncoder*, const FLAC__StreamMetadata* meta, void* cd)
{
    static_cast<FlacEncoder*>(cd)->on_metadata(meta);
}

// ── instance callbacks ────────────────────────────────────────

FLAC__StreamEncoderWriteStatus FlacEncoder::on_write(const FLAC__byte* buf, size_t bytes)
{
    if (wrapper->write(buf, bytes, 1) != 1)
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    byteswritten += bytes;
    if (auto info = _DATA->views.infopanel) info->bytes.fetch_add(bytes);
    return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

FLAC__StreamEncoderSeekStatus FlacEncoder::on_seek(FLAC__uint64 absolute_byte_offset)
{
    if (wrapper->seek((off_t)(absolute_byte_offset + start), SEEK_SET) < 0)
        return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
    return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

FLAC__StreamEncoderTellStatus FlacEncoder::on_tell(FLAC__uint64* absolute_byte_offset)
{
    off_t pos = wrapper->tell();
    if (pos < 0) return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
    *absolute_byte_offset = (FLAC__uint64)(pos - start);
    return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

void FlacEncoder::on_metadata(const FLAC__StreamMetadata* metadata)
{
    if (metadata->is_last) {
        if (wrapper->seek(start, SEEK_SET) == -1 ||
            wrapper->write(metadata, sizeof(FLAC__StreamMetadata), 1) != 1)
        {
            showToast(_STATE, "Encode: Write Error");
        }
    }
}