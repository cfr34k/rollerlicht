#include <stdlib.h>
#include <stdbool.h>

#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#include "sk6812.h"

/*!
 * Set up 38 kHz PWM signal for IR transmission.
 *
 * Timer 2 is used for this.
 */
static void setup_ir_transmission(void)
{
	// OC2B / PD3 / D3 as output
	PORTD &= ~(1 << PIN3);
	DDRD |= (1 << PIN3);

	// set up timer 2 for 38 kHz PWM
	TCCR2A = (1 << WGM21) | (1 << WGM20);
	TCCR2B = (1 << CS21) | (1 << WGM22);     // f_cpu/8

	OCR2A = 52;
	OCR2B = 11;

	TCCR2A |= (1 << COM2B1); // activate output pin
}

/*!
 * Set up Timer 1 and Input Capture system.
 *
 * Timer 1 runs at f_cpu/1024 = 15625 Hz and overflows every 65536 counts --> ~4.2 seconds
 * overflow tick.
 */
static void setup_timer1_input_capture(void)
{
	ACSR |= (1 << ACD); // disable analog comparator to avoid interference

	TCNT1 = 0;
	//OCR1A = 62499; // upper limit

	TCCR1A = 0;
	//TCCR1B = (1 << WGM12) | (1 << CS11); // CTC mode, OCR1A = Max.; ICES1 not set -> falling edge
	TCCR1B = (1 << ICNC1) | (1 << CS12) | (1 << CS10); // normal mode, Max. = 0xFFFF; Noise canceling enabled; ICES1 not set -> falling edge
}

#define WHEEL_CIRCUMFERENCE 785UL // millimeter, pi*250mm
#define TICKS_PER_TURN 5UL
#define TIMER_TICKS_PER_SECOND 15625UL
#define LED_DISTANCE 67UL // mm * 4

// python:
// x = np.arange(0, 2*np.pi, 2*np.pi/(27*8))
// y = np.round((np.sin(x/2)**(2.0)) * 0xFF)

const PROGMEM uint8_t pulse[216] = {
	0,   0,   0,   0,   1,   1,   2,   3,   3,   4,   5,
	6,   8,   9,  10,  12,  14,  15,  17,  19,  21,  23,
	25,  27,  30,  32,  35,  37,  40,  43,  46,  48,  51,
	54,  57,  61,  64,  67,  70,  74,  77,  80,  84,  87,
	91,  95,  98, 102, 105, 109, 113, 116, 120, 124, 127,
	131, 135, 139, 142, 146, 150, 153, 157, 160, 164, 168,
	171, 175, 178, 181, 185, 188, 191, 194, 198, 201, 204,
	207, 209, 212, 215, 218, 220, 223, 225, 228, 230, 232,
	234, 236, 238, 240, 241, 243, 245, 246, 247, 249, 250,
	251, 252, 252, 253, 254, 254, 255, 255, 255, 255, 255,
	255, 255, 254, 254, 253, 252, 252, 251, 250, 249, 247,
	246, 245, 243, 241, 240, 238, 236, 234, 232, 230, 228,
	225, 223, 220, 218, 215, 212, 209, 207, 204, 201, 198,
	194, 191, 188, 185, 181, 178, 175, 171, 168, 164, 160,
	157, 153, 150, 146, 142, 139, 135, 131, 128, 124, 120,
	116, 113, 109, 105, 102,  98,  95,  91,  87,  84,  80,
	77,  74,  70,  67,  64,  61,  57,  54,  51,  48,  46,
	43,  40,  37,  35,  32,  30,  27,  25,  23,  21,  19,
	17,  15,  14,  12,  10,   9,   8,   6,   5,   4,   3,
	3,   2,   1,   1,   0,   0,   0
};

#define LUT_LENGTH (sizeof(pulse) / sizeof(pulse[0]))

static void reset_strips(void)
{
	for(uint8_t i = 0; i < 54; i++) {
		sk6812_setpixel(i, 0x00, 0x00, 0x00, 0x00);
	}
}

static inline uint8_t clip(uint16_t v)
{
	if(v < 256) {
		return v;
	} else {
		return 255;
	}
}

static void idle_animation(uint16_t *ticks_since_last_update, bool reset) {

	static uint32_t global_time = 0;

	if(reset) {
		global_time = 0;
	} else {
		global_time += *ticks_since_last_update;

		int8_t shift = (global_time * 15 / TIMER_TICKS_PER_SECOND) % (3*27);

		if(shift >= 54) {
			return;
		}

		for(int8_t i = 0; i < 27; i++) {
			uint8_t v1, v2;

			int8_t idx = shift - i;
			if(idx >= 0 && idx <= 26) {
				v1 = pgm_read_byte(pulse + (idx << 3) + 4);
			} else {
				v1 = 0;
			}

			idx = shift - (26 - i);
			if(idx >= 0 && idx <= 26) {
				v2 = pgm_read_byte(pulse + (idx << 3) + 4);
			} else {
				v2 = 0;
			}

			uint8_t b = ((uint16_t)v1 + (uint16_t)v2) / 8;
			uint8_t w;

			if(b >= 32) {
				w = (b - 32) / 2;
			} else {
				w = 0;
			}

			sk6812_setpixel(i, 0x00, 0x00, b, w);
			sk6812_setpixel(27+i, 0x00, 0x00, b, w);
		}
	}
}

