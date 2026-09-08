#pragma once
#include <resample.h>
#include <audio/Recording.h>
#include <FileWrapper.h>
#include <FLAC/stream_decoder.h>
#include <FLAC/stream_encoder.h>
#include <memory>
#include <vector>

struct TRACK;

// ─────────────────────────────────────────────────────────────
//  FlacDecoder
// ─────────────────────────────────────────────────────────────
class FlacDecoder {
public:
    FlacDecoder() = default;
    ~FlacDecoder();

    std::shared_ptr<tsl::Recording> init(tsl::FileWrapper* fd, TRACK* track, long end);

private:
    tsl::AppState* _appState{};
    FLAC__StreamDecoder* dec{};
    uint64_t                 totalSamples{};
    long                     end{};
    ResamplerRounded<double> rs[2]{};
    std::vector<short>       audio[2];
    tsl::FileWrapper* fd{};
    off_t                    start{};
    unsigned long            off{};

    // static trampolines
    static FLAC__StreamDecoderReadStatus   s_read(const FLAC__StreamDecoder*, FLAC__byte[], size_t*, void*);
    static FLAC__StreamDecoderSeekStatus   s_seek(const FLAC__StreamDecoder*, FLAC__uint64, void*);
    static FLAC__StreamDecoderTellStatus   s_tell(const FLAC__StreamDecoder*, FLAC__uint64*, void*);
    static FLAC__StreamDecoderLengthStatus s_length(const FLAC__StreamDecoder*, FLAC__uint64*, void*);
    static FLAC__bool                      s_eof(const FLAC__StreamDecoder*, void*);
    static FLAC__StreamDecoderWriteStatus  s_write(const FLAC__StreamDecoder*, const FLAC__Frame*, const FLAC__int32* const [], void*);
    static void                            s_metadata(const FLAC__StreamDecoder*, const FLAC__StreamMetadata*, void*);
    static void                            s_error(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus, void*);

    // instance callbacks
    FLAC__StreamDecoderReadStatus  on_read(FLAC__byte* buf, size_t* bytes);
    FLAC__StreamDecoderSeekStatus  on_seek(FLAC__uint64 absolute_byte_offset);
    FLAC__StreamDecoderTellStatus  on_tell(FLAC__uint64* absolute_byte_offset);
    FLAC__StreamDecoderWriteStatus on_write(const FLAC__Frame* frame, const FLAC__int32* const buffer[]);
};

// ─────────────────────────────────────────────────────────────
//  FlacEncoder
// ─────────────────────────────────────────────────────────────
class FlacEncoder {
public:
    static constexpr int32_t bufsize = 1024;

    FlacEncoder() = default;
    ~FlacEncoder();

    size_t init(tsl::FileWrapper* wrapper, TRACK* track);

private:
    tsl::AppState* _appState{};
    FLAC__StreamEncoder* enc{};
    FLAC__int32          tmp[2 * bufsize]{};
    tsl::FileWrapper* wrapper{};
    off_t                start{};
    size_t               byteswritten{};

    // static trampolines
    static FLAC__StreamEncoderWriteStatus s_write(const FLAC__StreamEncoder*, const FLAC__byte*, size_t, uint32_t, uint32_t, void*);
    static FLAC__StreamEncoderSeekStatus  s_seek(const FLAC__StreamEncoder*, FLAC__uint64, void*);
    static FLAC__StreamEncoderTellStatus  s_tell(const FLAC__StreamEncoder*, FLAC__uint64*, void*);
    static void                           s_metadata(const FLAC__StreamEncoder*, const FLAC__StreamMetadata*, void*);

    // instance callbacks
    FLAC__StreamEncoderWriteStatus on_write(const FLAC__byte* buf, size_t bytes);
    FLAC__StreamEncoderSeekStatus  on_seek(FLAC__uint64 absolute_byte_offset);
    FLAC__StreamEncoderTellStatus  on_tell(FLAC__uint64* absolute_byte_offset);
    void                           on_metadata(const FLAC__StreamMetadata* metadata);
};