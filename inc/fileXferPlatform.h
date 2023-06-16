#ifndef FILE_XFER_PLATFORM_H
#define FILE_XFER_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void platform_send(uint8_t* data, uint16_t len);
uint16_t platform_read(uint8_t* data, uint16_t len);
void platform_sleep(uint32_t ms);
uint32_t platform_get_tick();
void log_info(const char* str, ...);
void log_debug(const char* str, ...);
void log_error(const char* str, ...);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FILE_XFER_PLATFORM_H */
