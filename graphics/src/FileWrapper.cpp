#include "FileWrapper.h"
using namespace tsl;

FileWrapper::FileWrapper() : external_data_(nullptr), external_size_(0), file_ptr_(nullptr),
position_(0), mode_(INTERNAL_BUFFER), is_open_(false), is_managed_(false) {
}

FileWrapper::~FileWrapper() {
	close();
}

void FileWrapper::move_from(FileWrapper&& other) noexcept {
	internal_data_ = std::move(other.internal_data_);
	external_data_ = other.external_data_;
	external_size_ = other.external_size_;
	file_ptr_ = other.file_ptr_;
	position_ = other.position_;
	mode_ = other.mode_;
	is_open_ = other.is_open_;
	is_managed_ = other.is_managed_;

	// Invalidate source
	other.external_data_ = nullptr;
	other.external_size_ = 0;
	other.file_ptr_ = nullptr;
	other.position_ = 0;
	other.is_open_ = false;
	other.is_managed_ = false;
}

// Initialize with FILE*
int FileWrapper::init(FILE* fp, bool isManaged) {
	close();
	if (!fp) return -1;

	file_ptr_ = fp;
	is_managed_ = isManaged;
	mode_ = FILE_POINTER;

	// Get current file position with error checking
	long pos = ftell(fp);
	if (pos == -1L) {
		// If ftell fails, assume position 0
		position_ = 0;
	}
	else {
		position_ = static_cast<size_t>(pos);
	}

	is_open_ = true;
	return 0;
}

// Initialize with external buffer
int FileWrapper::init(unsigned char* buffer, size_t size) {
	close();
	if (!buffer || size == 0) return -1;

	external_data_ = buffer;
	external_size_ = size;
	mode_ = EXTERNAL_BUFFER;
	position_ = 0;
	is_managed_ = false; // External buffers are never managed
	is_open_ = true;
	return 0;
}

// Initialize with internal buffer (empty)
int FileWrapper::init() {
	close();
		internal_data_.clear();
	mode_ = INTERNAL_BUFFER;
	position_ = 0;
	is_managed_ = false; // Internal buffers don't use FILE*
	is_open_ = true;
	return 0;
}

// Close file
int FileWrapper::close() {
	if (is_open_) {
		// Close FILE* if we're managing it
		if (mode_ == FILE_POINTER && is_managed_ && file_ptr_) {
			int result = fclose(file_ptr_);
			file_ptr_ = nullptr;
			if (result != 0) {
				is_open_ = false;
				is_managed_ = false;
				return -1;
			}
		}
		is_open_ = false;
		is_managed_ = false;
		return 0;
	}
	return -1;
}

// Read data - returns number of objects read (like fread)
size_t FileWrapper::read(void* buf, size_t size, size_t count) {
	if (!is_open_ || !buf || size == 0 || count == 0) {
		return 0;
	}

	// Check for potential overflow
	if (count > SIZE_MAX / size) {
		return 0;
	}

	switch (mode_) {
	case INTERNAL_BUFFER: {
		if (position_ >= internal_data_.size()) {
			return 0;
		}
		size_t available_bytes = internal_data_.size() - position_;
		size_t objects_that_fit = available_bytes / size;
		size_t actual_objects = std::min(count, objects_that_fit);
		size_t bytes_to_read = actual_objects * size;

		if (bytes_to_read > 0) {
			std::memcpy(buf, internal_data_.data() + position_, bytes_to_read);
			position_ += bytes_to_read;
		}
		return actual_objects;
	}

	case EXTERNAL_BUFFER: {
		if (position_ >= external_size_) {
			return 0;
		}
		size_t available_bytes = external_size_ - position_;
		size_t objects_that_fit = available_bytes / size;
		size_t actual_objects = std::min(count, objects_that_fit);
		size_t bytes_to_read = actual_objects * size;

		if (bytes_to_read > 0) {
			std::memcpy(buf, external_data_ + position_, bytes_to_read);
			position_ += bytes_to_read;
		}
		return actual_objects;
	}

	case FILE_POINTER: {
		if (!file_ptr_) return 0;
		size_t objects_read = fread(buf, size, count, file_ptr_);
		position_ += objects_read * size;
		return objects_read;
	}
	}
	return 0;
}

// Convenience method for byte-based reading (backwards compatibility)
size_t FileWrapper::read_bytes(void* buf, size_t count) {
	return read(buf, 1, count);
}

