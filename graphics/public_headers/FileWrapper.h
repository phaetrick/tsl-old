#pragma once
#ifndef _FILEWRAPPER_H_
#define _FILEWRAPPER_H_

#include <vector>
#include <memory>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <string>
#ifndef off_t
#define off_t long
#endif
// Forward declarations for audio libraries
extern "C" {
#include <lame.h>
#include "FLAC/metadata.h"
#include "FLAC/stream_encoder.h"
}
namespace tsl {
    class FileWrapper {
    public:
        enum Mode {
            INTERNAL_BUFFER,
            EXTERNAL_BUFFER,
            FILE_POINTER
        };

    private:
        std::vector<uint8_t> internal_data_;
        uint8_t* external_data_;
        size_t external_size_;
        FILE* file_ptr_;
        size_t position_;
        Mode mode_;
        bool is_open_;
        bool is_managed_; // Whether to close FILE* on destruction/close

        void move_from(FileWrapper&& other) noexcept;
    public:
        FileWrapper();

        // Move constructor
        FileWrapper(FileWrapper&& other) noexcept {
            move_from(std::move(other));
        }

        // Move assignment
        FileWrapper& operator=(FileWrapper&& other) noexcept {
            if (this != &other) {
                close(); // clean up current resources if needed
                move_from(std::move(other));
            }
            return *this;
        }

        FileWrapper(const FileWrapper&) = delete;
        FileWrapper& operator=(const FileWrapper&) = delete;

        ~FileWrapper();

        // Initialize with FILE*
        int init(FILE* fp, bool isManaged = false);

        // Initialize with external buffer
        int init(unsigned char* buffer, size_t size);

        // Initialize with internal buffer (empty)
        int init();

        // Close file
        virtual int close();

        // Read data - returns number of objects read (like fread)
        size_t read(void* buf, size_t size, size_t count);

        // Convenience method for byte-based reading (backwards compatibility)
        size_t read_bytes(void* buf, size_t count);

        // Write data - returns number of objects written (like fwrite)
        virtual size_t write(const void* buf, size_t size, size_t count);

        // Convenience method for byte-based writing (backwards compatibility)
        size_t write_bytes(const void* buf, size_t count);

        // Get current position
        off_t tell();

        // Seek to position
        off_t seek(off_t offset, int whence);
        // Utility methods
        size_t size() const;

        bool is_open() const;

        Mode get_mode() const;

        bool is_managed() const;

        // Rewind to beginning (like POSIX rewind())
        void rewind();
        bool moveClose(std::vector<unsigned char>& dest);
    };


    class AudioFileWrapper : public FileWrapper {
    public:
        enum AudioFormat {
            WAV_16BIT,
            WAV_32FLOAT,
            MP3_LAME,
            FLAC_16BIT,
            FLAC_24BIT,
            RAW_16BIT
        };

        enum SampleFormat {
            SAMPLE_INT16,
            SAMPLE_INT24,
            SAMPLE_FLOAT32,
            SAMPLE_FLOAT64
        };

        struct AlbumArt {
            std::vector<uint8_t> data;
            std::string mime_type;
            std::string description;
            uint32_t type; // 3 = front cover, 4 = back cover, etc.

            AlbumArt() : type(3) {} // Default to front cover
        };

        struct Metadata {
            std::string title;
            std::string artist;
            std::string album;
            std::string comment;
            std::string genre;
            std::string year;
            std::string track;
            std::string album_artist;
            AlbumArt artwork;

            Metadata() = default;
        };

    private:
        AudioFormat output_format_;
        SampleFormat input_format_;
        uint32_t sample_rate_;
        uint16_t channels_;
        bool header_written_;
        Metadata metadata_;

        // Audio conversion buffers
        std::vector<uint8_t> conversion_buffer_;
        std::vector<int16_t> int16_buffer_;
        std::vector<int32_t> int24_buffer_;
        std::vector<float> float32_buffer_;

        // Format-specific encoders
        lame_global_flags* lame_flags_;
        FLAC__StreamEncoder* flac_encoder_;
        FLAC__StreamMetadata* flac_metadata_[2]; // vorbis comment + picture

        // WAV header tracking
        size_t wav_data_size_;
        size_t wav_header_pos_;

