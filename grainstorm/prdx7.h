#pragma once
//
// Created by pr on 06.10.20.
//
//
// Created by pr on 30.06.20.
//

#ifndef PIANO_PHASE_PRDX7_H
#define PIANO_PHASE_PRDX7_H

#pragma once

#include <stdint.h>
#include <android/log.h>
#include <sys/types.h>
#include "defines.h"

enum dx7_events {
    DX7_NOTE_OFF = 0,
    DX7_NOTE_ON,
    DX7_KEY_PRESS,
    DX7_CONTROLLER,
    DX7_CHANPRESS,
    DX7_PITCHBEND
};


struct dx7_snd_event_t {
    uint8_t type{};
    uint8_t note{};
    uint8_t velocity{};
    uint8_t param{};
    uint8_t value{};
};

#define PRDX7_USE_FLOATING_POINT 1


#define PRDX7_MAX_POLYPHONY      64
#define PRDX7_DEFAULT_POLYPHONY  10

#define PRDX7_NUGGET_SIZE    64

#define PRDX7_PORT_OUTPUT  0
#define PRDX7_PORT_TUNING  1
#define PRDX7_PORT_VOLUME  2
#define PRDX7_PORTS_COUNT  3

#define DX_PROGRAM_BUFFER_SIZE         256
#define DX7_VOICE_SIZE_PACKED          128
#define DX7_VOICE_SIZE_UNPACKED        155
#define DX7_VOICE_PARAMETERS           146
#define DX7_PERFORMANCE_SIZE            64
#define DX7_DUMP_SIZE_VOICE_SINGLE   155+8
#define DX7_DUMP_SIZE_VOICE_BULK    4096+8

struct prdx7_synth_t;
struct prdx7_instance_t;
struct dx7_patch_t;
struct dx7_voice_t;
struct dx7_op_eg_t;
struct dx7_pitch_eg_t;
struct dx7_portamento_t;
struct dx7_op_t;

#ifndef PRDX7_USE_FLOATING_POINT
// #warning Note: using fixed point
typedef int32_t dx7_sample_t;
#else /* PRDX7_USE_FLOATING_POINT */
// #warning Note: using floating point
typedef float dx7_sample_t;
#endif /* PRDX7_USE_FLOATING_POINT */


#define DSSP_MONO_MODE_OFF  0
#define DSSP_MONO_MODE_ON   1
#define DSSP_MONO_MODE_ONCE 2
#define DSSP_MONO_MODE_BOTH 3

struct dx7_patch_t
{
    uint8_t data[128]{};  /* dx7_patch_t is packed patch data */
};


enum dx7_eg_mode {
    DX7_EG_FINISHED,
    DX7_EG_RUNNING,
    DX7_EG_SUSTAINING,
    DX7_EG_CONSTANT
};

struct dx7_op_eg_t   /* operator (amplitude) envelope generator */
{
    uint8_t base_rate[4];
    uint8_t base_level[4];
    uint8_t rate[4];
    uint8_t level[4];

    int32_t mode;        /* enum dx7_eg_mode (finished, running, sustaining, constant) */
    int32_t phase;       /* 0, 1, 2, or 3 */
    dx7_sample_t value;
    int32_t duration;    /* op envelope durations are in frames */
    dx7_sample_t increment;
    dx7_sample_t target;
    int32_t in_precomp;
    int32_t postcomp_duration;
    dx7_sample_t postcomp_increment;
};

struct dx7_pitch_eg_t   /* pitch envelope generator */
{
    uint8_t rate[4];
    uint8_t level[4];

    int32_t mode;        /* enum dx7_eg_mode (finished, running, sustaining, constant) */
    int32_t phase;       /* 0, 1, 2, or 3 */
    double value;       /* in semitones, zero when level is 50 */
    int32_t duration;    /* pitch envelope durations are in bursts ('nuggets') */
    double increment;
    double target;
};

struct dx7_portamento_t  /* portamento generator */
{
    int32_t segment;    /* ... 3, 2, 1, or 0 */
    double value;      /* in semitones, zero is destination pitch */
    int32_t duration;   /* portamento segments are in bursts */
    double increment;
    double target;
};

enum dx7_ops {
    OP_1 = 0,
    OP_2,
    OP_3,
    OP_4,
    OP_5,
    OP_6,
    MAX_DX7_OPERATORS
};


struct dx7_op_t   /* operator */
{
    double frequency;
    dx7_sample_t phase;
    dx7_sample_t phase_increment;

    dx7_op_eg_t eg;

    uint8_t level_scaling_bkpoint;
    uint8_t level_scaling_l_depth;
    uint8_t level_scaling_r_depth;
    uint8_t level_scaling_l_curve;
    uint8_t level_scaling_r_curve;
    uint8_t rate_scaling;
    uint8_t amp_mod_sens;
    uint8_t velocity_sens;
    uint8_t output_level;
    uint8_t osc_mode;
    uint8_t coarse;
    uint8_t fine;
    uint8_t detune;
};