// Write data - returns number of objects written (like fwrite)
size_t FileWrapper::write(const void* buf, size_t size, size_t count) {
	if (!is_open_ || !buf || size == 0 || count == 0) {
		return 0;
	}

	// Check for potential overflow
	if (count > SIZE_MAX / size) {
		return 0;
	}

	size_t total_bytes = size * count;

	switch (mode_) {
	case INTERNAL_BUFFER: {
		     
			// Always expand buffer if necessary - never fail
			if (position_ + total_bytes > internal_data_.size()) {
				try {
					internal_data_.resize(position_ + total_bytes);
				}
				catch (const std::bad_alloc&) {
					return 0;
				}
			}
			 
			std::memcpy(internal_data_.data() + position_, buf, total_bytes);
			position_ += total_bytes;
			return count; // All objects written
	}

	case EXTERNAL_BUFFER: {
		// Check how many complete objects we can fit
		if (position_ >= external_size_) {
			return 0; // No space
		}

		size_t available_bytes = external_size_ - position_;
		size_t objects_that_fit = available_bytes / size;
		size_t actual_objects = std::min(count, objects_that_fit);
		size_t bytes_to_write = actual_objects * size;

		if (bytes_to_write > 0) {
			std::memcpy(external_data_ + position_, buf, bytes_to_write);
			position_ += bytes_to_write;
		}
		return actual_objects;
	}

	case FILE_POINTER: {
		if (!file_ptr_) return 0;
		size_t objects_written = fwrite(buf, size, count, file_ptr_);
		position_ += objects_written * size;
		return objects_written;
	}
	}
	return 0;
}

// Convenience method for byte-based writing (backwards compatibility)
size_t FileWrapper::write_bytes(const void* buf, size_t count) {
	return write(buf, 1, count);
}

// Get current position
off_t FileWrapper::tell() {
	if (!is_open_) {
		return -1;
	}

	if (mode_ == FILE_POINTER) {
		if (!file_ptr_) return -1;
		return ftell(file_ptr_);
	}
	return static_cast<off_t>(position_);
}

// Seek to position
off_t FileWrapper::seek(off_t offset, int whence) {
	if (!is_open_) {
		return -1;
	}

	switch (mode_) {
	case INTERNAL_BUFFER: {
		size_t new_pos;
		switch (whence) {
		case SEEK_SET:
			if (offset < 0) return -1;
			new_pos = static_cast<size_t>(offset);
			break;
		case SEEK_CUR:
			if (offset < 0 && static_cast<size_t>(-offset) > position_) return -1;
			new_pos = position_ + offset;
			break;
		case SEEK_END:
			if (offset < 0 && static_cast<size_t>(-offset) > internal_data_.size()) return -1;
			new_pos = internal_data_.size() + offset;
			break;
		default:
			return -1;
		}

			if (new_pos > internal_data_.size()) {
				internal_data_.resize(new_pos);
			}
			position_ = new_pos;
			return static_cast<off_t>(position_);
		
	}

	case EXTERNAL_BUFFER: {
		size_t new_pos;
		switch (whence) {
		case SEEK_SET:
			if (offset < 0) return -1;
			new_pos = static_cast<size_t>(offset);
			break;
		case SEEK_CUR:
			if (offset < 0 && static_cast<size_t>(-offset) > position_) return -1;
			new_pos = position_ + offset;
			break;
		case SEEK_END:
			if (offset < 0 && static_cast<size_t>(-offset) > external_size_) return -1;
			new_pos = external_size_ + offset;
			break;
		default:
			return -1;
		}

		// Clamp to buffer bounds
		if (new_pos > external_size_) {
			new_pos = external_size_;
		}
		position_ = new_pos;
		return static_cast<off_t>(position_);
	}

	case FILE_POINTER: {
		if (!file_ptr_) return -1;
		if (fseek(file_ptr_, offset, whence) == 0) {
			long pos = ftell(file_ptr_);
			if (pos != -1L) {
				position_ = static_cast<size_t>(pos);
				return static_cast<off_t>(position_);
			}
		}
		return -1;
	}
	}
	return -1;
}

// Utility methods
size_t FileWrapper::size() const {
	switch (mode_) {
	case INTERNAL_BUFFER:
		return internal_data_.size();
	case EXTERNAL_BUFFER:
		return external_size_;
	case FILE_POINTER: {
		if (!file_ptr_) return 0;
		// Thread-safe file size calculation
		long current = ftell(file_ptr_);
		if (current == -1L) return 0;

		if (fseek(file_ptr_, 0, SEEK_END) != 0) return 0;
		long size = ftell(file_ptr_);
		if (size == -1L) {
			fseek(file_ptr_, current, SEEK_SET); // Restore position
			return 0;
		}

		if (fseek(file_ptr_, current, SEEK_SET) != 0) return 0;
		return static_cast<size_t>(size);
	}
	}
	return 0;
}

bool FileWrapper::is_open() const {
	return is_open_;
}

FileWrapper::Mode FileWrapper::get_mode() const {
	return mode_;
}

bool FileWrapper::is_managed() const {
	return is_managed_;
}

// Rewind to beginning (like POSIX rewind())
void FileWrapper::rewind() {
	if (!is_open_) {
		return;
	}

	switch (mode_) {
	case INTERNAL_BUFFER:
	case EXTERNAL_BUFFER:
		position_ = 0;
		break;

	case FILE_POINTER:
		if (file_ptr_) {
			::rewind(file_ptr_); // Use standard rewind()
			position_ = 0;
		}
		break;
	}
}

bool FileWrapper::moveClose(std::vector<unsigned char>& dest) {
	if (is_open_)
		close();
	dest = std::move(internal_data_);
	return true;
}


