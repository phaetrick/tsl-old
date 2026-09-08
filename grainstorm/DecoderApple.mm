// DecoderApple.mm  (Objective-C++ — add to CMake with .mm extension)
#if defined(__APPLE__)

#include "WindowsMediaDecoder.h"
#include <AudioToolbox/ExtendedAudioFile.h>
#include <CoreFoundation/CoreFoundation.h>
#include <algorithm>

int tsl::decoding::decodeWithMediaFoundation(
    tsl::Decdata& rec,
    std::function<int(tsl::Decdata&, void*, uint64_t)> func)
{
    // Open file
    CFStringRef cfPath = CFStringCreateWithCString(
        kCFAllocatorDefault, rec.fileName.c_str(), kCFStringEncodingUTF8);
    CFURLRef url = CFURLCreateWithFileSystemPath(
        kCFAllocatorDefault, cfPath, kCFURLPOSIXPathStyle, false);
    CFRelease(cfPath);

    ExtAudioFileRef fileRef = nullptr;
    OSStatus err = ExtAudioFileOpenURL(url, &fileRef);
    CFRelease(url);
    if (err != noErr) return 2;

    // Set client format: 16-bit PCM, interleaved, target sr/channels
    AudioStreamBasicDescription clientFmt = {};
    clientFmt.mSampleRate       = rec.sr;
    clientFmt.mFormatID         = kAudioFormatLinearPCM;
    clientFmt.mFormatFlags      = kAudioFormatFlagIsSignedInteger
                                | kAudioFormatFlagIsPacked;
    clientFmt.mBitsPerChannel   = 16;
    clientFmt.mChannelsPerFrame = rec.channels;
    clientFmt.mBytesPerFrame    = sizeof(int16_t) * rec.channels;
    clientFmt.mFramesPerPacket  = 1;
    clientFmt.mBytesPerPacket   = clientFmt.mBytesPerFrame;

    err = ExtAudioFileSetProperty(fileRef,
        kExtAudioFileProperty_ClientDataFormat,
        sizeof(clientFmt), &clientFmt);
    if (err != noErr) { ExtAudioFileDispose(fileRef); return 1; }

    // Get total frame count to clamp rec.off
    SInt64 totalFrames = 0;
    UInt32 propSize = sizeof(totalFrames);
    ExtAudioFileGetProperty(fileRef,
        kExtAudioFileProperty_FileLengthFrames, &propSize, &totalFrames);
    if (totalFrames > 0)
        rec.off = std::min<size_t>(static_cast<size_t>(totalFrames), rec.off);

    // Decode in chunks
    const UInt32 CHUNK = 4096;
    std::vector<int16_t> buf(CHUNK * rec.channels);

    AudioBufferList abl;
    abl.mNumberBuffers              = 1;
    abl.mBuffers[0].mNumberChannels = rec.channels;
    abl.mBuffers[0].mDataByteSize   = buf.size() * sizeof(int16_t);
    abl.mBuffers[0].mData           = buf.data();

    size_t offset = 0;
    while (offset < rec.off) {
        UInt32 frames = std::min<UInt32>(CHUNK,
            static_cast<UInt32>(rec.off - offset));
        abl.mBuffers[0].mDataByteSize = frames * rec.channels * sizeof(int16_t);

        err = ExtAudioFileRead(fileRef, &frames, &abl);
        if (err != noErr || frames == 0) break;

        int ret = func(rec, buf.data(), frames);
        if (ret != 0) {
            ExtAudioFileDispose(fileRef);
            return 3;
        }
        offset += frames;
    }

    ExtAudioFileDispose(fileRef);
    return 0;
}

#endif