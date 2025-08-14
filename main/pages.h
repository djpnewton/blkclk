#ifndef __PAGES_H__
#define __PAGES_H__

#include "esp_err.h"

enum page_id
{
    PAGE_NONE,
    PAGE_HOME,
    PAGE_WIFI,
};

esp_err_t pages_init();
esp_err_t page_display(enum page_id id);
esp_err_t screen_turn_off();
esp_err_t screen_turn_on();


#endif // __PAGES_H__