AudioFileWrapper::AudioFileWrapper() : FileWrapper(),
output_format_(RAW_16BIT),
input_format_(SAMPLE_INT16),
sample_rate_(44100),
channels_(2),
header_written_(false),
lame_flags_(nullptr),
flac_encoder_(nullptr),
wav_data_size_(0),
wav_header_pos_(0),
mp3_id3v2_size_(0),
mp3_audio_start_pos_(0),
mp3_lame_tag_written_(false),
mp3_lame_tag_pos_(0) {

	flac_metadata_[0] = nullptr;
	flac_metadata_[1] = nullptr;
}

AudioFileWrapper::~AudioFileWrapper() {
	cleanup_encoders();
}

// Set metadata for the audio file
void AudioFileWrapper::set_metadata(const Metadata& metadata) {
	metadata_ = metadata;
}

// Set album artwork
void AudioFileWrapper::set_artwork(const std::vector<uint8_t>& data, const std::string& mime_type,
	const std::string& description, uint32_t type) {
	metadata_.artwork.data = data;
	metadata_.artwork.mime_type = mime_type;
	metadata_.artwork.description = description;
	metadata_.artwork.type = type;
}

// Initialize with audio parameters
int AudioFileWrapper::init_audio(AudioFormat format, uint32_t sample_rate, uint16_t channels,
	SampleFormat input_format) {
	output_format_ = format;
	sample_rate_ = sample_rate;
	channels_ = channels;
	input_format_ = input_format;
	header_written_ = false;
	wav_data_size_ = 0;
	mp3_id3v2_size_ = 0;
	mp3_audio_start_pos_ = 0;
	mp3_lame_tag_pos_ = 0;
	mp3_lame_tag_written_ = false;

	cleanup_encoders();

	return setup_encoder();
}

// Override write to handle audio conversion
size_t AudioFileWrapper::write(const void* buf, size_t size, size_t count) {
	if (!is_open() || !buf || size == 0) {
		return 0;
	}

	// Write header if not done yet
	if (!header_written_) {
		if (write_header() != 0) {
			return 0;
		}
		header_written_ = true;
	}

	// Convert and write audio data
	return write_audio_data(buf, size, count);
}

// Finalize audio file (important for MP3 and FLAC)
int AudioFileWrapper::finalize() {
	if (!is_open()) return -1;

	int result = 0;

	switch (output_format_) {
	case WAV_16BIT:
	case WAV_32FLOAT:
		result = finalize_wav();
		break;
	case MP3_LAME:
		result = finalize_mp3();
		break;
	case FLAC_16BIT:
	case FLAC_24BIT:
		result = finalize_flac();
		break;
	}

	return result;
}

// Override close to finalize before closing
int AudioFileWrapper::close() {
	if (is_open()) {
		finalize();
		cleanup_encoders();
	}
	return FileWrapper::close();
}

void AudioFileWrapper::cleanup_encoders() {
	if (lame_flags_) {
		lame_close(lame_flags_);
		lame_flags_ = nullptr;
	}
	if (flac_encoder_) {
		FLAC__stream_encoder_delete(flac_encoder_);
		flac_encoder_ = nullptr;
	}
	// Clean up FLAC metadata
	if (flac_metadata_[0]) {
		FLAC__metadata_object_delete(flac_metadata_[0]);
		flac_metadata_[0] = nullptr;
	}
	if (flac_metadata_[1]) {
		FLAC__metadata_object_delete(flac_metadata_[1]);
		flac_metadata_[1] = nullptr;
	}
}

int AudioFileWrapper::setup_encoder() {
	switch (output_format_) {
	case RAW_16BIT:
	case WAV_16BIT:
	case WAV_32FLOAT:
		return 0; // No special setup needed for WAV

	case MP3_LAME:
		return setup_mp3_encoder();

	case FLAC_16BIT:
	case FLAC_24BIT:
		return setup_flac_encoder();
	}
	return -1;
}

int AudioFileWrapper::setup_mp3_encoder() {
	lame_flags_ = lame_init();
	if (!lame_flags_) return -1;

	if (lame_set_in_samplerate(lame_flags_, sample_rate_) < 0 ||
		lame_set_num_channels(lame_flags_, channels_) < 0 ||
		lame_set_out_samplerate(lame_flags_, sample_rate_) < 0 ||
		lame_set_quality(lame_flags_, 2) < 0 || // High quality
		lame_set_brate(lame_flags_, 320) < 0 || // 320 kbps
		lame_set_mode(lame_flags_, channels_ == 1 ? MONO : STEREO) < 0) {
		lame_close(lame_flags_);
		lame_flags_ = nullptr;
		return -1;
	}

	// Disable automatic ID3 writing - we'll handle it manually
	lame_set_write_id3tag_automatic(lame_flags_, 0);

	// Set metadata in LAME
	if (!metadata_.title.empty()) {
		id3tag_set_title(lame_flags_, metadata_.title.c_str());
	}
	if (!metadata_.artist.empty()) {
		id3tag_set_artist(lame_flags_, metadata_.artist.c_str());
	}
	if (!metadata_.album.empty()) {
		id3tag_set_album(lame_flags_, metadata_.album.c_str());
	}
	if (!metadata_.comment.empty()) {
		id3tag_set_comment(lame_flags_, metadata_.comment.c_str());
	}
	if (!metadata_.genre.empty()) {
		id3tag_set_genre(lame_flags_, metadata_.genre.c_str());
	}
	if (!metadata_.year.empty()) {
		// id3tag_set_year(lame_flags_, metadata_.year.c_str());
	}
	if (!metadata_.track.empty()) {
		id3tag_set_track(lame_flags_, metadata_.track.c_str());
	}
	if (!metadata_.album_artist.empty()) {
		id3tag_set_artist(lame_flags_, metadata_.album_artist.c_str());
	}

	// Add album artwork if present
	if (!metadata_.artwork.data.empty()) {
		id3tag_set_albumart(lame_flags_,
			(const char*)metadata_.artwork.data.data(),
			metadata_.artwork.data.size());
	}

	if (lame_init_params(lame_flags_) < 0) {
		lame_close(lame_flags_);
		lame_flags_ = nullptr;
		return -1;
	}

	return 0;
}