        // MP3 tag tracking
        size_t mp3_id3v2_size_;
        size_t mp3_audio_start_pos_;
        size_t mp3_lame_tag_pos_;
        bool mp3_lame_tag_written_;

        void move_from(AudioFileWrapper&& other) noexcept;

    public:
        AudioFileWrapper();

        AudioFileWrapper(AudioFileWrapper&& other) noexcept
            : FileWrapper(std::move(other)) // call base move
        {
            move_from(std::move(other));
        }

        AudioFileWrapper& operator=(AudioFileWrapper&& other) noexcept {
            if (this != &other) {
                this->FileWrapper::operator=(std::move(other));
                cleanup_encoders(); // clean up current audio encoders if any
                move_from(std::move(other));
            }
            return *this;
        }

        ~AudioFileWrapper();

        // Set metadata for the audio file
        void set_metadata(const Metadata& metadata);

        // Set individual metadata fields
        void set_title(const std::string& title) { metadata_.title = title; }
        void set_artist(const std::string& artist) { metadata_.artist = artist; }
        void set_album(const std::string& album) { metadata_.album = album; }
        void set_comment(const std::string& comment) { metadata_.comment = comment; }
        void set_genre(const std::string& genre) { metadata_.genre = genre; }
        void set_year(const std::string& year) { metadata_.year = year; }
        void set_track(const std::string& track) { metadata_.track = track; }
        void set_album_artist(const std::string& album_artist) { metadata_.album_artist = album_artist; }

        // Set album artwork
        void set_artwork(const std::vector<uint8_t>& data, const std::string& mime_type,
            const std::string& description = "Cover", uint32_t type = 3);
        // Initialize with audio parameters
        
        int init_audio(AudioFormat format, uint32_t sample_rate, uint16_t channels,
            SampleFormat input_format = SAMPLE_INT16);
        // Override write to handle audio conversion

        size_t write(const void* buf, size_t size, size_t count) override;

        // Finalize audio file (important for MP3 and FLAC)
        int finalize();

        // Override close to finalize before closing
        int close() override;

    private:
        void cleanup_encoders();

        int setup_encoder();

        int setup_mp3_encoder();

        int setup_flac_encoder();
        int write_header();

        int write_wav_header();

        size_t write_audio_data(const void* buf, size_t size, size_t count);

        size_t write_wav_data(const void* buf, size_t size, size_t count);

        size_t write_mp3_data(const void* buf, size_t size, size_t count);

        size_t write_flac_data(const void* buf, size_t size, size_t count);

        bool convert_to_int16(const void* buf, size_t bytes);

        bool convert_to_int24(const void* buf, size_t bytes);
        
        bool convert_to_float32(const void* buf, size_t bytes);

        int finalize_wav();

        int finalize_mp3(); 

        int setup_flac_metadata();

        int write_mp3_id3v2_header();

        int finalize_flac();
        static FLAC__StreamEncoderSeekStatus flac_seek_callback(
            const FLAC__StreamEncoder* encoder, FLAC__uint64 absolute_byte_offset, void* client_data);

        static FLAC__StreamEncoderTellStatus flac_tell_callback(
            const FLAC__StreamEncoder* encoder, FLAC__uint64* absolute_byte_offset, void* client_data);
        // FLAC callback for writing encoded data
        static FLAC__StreamEncoderWriteStatus flac_write_callback(
            const FLAC__StreamEncoder* encoder,
            const FLAC__byte buffer[],
            size_t bytes,
            unsigned samples,
            unsigned current_frame,
            void* client_data);
    }; 
}
/*
// Set metadata before initializing
audio.set_title("My Song");
audio.set_artist("My Artist");
audio.set_album("My Album");
audio.set_comment("Recorded in 2024");
audio.set_year("2024");
audio.set_genre("Rock");

// Load and set album artwork
std::vector<uint8_t> artwork_data; // Load your image data
audio.set_artwork(artwork_data, "image/jpeg", "Front Cover");

// Initialize with desired format
audio.init_audio(AudioFileWrapper::MP3_LAME, 44100, 2);

// Write audio data
float samples[] = {0.5f, -0.3f, 0.8f, -0.1f};
audio.write(samples, sizeof(float), 4);

audio.close(); // Properly finalizes with all metadata*/
#endif // !_FILEWRAPPER_H_
