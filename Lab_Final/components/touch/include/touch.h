#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>

#define TOUCH_VOLUME_UP 1
#define TOUCH_PLAY_PAUSE 2
#define TOUCH_RING 4
#define TOUCH_RECORD 5
#define TOUCH_PHOTO 6
#define TOUCH_NETWORK 11
#define TOUCH_VOLUME_DOWN 14

void touch_init(void);

void touch_update(void);

bool touch_pressed(uint8_t button);

#endif
