#ifndef FILE_XFER_UTILS_H
#define FILE_XFER_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdio.h>

#define CRC_BUFFER_SIZE  8192

uint32_t crc32_compute_buf(uint32_t in_crc32, const void *buf, size_t len);
int crc32_compute_file(FILE *file, uint32_t *out_crc32);
void write_uint32_le(uint32_t num, uint8_t *ptr);
void write_uint16_le(uint16_t num, uint8_t *ptr);
uint32_t get_uint32_by_ptr(void *ptr);
uint16_t get_uint16_by_ptr(void *ptr);
void hexdump(void *mem, unsigned int len, int (*print_fp)(const char *fmt, ...));
void print_str_hex(uint8_t* str, uint32_t str_len, int (*print_fp)(const char *fmt, ...));

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FILE_XFER_UTILS_H */
