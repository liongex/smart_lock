#ifndef DRIVER_DISPLAY_H
#define DRIVER_DISPLAY_H

void display_hw_init(void);
void display_show_message(const char* line1, const char* line2);
void display_power_off(void);
void display_power_on(void);

#endif // DRIVER_DISPLAY_H