int AudioFileWrapper::setup_flac_encoder() {
	flac_encoder_ = FLAC__stream_encoder_new();
	if (!flac_encoder_) return -1;

	bool success = FLAC__stream_encoder_set_channels(flac_encoder_, channels_) &&
		FLAC__stream_encoder_set_sample_rate(flac_encoder_, sample_rate_) &&
		FLAC__stream_encoder_set_compression_level(flac_encoder_, 5);

	if (output_format_ == FLAC_16BIT) {
		success &= FLAC__stream_encoder_set_bits_per_sample(flac_encoder_, 16);
	}
	else {
		success &= FLAC__stream_encoder_set_bits_per_sample(flac_encoder_, 24);
	}

	if (!success) {
		FLAC__stream_encoder_delete(flac_encoder_);
		flac_encoder_ = nullptr;
		return -1;
	}

	// Setup metadata
	if (setup_flac_metadata() != 0) {
		FLAC__stream_encoder_delete(flac_encoder_);
		flac_encoder_ = nullptr;
		return -1;
	}

	// Set metadata
	FLAC__StreamMetadata* metadata_array[2];
	int metadata_count = 0;

	if (flac_metadata_[0]) metadata_array[metadata_count++] = flac_metadata_[0];
	if (flac_metadata_[1]) metadata_array[metadata_count++] = flac_metadata_[1];

	if (metadata_count > 0) {
		if (!FLAC__stream_encoder_set_metadata(flac_encoder_, metadata_array, metadata_count)) {
			FLAC__stream_encoder_delete(flac_encoder_);
			flac_encoder_ = nullptr;
			return -1;
		}
	}

	// Set up callback for writing encoded data
	if (FLAC__stream_encoder_init_stream(flac_encoder_,
		flac_write_callback,    // Write encoded data
		flac_seek_callback,     // Seek to position  
		flac_tell_callback,     // Get current position
		nullptr,                // Metadata callback (not needed)
		this) != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		FLAC__stream_encoder_delete(flac_encoder_);
		flac_encoder_ = nullptr;
		return -1;
	}

	return 0;
}

int AudioFileWrapper::write_header() {
	switch (output_format_) {
	case WAV_16BIT:
	case WAV_32FLOAT:
		return write_wav_header();
	case MP3_LAME:
		return write_mp3_id3v2_header();
	case RAW_16BIT:
	case FLAC_16BIT:
	case FLAC_24BIT:
		return 0; // FLAC metadata handled by encoder
	}
	return -1;
}

int AudioFileWrapper::write_wav_header() {
	// Use pragma pack for better cross-compiler compatibility
#pragma pack(push, 1)
	struct WAVHeader {
		char riff[4] = { 'R', 'I', 'F', 'F' };
		uint32_t file_size;
		char wave[4] = { 'W', 'A', 'V', 'E' };
		char fmt[4] = { 'f', 'm', 't', ' ' };
		uint32_t fmt_size = 16;
		uint16_t format; // 1 = PCM, 3 = IEEE float
		uint16_t channels;
		uint32_t sample_rate;
		uint32_t byte_rate;
		uint16_t block_align;
		uint16_t bits_per_sample;
		char data[4] = { 'd', 'a', 't', 'a' };
		uint32_t data_size;
	};
#pragma pack(pop)

	WAVHeader header;
	header.channels = channels_;
	header.sample_rate = sample_rate_;

	if (output_format_ == WAV_32FLOAT) {
		header.format = 3; // IEEE float
		header.bits_per_sample = 32;
		header.byte_rate = sample_rate_ * channels_ * 4; // 32-bit samples
		header.block_align = channels_ * 4;
	}
	else { // WAV_16BIT
		header.format = 1; // PCM
		header.bits_per_sample = 16;
		header.byte_rate = sample_rate_ * channels_ * 2; // 16-bit samples
		header.block_align = channels_ * 2;
	}

	header.file_size = 0; // Will be updated in finalize
	header.data_size = 0; // Will be updated in finalize

	wav_header_pos_ = tell();
	size_t written = FileWrapper::write(&header, sizeof(header), 1);
	return (written == 1) ? 0 : -1;
}

