# fanner

Simple, microcontroller-based PWM fan controller.

Has customizable temperature and PWM ranges.

## Requirements

* make
* avr-libc
* avr-gcc
* KiCAD 4+

## Software

### Customize

Open fanner.c in a text editor and adapt the parameters near the top.

The predefined voltages are optimized for the MCP-7900 temperature sensor.

PWM output values depend on the FAN used and require some experimentation.

### Build

```bash
make
```

### Flash

```
make flash
```

### Code

The code heavily relies on compile-time optimiziation and precalculation
of the (linear) transfer function coefficients.

At runtime, only 16-bit multiplication and addition is used to transform
the the values sampled by the AD converter into PWM output values.

The transfer function is a very simple piecewise linear curve, represented
by the formula:

    Sd      := (Dhigh - Dlow) / (Thigh - Tlow)
    ADClow  := ADC(Tlow) = (Tlow * Ss + Vofs) / Vref * ADCmax
    ADChigh := ADC(Thigh) = (Thigh * Ss + Vofs) / Vref * ADCmax
    D(ADC)  := ADC <= ADClow          : Dlow
               ADC >= ADChigh         : Dhigh
               ADClow < ADC < ADChigh : [Vref / ADCmax / Ss * Sd] - [Vofs / Ss * Sd]

Where:

    D    : Output duty cycle (Dlow and Dhigh are the flat parts of the curve) [%]
    Vref : Sensor reference voltage (the voltage corresponding to ADCmax) [V]
    ADC  : Input value from AD converter (ADClow and ADChigh are the values at
           lowest and highest temperatures on the transfer curve, respectively;
           ADCmax is the value generated when reference voltage is measured) [1]
    T    : Sensor temperature (with Tlow and Thigh corresponding to the Dlow
           and Dhigh duty cycle values) [°C]
    Ss   : Sensor voltage slope [V/°C]
    Vofs : Sensor offset voltage (voltage at lowest temperature) [V]
    Sd   : Output slope [%/°C]

To reduce sudden jumps in fan speed, a ramping function is used. The RAMP_DELTA
and RAMP_DELAY parameters allow for tuning the ramping speed.

## Electronics

### Design

Use KiCAD version 4 or later.

The board layout is based on THT components for easier building. And because
I had the components lying around in my workshop.

If you want the board in SMD, you have to design it yourself.

### Circuit

The circuit is really simple and just consists of a small microcontroller
(the ATtiny13/A), a voltage regulator (fans usually run on 12V) and some
pin headers for connecting the sensor, power and the fan.

The button and the corresponding pull-up resistor as well as the debouncing
capacitor are optional and currently not used in the example code.

## Copyright

© 2018-2019 Gregor Riepl

PCB designs and schematics are licensed under the CERN Open Hardware License 1.2.
Program code and build scripts are licensed under the Simplified BSD License.

See the accompanying LICENSE-HARDWARE and LICENSE-SOFTWARE files for details.

All other rights are reserved.
