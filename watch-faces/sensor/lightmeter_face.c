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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lightmeter_face.h"
#include "watch.h"
#include "adc.h"
#include "hal_gpio.h"

#ifdef HAS_IR_SENSOR

uint16_t lightmeter_mod(uint16_t m, uint16_t n) { return (m%n + n)%n; }

// Helper function to convert ADC reading to lux
// NOTE: These constants are placeholders and need empirical calibration!
static float adc_to_lux(uint16_t adc_value) {
    // Simple logarithmic conversion: lux = scale * 2^(adc / sensitivity)
    // This provides a wide dynamic range suitable for light sensing
    // TODO: Calibrate ADC_LUX_SCALE and ADC_LUX_SENSITIVITY against known light source
    if (adc_value == 0) return 0.0;
    return ADC_LUX_SCALE * pow(2.0, (float)adc_value / ADC_LUX_SENSITIVITY);
}

// Helper function to read the phototransistor sensor
static float lightmeter_read_sensor(void) {
    uint16_t adc_value = adc_get_analog_value(HAL_GPIO_IRSENSE_pin());
    return adc_to_lux(adc_value);
}

void lightmeter_face_setup(uint8_t watch_face_index, void ** context_ptr) {
    (void) watch_face_index;
    if (*context_ptr == NULL) {
        *context_ptr = malloc(sizeof(lightmeter_state_t));
        lightmeter_state_t *state = (lightmeter_state_t*) *context_ptr;
        state->lux = 0.0;
        state->mode = 0;
        state->iso = LIGHTMETER_ISO_100;
        state->ap = LIGHTMETER_AP_4P0;
    }
}

void lightmeter_face_activate(void *context) {
    lightmeter_state_t *state = (lightmeter_state_t *)context;

    // Configure IR sensor GPIO pins
    HAL_GPIO_IR_ENABLE_out();
    HAL_GPIO_IR_ENABLE_clr();  // Set low to enable sensor
    HAL_GPIO_IRSENSE_pmuxen(HAL_GPIO_PMUX_ADC);

    // Initialize and enable ADC
    adc_init();
    adc_enable();

    // Request tick events for continuous measurement (1Hz)
    movement_request_tick_frequency(1);

    // Display most current reading
    lightmeter_show_ev(state);
}

void lightmeter_show_ev(lightmeter_state_t *state) {

    float ev = fmax(fmin(
                 log2(state->lux) +
                 lightmeter_isos[state->iso].ev +
                 LIGHTMETER_CALIBRATION,
            99), -9);
    int evt = round(2*ev); // Truncated EV

    // Print EV
    char strbuff[7];
    watch_clear_all_indicators();
    watch_display_text_with_fallback(WATCH_POSITION_TOP, "EV", "EV");

    sprintf(strbuff, "%2i", (uint16_t) abs(evt/2)); // Print whole part of EV
    watch_display_text(WATCH_POSITION_TOP_RIGHT, strbuff);
    if(evt%2) watch_set_indicator(WATCH_INDICATOR_LAP); // Indicate half stop
    if(ev<0) watch_set_pixel(1,9);  // Indicate negative EV

    // Handle lux mode
    if(state->mode == 1) {
        sprintf(strbuff, "%6.0f", fmin(state->lux, 999999.0));
        watch_display_text(WATCH_POSITION_BOTTOM, strbuff);
        return;
    }

    // Find and print best shutter speed
    uint16_t bestsh = 0;
    float besterr = 1.0/0.0;
    float errbuf = 1.0/0.0;
    float comp_ev = ev + lightmeter_aps[state->ap].ev;
    for(uint16_t ind = 2; ind < LIGHTMETER_N_SHS; ind++) {
        errbuf = comp_ev + lightmeter_shs[ind].ev;
        if( fabs(errbuf) < fabs(besterr)) {
            besterr = errbuf;
            bestsh = ind;
        }
    }
    if(besterr >= 0.5) watch_display_text(WATCH_POSITION_BOTTOM_LEFT, lightmeter_shs[LIGHTMETER_SH_HIGH].str);
    else if(besterr <= -0.5) watch_display_text(WATCH_POSITION_BOTTOM_LEFT, lightmeter_shs[LIGHTMETER_SH_LOW].str);
    else watch_display_text(WATCH_POSITION_BOTTOM_LEFT, lightmeter_shs[bestsh].str);

    // Print aperture
    watch_display_text(WATCH_POSITION_BOTTOM_RIGHT, lightmeter_aps[state->ap].str);
}

bool lightmeter_face_loop(movement_event_t event, void *context) {
    lightmeter_state_t *state = (lightmeter_state_t *)context;

    switch (event.event_type) {
        case EVENT_ACTIVATE:
            lightmeter_show_ev(state);
            break;

        case EVENT_LIGHT_BUTTON_DOWN:
            // Suppress LED to avoid interfering with light sensing
            break;

        case EVENT_ALARM_BUTTON_UP: // Increment aperture
            state->ap = lightmeter_mod(state->ap+1, LIGHTMETER_N_APS);
            lightmeter_show_ev(state);
            break;

        case EVENT_LIGHT_BUTTON_UP: // Decrement aperture
            if(state->ap == 0) state->ap = LIGHTMETER_N_APS-1;
            else state->ap = lightmeter_mod(state->ap-1, LIGHTMETER_N_APS);
            lightmeter_show_ev(state);
            break;

        case EVENT_LIGHT_LONG_PRESS: // Cycle ISO
            state->iso = lightmeter_mod(state->iso+1, LIGHTMETER_N_ISOS);

            watch_clear_all_indicators();
            watch_display_text_with_fallback(WATCH_POSITION_TOP, "EV", "EV");
            watch_display_text(WATCH_POSITION_BOTTOM, lightmeter_isos[state->iso].str);
            break;

        case EVENT_TICK: // Take continuous measurements
            // Read sensor synchronously (no waiting needed with ADC)
            state->lux = lightmeter_read_sensor();
            lightmeter_show_ev(state);
            break;

        case EVENT_MODE_LONG_PRESS: // Toggle mode
            state->mode = !state->mode;
            lightmeter_show_ev(state);
            break;

        case EVENT_TIMEOUT:
            movement_move_to_face(0);
            break;

        default:
            return movement_default_loop_handler(event);
    }
    return true;
}

void lightmeter_face_resign(void *context) {
    (void) context;

    // Disable ADC
    adc_disable();

    // Disable IR sensor pins
    HAL_GPIO_IRSENSE_pmuxdis();
    HAL_GPIO_IRSENSE_off();
    HAL_GPIO_IR_ENABLE_off();
}

#endif // HAS_IR_SENSOR

