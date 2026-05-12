/**
 * @file time_sync.h
 * @brief POSIX TZ setup and SNTP after network has IP.
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

void time_sync_init_timezone(void);
void time_sync_sntp_after_ip(void);

#endif
