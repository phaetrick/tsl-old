#include "defines.h"
#include "logger.h"
#include "tools.h"
#include <cstdlib>
#include <cmath>
#include <chrono>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>


// Characters not allowed in filenames on most platforms
static constexpr const char* INVALID_CHARS = "\\/:?\"<>|*";

std::string tsl::trimToValidFilename(std::string name) {
	// Step 1: Trim leading/trailing whitespace
	const auto start = name.find_first_not_of(" \t\n\r\f\v");
	const auto end = name.find_last_not_of(" \t\n\r\f\v");
	if (start == std::string::npos)
		return "untitled"; // fallback if name is all whitespace

	name = name.substr(start, end - start + 1);

	// Step 2: Replace invalid filename characters with '_'
	std::replace_if(name.begin(), name.end(), [](char c) {
		return std::string(INVALID_CHARS).find(c) != std::string::npos;
		}, '_');

	// Step 3: Ensure it's not empty
	if (name.empty())
		return "untitled";
	return name;
}


std::string tsl::generateTimestampedName(const std::string& prefix) {
	auto now = std::chrono::system_clock::now();
	std::time_t now_time = std::chrono::system_clock::to_time_t(now);

	std::tm tm;
#if defined(_WIN32)
	localtime_s(&tm, &now_time);  // Windows-safe
#else
	localtime_r(&now_time, &tm);  // POSIX-safe
#endif

	std::ostringstream oss;
	oss << prefix << "_"
		<< std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");

	return oss.str();
}





struct timespec diff(struct timespec start, struct timespec end) {
	struct timespec temp;
	if ((end.tv_nsec - start.tv_nsec) < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec - 1;
		temp.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
	}
	else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp;
}


void time_convert_p(TIME_P& t, int sr, double offset, float speed) {
	if (speed != 0 && offset > 0)
		time_convert(t, sr, offset / std::abs(speed));
	else
		time_convert(t, 0, 0);
}

void time_convert(TIME_P& time_p, int sr, double samples)
{
	if (samples <= 0.0 || sr <= 0)
	{
		time_p.h = 0;
		time_p.m = 0;
		time_p.s = 0;
		time_p.ms = 0;
		return;
	}

	// samples -> seconds
	double totalSeconds = samples / (double)sr;

	// decompose
	time_p.h = (unsigned int)(totalSeconds / 3600.0);
	totalSeconds -= time_p.h * 3600.0;

	time_p.m = (unsigned int)(totalSeconds / 60.0);
	totalSeconds -= time_p.m * 60.0;

	time_p.s = (unsigned int)(totalSeconds);
	totalSeconds -= time_p.s;

	time_p.ms = (unsigned int)(totalSeconds * 1000.0);
}

bool isround(double num) {
	return (num - floor(num) != 0);
}

uint32_t largestdivisor(uint32_t number, uint32_t start) {
	for (int i = start; i <= number / 2; i++) {
		if (number % i == 0 && number != i) {
			return number / i;
		}//end if
	}
	return 1;
}

int16_t tsl::to16Bit(double fval) {
	fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
	fval *= 32768.0;
	auto sample = static_cast<int32_t>(fval);
	// clip to 16-bit range
	if (sample < 0) sample = 0;
	else if (sample > 0x0FFFF) sample = 0x0FFFF;
	sample -= 32768; // center at zero
	return  static_cast<int16_t>(sample);
}

int32_t tsl::to24Bit(double fval) {
	fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
	fval *= CONV24BIT;
	auto sample = static_cast<int32_t>(fval);
	// clip to 16-bit range
	if (sample < 0) sample = 0;
	else if (sample > 0x0FFFFFF16) sample = 0x0FFFFFF16;
	sample -= CONV24BIT; // center at zero
	return  static_cast<int32_t>(sample);
}


void convertFloatToPcm16(const float* source, int16_t* destination, int32_t numSamples) {
	for (int i = 0; i < numSamples; i++) {
		float fval = source[i];
		fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
		fval *= 32768.0f;
		auto sample = static_cast<int32_t>(fval);
		// clip to 16-bit range
		if (sample < 0) sample = 0;
		else if (sample > 0x0FFFF) sample = 0x0FFFF;
		sample -= 32768; // center at zero
		destination[i] = static_cast<int16_t>(sample);
	}
}

