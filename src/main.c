#include <avr/io.h>
#include <util/delay.h>

#include <stdbool.h>

#include "sk6812.h"

int main(void)
{
	PORTB = 0;
	DDRB |= (1 << PIN5);

	sk6812_init();

	sk6812_setpixel(0, 0xFF, 0x00, 0x00, 0x00);
	sk6812_setpixel(1, 0x00, 0xFF, 0x00, 0x00);
	sk6812_setpixel(2, 0x00, 0x00, 0xFF, 0x00);
	sk6812_setpixel(3, 0x00, 0x00, 0x00, 0xFF);

	while(true) {
		sk6812_update();
		_delay_us(80);
	}

	return 0;
}
