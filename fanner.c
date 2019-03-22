#include <stdint.h>
#include <avr/io.h>
#include <util/delay.h>

/*
pin configuration:
PB0 SENSE
PB1 PWM
PB3 SWITCH
PB4 TEMP
*/

/* max change on each ramp steap */
#define RAMP_DELTA 1
/* delay between PWM updates (ms) */
#define RAMP_DELAY 10

/* reference voltage */
#define VOLTAGE_REF 5.0
/* sense voltage offset */
#define SENSE_OFFSET 0.5
/* sense voltage slope */
#define SENSE_SLOPE 0.01
/* ADC value at Vref */
#define ADC_MAX 255
/* lower temperature bound */
#define TEMP_LOW 20
/* upper temperature bound */
#define TEMP_HIGH 80
/* duty cycle at low bound */
#define DUTY_LOW 63
/* duty cycle at high bound */
#define DUTY_HIGH 255

/*
reference voltage: Vref : Vcc = 5V
sensor slope: Ss : 10mV/K
sensor offset: Vofs : 500mV
ADC range: ADC : ADCmin = 0 = 0V .. ADCmax = 255 = Vref
temperature range: T [°C] : Tmin = -50°C .. Tmax = 100°C
duty cycle: D : Dmin = 0 = 0% .. Dmax = 255 = 100%

mapping range temperature: T : Tlow = ? .. Thigh = ?
mapping range duty cycle: D : Dlow = ? .. Dhigh = ?

transfer curve: from linear to piecewise linear, clamped to low and high value
              _Thigh
            _/
          _/
        _/
      _/
Tlow_/
           ____Dhigh
          /
         /
Dlow____/ D(T)

D(T) := T <= Tlow : Dlow
        T >= Thigh : Dhigh
        Tlow < T < Thigh : T * Sd
output slope: Sd := (Dhigh - Dlow) / (Thigh - Tlow)

sensor voltage: Vs(T) := T * Ss + Vofs
T := (Vs - Vofs) / Ss
ADC output: ADC(T) := Vs / Vref * ADCmax
ADC(T) := (T * Ss + Vofs) / Vref * ADCmax
Vs(ADC) := ADC * Vref / ADCmax
T(ADC) := (Vref * ADC / ADCmax - Vofs) / Ss

ADClow := ADC(Tlow) = (Tlow * Ss + Vofs) / Vref * ADCmax
ADChigh := ADC(Thigh) = (Thigh * Ss + Vofs) / Vref * ADCmax
D(ADC) := 
  ADC <= ADClow : Dlow
  ADC >= ADChigh : Dhigh
  ADClow < ADC < ADChigh : T(ADC) * Sd =
    (Vref * ADC / ADCmax - Vofs) / Ss * Sd =
    ADC * [Vref / ADCmax / Ss * Sd] - [Vofs / Ss * Sd]
*/

/* Sd := (Dhigh - Dlow) / (Thigh - Tlow) */
#define DT_SLOPE ((double) DUTY_HIGH - (double) DUTY_LOW) / ((double) TEMP_HIGH - (double) TEMP_LOW)
/* D := ADC * MAD_M + MAD_A */
#define MAD_M ((double) VOLTAGE_REF / (double) ADC_MAX / (double) SENSE_SLOPE * (double) DT_SLOPE)
#define MAD_S ((double) SENSE_OFFSET / (double) SENSE_SLOPE * (double) DT_SLOPE)
/* same, 8+8bit for fixed point arithmetics */
#define MADS_M (uint16_t) (MAD_M * 255.0)
#define MADS_S (uint16_t) (MAD_S * 255.0)
/* ADClow := ADC(Tlow) = (Tlow * Ss + Vofs) / Vref * ADCmax */
#define ADC_LOW (uint8_t) ((TEMP_LOW * SENSE_SLOPE + SENSE_OFFSET) / VOLTAGE_REF * ADC_MAX)
/* ADChigh := ADC(Thigh) = (Thigh * Ss + Vofs) / Vref * ADCmax */
#define ADC_HIGH (uint8_t) ((TEMP_HIGH * SENSE_SLOPE + SENSE_OFFSET) / VOLTAGE_REF * ADC_MAX)

