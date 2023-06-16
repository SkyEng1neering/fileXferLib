#ifndef FILE_XFER_H
#define FILE_XFER_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>
#include "fileXferConf.h"

#define FXFER_PARSE_STATES_NUM         3

enum file_xfer_parse_states {
    FXFER_PSTATE_WAIT_PREAMBLE = 0,
    FXFER_PSTATE_WAIT_BODY,
    FXFER_PSTATE_PROCESS_MSG
};

enum file_xfer_session_states {
    FXFER_SSTATE_IDLE = 0,
    FXFER_SSTATE_WAIT_HANDSHAKE,
    FXFER_SSTATE_WAIT_FILEHASH,
    FXFER_SSTATE_WAIT_FILESLIST,
    FXFER_SSTATE_WAIT_ACK,
    FXFER_SSTATE_WAIT_FILESEND_ACK,
    FXFER_SSTATE_WAIT_FILEREQ_ACK,
    FXFER_SSTATE_WAIT_FILE,
    FXFER_SSTATE_ERR_RECEIVED
};

enum file_xfer_err_states {
    FXFER_NO_ERROR = 0,
    FXFER_ERR_NO_HANDSHAKE,
    FXFER_ERR_WRONG_CRC,
    FXFER_ERR_UNEXPECTED_PACKET,
    FXFER_ERR_BAD_REQUEST,
    FXFER_ERR_NO_MEMORY
};

struct file_xfer_stat {
    char file_name_temp[FXFER_FILE_NAME_LEN_MAX];
    uint16_t tx_buf_fill_size;
    uint16_t rx_buf_fill_size;
    bool handshake_done_flag;
    uint16_t respondent_winsize;
    enum file_xfer_parse_states parse_state;
    enum file_xfer_session_states session_state;
    enum file_xfer_err_states last_error;
};

bool make_handshake(uint16_t window_size);
bool request_files_list();
bool request_file_hash(const char* filename);
bool send_file(const char* filename);
void fxfer_parser();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FILE_XFER_H */