size_t AudioFileWrapper::write_audio_data(const void* buf, size_t size, size_t count) {
	switch (output_format_) {
	case RAW_16BIT:
	case WAV_16BIT:
	case WAV_32FLOAT:
		return write_wav_data(buf, size, count);
	case MP3_LAME:
		return write_mp3_data(buf, size, count);
	case FLAC_16BIT:
	case FLAC_24BIT:
		return write_flac_data(buf, size, count);
	}
	return 0;
}





size_t AudioFileWrapper::write_wav_data(const void* buf, size_t size, size_t count) {
	const void* converted_data = buf;
	size_t converted_size = size;
	size_t converted_count = count;

	if (output_format_ == WAV_16BIT || output_format_ == RAW_16BIT) {
		// Convert input to 16-bit if necessary
		if (input_format_ != SAMPLE_INT16) {
			if (!convert_to_int16(buf, size * count)) {
				return 0;
			}
			converted_data = int16_buffer_.data();
			converted_size = sizeof(int16_t);
			converted_count = int16_buffer_.size();
		}
	}
	else { // WAV_32FLOAT
		// Convert input to 32-bit float if necessary
		if (input_format_ != SAMPLE_FLOAT32) {
			if (!convert_to_float32(buf, size * count)) {
				return 0;
			}
			converted_data = float32_buffer_.data();
			converted_size = sizeof(float);
			converted_count = float32_buffer_.size();
		}
	}

	size_t written = FileWrapper::write(converted_data, converted_size, converted_count);
	wav_data_size_ += written * converted_size;

	// Return original count if we did conversion, otherwise return actual written
	return (output_format_ == WAV_16BIT && input_format_ != SAMPLE_INT16) ||
		(output_format_ == WAV_32FLOAT && input_format_ != SAMPLE_FLOAT32) ? count : written;
}

size_t AudioFileWrapper::write_mp3_data(const void* buf, size_t size, size_t count) {
	if (!lame_flags_) return 0;

	// Write LAME tag placeholder on first audio write if not done yet
	if (!mp3_lame_tag_written_) {
		mp3_lame_tag_pos_ = tell();

		// Write placeholder for LAME tag (typically ~417-1441 bytes, we'll use 1500 to be safe)
		std::vector<uint8_t> placeholder(1500, 0);
		FileWrapper::write(placeholder.data(), 1, placeholder.size());
		mp3_lame_tag_written_ = true;
	}

	// Convert input to 16-bit if necessary
	if (input_format_ != SAMPLE_INT16) {
		if (!convert_to_int16(buf, size * count)) {
			return 0;
		}
	}
	else {
		// Copy input data to int16 buffer
		size_t samples = (size * count) / sizeof(int16_t);
			int16_buffer_.resize(samples);
			std::memcpy(int16_buffer_.data(), buf, size * count);
	}

	// Prepare MP3 output buffer
	size_t samples_per_channel = int16_buffer_.size() / channels_;
		conversion_buffer_.resize(static_cast<size_t>(samples_per_channel * 1.25 + 7200)); // LAME recommendation
	
	int encoded_bytes;
	if (channels_ == 1) {
		encoded_bytes = lame_encode_buffer(lame_flags_,
			int16_buffer_.data(), nullptr, samples_per_channel,
			conversion_buffer_.data(), conversion_buffer_.size());
	}
	else {
		// Interleaved stereo to separate channels
			std::vector<int16_t> left(samples_per_channel), right(samples_per_channel);
			for (size_t i = 0; i < samples_per_channel; i++) {
				left[i] = int16_buffer_[i * 2];
				right[i] = int16_buffer_[i * 2 + 1];
			}

			encoded_bytes = lame_encode_buffer(lame_flags_,
				left.data(), right.data(), samples_per_channel,
				conversion_buffer_.data(), conversion_buffer_.size());
		
	}

	if (encoded_bytes < 0) {
		return 0; // LAME error
	}

	if (encoded_bytes > 0) {
		FileWrapper::write(conversion_buffer_.data(), 1, encoded_bytes);
	}

	return count;
}

size_t AudioFileWrapper::write_flac_data(const void* buf, size_t size, size_t count) {
	if (!flac_encoder_) return 0;

	size_t total_bytes = size * count;
	size_t samples_per_channel;

	// Convert input to appropriate format
	if (output_format_ == FLAC_16BIT) {
		if (input_format_ != SAMPLE_INT16) {
			if (!convert_to_int16(buf, total_bytes)) return 0;
		}
		else {
			size_t samples = total_bytes / sizeof(int16_t);
				int16_buffer_.resize(samples);
				std::memcpy(int16_buffer_.data(), buf, total_bytes);
		}
		samples_per_channel = int16_buffer_.size() / channels_;

		// Convert to 32-bit for FLAC encoder
			int24_buffer_.resize(int16_buffer_.size());
			for (size_t i = 0; i < int16_buffer_.size(); i++) {
				int24_buffer_[i] = static_cast<int32_t>(int16_buffer_[i]);
			}
		
	}
	else { // FLAC_24BIT
		if (!convert_to_int24(buf, total_bytes)) return 0;
		samples_per_channel = int24_buffer_.size() / channels_;
	}

	// Encode with FLAC
	FLAC__bool result = FLAC__stream_encoder_process_interleaved(flac_encoder_,
		int24_buffer_.data(), samples_per_channel);

	if (!result) {
		return 0; // FLAC encoding error
	}

	return count;
}

