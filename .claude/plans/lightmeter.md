# Lightmeter Face Port: OPT3001 to Phototransistor

## Project Overview

Port the legacy lightmeter watch face from the OPT3001 I2C digital light sensor to the new phototransistor-based analog light sensor on the OSO-SWAT-C1 (Sensor Watch Pro) board.

---

## Initial Requirements Analysis

### Hardware Changes Discovered

**Legacy Hardware (OSO-SWAT-A1):**
- **Sensor**: OPT3001 digital ambient light sensor
- **Interface**: I2C communication (address 0x44)
- **Operation**: Asynchronous - trigger conversion, poll for completion, read result
- **Output**: Direct lux measurements

**New Hardware (OSO-SWAT-C1 / Sensor Watch Pro):**
- **Sensor**: Q_Photo_NPN phototransistor (specific part number TBD)
- **Package**: 0603 SMD
- **Interface**: Analog ADC reading
- **Pins**:
  - `IR_ENABLE` (GPIO PB22) - Power/bias control
  - `IRSENSE` (GPIO PA04) - ADC input
- **Operation**: Synchronous - direct ADC read returns immediately
- **Output**: Raw ADC values (0-65535) requiring conversion to lux

**Source Files:**
- Hardware definitions: `/home/ben/dev/Sensor-Watch/boards/OSO-SWAT-C1-00/pins.h:19-21`
- Schematic: `/home/ben/dev/Sensor-Watch/PCB/Main Boards/OSO-SWAT-C1/OSO-SWAT-C1.kicad_sch`
- Reference implementation: `/home/ben/dev/second-movement/watch-faces/demo/light_sensor_face.c`

---

## Project Plan

### Phase 1: Code Structure Setup ✅
- Used boilerplate generator: `python3 watch_face.py sensor lightmeter`
- Created initial file structure in `/home/ben/dev/second-movement/watch-faces/sensor/`
- Files auto-added to build system (`watch-faces.mk`, `movement_faces.h`)

### Phase 2: Header File Migration ✅
1. Add `#ifdef HAS_IR_SENSOR` guard for conditional compilation
2. Port all constants from legacy (shutter speeds) and simplify ISO/aperture to full stops
3. Update state structure:
   - **Remove**: `waiting_for_conversion` (no longer needed for synchronous ADC)
   - **Keep**: `iso`, `ap`, `lux`, `mode`
4. Add ADC calibration constants (placeholders)
5. Update documentation for new hardware

### Phase 3: Implementation ✅
1. **Hardware Interface**:
   - Replace I2C calls with ADC initialization
   - Configure GPIO pins for IR sensor
   - Implement ADC-to-lux conversion function

2. **State Management Simplification**:
   - Remove async conversion waiting logic
   - Make all sensor reads synchronous
   - Simplify event loop

3. **Display API Updates**:
   - Migrate from `watch_display_string()` to `watch_display_text_with_fallback()`
   - Update position constants (WATCH_POSITION_TOP, etc.)

4. **Event Handling**:
   - Remove EVENT_TICK polling
   - Make EVENT_ALARM_LONG_PRESS trigger direct read
   - Add EVENT_LIGHT_BUTTON_DOWN to suppress LED

### Phase 4: Testing & Calibration ⏳
- Compile with `BOARD=sensorwatch_pro`
- Test basic functionality
- Empirical calibration of ADC-to-lux conversion
- Validate EV calculations

### Phase 5: Auto-Measurement Enhancement ✅
- Added continuous measurement using EVENT_TICK (1Hz update rate)
- Sensor now automatically updates readings every second
- Removed manual trigger entirely (EVENT_ALARM_LONG_PRESS handler removed)
- Updated documentation to reflect auto-measurement behavior

### Phase 6: ISO Range Update ✅
- Simplified ISO range to full stops only: 100, 200, 400, 800, 1600, 3200
- Removed fractional ISO values (25, 50, 160) for cleaner cycling
- Default remains ISO 100
- All EV values adjusted accordingly (0, 1, 2, 3, 4, 5)

### Phase 7: Aperture Range Update ✅
- Simplified aperture range to full stops only: f/1.4, f/2.0, f/2.8, f/4.0, f/5.6, f/8, f/11, f/16, f/22
- Removed half-stop apertures (f/1.8, f/2.4, f/3.3, f/4.8, f/6.7, f/9.5, f/13, f/19)
- Reduced from 17 values to 9 values for simpler operation
- Default remains f/4.0
- EV values: 0, -1, -2, -3, -4, -5, -6, -7, -8

---

## Implemented Changes

### Files Created
- `/home/ben/dev/second-movement/watch-faces/sensor/lightmeter_face.h`
- `/home/ben/dev/second-movement/watch-faces/sensor/lightmeter_face.c`

