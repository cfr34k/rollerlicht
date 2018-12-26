#include <avr/io.h>

#include <string.h>

#include "sk6812.h"

// OC0B / PD5 / D5
#define SK6812_PORT PORTD
#define SK6812_DDR DDRD
#define SK6812_PIN PIN5

static struct SK6812Pixel sk6812_data[NLEDS];

static const uint8_t THR_LOW  =  4;
static const uint8_t THR_HIGH =  9;

void sk6812_init(void)
{
	memset(&sk6812_data, 0, sizeof(sk6812_data));

	SK6812_PORT &= ~(1 << SK6812_PIN);
	SK6812_DDR  |= (1 << SK6812_PIN);

	TCCR0A = (1 << WGM01) | (1 << WGM00);
	TCCR0B = (1 << WGM02);

	TCNT0 = 0;

	OCR0A = 18;
	OCR0B = THR_LOW;
}

void sk6812_setpixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b, uint8_t w)
{
	struct SK6812Pixel tmp = {
		.r = r,
		.g = g,
		.b = b,
		.w = w};

	sk6812_data[idx] = tmp;
}

inline static void start_timer(void)
{
	TCCR0B |= (1 << CS00);
}

inline static void stop_timer(void)
{
	TCCR0B &= ~(1 << CS00);
	TCCR0A &= ~(1 << COM0B1);
}

inline static void wait_for_overflow(void)
{
	while((TIFR0 & (1 << TOV0)) == 0); // wait for next timer compare
	TIFR0 |= (1 << TOV0); // clear interrupt flag
}

inline static void set_output_compare(uint8_t bit)
{
	if(bit) {
		OCR0B = THR_HIGH;
	} else {
		OCR0B = THR_LOW;
	}
}

static void send_byte(uint8_t byte)
{
	wait_for_overflow();
	TCCR0A |= (1 << COM0B1); // activate output pin
	set_output_compare(byte & (1 << 7));
	wait_for_overflow();
	set_output_compare(byte & (1 << 6));
	wait_for_overflow();
	set_output_compare(byte & (1 << 5));
	wait_for_overflow();
	set_output_compare(byte & (1 << 4));
	wait_for_overflow();
	set_output_compare(byte & (1 << 3));
	wait_for_overflow();
	set_output_compare(byte & (1 << 2));
	wait_for_overflow();
	set_output_compare(byte & (1 << 1));
	wait_for_overflow();
	set_output_compare(byte & (1 << 0));
}

void sk6812_update(void)
{
	uint8_t *u8data = (uint8_t*)sk6812_data;

	start_timer();

	for(uint16_t i = 0; i < NLEDS*4; i++) {
		send_byte(u8data[i]);
	}

	TIFR0 |= (1 << OCF0B); // clear interrupt flag
	while((TIFR0 & (1 << OCF0B)) == 0); // wait for next timer compare

	stop_timer();
	SK6812_PORT &= ~(1 << SK6812_PIN);
}