#if 0
static void walking_light(uint32_t *offset) {
	static uint8_t last_index = 42;

	uint8_t r = 0, g = 0, b = 0;

	uint8_t index = (uint8_t)((*offset >> 8) & 0xFF) % 33;

	if(last_index != 0 && index == 0) {
		uint8_t color = rand() & 0x7;

		r = color & (1 << 0) ? 0xFF : 0x00;
		g = color & (1 << 1) ? 0xFF : 0x00;
		b = color & (1 << 2) ? 0xFF : 0x00;
	}

	last_index = index;

	for(uint8_t off = 0; off < 3; off++) {
		uint8_t local_idx = index + off - 2;

		if(local_idx >= 27) {
			continue;
		}

		sk6812_setpixel(local_idx, r, g, b, 0x80);
		sk6812_setpixel(27+local_idx, r, g, b, 0x80);
	}
}

#else

#define SPEEDUP 4
#define RAINBOW_LENGTH (3 * LUT_LENGTH / 2)

static void rainbow(uint32_t *offset) {
	uint8_t r = 0, g = 0, b = 0;

	uint16_t start_idx = (uint16_t)((*offset >> 8) & 0xFFFF);

	for(uint8_t i = 0; i < 27; i++) {
		int16_t local_idx = ((start_idx + (26-i)) * SPEEDUP) % RAINBOW_LENGTH;

		if(local_idx < (signed)LUT_LENGTH/2) {
			b = pgm_read_byte(pulse + local_idx + LUT_LENGTH/2);
		}

		if(local_idx < (signed)LUT_LENGTH) {
			r = pgm_read_byte(pulse + local_idx);
		}

		local_idx -= LUT_LENGTH/2;

		if(local_idx >= 0 && local_idx < (signed)LUT_LENGTH) {
			g = pgm_read_byte(pulse + local_idx);
		}

		local_idx -= LUT_LENGTH/2;

		if(local_idx >= 0 && local_idx < (signed)LUT_LENGTH) {
			b = pgm_read_byte(pulse + local_idx);
		}

		if(local_idx >= (signed)LUT_LENGTH/2 && local_idx < (signed)LUT_LENGTH) {
			r = pgm_read_byte(pulse + local_idx - LUT_LENGTH/2);
		}

		r >>= 2;
		g >>= 2;
		b >>= 2;

		sk6812_setpixel(i, r, g, b, 0x00);
		sk6812_setpixel(27+i, r, g, b, 0x00);
	}
}

#endif

int main(void)
{
	uint16_t cur_capture = 0;
	uint16_t last_capture = 0;
	uint16_t time_delta;

	uint16_t last_frame_update_tick = 0;
	uint16_t frame_update_tick = 0;
	uint16_t frame_update_time_delta;

	uint32_t speed = 0;

	uint32_t led_offset = 0;

	uint16_t ticks_since_last_update = 0;
	bool idle = true;

	DDRB |= (1 << PIN5);
	PORTB |= (1 << PIN5);

	sk6812_init();
	reset_strips();

	setup_ir_transmission();
	setup_timer1_input_capture();

	while(true) {
		if((TIFR1 & (1 << ICF1))) { // check input capture flag
			cur_capture = ICR1;
			TIFR1 |= (1 << ICF1);

			// calculate speed
			time_delta = cur_capture - last_capture;
			/*if(time_delta > 32768) {
				time_delta -= 32768;
			}*/

			if(time_delta > 468) { // filter: ignore intervals < 30 ms
				last_capture = cur_capture;
				uint32_t new_speed = WHEEL_CIRCUMFERENCE * TIMER_TICKS_PER_SECOND / time_delta / TICKS_PER_TURN; // millimeter per second
				//                   ^^^ unit: mm ^^^^^^                            ^ ticks (unit: seconds)

				uint32_t tmp_speed = (speed << 4UL) - speed + new_speed;
				speed = (tmp_speed >> 4UL);

				PORTB ^= (1 << PIN5);

				idle = false;
				ticks_since_last_update = 0;
			}
		}

		if(TIFR1 & (1 << TOV1)) {
			TIFR1 |= (1 << TOV1);
			if(!idle && (ticks_since_last_update > TIMER_TICKS_PER_SECOND)) {
				// no capture --> idle mode
				idle = true;
				speed = 0;

				idle_animation(NULL, true);
			}
		}

		// calculate update interval
		frame_update_tick = TCNT1;
		frame_update_time_delta = frame_update_tick - last_frame_update_tick;
		/*if(frame_update_time_delta > 32768) {
			frame_update_time_delta -= 32768;
		}*/

		last_frame_update_tick = frame_update_tick;

		if(!idle) {
			ticks_since_last_update += frame_update_time_delta;
		}

		// secs = frame_update_time_delta / TIMER_TICKS_PER_SECOND;   value < 1
		// led_offset += 16 * secs * speed / LED_DISTANCE

		led_offset += 0x100UL * ((uint32_t)frame_update_time_delta) * ((uint32_t)speed) / TIMER_TICKS_PER_SECOND * 4UL / LED_DISTANCE;

		if(led_offset >= 0x80000000) {
			led_offset -= 0x80000000;
		}

		// update LEDs
		reset_strips();

		if(idle) {
			idle_animation(&frame_update_time_delta, false);
		} else {
			//walking_light(&led_offset);
			rainbow(&led_offset);
		}

		sk6812_update();
		_delay_us(80); // TODO: remove?

	}

	return 0;
}
