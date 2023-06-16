#ifndef FILE_XFER_CALLBACKS_H
#define FILE_XFER_CALLBACKS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdint.h>
#include <stdbool.h>

void files_list_gotten_cb(uint8_t files_num, uint8_t *files_names_arr);
void form_files_list_cb(uint8_t *payload_ptr, uint16_t free_space, uint16_t *payload_len);

void file_hash_gotten_cb(uint32_t *file_hash);
bool get_file_hash_cb(const char *file_name, uint32_t *file_hash);

bool get_file_size_cb(const char *file_name, uint32_t *file_size);
bool file_read_partial_cb(const char *file_name, uint32_t offset,
        uint32_t chunc_size, uint8_t *out_buf);
bool file_append_cb(const char *file_name, uint32_t chunc_size,
        uint8_t *in_buf, bool *eof_flag);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FILE_XFER_CALLBACKS_H */
