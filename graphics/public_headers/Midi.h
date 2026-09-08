#pragma once

#include "types.h"
#include "defines.h"
#include <string>
#include <map>

#define NUM_MIDICHANNELS 16
#define NUM_MIDI_CONTROL 120
#define NUM_MIDI_NOTEON 127

namespace tsl {
    namespace midi {

        static constexpr double MIDIDIVISOR = 1.0 / 127.0;

        static constexpr uint8_t STATUS_REALTIME_EVENT = 0x01;
        static constexpr uint8_t STATUS_COMMAND_MASK = 0xF0;
        static constexpr uint8_t STATUS_CHANNEL_MASK = 0x0F;
        static constexpr uint8_t STATUS_NOTE_OFF = 0x80;
        static constexpr uint8_t STATUS_NOTE_ON = 0x90;
        static constexpr uint8_t STATUS_POLYPHONIC_AFTERTOUCH = 0xA0;
        static constexpr uint8_t STATUS_CONTROL_CHANGE = 0xB0;
        static constexpr uint8_t STATUS_PROGRAM_CHANGE = 0xC0;
        static constexpr uint8_t STATUS_CHANNEL_PRESSURE = 0xD0;
        static constexpr uint8_t STATUS_PITCH_BEND = 0xE0;
        static constexpr uint8_t STATUS_SYSTEM_EXCLUSIVE = 0xF0;
        static constexpr uint8_t STATUS_MIDI_TIME_CODE = 0xF1;
        static constexpr uint8_t STATUS_SONG_POSITION = 0xF2;
        static constexpr uint8_t STATUS_SONG_SELECT = 0xF3;
        static constexpr uint8_t STATUS_TUNE_REQUEST = 0xF6;
        static constexpr uint8_t STATUS_END_SYSEX = 0xF7;
        static constexpr uint8_t STATUS_TIMING_CLOCK = 0xF8;
        static constexpr uint8_t STATUS_START = 0xFA;
        static constexpr uint8_t STATUS_CONTINUE = 0xFB;
        static constexpr uint8_t STATUS_STOP = 0xFC;
        static constexpr uint8_t STATUS_ACTIVE_SENSING = 0xFE;
        static constexpr uint8_t STATUS_RESET = 0xFF;

        static constexpr int CHANNEL_BYTE_LENGTHS[] = { 3, 3, 3, 3, 2, 2, 3 };
        static constexpr int SYSTEM_BYTE_LENGTHS[] = { 1, 2, 3, 2, 1, 1, 1, 1, 1,
                                                        1, 1, 1, 1, 1, 1, 1 };

        extern const std::map<std::string, int> midiNoteNames;

        namespace MidiNotes {
            float       midiToFreq(int note);
            int         freqToMidi(float freq);
            int         noteNameToMidi(const std::string& noteName);
            float       noteNameToFreq(const std::string& noteName);
            std::string midiToNoteName(int note);
            std::string freqToNoteName(float freq);
        }

    } // namespace midi
} // namespace tsl