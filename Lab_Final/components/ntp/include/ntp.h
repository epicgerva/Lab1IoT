#ifndef NTP_SYNC_H
#define NTP_SYNC_H

#include "esp_err.h"
#include <time.h>
#include <stdbool.h>


esp_err_t ntp_init(void);
esp_err_t ntp_start(void);
void ntp_stop(void);
bool ntp_synced(void);
esp_err_t ntp_time(char *buffer, size_t buffer_size);
void ntp_print_status(void);

#endif // NTP_SYNC_H 