### Files Modified
- `/home/ben/dev/second-movement/watch-faces.mk` - Added lightmeter_face.c to build
- `/home/ben/dev/second-movement/movement_faces.h` - Added lightmeter_face.h include

### Key Implementation Details

#### 1. Hardware Interface (`lightmeter_face.c:33-51`)

```c
#ifdef HAS_IR_SENSOR

// ADC to lux conversion with logarithmic scale
static float adc_to_lux(uint16_t adc_value) {
    if (adc_value == 0) return 0.0;
    return ADC_LUX_SCALE * pow(2.0, (float)adc_value / ADC_LUX_SENSITIVITY);
}

static float lightmeter_read_sensor(void) {
    uint16_t adc_value = adc_get_analog_value(HAL_GPIO_IRSENSE_pin());
    return adc_to_lux(adc_value);
}
```

**Calibration Constants** (lightmeter_face.h:65-67):
```c
#define ADC_LUX_SCALE 1.0           // NEEDS CALIBRATION
#define ADC_LUX_SENSITIVITY 4096.0  // NEEDS CALIBRATION
```

#### 2. Activation Sequence (`lightmeter_face.c:65-79`)

**Removed** (legacy):
```c
watch_enable_i2c();
```

**Added** (new):
```c
HAL_GPIO_IR_ENABLE_out();               // Configure enable pin as output
HAL_GPIO_IR_ENABLE_clr();               // Set low to power sensor
HAL_GPIO_IRSENSE_pmuxen(HAL_GPIO_PMUX_ADC);  // Configure sense pin for ADC
adc_init();
adc_enable();
```

#### 3. Measurement Trigger (`lightmeter_face.c:161-180`)

**Removed** (legacy async):
```c
opt3001_writeConfig(lightmeter_addr, lightmeter_takeNewReading);
state->waiting_for_conversion = 1;
// ... later in EVENT_TICK: poll for conversion ready
```

**Added** (new synchronous with auto-measurement):
```c
case EVENT_TICK: // Take continuous measurements (1Hz)
    state->lux = lightmeter_read_sensor();  // Direct read, no waiting
    lightmeter_show_ev(state);
    break;
```

**Key changes**:
- Measurements now happen automatically every second via EVENT_TICK
- EVENT_ALARM_LONG_PRESS handler completely removed (no longer needed)
- Tick frequency requested in `lightmeter_face_activate()` with `movement_request_tick_frequency(1)`

#### 4. Display Updates (`lightmeter_face.c:81-125`)

**Changed**:
- `watch_display_string("EV        ", 0)` → `watch_display_text_with_fallback(WATCH_POSITION_TOP, "EV", "EV")`
- Used position constants: `WATCH_POSITION_TOP`, `WATCH_POSITION_TOP_RIGHT`, `WATCH_POSITION_BOTTOM`, etc.
- Kept legacy math functions: `log2()`, `fabs()`, `fmax()`, `fmin()`

#### 5. Cleanup (`lightmeter_face.c:187-197`)

**Removed** (legacy):
```c
opt3001_writeConfig(lightmeter_addr, lightmeter_off);
watch_disable_i2c();
```

**Added** (new):
```c
adc_disable();
HAL_GPIO_IRSENSE_pmuxdis();
HAL_GPIO_IRSENSE_off();
HAL_GPIO_IR_ENABLE_off();
```

### Preserved Legacy Functionality

All user-facing features remain identical (except measurement behavior, ISO, and aperture):
- **Aperture adjustment**: ALARM (+1 stop), LIGHT (-1 stop) - full stops only (f/1.4 - f/22)
- **ISO cycling**: LIGHT long-press (100-3200 in full stops)
- **Measurement behavior**: Now fully automatic (1Hz continuous), no manual trigger needed
- **Mode toggle**: MODE long-press (EV ↔ lux display)
- **Display layout**: Unchanged (EV + shutter speed + aperture)

---

## Pending Tasks

### Critical: Calibration ⚠️

The ADC-to-lux conversion uses **placeholder constants** that produce incorrect readings. Must calibrate empirically:

**Procedure**:
1. Set up reference light source with known lux value (or use calibrated light meter)
2. Take multiple ADC readings at various light levels
3. Record pairs: `(adc_value, known_lux)`
4. Fit logarithmic curve to data points
5. Update constants in `lightmeter_face.h:66-67`:
   - `ADC_LUX_SCALE` - scales output magnitude
   - `ADC_LUX_SENSITIVITY` - controls dynamic range

**Expected curve form**:
```
lux = ADC_LUX_SCALE * 2^(adc_value / ADC_LUX_SENSITIVITY)
```