constexpr int16_t to16Bit(double fval) {
	fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
	fval *= 32768.0;
	auto sample = static_cast<int32_t>(fval);
	// clip to 16-bit range
	if (sample < 0) sample = 0;
	else if (sample > 0x0FFFF) sample = 0x0FFFF;
	sample -= 32768; // center at zero
	return  static_cast<int16_t>(sample);
}

bool AudioFileWrapper::convert_to_int16(const void* buf, size_t bytes) {
		switch (input_format_) {
		case SAMPLE_INT16:
			// Direct copy
			int16_buffer_.resize(bytes / sizeof(int16_t));
			std::memcpy(int16_buffer_.data(), buf, bytes);
			return true;

		case SAMPLE_FLOAT32: {
			size_t samples = bytes / sizeof(float);
			int16_buffer_.resize(samples);
			const float* float_data = static_cast<const float*>(buf);

			for (size_t i = 0; i < samples; i++) {
				int16_buffer_[i] = to16Bit(float_data[i]);
			}
			return true;
		}

		case SAMPLE_FLOAT64: {
			size_t samples = bytes / sizeof(double);
			int16_buffer_.resize(samples);
			const double* float_data = static_cast<const double*>(buf);

			for (size_t i = 0; i < samples; i++) {
				int16_buffer_[i] = to16Bit(float_data[i]);
			}
			return true;
		}

		case SAMPLE_INT24: {
			size_t samples = bytes / 3; // 3 bytes per 24-bit sample
			int16_buffer_.resize(samples);
			const uint8_t* byte_data = static_cast<const uint8_t*>(buf);

			for (size_t i = 0; i < samples; i++) {
				// Convert 24-bit to 16-bit (little endian)
				int32_t sample = (byte_data[i * 3] | (byte_data[i * 3 + 1] << 8) | (byte_data[i * 3 + 2] << 16));
				if (sample & 0x800000) sample |= 0xFF000000; // Sign extend
				int16_buffer_[i] = static_cast<int16_t>(sample >> 8);
			}
			return true;
		}
		}
	
	return false;
}


constexpr int32_t to24Bit(double fval) {
	//CONV24BIT = (8388608.0); // 2^23, to convert to 24-bit signed integer range
	fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
	fval *= 8388608.0;
	auto sample = static_cast<int32_t>(fval);
	// clip to 16-bit range
	if (sample < 0) sample = 0;
	else if (sample > 0x0FFFFFF16) sample = 0x0FFFFFF16;
	sample -= 8388608; // center at zero
	return  static_cast<int32_t>(sample);
}

bool AudioFileWrapper::convert_to_int24(const void* buf, size_t bytes) {
		switch (input_format_) {
		case SAMPLE_INT16: {
			size_t samples = bytes / sizeof(int16_t);
			int24_buffer_.resize(samples);
			const int16_t* int16_data = static_cast<const int16_t*>(buf);

			for (size_t i = 0; i < samples; i++) {
				int24_buffer_[i] = static_cast<int32_t>(int16_data[i]) << 8;
			}
			return true;
		}

		case SAMPLE_FLOAT32: {
			size_t samples = bytes / sizeof(float);
			int24_buffer_.resize(samples);
			const float* float_data = static_cast<const float*>(buf);

			for (size_t i = 0; i < samples; i++) {
				int24_buffer_[i] = to24Bit(float_data[i]);
			}
			return true;
		}

		case SAMPLE_FLOAT64: {
			size_t samples = bytes / sizeof(double);
			int24_buffer_.resize(samples);
			const double* float_data = static_cast<const double*>(buf);

			for (size_t i = 0; i < samples; i++) {
				int24_buffer_[i] = to24Bit(float_data[i]);
			}
			return true;
		}

		case SAMPLE_INT24: {
			size_t samples = bytes / 3;
			int24_buffer_.resize(samples);
			const uint8_t* byte_data = static_cast<const uint8_t*>(buf);

			for (size_t i = 0; i < samples; i++) {
				int32_t sample = (byte_data[i * 3] | (byte_data[i * 3 + 1] << 8) | (byte_data[i * 3 + 2] << 16));
				if (sample & 0x800000) sample |= 0xFF000000; // Sign extend
				int24_buffer_[i] = sample;
			}
			return true;
		}
		}
	return false;
}