int main(void) __attribute__((noreturn));
static uint8_t ramp(uint8_t value, uint8_t target);
static void init(void);
static void hw_init(void);
static uint8_t scale(uint8_t adc);

struct {
	uint8_t ramp_current;
	uint8_t ramp_target;
} globals __attribute__((section(".noinit")));

uint8_t ramp(uint8_t current, uint8_t target) {
	if (current != target) {
		if (current > UINT8_MAX - RAMP_DELTA) {
			if (target > current) {
				return target;
			} else {
				if (current - RAMP_DELTA > target) {
					return current - RAMP_DELTA;
				} else {
					return target;
				}
			}
		} else if (current < 0 + RAMP_DELTA) {
			if (target < current) {
				return target;
			} else {
				if (current + RAMP_DELTA < target) {
					return current + RAMP_DELTA;
				} else {
					return target;
				}
			}
		} else {
			if (current + RAMP_DELTA < target) {
				return current + RAMP_DELTA;
			} else if (current - RAMP_DELTA > target) {
				return current - RAMP_DELTA;
			} else {
				return target;
			}
		}
	}
	return current;
}

void init(void) {
	globals.ramp_current = 0;
	globals.ramp_target = 0;
	
}

void hw_init(void) {
	/* set system clock to RC oscillator frequency / 1 (9.6MHz), overriding CKDIV8 */
	CLKPR = _BV(CLKPCE);
	CLKPR = 0;

	/* configure PD5 as PWM output */
	DDRB |= _BV(PB1);
	
	/* disable digital drivers on the analog comparator inputs */
	/*DIDR = _BV(AIN1D) | _BV(AIN0D);*/
	/* enable analog comparator */
	/*ACSR &= ~_BV(ACD);*/
	
	/* configure the ADC: Vcc reference, left adjust, ADC2/PB4 input */
	ADMUX = _BV(ADLAR) | _BV(MUX1);
	/* CPU clock = 9.6MHz / CLKDIV8 = 9.6MHz
	 * ADC clock < 200kHz => divider = 9.6MHz / 0.2MHz = 48
	 * divider = 64 => clock = 9.6MHz / 64 = 150kHz
	 * also enable auto mode. */
	ADCSRA = _BV(ADATE) | _BV(ADPS2) | _BV(ADPS1);
	/* set free running mode */
	ADCSRB &= ~(_BV(ADTS2) | _BV(ADTS1) | _BV(ADTS0));
	/* disable digital drivers on the analog input */
	DIDR0 &= ~_BV(ADC2D);
	/* enable the ADC and start converting */
	ADCSRA |= _BV(ADEN) | _BV(ADSC);
	/* the ADC will take 25 cycles (~167us) to finish the first conversion */
	_delay_us(200);
	
	/* enable PWM generator: fast PWM, 0..0xff, clear on match, set at TOP */
	OCR0B = 0;
	TCCR0A = _BV(COM0B1) | _BV(WGM01) | _BV(WGM00);
	/* clock select: CS[2:0]=001 -> CLK / 1 / PWMmax = 9.6MHz / 1 / 255 = 37.6kHz */
	TCCR0B = _BV(CS00);
}

uint8_t scale(uint8_t adc) {
	if (adc <= ADC_LOW) {
		return DUTY_LOW;
	} else if (adc >= ADC_HIGH) {
		return DUTY_HIGH;
	} else {
		uint16_t s = (uint16_t) adc * MADS_M - MADS_S;
		return (uint8_t) (s / 256);
	}
}

int main(void) {
	init();
	hw_init();
	
	while (1) {
		/* wait a jiffy between updates */
		_delay_ms(RAMP_DELAY);
		/* measure the temperature sensor voltage (only the top-most 8 bits) */
		uint8_t adc = ADCH;
		globals.ramp_target = scale(adc);
		/* ramp up or down */
		globals.ramp_current = ramp(globals.ramp_current, globals.ramp_target);
		/* set PWM value */
		OCR0B = globals.ramp_current;
	}
}
