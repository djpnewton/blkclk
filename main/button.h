#ifndef __BUTTON_H__
#define __BUTTON_H__

#include <stdbool.h>

typedef enum
{
    BUTTON_NONE = 0,
    BUTTON_1_HELD,
    BUTTON_1_ACTIVATED,
    BUTTON_1_REPEAT,
    BUTTON_2_HELD,
    BUTTON_2_ACTIVATED,
    BUTTON_2_REPEAT,
    BUTTON_BOTH_HELD,
    BUTTON_BOTH_ACTIVATED,
    BUTTON_BOTH_REPEAT
} button_state_t;

void button_init(void);
button_state_t button_state(void);

#endif // __BUTTON_H__
