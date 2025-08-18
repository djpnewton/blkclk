#ifndef __PAGES_H__
#define __PAGES_H__

#include "esp_err.h"

enum page_id
{
    PAGE_NONE,
    PAGE_HOME,
    PAGE_WIFI_SCAN,
    PAGE_WIFI_SCAN_FAIL,
    PAGE_WIFI_LIST,
    PAGE_WIFI_ENTER_PASSWORD,
};

enum page_action_t
{
    PAGE_ACTION_NONE,
    PAGE_ACTION_EXIT,
    PAGE_ACTION_WIFI_AP_SELECT,
    PAGE_ACTION_WIFI_PASSWORD_SUBMIT,
};

esp_err_t pages_init();
esp_err_t page_init(enum page_id id);
esp_err_t page_display(enum page_id id);
enum page_action_t page_action(enum page_id id);
void page_up(enum page_id id);
void page_down(enum page_id id);
esp_err_t screen_turn_off();
esp_err_t screen_turn_on();

#endif // __PAGES_H__