bool AudioFileWrapper::convert_to_float32(const void* buf, size_t bytes) {
		switch (input_format_) {
		case SAMPLE_FLOAT32: {
			// Direct copy
			float32_buffer_.resize(bytes / sizeof(float));
			std::memcpy(float32_buffer_.data(), buf, bytes);
			return true;
		}
		case SAMPLE_FLOAT64: {
			size_t samples = bytes / sizeof(double);
			float32_buffer_.resize(samples);
			const double* double_data = static_cast<const double*>(buf);

			for (size_t i = 0; i < samples; i++) {
				float32_buffer_[i] = static_cast<float>(double_data[i]);
			}
			return true;
		}

		case SAMPLE_INT16: {
			size_t samples = bytes / sizeof(int16_t);
			float32_buffer_.resize(samples);
			const int16_t* int16_data = static_cast<const int16_t*>(buf);

			for (size_t i = 0; i < samples; i++) {
				float32_buffer_[i] = static_cast<float>(int16_data[i]) / 32768.0f;
			}
			return true;
		}

		case SAMPLE_INT24: {
			size_t samples = bytes / 3; // 3 bytes per 24-bit sample
			float32_buffer_.resize(samples);
			const uint8_t* byte_data = static_cast<const uint8_t*>(buf);

			for (size_t i = 0; i < samples; i++) {
				// Convert 24-bit to float (little endian)
				int32_t sample = (byte_data[i * 3] | (byte_data[i * 3 + 1] << 8) | (byte_data[i * 3 + 2] << 16));
				if (sample & 0x800000) sample |= 0xFF000000; // Sign extend
				float32_buffer_[i] = static_cast<float>(sample) / 8388608.0f;
			}
			return true;
		}
		}
	
	return false;
}

int AudioFileWrapper::finalize_wav() {
	// Update WAV header with correct file sizes
	auto current_pos = tell();
	if (current_pos == -1) return -1;

	// Update file size (total file size - 8)
	uint32_t file_size = static_cast<uint32_t>(current_pos - 8);
	if (seek(wav_header_pos_ + 4, SEEK_SET) == -1) return -1;
	if (FileWrapper::write(&file_size, sizeof(file_size), 1) != 1) return -1;

	// Update data size
	if (seek(wav_header_pos_ + 40, SEEK_SET) == -1) return -1;
	if (FileWrapper::write(&wav_data_size_, sizeof(wav_data_size_), 1) != 1) return -1;

	if (seek(current_pos, SEEK_SET) == -1) return -1;
	return 0;
}

int AudioFileWrapper::finalize_mp3() {
	if (!lame_flags_) return -1;

	// Flush remaining MP3 data
		conversion_buffer_.resize(7200); // LAME recommendation for flush buffer
	
	int encoded_bytes = lame_encode_flush(lame_flags_,
		conversion_buffer_.data(), conversion_buffer_.size());

	if (encoded_bytes < 0) return -1;

	if (encoded_bytes > 0) {
		FileWrapper::write(conversion_buffer_.data(), 1, encoded_bytes);
	}

	// Now write the actual LAME tag in the reserved space
	if (mp3_lame_tag_written_) {
			conversion_buffer_.resize(2880); // Size for LAME tag
		
		int tag_bytes = lame_get_lametag_frame(lame_flags_,
			conversion_buffer_.data(), conversion_buffer_.size());

		if (tag_bytes > 0) {
			// Save current position
			off_t current_pos = tell();
			if (current_pos == -1) return -1;

			// Seek to the LAME tag position
			if (seek(mp3_lame_tag_pos_, SEEK_SET) == -1) return -1;

			// Write the actual LAME tag (overwriting the placeholder)
			FileWrapper::write(conversion_buffer_.data(), 1, tag_bytes);

			// Restore position
			if (seek(current_pos, SEEK_SET) == -1) return -1;
		}
	}

	return 0;
}

int AudioFileWrapper::setup_flac_metadata() {
	// Create vorbis comment block
	flac_metadata_[0] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
	if (!flac_metadata_[0]) return -1;

	// Add text metadata
	FLAC__StreamMetadata_VorbisComment_Entry entry;

	auto add_field = [&](const std::string& field, const std::string& value) {
		if (!value.empty()) {
			if (FLAC__metadata_object_vorbiscomment_entry_from_name_value_pair(&entry,
				field.c_str(), value.c_str())) {
				FLAC__metadata_object_vorbiscomment_append_comment(flac_metadata_[0], entry, false);
			}
		}
		};

	add_field("TITLE", metadata_.title);
	add_field("ARTIST", metadata_.artist);
	add_field("ALBUM", metadata_.album);
	add_field("COMMENT", metadata_.comment);
	add_field("GENRE", metadata_.genre);
	add_field("DATE", metadata_.year);
	add_field("TRACKNUMBER", metadata_.track);
	add_field("ALBUMARTIST", metadata_.album_artist);

	// Create picture block if artwork exists
	if (!metadata_.artwork.data.empty()) {
		flac_metadata_[1] = FLAC__metadata_object_new(FLAC__METADATA_TYPE_PICTURE);
		if (flac_metadata_[1]) {
			FLAC__StreamMetadata_Picture* picture = &flac_metadata_[1]->data.picture;

			picture->type = static_cast<FLAC__StreamMetadata_Picture_Type>(metadata_.artwork.type);

			// Set mime type
			if (FLAC__metadata_object_picture_set_mime_type(flac_metadata_[1],
				(char*)metadata_.artwork.mime_type.c_str(), true)) {

				// Set description
				if (FLAC__metadata_object_picture_set_description(flac_metadata_[1],
					(FLAC__byte*)metadata_.artwork.description.c_str(), true)) {

					// Set picture data
					if (!FLAC__metadata_object_picture_set_data(flac_metadata_[1],
						metadata_.artwork.data.data(), metadata_.artwork.data.size(), true)) {
						FLAC__metadata_object_delete(flac_metadata_[1]);
						flac_metadata_[1] = nullptr;
					}
				}
				else {
					FLAC__metadata_object_delete(flac_metadata_[1]);
					flac_metadata_[1] = nullptr;
				}
			}
			else {
				FLAC__metadata_object_delete(flac_metadata_[1]);
				flac_metadata_[1] = nullptr;
			}
		}
	}

	return 0;
}

