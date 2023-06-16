# fileXferLib
_Library for p2p files transfer_

May be used for p2p files transfer files using any interface between 2 points

## Features
List of features supported by the protocol:
- File send
- File request
- Get available files list
- Get hash for concrete file

## Limitations
List of protocol limitations:
- Designed for transfer files up to 1 MB
- Commands are handled synchronously
- Directories aren't supported, it used for 'plain' files structure
- Currently not supported fragmentation of FILES_LIST_RES packet, so case when total list of files doesn't fit to FILES_LIST_RES packet is available (in case of little WINDOW_SIZE or big amount of files stored in requested device)
- It is possible to request list of available file names from respondent, but not the file sizes
- File name length max is 255 bytes
- Maximum files number on storage is 65535 bytes

## How to use
Library contain the protocol logic, and interacts with the system via platform specific functions. Also it uses event callbacks that used to store there some specific logic like save the received files or calculate file hash. So the first thing you need to do is implement in your code platform specific functions with prototypes described in ```fileXferPlatform.h ```:

```
void platform_send(uint8_t* data, uint16_t len);
uint16_t platform_read(uint8_t* data, uint16_t len);
void platform_sleep(uint32_t ms);
uint32_t platform_get_tick();
void log_info(const char* str, ...);
void log_debug(const char* str, ...);
void log_error(const char* str, ...);
```

Also you need to implement specific callbacks with prototypes described in ```fileXferCallbacks.h```:
```
void files_list_gotten_cb(uint8_t files_num, uint8_t *files_names_arr);
void form_files_list_cb(uint8_t *payload_ptr, uint16_t free_space, uint16_t *payload_len);

void file_hash_gotten_cb(uint32_t *file_hash);
bool get_file_hash_cb(const char *file_name, uint32_t *file_hash);

bool get_file_size_cb(const char *file_name, uint32_t *file_size);
bool file_read_partial_cb(const char *file_name, uint32_t offset,
                                uint32_t chunc_size, uint8_t *out_buf);
bool file_append_cb(const char *file_name, uint32_t chunc_size,
                                uint8_t *in_buf, bool *eof_flag);
```

After this stage you need to run function ```void fxfer_parser();``` in a loop in different thread (or if you don't want to use OS in your project just call it in interrupt handler for every new byte of data received by communication interface).

That's it, functions that you can use are described in ```fileXfer.h```:
```
bool make_handshake(uint16_t window_size);
bool request_files_list();
bool request_file_hash(const char* filename);
bool send_file(const char* filename);
void fxfer_parser();
```