enum dx7_lfo_status {
    DX7_LFO_DELAY,
    DX7_LFO_FADEIN,
    DX7_LFO_ON
};

enum dx7_voice_status {
    DX7_VOICE_OFF,       /* silent: is not processed by render loop */
    DX7_VOICE_ON,        /* has not received a note off event */
    DX7_VOICE_SUSTAINED, /* has received note off, but sustain controller is on */
    DX7_VOICE_RELEASED   /* had note off, not sustained, in final decay phase of envelopes */
};

/*
 * dx7_voice_t
 */
struct dx7_voice_t {
    prdx7_instance_t *instance{};

    uint32_t note_id{};

    unsigned char status{DX7_VOICE_OFF};
    unsigned char key{};
    unsigned char velocity{};
    unsigned char rvelocity{};   /* the note-off velocity */

    /* persistent voice state */
    dx7_op_t op[MAX_DX7_OPERATORS]{};

    double last_pitch{};
    dx7_pitch_eg_t pitch_eg{};
    dx7_portamento_t portamento{};
    float last_port_tuning{};
    double pitch_mod_depth_pmd{};
    double pitch_mod_depth_mods{};

    uint8_t algorithm{};
    dx7_sample_t feedback{};
    dx7_sample_t feedback_multiplier{};
    uint8_t osc_key_sync{};

    uint8_t lfo_speed{};
    uint8_t lfo_delay{};
    uint8_t lfo_pmd{};
    uint8_t lfo_amd{};
    uint8_t lfo_key_sync{};
    uint8_t lfo_wave{};
    uint8_t lfo_pms{};

    int32_t transpose{};

    /* modulation */
    int32_t mods_serial{};
    dx7_sample_t amp_mod_env_value{};
    int32_t amp_mod_env_duration{};
    dx7_sample_t amp_mod_env_increment{};
    dx7_sample_t amp_mod_env_target{};
    dx7_sample_t amp_mod_lfo_mods_value{};
    int32_t amp_mod_lfo_mods_duration{};
    dx7_sample_t amp_mod_lfo_mods_increment{};
    dx7_sample_t amp_mod_lfo_mods_target{};
    dx7_sample_t amp_mod_lfo_amd_value{};
    int32_t amp_mod_lfo_amd_duration{};
    dx7_sample_t amp_mod_lfo_amd_increment{};
    dx7_sample_t amp_mod_lfo_amd_target{};
    int32_t lfo_delay_segment{};
    dx7_sample_t lfo_delay_value{};
    int32_t lfo_delay_duration{};
    dx7_sample_t lfo_delay_increment{};

    /* volume */
    float last_port_volume{};
    unsigned long last_cc_volume{};
    float volume_value{};
    int32_t volume_duration{};
    float volume_increment{};
    float volume_target{};
};

/*
 * prdx7_instance_t
 */
struct prdx7_instance_t {
    prdx7_instance_t *next;

    /* output */
    /* input */
    float tuning{440.f};
    float volume{120};

    float sample_rate{};
    float nugget_rate{};       /* nuggets per second */
    int32_t ramp_duration{};     /* frames per ramp for mods and volume */
    dx7_sample_t dx7_eg_max_slew{};   /* max op eg increment, in units per frame */

    /* voice tracking */
    int32_t polyphony{};         /* requested polyphony, must be <= PRDX7_MAX_POLYPHONY */
    int32_t monophonic{};        /* true if operating in monophonic mode */
    int32_t max_voices{};        /* current max polyphony, either requested polyphony above or 1 while in monophonic mode */
    int32_t current_voices{};    /* count of currently playing voices */
    dx7_voice_t *mono_voice{};
    unsigned char last_key{};          /* portamento starting key */
    signed char held_keys[8]{};      /* for monophonic key tracking, an array of note-ons, most recently received first */

    /* patches and edit buffer */
    pthread_mutex_t patches_mutex;
    int32_t pending_program_change{};

    dx7_patch_t patches[DX_PROGRAM_BUFFER_SIZE]{};

    int32_t current_program{};
    uint8_t current_patch_buffer[DX7_VOICE_SIZE_UNPACKED]{};  /* current unpacked patch in use */

    int32_t overlay_program{};   /* program to which 'configure edit_buffer' patch applies, or -1 */
    uint8_t overlay_patch_buffer[DX7_VOICE_SIZE_UNPACKED]{};  /* 'configure edit_buffer' patch */

    /* global performance parameter buffer */
    uint8_t performance_buffer[DX7_PERFORMANCE_SIZE]{};

    /* current performance perameters (from global buffer or current patch) */
    uint8_t pitch_bend_range{};         /* in semitones */
    uint8_t portamento_time{};
    uint8_t mod_wheel_sensitivity{};
    uint8_t mod_wheel_assign{};
    uint8_t foot_sensitivity{};
    uint8_t foot_assign{};
    uint8_t pressure_sensitivity{};
    uint8_t pressure_assign{};
    uint8_t breath_sensitivity{};
    uint8_t breath_assign{};

