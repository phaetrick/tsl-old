#include "WindowsMediaDecoder.h"
#include <Windows.h>
#include <mfapi.h>
#include <mfobjects.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <iostream>
#include <wrl/client.h>
#include <propvarutil.h>  // For PropVariantClear
#include <propidl.h>      // For PROPVARIANT
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")


using Microsoft::WRL::ComPtr;


int tsl::decoding::decodeWithMediaFoundation(tsl::Decdata &rec, std::function<int(tsl::Decdata&, void*, uint64_t)> func) {
    std::wstring wpath(rec.fileName.begin(), rec.fileName.end());

    // Initialize Media Foundation
    MFStartup(MF_VERSION);

    // Create Source Reader
    ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromURL(wpath.c_str(), nullptr, &reader))) {
        MFShutdown();
        return 2;
    }

    // Define target media type: 16-bit PCM
    ComPtr<IMFMediaType> pcmType;
    MFCreateMediaType(&pcmType);
    pcmType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pcmType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM); // 16-bit PCM

    // These are crucial for consistent decoding
    pcmType->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
    pcmType->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, rec.sr); // You can omit this to keep source rate
    pcmType->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, rec.channels);            // Default to stereo

    // Reader will negotiate as close as possible
    if (FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pcmType.Get()))) {
        MFShutdown();
        return 1;
    }

    // Get the actual media type to read attributes
    ComPtr<IMFMediaType> actualType;
    reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &actualType);


    // Optional: get total duration
    PROPVARIANT prop;
    PropVariantInit(&prop);  // Always initialize

    UINT64 duration = 0;
    if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &prop))) {
        if (prop.vt == VT_UI8)
            duration = prop.uhVal.QuadPart;
    }
    PropVariantClear(&prop);  // Always clear to avoid leaks

    // Convert duration to frames using sample rate
    UINT64 totalFrames = 0;
    if (duration > 0 && rec.sr > 0) {
        totalFrames = (duration * rec.sr) / 10000000.;
        rec.off = std::min<size_t>(totalFrames, rec.off);
    }
    size_t offset = 0;
    // Read audio samples
    while (offset < rec.off) {
        ComPtr<IMFSample> sample;
        DWORD flags = 0;
        if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, nullptr, &sample))) {
            break;
        }
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;

        ComPtr<IMFMediaBuffer> buffer;
        if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;

        BYTE* data = nullptr;
        DWORD maxLen = 0, curLen = 0;
        if (SUCCEEDED(buffer->Lock(&data, &maxLen, &curLen)) && data && curLen > 0) {
            size_t samples = curLen / sizeof(int16_t) / rec.channels;
            auto ret = func(rec, data, samples);
            buffer->Unlock();
            if (ret != 0) {
                MFShutdown();
                return 3;
            }
            offset += samples;
        }
        
    }

    MFShutdown();
    return 0;
}
