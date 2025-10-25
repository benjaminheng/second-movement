/*
 * MIT License
 *
 * Copyright (c) 2022 CC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include "movement.h"

#ifdef HAS_IR_SENSOR

/*
 * Aperture-priority Light Meter Face
 *
 * This face uses the phototransistor light sensor on the Sensor Watch Pro board
 * to provide photographic light metering functionality.
 *
 * The sensor takes continuous measurements automatically at 1Hz and updates the display
 * in real-time with exposure recommendations.
 *
 * Controls:
 *
 *  - Measurements are taken automatically every second (no manual trigger needed).
 *
 *  - ISO setting can be cycled by long-pressing Light (100-3200 in full stops).
 *
 *  - EV measurement in the top right: "LAP" indicates "half stop".
 *    So "LAP -1" means EV = -1.5. Likewise "LAP 13" means EV = +13.5
 *
 *  - Aperture in the bottom right: the last 3 main digits are the f-stop.
 *    Adjust this number in full-stop increments using Alarm = +1 and Light = -1.
 *
 *  - Best shutter speed in the bottom left: the first 3 digits are the shutter speed.
 *    Some special chars are needed here: "-" = seconds, "h" = extra half second, "K" = thousands.
 *    "HI" or "LO" if there's no shutter in the dictionary within 0.5 stops of correct exposure.
 *
 *  - Mode long-press changes the main digits to show raw sensor lux measurements.
 *
 * Calibration Note:
 *   The ADC-to-lux conversion uses placeholder constants that should be calibrated
 *   empirically for accurate readings. Compare readings with a known light meter
 *   and adjust ADC_LUX_SCALE and ADC_LUX_SENSITIVITY in the source file.
 */

// Calibration constant from legacy - may need adjustment for phototransistor
#define LIGHTMETER_CALIBRATION 2.58

// ADC to lux conversion constants - THESE NEED EMPIRICAL CALIBRATION
#define ADC_LUX_SCALE 1.0
#define ADC_LUX_SENSITIVITY 4096.0

typedef struct {
    char * str;
    float ev;
} lightmeter_ev_t;

static const lightmeter_ev_t lightmeter_isos[] = {
    {" i 100", 0},
    {" i 200", 1},
    {" i 400", 2},
    {" i 800", 3},
    {" i1600", 4},
    {" i3200", 5}};
typedef enum {
    LIGHTMETER_ISO_100, LIGHTMETER_ISO_200, LIGHTMETER_ISO_400, LIGHTMETER_ISO_800, LIGHTMETER_ISO_1600, LIGHTMETER_ISO_3200,
    LIGHTMETER_N_ISOS
} lightmeter_iso_t;

static const lightmeter_ev_t lightmeter_aps[] = {
    {"1.4", 0},
    {"2.0", -1},
    {"2.8", -2},
    {"4.0", -3},
    {"5.6", -4},
    {"8.0", -5},
    {"11.", -6},
    {"16.", -7},
    {"22.", -8}};
typedef enum {
    LIGHTMETER_AP_1P4, LIGHTMETER_AP_2P0, LIGHTMETER_AP_2P8, LIGHTMETER_AP_4P0, LIGHTMETER_AP_5P6, LIGHTMETER_AP_8P0, LIGHTMETER_AP_11, LIGHTMETER_AP_16, LIGHTMETER_AP_22,
    LIGHTMETER_N_APS
} lightmeter_ap_t;

static const lightmeter_ev_t lightmeter_shs[] = {
    {"LO-",   99},
    {"HI ",  -99},
    {"30-",  5.0},
    {"20-",  4.5},
    {"15-",  4.0},
    {"11-",  3.5},
    {"8- ",  3.0},
    {"6- ",  2.5},
    {"4- ",  2.0},
    {"3- ",  1.5},
    {"2- ",  1.0},
    {"1h-",  0.5},
    {"1  ",  0.0},
    {"1h ", -0.5},
    {"2  ", -1.0},
    {"3  ", -1.5},
    {"4  ", -2.0},
    {"6  ", -2.5},
    {"8  ", -3.0},
    {"12 ", -3.5},
    {"15 ", -4.0},
    {"20 ", -4.5},
    {"30 ", -5.0},
    {"45 ", -5.5},
    {"60 ", -6.0},
    {"90 ", -6.5},
    {"125", -7.0},
    {"180", -7.5},
    {"250", -8.0},
    {"350", -8.5},
    {"500", -9.0},
    {"750", -9.5},
    {"1K ", -10.0},
    {"1K5", -10.5},
    {"2K ", -11.0},
    {"3K ", -11.5},
    {"4K ", -12.0},
    {"6K ", -12.5},
    {"8K ", -13.0}};
typedef enum {
    LIGHTMETER_SH_LOW, LIGHTMETER_SH_HIGH,
    LIGHTMETER_SH_30S, LIGHTMETER_SH_20S, LIGHTMETER_SH_15S, LIGHTMETER_SH_11S, LIGHTMETER_SH_8S, LIGHTMETER_SH_6S, LIGHTMETER_SH_3S, LIGHTMETER_SH_4S, LIGHTMETER_SH_2S, LIGHTMETER_SH_1HS,
    LIGHTMETER_SH_1, LIGHTMETER_SH_1H, LIGHTMETER_SH_2, LIGHTMETER_SH_3, LIGHTMETER_SH_4, LIGHTMETER_SH_6, LIGHTMETER_SH_8, LIGHTMETER_SH_12, LIGHTMETER_SH_15, LIGHTMETER_SH_20, LIGHTMETER_SH_30, LIGHTMETER_SH_45, LIGHTMETER_SH_60, LIGHTMETER_SH_90, LIGHTMETER_SH_125, LIGHTMETER_SH_180, LIGHTMETER_SH_250, LIGHTMETER_SH_350, LIGHTMETER_SH_500, LIGHTMETER_SH_750,
    LIGHTMETER_SH_1K, LIGHTMETER_SH_1K5, LIGHTMETER_SH_2K, LIGHTMETER_SH_3K, LIGHTMETER_SH_4K, LIGHTMETER_SH_6K, LIGHTMETER_SH_8K,
    LIGHTMETER_N_SHS
} lightmeter_sh_t;

typedef struct {
    lightmeter_iso_t iso;
    lightmeter_ap_t ap;
    float lux;
    int mode;
} lightmeter_state_t;

uint16_t lightmeter_mod(uint16_t m, uint16_t n);

void lightmeter_face_setup(uint8_t watch_face_index, void ** context_ptr);
void lightmeter_face_activate(void *context);
void lightmeter_show_ev(lightmeter_state_t *state);
bool lightmeter_face_loop(movement_event_t event, void *context);
void lightmeter_face_resign(void *context);

#define lightmeter_face ((const watch_face_t){ \
    lightmeter_face_setup, \
    lightmeter_face_activate, \
    lightmeter_face_loop, \
    lightmeter_face_resign, \
    NULL, \
})

#endif // HAS_IR_SENSOR