    /* current non-LADSPA-port-mapped controller values */
    unsigned char key_pressure[128]{};
    unsigned char cc[128]{};                  /* controller values */
    unsigned char channel_pressure{};
    int32_t pitch_wheel{};              /* range is -8192 - 8191 */

    /* translated port and controller values */
    double fixed_freq_multiplier{};
    unsigned long cc_volume{};                /* volume msb*128 + lsb, max 16256 */
    double pitch_bend{};               /* frequency shift, in semitones */
    int32_t mods_serial{};
    float mod_wheel{};
    float foot{};
    float breath{};

    uint8_t lfo_speed{};
    uint8_t lfo_wave{};
    uint8_t lfo_delay{};
    dx7_sample_t lfo_delay_value[3]{};
    int32_t lfo_delay_duration[3]{};
    dx7_sample_t lfo_delay_increment[3]{};
    int32_t lfo_phase{};
    dx7_sample_t lfo_value{};
    double lfo_value_for_pitch{};      /* no delay, unramped */
    int32_t lfo_duration{};
    dx7_sample_t lfo_increment{};
    dx7_sample_t lfo_target{};
    dx7_sample_t lfo_increment0{};
    dx7_sample_t lfo_increment1{};
    int32_t lfo_duration0{};
    int32_t lfo_duration1{};
    dx7_sample_t lfo_buffer[PRDX7_NUGGET_SIZE]{};

    // dx7_sample_t lfo_buffer[PRDX7_NUGGET_SIZE]{};
#ifdef PRDX7_DEBUG_CONTROL
    dx7_sample_t    feedback_mod{};
#endif
};

/*
 * prdx7_synth_t
 */
struct prdx7_synth_t {
    prdx7_synth_t();
    ~prdx7_synth_t(){LOGE("DESTR");}
    int32_t instance_count{};

    pthread_mutex_t mutex;
    int32_t mutex_grab_failed{};

    unsigned long nugget_remains{};

    uint32_t note_id{};           /* incremented for every new note, used for voice-stealing prioritization */
    int32_t global_polyphony{PRDX7_DEFAULT_POLYPHONY};  /* must be <= PRDX7_MAX_POLYPHONY */

    dx7_voice_t voice[PRDX7_MAX_POLYPHONY];
    prdx7_instance_t *instances{nullptr};

};

prdx7_instance_t *
prdx7_instantiate(
        unsigned long sample_rate);

void
prdx7_activate(prdx7_instance_t *handle);

/* prdx7_synth.c */
void dx7_voice_off(dx7_voice_t *voice);

void dx7_voice_start_voice(dx7_voice_t *voice);

void prdx7_synth_all_voices_off(void);

void prdx7_instance_all_voices_off(prdx7_instance_t *instance);

void prdx7_instance_note_off(prdx7_instance_t *instance, unsigned char key,
                             unsigned char rvelocity);

void prdx7_instance_all_notes_off(prdx7_instance_t *instance);

void prdx7_instance_note_on(prdx7_instance_t *instance, unsigned char key,
                            unsigned char velocity);

void prdx7_instance_key_pressure(prdx7_instance_t *instance,
                                 unsigned char key, unsigned char pressure);

void prdx7_instance_damp_voices(prdx7_instance_t *instance);

void prdx7_instance_control_change(prdx7_instance_t *instance,
                                   uint32_t param, signed int32_t value);

void prdx7_instance_channel_pressure(prdx7_instance_t *instance,
                                     signed int32_t pressure);

void prdx7_instance_pitch_bend(prdx7_instance_t *instance, signed int32_t value);

void
prdx7_instance_select_program(prdx7_instance_t *instance, unsigned long program);

void prdx7_select_program(prdx7_instance_t *handle, unsigned long program);

char *prdx7_instance_get_program_descriptor(prdx7_instance_t *instance,
                                            unsigned long program);

char *prdx7_instance_handle_patches(prdx7_instance_t *instance,
                                    const char *key, const char *value);

char *prdx7_instance_handle_edit_buffer(prdx7_instance_t *instance,
                                        const char *value);

char *prdx7_instance_handle_monophonic(prdx7_instance_t *instance,
                                       const char *value);

char *prdx7_instance_handle_polyphony(prdx7_instance_t *instance,
                                      const char *value);

char *prdx7_synth_handle_global_polyphony(const char *value);

char *prdx7_instance_handle_performance(prdx7_instance_t *instance,
                                        const char *value);

void prdx7_synth_render_voices(float *out,
                               unsigned long sample_count,
                               int32_t do_control_update);
void prdx7_handle_event(prdx7_instance_t *instance, const void &event);


#endif //PIANO_PHASE_PRDX7_H