**Calibration points to test**:
- Dark room: ~1 lux
- Indoor lighting: ~100-500 lux
- Bright sunlight: ~10,000+ lux

May also need to adjust `LIGHTMETER_CALIBRATION` (currently 2.58) for overall EV offset.

### Testing Checklist

- [ ] **Compilation**: Build for `BOARD=sensorwatch_pro DISPLAY=classic`
- [ ] **Basic functionality**: Face loads without crashing
- [ ] **Button controls**:
  - [ ] ALARM: Aperture increment works
  - [ ] LIGHT: Aperture decrement works
  - [ ] LIGHT long-press: ISO cycling works
  - [ ] MODE long-press: Toggle lux/EV mode
- [ ] **Auto-measurement**:
  - [ ] Readings update automatically every second
  - [ ] Display refreshes continuously without user input
- [ ] **Display correctness**:
  - [ ] EV values display (may be inaccurate until calibrated)
  - [ ] Shutter speed recommendations appear
  - [ ] Aperture settings visible
  - [ ] Lux mode shows numeric values
- [ ] **LED suppression**: LIGHT button doesn't illuminate LED
- [ ] **Power management**: Face resigns properly on timeout

### Future Enhancements

1. **Identify phototransistor part number**:
   - Inspect PCB markings with magnification
   - Check assembly documentation
   - Would provide datasheet for better initial calibration

2. **Multi-point calibration**:
   - Store calibration curve in multiple segments
   - Better accuracy across wide lux range

3. **Incident vs. reflected metering**:
   - Document optimal sensor orientation
   - Consider adding mode indicator

---

## Technical Notes

### Why Logarithmic Conversion?

Phototransistors have logarithmic response to light intensity, and human perception of brightness is also logarithmic (Weber-Fechner law). The conversion formula:

```c
lux = scale * 2^(adc / sensitivity)
```

This provides:
- **Wide dynamic range**: Can measure from dim to bright light
- **Perceptually uniform**: Steps in ADC correspond to perceptually equal brightness changes
- **EV compatibility**: Log₂ is native to EV system (each EV stop = 2× light)

### ADC Considerations

- **Resolution**: 16-bit ADC (0-65535)
- **Reference**: VDDANA (3.3V on SAM L22)
- **Oversampling**: 16 samples per reading (configured in adc_init)
- **Conversion time**: ~32 microseconds
- **Input impedance**: Phototransistor output needs buffering or high-impedance input

### Display Positions

New framework uses named position constants:
- `WATCH_POSITION_TOP` - Top line label
- `WATCH_POSITION_TOP_RIGHT` - Top right digits
- `WATCH_POSITION_BOTTOM` - Full bottom line
- `WATCH_POSITION_BOTTOM_LEFT` - Bottom left 3 chars
- `WATCH_POSITION_BOTTOM_RIGHT` - Bottom right 3 chars
- `WATCH_POSITION_SECONDS` - Seconds area

---

## Reference Files

### Legacy Implementation
- `/home/ben/dev/second-movement/legacy/watch_faces/sensor/lightmeter_face.c`
- `/home/ben/dev/second-movement/legacy/watch_faces/sensor/lightmeter_face.h`

### Hardware Definitions
- Board pins: `/home/ben/dev/second-movement/gossamer/boards/sensorwatch_pro/pins.h:33-36`
- Schematic: `/home/ben/dev/Sensor-Watch/PCB/Main Boards/OSO-SWAT-C1/`

### Reference Implementations
- Light sensor demo: `/home/ben/dev/second-movement/watch-faces/demo/light_sensor_face.c`
- ADC API: `/home/ben/dev/second-movement/gossamer/common/adc.h`
- Temperature face (similar structure): `/home/ben/dev/second-movement/watch-faces/sensor/temperature_display_face.c`

---

## Build Instructions

```bash
cd /home/ben/dev/second-movement
make BOARD=sensorwatch_pro DISPLAY=custom
```

To test without the sensor (will compile but not link face):
```bash
make BOARD=sensorwatch_blue DISPLAY=classic  # No HAS_IR_SENSOR defined
```

---

## Questions for Future Work

1. **Part Number**: What is the exact phototransistor part number? Need for datasheet.
2. **Calibration Target**: What lux accuracy is acceptable? ±10%? ±20%?
3. **Power Consumption**: Should we disable sensor between readings or leave enabled?
4. **Spectral Response**: Is the phototransistor IR-biased? May need correction factor for visible light.
5. **Temperature Compensation**: Does ADC or phototransistor need temperature compensation?

---

*Document created: 2025-10-25*
*Last updated: 2025-10-25*
