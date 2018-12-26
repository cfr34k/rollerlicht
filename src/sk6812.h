#ifndef SK6812_H
#define SK6812_H

#include <stdint.h>

#define NLEDS 54

/*!
 * Structure representing a pixel on the SK6812 strip.
 *
 * The elements are ordered as they have to be sent out to the strip.
 */
struct SK6812Pixel {
	uint8_t g, r, b, w;
};

void sk6812_init(void);
void sk6812_setpixel(uint8_t idx, uint8_t r, uint8_t g, uint8_t b, uint8_t w);
void sk6812_update(void);

#endif // SK6812_H
