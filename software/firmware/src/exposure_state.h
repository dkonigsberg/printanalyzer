/**
 * Represents and manipulates the state of currently selected
 * exposure settings.
 */
#ifndef EXPOSURE_STATE_H
#define EXPOSURE_STATE_H

#include <stdint.h>
#include <stdbool.h>

#include "contrast.h"

#define EXPOSURE_BURN_DODGE_MAX 9

typedef enum {
    EXPOSURE_MODE_PRINTING_BW = 0,
    EXPOSURE_MODE_PRINTING_COLOR,
    EXPOSURE_MODE_DENSITOMETER,
    EXPOSURE_MODE_CALIBRATION
} exposure_mode_t;

typedef enum {
    EXPOSURE_ADJ_TWELFTH = 1,
    EXPOSURE_ADJ_SIXTH = 2,
    EXPOSURE_ADJ_QUARTER = 3,
    EXPOSURE_ADJ_THIRD = 4,
    EXPOSURE_ADJ_HALF = 6,
    EXPOSURE_ADJ_WHOLE = 12
} exposure_adjustment_increment_t;

typedef enum {
    EXPOSURE_PEV_PRESET_BASE = 0,
    EXPOSURE_PEV_PRESET_STRIP
} exposure_pev_preset_t;

typedef struct __exposure_adjustment_t {
    contrast_grade_t contrast_grade;
    int8_t numerator;
    uint8_t denominator;
} exposure_burn_dodge_t;

typedef struct __exposure_state_t exposure_state_t;

#define EXPOSURE_TONE_IS_LOWER_BOUND(x) ((x) & 0x00000001UL)
#define EXPOSURE_TONE_IS_UPPER_BOUND(x) ((x) & 0x00010000UL)
#define EXPOSURE_TONE_IS_SET(x, i)      ((x) & (1UL << (i)))

exposure_state_t *exposure_state_create();
void exposure_state_free(exposure_state_t *state);

void exposure_state_defaults(exposure_state_t *state);

exposure_mode_t exposure_get_mode(const exposure_state_t *state);
void exposure_set_mode(exposure_state_t *state, exposure_mode_t mode);

void exposure_set_base_time(exposure_state_t *state, float value);

void exposure_set_min_exposure_time(exposure_state_t *state, float value);
float exposure_get_min_exposure_time(const exposure_state_t *state);

float exposure_get_exposure_time(const exposure_state_t *state);

int exposure_get_active_paper_profile_index(const exposure_state_t *state);
bool exposure_set_active_paper_profile_index(exposure_state_t *state, int index);
void exposure_clear_active_paper_profile(exposure_state_t *state);

uint32_t exposure_add_meter_reading(exposure_state_t *state, float lux);
uint32_t exposure_get_meter_reading_tone(const exposure_state_t *state, float lux);
float exposure_get_lowest_meter_reading(exposure_state_t *state);
void exposure_clear_meter_readings(exposure_state_t *state);

/*
 * Get the tone graph for the current exposure and readings.
 *
 * The tone graph is represented with bit flags in the lower
 * 17 bits of a 32-bit unsigned integer as follows:
 *
 *  1 | 1  1  1  1  1  1       |
 *  6 | 5  4  3  2  1  0  9  8 | 7  6  5  4  3  2  1  0
 * [<]|[ ][ ][ ][ ][ ][ ][ ][ ]|[ ][ ][ ][ ][ ][ ][ ][>]
 *  + |                        |                      -
 *
 * The low bit represents an under-exposure tone, and the high
 * bit represents an over-exposure tone. The tones in-between
 * are numbered from 1 through 15. Macros for parsing this are
 * provided for convenience.
 */
uint32_t exposure_get_tone_graph(const exposure_state_t *state);
uint32_t exposure_get_adjusted_tone_graph(const exposure_state_t *state, int adjustment);
uint32_t exposure_get_absolute_tone_graph(const exposure_state_t *state, float exposure_time);
uint32_t exposure_get_burn_dodge_tone_graph(const exposure_state_t *state, const exposure_burn_dodge_t *burn_dodge);

uint32_t exposure_get_calibration_pev(const exposure_state_t *state);

void exposure_adj_increase(exposure_state_t *state);
void exposure_adj_decrease(exposure_state_t *state);
int exposure_adj_get(const exposure_state_t *state);
void exposure_adj_set(exposure_state_t *state, int value);
int exposure_adj_min(exposure_state_t *state);
int exposure_adj_max(exposure_state_t *state);

contrast_grade_t exposure_get_contrast_grade(const exposure_state_t *state);
void exposure_contrast_increase(exposure_state_t *state);
void exposure_contrast_decrease(exposure_state_t *state);

uint16_t exposure_get_channel_value(const exposure_state_t *state, int index);
void exposure_set_channel_default_value(exposure_state_t *state, int index, uint16_t value);
void exposure_channel_increase(exposure_state_t *state, int index, uint8_t amount);
void exposure_channel_decrease(exposure_state_t *state, int index, uint8_t amount);
bool exposure_get_channel_wide_mode(const exposure_state_t *state);
void exposure_set_channel_wide_mode(exposure_state_t *state, bool wide_mode);

exposure_pev_preset_t exposure_calibration_pev_get_preset(const exposure_state_t *state);
void exposure_calibration_pev_set_preset(exposure_state_t *state, exposure_pev_preset_t preset);
void exposure_calibration_pev_increase(exposure_state_t *state);
void exposure_calibration_pev_decrease(exposure_state_t *state);

int exposure_adj_increment_get(const exposure_state_t *state);
void exposure_adj_increment_increase(exposure_state_t *state);
void exposure_adj_increment_decrease(exposure_state_t *state);
uint8_t exposure_adj_increment_get_denominator(const exposure_state_t *state);

int exposure_burn_dodge_count(const exposure_state_t *state);
const exposure_burn_dodge_t *exposure_burn_dodge_get(const exposure_state_t *state, int index);
void exposure_burn_dodge_set(exposure_state_t *state, const exposure_burn_dodge_t *entry, int index);
void exposure_burn_dodge_delete(exposure_state_t *state, int index);
void exposure_burn_dodge_delete_all(exposure_state_t *state);

float exposure_get_test_strip_time_incremental(const exposure_state_t *state,
    int patch_min, unsigned int patches_covered);
float exposure_get_test_strip_time_complete(const exposure_state_t *state, int patch);
uint32_t exposure_get_test_strip_patch_pev(const exposure_state_t *state, int patch);

#endif /* EXPOSURE_STATE_H */
