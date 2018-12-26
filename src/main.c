#include <stdlib.h>
#include <stdbool.h>

#include <avr/io.h>
#include <util/delay.h>

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

static void reset_strips(void)
{
	for(uint8_t i = 0; i < 54; i++) {
		sk6812_setpixel(i, 0x00, 0x00, 0x00, 0x00);
	}
}

int main(void)
{
	uint16_t cur_capture = 0;
	uint16_t last_capture = 0;
	uint16_t time_delta;

	uint16_t last_led_update_tick = 0;
	uint16_t led_update_tick = 0;
	uint16_t led_update_time_delta;

	uint32_t speed = 0;

	uint32_t led_offset = 0;

	uint8_t last_index = 42;

	uint8_t r = 0, g = 0, b = 0;

	bool data_captured_since_last_overflow = true;
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
			if(time_delta > 32768) {
				time_delta -= 32768;
			}

			//if(time_delta > 468) { // filter: ignore intervals < 30 ms
			if(time_delta > 234) { // filter: ignore intervals < 15 ms
				last_capture = cur_capture;
				uint32_t new_speed = WHEEL_CIRCUMFERENCE * TIMER_TICKS_PER_SECOND / time_delta / TICKS_PER_TURN; // millimeter per second
				//                   ^^^ unit: mm ^^^^^^                            ^ ticks (unit: seconds)

				uint32_t tmp_speed = (speed << 4UL) - speed + new_speed;
				speed = (tmp_speed >> 4UL);

				PORTB ^= (1 << PIN5);

				idle = false;
				data_captured_since_last_overflow = true;
			}
		}

		/*
		if((PINB & (1 << PIN0)) != 0) {
			PORTB |= (1 << PIN5);
		} else {
			PORTB &= ~(1 << PIN5);
		}
		*/

		if(TIFR1 & (1 << TOV1)) {
			TIFR1 |= (1 << TOV1);
			if(!data_captured_since_last_overflow) {
				// no capture --> idle mode
				idle = true;
			}

			data_captured_since_last_overflow = false;
		}

		// calculate update interval
		led_update_tick = TCNT1;
		led_update_time_delta = led_update_tick - last_led_update_tick;
		if(led_update_time_delta > 32768) {
			led_update_time_delta -= 32768;
		}

		last_led_update_tick = led_update_tick;

		// secs = led_update_time_delta / TIMER_TICKS_PER_SECOND;   value < 1
		// led_offset += 16 * secs * speed / LED_DISTANCE

		led_offset += 0x100UL * ((uint32_t)led_update_time_delta) * ((uint32_t)speed) / TIMER_TICKS_PER_SECOND * 4UL / LED_DISTANCE;

		if(led_offset >= 0x80000000) {
			led_offset -= 0x80000000;
		}

		// update LEDs
		reset_strips();


		if(idle) {
			for(uint8_t i = 0; i < 54; i++) {
				sk6812_setpixel(i, 0x00, 0x00, 0x10, 0x00);
			}
		} else {
			uint8_t index = (uint8_t)((led_offset >> 8) & 0xFF) % 33;

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

		sk6812_update();
		_delay_us(80); // TODO: remove?

	}

	return 0;
}