int AudioFileWrapper::write_mp3_id3v2_header() {
	if (!lame_flags_) return -1;

	// Get ID3v2 tag size
		conversion_buffer_.resize(128 * 1024); // Large buffer for ID3v2
	
	int id3v2_size = lame_get_id3v2_tag(lame_flags_,
		conversion_buffer_.data(), conversion_buffer_.size());

	if (id3v2_size < 0) return -1;

	if (id3v2_size > 0) {
		mp3_id3v2_size_ = id3v2_size;
		mp3_audio_start_pos_ = id3v2_size;
		FileWrapper::write(conversion_buffer_.data(), 1, id3v2_size);
	}
	else {
		mp3_id3v2_size_ = 0;
		mp3_audio_start_pos_ = 0;
	}

	return 0;
}

int AudioFileWrapper::finalize_flac() {
	if (!flac_encoder_) return -1;

	FLAC__bool result = FLAC__stream_encoder_finish(flac_encoder_);
	return result ? 0 : -1;
}

// FLAC callback for writing encoded data
FLAC__StreamEncoderWriteStatus AudioFileWrapper::flac_write_callback(
	const FLAC__StreamEncoder* encoder,
	const FLAC__byte buffer[],
	size_t bytes,
	unsigned samples,
	unsigned current_frame,
	void* client_data) {

	AudioFileWrapper* wrapper = static_cast<AudioFileWrapper*>(client_data);
	if (!wrapper) return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;

	size_t written = wrapper->FileWrapper::write(buffer, 1, bytes);

	return (written == bytes) ?
		FLAC__STREAM_ENCODER_WRITE_STATUS_OK :
		FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
}

FLAC__StreamEncoderSeekStatus AudioFileWrapper::flac_seek_callback(
	const FLAC__StreamEncoder* encoder, FLAC__uint64 absolute_byte_offset, void* client_data) {
	AudioFileWrapper* wrapper = static_cast<AudioFileWrapper*>(client_data);
	if (!wrapper) return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;

	if (wrapper->seek(absolute_byte_offset, SEEK_SET) >= 0) {
		return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
	}
	return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
}

FLAC__StreamEncoderTellStatus AudioFileWrapper::flac_tell_callback(
	const FLAC__StreamEncoder* encoder, FLAC__uint64* absolute_byte_offset, void* client_data) {
	AudioFileWrapper* wrapper = static_cast<AudioFileWrapper*>(client_data);
	if (!wrapper || !absolute_byte_offset) return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;

	off_t pos = wrapper->tell();
	if (pos >= 0) {
		*absolute_byte_offset = pos;
		return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
	}
	return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
}

void AudioFileWrapper::move_from(AudioFileWrapper&& other) noexcept {
	output_format_ = other.output_format_;
	input_format_ = other.input_format_;
	sample_rate_ = other.sample_rate_;
	channels_ = other.channels_;
	header_written_ = other.header_written_;
	metadata_ = std::move(other.metadata_);

	conversion_buffer_ = std::move(other.conversion_buffer_);
	int16_buffer_ = std::move(other.int16_buffer_);
	int24_buffer_ = std::move(other.int24_buffer_);
	float32_buffer_ = std::move(other.float32_buffer_);

	// Transfer ownership of encoders
	lame_flags_ = other.lame_flags_;
	flac_encoder_ = other.flac_encoder_;
	flac_metadata_[0] = other.flac_metadata_[0];
	flac_metadata_[1] = other.flac_metadata_[1];

	wav_data_size_ = other.wav_data_size_;
	wav_header_pos_ = other.wav_header_pos_;
	mp3_id3v2_size_ = other.mp3_id3v2_size_;
	mp3_audio_start_pos_ = other.mp3_audio_start_pos_;
	mp3_lame_tag_pos_ = other.mp3_lame_tag_pos_;
	mp3_lame_tag_written_ = other.mp3_lame_tag_written_;

	// Clear source
	other.lame_flags_ = nullptr;
	other.flac_encoder_ = nullptr;
	other.flac_metadata_[0] = nullptr;
	other.flac_metadata_[1] = nullptr;
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