void convertFloatToPcm24(const float* in, int32_t* out, int s) {
	for (int i = 0; i < s; i++) {
		out[i] = ((int32_t)(in[i] * CONV24BIT));
	}
};


void convertFloatToPcm32(const float* source, int32_t* destination, int32_t numSamples) {
	for (int i = 0; i < numSamples; i++) {
		float fval = source[i];
		fval += 1.0; // to avoid discontinuity at 0.0 caused by truncation
		fval *= 32768.0f;
		int32_t sample = static_cast<int32_t>(fval);
		// clip to 16-bit range
		if (sample < 0) sample = 0;
		else if (sample > 0x0FFFF) sample = 0x0FFFF;
		sample -= 32768; // center at zero
		destination[i] = sample;
	}
}


int findIndexFloatWithLen(const float array[], float target, int len) {
	for (int i = 0; i < len; i++)
		if (array[i] == target)
			return i;
	return -1;
}

int findIndexFloat(const float array[], float target) {
	int i = 0;
	while (array[i] != target) i++;

	return i;
}

int findIndexChar(const std::string_view array[],
	size_t size,
	const char* target)
{
	for (size_t i = 0; i < size; ++i)
		if (array[i] == target)
			return static_cast<int>(i);

	return -1;
}


unsigned NextPow2(unsigned x) {
	--x;
	x |= x >> 1u;
	x |= x >> 2u;
	x |= x >> 4u;
	x |= x >> 8u;
	x |= x >> 16u;
	return ++x;
}


static void normalize(float* dest, float* src, int s) {
	float* maxsrc = std::max_element(src, src + s);
	float* maxdest = std::max_element(dest, dest + s);
	if (maxdest != nullptr && *maxdest != 0) {
		float norm = *maxsrc / *maxdest;
		for (int i = 0; i < s; i++)
			dest[i] *= norm;
	}
}



inline bool isPrime(int number) {
	if (number == 2) return true;
	if (number & 1u) {
		for (int i = 3; i < (int)sqrt((double)number) + 1; i += 2)
			if ((number % i) == 0) return false;
		return true; // prime
	}
	else return false; // even
}

int findNextPrime(int n) {
	if ((n & 1) == 0) n++;
	while (!isPrime(n)) n += 2;
	return n;
}


void printheader(const unsigned char* data, int size, const char* symbol) {
	std::vector<unsigned char> tmp(size + 4, 0);
	memcpy(tmp.data(), data, size);
	std::vector<char> out;
	char buf[500];


	snprintf(buf, 500,
		"static const unsigned int %s_%ssize = %d;\n", symbol, "", (int)size);
	out.reserve(out.size() + strlen(buf));
	for (int i = 0; i < strlen(buf); i++)
		out.push_back(buf[i]);
	snprintf(buf, 500,
		"static const unsigned int %s_%sdata[%d/4] =\n{", symbol, "",
		(int)((size + 3) / 4) * 4);
	out.reserve(out.size() + strlen(buf));

	for (int i = 0; i < strlen(buf); i++)
		out.push_back(buf[i]);

	int column = 0;
	for (int i = 0; i < size; i += 4) {
		unsigned int d = *(unsigned int*)(tmp.data() + i);
		if ((column++ % 48) == 0)
			snprintf(buf, 500, "\n    0x%08x, ", d);
		else
			snprintf(buf, 500, "0x%08x, ", d);
		out.reserve(out.size() + strlen(buf));
		for (int j = 0; j < strlen(buf); j++)
			out.push_back(buf[j]);
	}
	snprintf(buf, 500, "\n};\n\n");
	out.reserve(out.size() + strlen(buf));

	for (int j = 0; j < strlen(buf); j++)
		out.push_back(buf[j]);

	//logString(out.data());
		//LOGE("Size %d %s", out.size(), out.data());
}