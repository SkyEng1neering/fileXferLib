#include <string.h>
#include "fileXfer.h"
#include "fileXferUtils.h"
#include "fileXferDefines.h"
#include "fileXferPlatform.h"
#include "fileXferCallbacks.h"

/* Tx/Rx buffers */
static uint8_t tx_buf[FXFER_TX_BUF_SIZE];
static uint8_t rx_buf[FXFER_RX_BUF_SIZE];

/* State structure */
static struct file_xfer_stat fxfer_status = {
    .handshake_done_flag = false,
    .parse_state = FXFER_PSTATE_WAIT_PREAMBLE,
    .session_state = FXFER_SSTATE_IDLE,
    .last_error = FXFER_NO_ERROR
};

/* Utility functions for forming message */
static void fill_preamble();
static void fill_msg_id(uint8_t msg_id);
static void fill_len(uint16_t msg_len);
static void fill_payload(uint8_t *data, uint16_t len);
static void fill_msg_crc();
static void send_msg();

/* Functions used for parsing incoming messages */
static void parser_wait_preamble();
static void parser_wait_body();
static void parser_process_message();

/* Functions for make responses */
static void report_nack(uint8_t error_code);
static void report_ack();

/* Message handlers */
static void handshake_req_handler(void* arg);
static void handshake_res_handler(void* arg);
static void files_list_req_handler(void* arg);
static void files_list_res_handler(void* arg);
static void file_hash_req_handler(void* arg);
static void file_hash_res_handler(void* arg);
static void file_send_req_handler(void* arg);
static void file_receive_req_handler(void* arg);
static void file_data_handler(void* arg);
static void ack_handler(void* arg);
static void nack_handler(void* arg);
static void default_handler(void* arg);

/* Array of parser functions */
static void (*parse_func_arr[FXFER_PARSE_STATES_NUM])() = {
        parser_wait_preamble,
        parser_wait_body,
        parser_process_message
};

/* Array of on-receive msg handlers */
static void (*msg_handlers_arr[FXFER_PACKS_NUM])(void*) = {
        default_handler,
        handshake_req_handler,
        handshake_res_handler,
        files_list_req_handler,
        files_list_res_handler,
        file_hash_req_handler,
        file_hash_res_handler,
        file_send_req_handler,
        file_receive_req_handler,
        file_data_handler,
        ack_handler,
        nack_handler
};

void fxfer_parser() {
    /* Call parser function that corresponds to current state */
    parse_func_arr[fxfer_status.parse_state]();
}

/* Waiting for preamble, and switch state when it's found */
static void parser_wait_preamble() {
    static uint32_t preamble = FXFER_PACK_PREAMBLE;
    static uint8_t pream_ind = 0;
    fxfer_status.rx_buf_fill_size = 0;

    /* Read the byte and compare it with expected value */
    uint8_t data = 0;
    uint16_t res = platform_read(&data, sizeof(data));

    if (res != sizeof(data)) {
        /* Some read error */
        fxfer_status.parse_state = FXFER_PSTATE_WAIT_PREAMBLE;
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        log_error("platform_read() error, read %u bytes instead of %u\n", res, sizeof(data));
        return;
    }

    if (data == ((uint8_t*)&preamble)[pream_ind]) {
        if (pream_ind == 0) {
            /* The first byte of preamble received */
            pream_ind++;
        } else if (pream_ind == 1) {
            /* The second byte of preamble received */
            pream_ind++;
        } else if (pream_ind == 2) {
            /* The third byte of preamble received */
            pream_ind++;
        } else if (pream_ind == 3) {
            /* The fourth byte of preamble received */
            pream_ind = 0;

            /* Place it to the receive storage */
            write_uint32_le(FXFER_PACK_PREAMBLE, rx_buf);
            fxfer_status.rx_buf_fill_size += FXFER_PACK_PREAM_FIELD_LEN;

            /* Change parse state */
            fxfer_status.parse_state = FXFER_PSTATE_WAIT_BODY;
        }
    } else {
        pream_ind = 0;
    }
}

/* Accumulates other part of packet and check it's validity */
static void parser_wait_body() {
    /* Get MSG_ID and LEN */
    uint16_t read_len = FXFER_PACK_MSGID_FIELD_LEN + FXFER_PACK_LEN_FIELD_LEN;
    uint16_t res = platform_read(&rx_buf[fxfer_status.rx_buf_fill_size], read_len);
    if (res != read_len) {
        /* Some read error */
        fxfer_status.parse_state = FXFER_PSTATE_WAIT_PREAMBLE;
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        log_error("platform_read() error, read %u bytes instead of %u\n", res, read_len);
        return;
    }
    fxfer_status.rx_buf_fill_size += read_len;

    /* Get payload */
    uint16_t len = get_uint16_by_ptr(&rx_buf[FXFER_PACK_LEN_IND]);

    /* Check if it's not enough place in rx buffer */
    uint16_t free_space = FXFER_RX_BUF_SIZE - fxfer_status.rx_buf_fill_size;
    uint16_t needed_space = len + FXFER_PACK_CRC_FIELD_LEN;
    if (free_space < needed_space) {
        /* Not enough memory in rx buffer */
        fxfer_status.parse_state = FXFER_PSTATE_WAIT_PREAMBLE;
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        log_error("Not enough space in rx buffer. %u bytes is available, "
                "while %u needed to store the packet\n", free_space, needed_space);
        report_nack(FXFER_NACK_ERR_NO_MEMORY);
        return;
    }

    /* Get the rest part of data */
    read_len = len + FXFER_PACK_CRC_FIELD_LEN;
    res = platform_read(&rx_buf[fxfer_status.rx_buf_fill_size], read_len);
    if (res != read_len) {
        /* Some read error */
        fxfer_status.parse_state = FXFER_PSTATE_WAIT_PREAMBLE;
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        log_error("platform_read() error, read %u bytes instead of %u\n", res, read_len);
        return;
    }
    fxfer_status.rx_buf_fill_size += read_len;

    /* Gotten full packet, check it's validity */
    uint16_t msg_len_without_crc = fxfer_status.rx_buf_fill_size - FXFER_PACK_CRC_FIELD_LEN;
    uint32_t pack_crc32 = get_uint32_by_ptr(&rx_buf[msg_len_without_crc]);
    uint32_t calc_crc32 = crc32_compute_buf(0, rx_buf, msg_len_without_crc);
    if (pack_crc32 != calc_crc32) {
        /* Packet with wrong crc32 */
        fxfer_status.parse_state = FXFER_PSTATE_WAIT_PREAMBLE;
        log_error("Gotten packet with wrong crc. Given: 0x%08X, calculated: 0x%08X\n",
                pack_crc32, calc_crc32);
        report_nack(FXFER_NACK_ERR_WRONG_CRC);
        return;
    }

    /* Packet is valid, switch state */
    fxfer_status.parse_state = FXFER_PSTATE_PROCESS_MSG;
}

static void parser_process_message() {
    /* Gotten MSG_ID, check it */
    uint8_t msg_id = rx_buf[FXFER_PACK_MSGID_IND];
    if (msg_id < FXFER_PACK_ID_MIN || msg_id > FXFER_PACK_ID_MAX) {
        /* Unrecognized message ID */
        fxfer_status.parse_state = FXFER_PSTATE_WAIT_PREAMBLE;
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        log_error("Gotten unrecognized message id: %u\n", msg_id);
        return;
    }

    /* Call corresponding msg handler */
    uint16_t payload_ind = FXFER_PACK_PREAM_FIELD_LEN + FXFER_PACK_MSGID_FIELD_LEN
            + FXFER_PACK_LEN_FIELD_LEN;
    msg_handlers_arr[msg_id](&rx_buf[payload_ind]);
    fxfer_status.parse_state = FXFER_PSTATE_WAIT_PREAMBLE;
}

static void report_nack(uint8_t error_code) {
    fill_preamble();
    fill_msg_id(FXFER_PACK_NACK);
    fill_len(sizeof(uint8_t));
    fill_payload((uint8_t *)&error_code, sizeof(uint8_t));
    fill_msg_crc();
    send_msg();
}

static void report_ack() {
    fill_preamble();
    fill_msg_id(FXFER_PACK_ACK);
    fill_len(0);
    fill_msg_crc();
    send_msg();
}

bool make_handshake(uint16_t window_size) {
    /* Form HANDSHAKE_REQ */
    fill_preamble();
    fill_msg_id(FXFER_PACK_HANDSHAKE_REQ);
    fill_len(sizeof(uint16_t));
    fill_payload((uint8_t *)&window_size, sizeof(uint16_t));
    fill_msg_crc();

    /* Switch session state */
    fxfer_status.session_state = FXFER_SSTATE_WAIT_HANDSHAKE;
    fxfer_status.last_error = FXFER_NO_ERROR;

    /* Send message */
    send_msg();

    /* Wait cycle with short sleep */
    uint32_t start_tick = platform_get_tick();
    bool timeout_flag = false;
    while (fxfer_status.session_state == FXFER_SSTATE_WAIT_HANDSHAKE) {
        if (platform_get_tick() - start_tick >= FXFER_RESPONSE_TIMEOUT_TICKS) {
            timeout_flag = true;
            break;
        }
        platform_sleep(10);
    }

    /* Handle timeout */
    if (timeout_flag == true) {
        log_error("make_handshake() timeout\n");
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    /* Handle possible errors */
    if (fxfer_status.session_state == FXFER_SSTATE_ERR_RECEIVED) {
        log_error("make_handshake() error: %u\n", fxfer_status.last_error);
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    log_debug("Handshake done\n");
    return true;
}

bool request_files_list() {
    /* Form FXFER_PACK_FILES_LIST_REQ */
    fill_preamble();
    fill_msg_id(FXFER_PACK_FILES_LIST_REQ);
    fill_len(0);
    fill_msg_crc();

    /* Switch session state */
    fxfer_status.session_state = FXFER_SSTATE_WAIT_FILESLIST;
    fxfer_status.last_error = FXFER_NO_ERROR;

    /* Send message */
    send_msg();

    /* Wait cycle with short sleep */
    uint32_t start_tick = platform_get_tick();
    bool timeout_flag = false;
    while (fxfer_status.session_state == FXFER_SSTATE_WAIT_FILESLIST) {
        if (platform_get_tick() - start_tick >= FXFER_RESPONSE_TIMEOUT_TICKS) {
            timeout_flag = true;
            break;
        }
        platform_sleep(10);
    }

    /* Handle timeout */
    if (timeout_flag == true) {
        log_error("request_files_list() timeout\n");
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    /* Handle possible errors */
    if (fxfer_status.session_state == FXFER_SSTATE_ERR_RECEIVED) {
        log_error("request_files_list() error: %u\n", fxfer_status.last_error);
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    log_debug("Files list requested\n");
    return true;
}

bool request_file_hash(const char* filename) {
    /* Form FXFER_PACK_FILE_HASH_REQ */
    fill_preamble();
    fill_msg_id(FXFER_PACK_FILE_HASH_REQ);
    uint16_t len = (uint16_t)strlen(filename);
    fill_len(len + 1); //+1 to count \0
    fill_payload((uint8_t *)filename, len + 1);
    fill_msg_crc();

    /* Switch session state */
    fxfer_status.session_state = FXFER_SSTATE_WAIT_FILEHASH;
    fxfer_status.last_error = FXFER_NO_ERROR;

    /* Send message */
    send_msg();

    /* Wait cycle with short sleep */
    uint32_t start_tick = platform_get_tick();
    bool timeout_flag = false;
    while (fxfer_status.session_state == FXFER_SSTATE_WAIT_FILEHASH) {
        if (platform_get_tick() - start_tick >= FXFER_RESPONSE_TIMEOUT_TICKS) {
            timeout_flag = true;
            break;
        }
        platform_sleep(10);
    }

    /* Handle timeout */
    if (timeout_flag == true) {
        log_error("request_file_hash() timeout\n");
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    /* Handle possible errors */
    if (fxfer_status.session_state == FXFER_SSTATE_ERR_RECEIVED) {
        log_error("request_file_hash() error: %u\n", fxfer_status.last_error);
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    log_debug("File hash requested\n");
    return true;
}

bool send_file(const char* filename) {
    /* Request file send procedure */
    fill_preamble();
    fill_msg_id(FXFER_PACK_FILE_SEND_REQ);
    uint16_t len = (uint16_t)strlen(filename);
    fill_len(len + 1); //+1 to count \0
    fill_payload((uint8_t *)filename, len + 1);
    fill_msg_crc();

    /* Switch session state */
    fxfer_status.session_state = FXFER_SSTATE_WAIT_ACK;
    fxfer_status.last_error = FXFER_NO_ERROR;

    /* Send message */
    send_msg();

    /* Wait cycle with short sleep */
    uint32_t start_tick = platform_get_tick();
    bool timeout_flag = false;
    while (fxfer_status.session_state == FXFER_SSTATE_WAIT_ACK) {
        if (platform_get_tick() - start_tick >= FXFER_RESPONSE_TIMEOUT_TICKS) {
            timeout_flag = true;
            break;
        }
        platform_sleep(10);
    }

    /* Handle timeout */
    if (timeout_flag == true) {
        log_error("Request file send timeout\n");
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    /* Handle possible errors */
    if (fxfer_status.session_state == FXFER_SSTATE_ERR_RECEIVED) {
        log_error("Request file send error: %u\n", fxfer_status.last_error);
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        fxfer_status.last_error = FXFER_NO_ERROR;
        return false;
    }

    log_debug("File send request accepted, start sending the file\n");

    /* Get file size */
    uint32_t file_size = 0;
    bool res = get_file_size_cb(filename, &file_size);
    if (res != true) {
        /* Get file size error */
        log_error("Get size of file %s error\n", filename);
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        return false;
    }

    log_debug("Size of file %s is %u bytes\n", filename, file_size);

    /* Calc segments number for file */
    uint16_t seg_num = file_size % (fxfer_status.respondent_winsize - 2) > 0
                    ? (file_size / (fxfer_status.respondent_winsize - 2)) + 1
                    : file_size / (fxfer_status.respondent_winsize - 2);
    uint16_t current_seg_ind = seg_num - 1;
    uint16_t current_offset = 0;
    log_debug("Segments total: %u, the first seg_ind: %u\n", seg_num, current_seg_ind);

    for (uint16_t i = 0; i < seg_num; i++) {
        /* Calculate current chunc size */
        uint16_t current_chunc_size = current_seg_ind > 0 ? fxfer_status.respondent_winsize - 2
                : file_size % (fxfer_status.respondent_winsize - 2);

        /* Form data packet */
        fill_preamble();
        fill_msg_id(FXFER_PACK_FILE_DATA);
        fill_len(sizeof(uint16_t) + current_chunc_size); //seg_ind + seg_data
        write_uint16_le(current_seg_ind, &tx_buf[FXFER_PACK_PAYLOAD_IND]);
        if (file_read_partial_cb(filename, current_offset, current_chunc_size,
                &tx_buf[sizeof(uint16_t) + FXFER_PACK_PAYLOAD_IND]) != true) {
            /* Platform error */
            log_error("File read partial error. Filename: %s, total size: %u, "
                    "offset: %u, chunk size: %u\n",
                    filename, file_size, current_offset, current_chunc_size);
            fxfer_status.session_state = FXFER_SSTATE_IDLE;
            return false;
        }
        fxfer_status.tx_buf_fill_size += sizeof(uint16_t) + current_chunc_size;
        fill_msg_crc();

        /* Switch session state */
        fxfer_status.session_state = FXFER_SSTATE_WAIT_ACK;
        fxfer_status.last_error = FXFER_NO_ERROR;

        /* Send message */
        send_msg();

        /* Wait for ACK */
        /* Wait cycle with short sleep */
        uint32_t start_tick = platform_get_tick();
        bool timeout_flag = false;
        while (fxfer_status.session_state == FXFER_SSTATE_WAIT_ACK) {
            if (platform_get_tick() - start_tick >= FXFER_RESPONSE_TIMEOUT_TICKS) {
                timeout_flag = true;
                break;
            }
            platform_sleep(1);
        }

        /* Handle timeout */
        if (timeout_flag == true) {
            log_error("ACK wait timeout\n");
            fxfer_status.session_state = FXFER_SSTATE_IDLE;
            fxfer_status.last_error = FXFER_NO_ERROR;
            return false;
        }

        /* Handle possible errors */
        if (fxfer_status.session_state == FXFER_SSTATE_ERR_RECEIVED) {
            log_error("File send error: %u\n", fxfer_status.last_error);
            fxfer_status.session_state = FXFER_SSTATE_IDLE;
            fxfer_status.last_error = FXFER_NO_ERROR;
            return false;
        }

        log_debug("Sent seg_in: %u, with offset %u\n", current_seg_ind, current_offset);
        current_seg_ind--;
        current_offset += current_chunc_size;
    }

    fxfer_status.session_state = FXFER_SSTATE_IDLE;
    log_debug("File %s, with size %u bytes sent successfully\n", filename, file_size);
    return true;
}

/* Message handlers */
static void handshake_req_handler(void* arg) {
    uint16_t win_size = get_uint16_by_ptr(arg);
    log_debug("Handshake request received, with window size: %u\n", win_size);

    /* Save handshake result */
    fxfer_status.respondent_winsize = win_size;
    fxfer_status.handshake_done_flag = true;

    /* Respond with FXFER_PACK_HANDSHAKE_RES */
    fill_preamble();
    fill_msg_id(FXFER_PACK_HANDSHAKE_RES);
    fill_len(sizeof(uint16_t));
    uint16_t window_size = FXFER_DEFAULT_WINDOW_SIZE;
    fill_payload((uint8_t *)&window_size, sizeof(uint16_t));
    fill_msg_crc();
    send_msg();
    log_debug("Handshake response sent, with window size: %u\n", window_size);
}

static void handshake_res_handler(void* arg) {
    uint16_t win_size = get_uint16_by_ptr(arg);
    log_debug("Handshake response received, with window size: %u\n", win_size);
    if (fxfer_status.session_state == FXFER_SSTATE_WAIT_HANDSHAKE) {
        fxfer_status.respondent_winsize = win_size;
        fxfer_status.handshake_done_flag = true;
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
    } else {
        log_error("Packet wasn't awaited\n");
        report_nack(FXFER_NACK_ERR_UNEXPECTED_PACKET);
    }
}

static void files_list_req_handler(void* arg) {
    log_debug("Files list request received\n");

    /* Check if handshake wasn't yet */
    if (fxfer_status.handshake_done_flag == false) {
        log_error("There was no handshake yet\n");
        report_nack(FXFER_NACK_ERR_NO_HANDSHAKE);
        return;
    }

    /* Respond with FXFER_PACK_FILES_LIST_RES */
    fill_preamble();
    fill_msg_id(FXFER_PACK_FILES_LIST_RES);

    uint16_t free_space_in_tx_buf = FXFER_TX_BUF_SIZE - FXFER_PACK_PREAM_FIELD_LEN
            - FXFER_PACK_MSGID_FIELD_LEN - FXFER_PACK_LEN_FIELD_LEN
            - FXFER_PACK_CRC_FIELD_LEN;
    uint16_t free_space = fxfer_status.respondent_winsize > free_space_in_tx_buf
            ? free_space_in_tx_buf : fxfer_status.respondent_winsize;
    uint8_t *payload_ptr = &tx_buf[FXFER_PACK_PAYLOAD_IND];
    uint16_t payload_len;

    form_files_list_cb(payload_ptr, free_space, &payload_len);
    fxfer_status.tx_buf_fill_size += payload_len;

    fill_len(payload_len);
    fill_msg_crc();
    send_msg();
    log_debug("Files list response sent\n");
}

static void files_list_res_handler(void* arg) {
    /* Get file numbers and filenames array */
    uint8_t *payload = (uint8_t *)arg;
    uint8_t files_num = payload[0];
    uint8_t *filenames_arr = &payload[1];
    log_debug("Files list response received, with files num: %u\n", files_num);
    if (fxfer_status.session_state == FXFER_SSTATE_WAIT_FILESLIST) {
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        files_list_gotten_cb(files_num, filenames_arr);
    } else {
        log_error("Packet wasn't awaited\n");
        report_nack(FXFER_NACK_ERR_UNEXPECTED_PACKET);
    }
}

static void file_hash_req_handler(void* arg) {
    log_debug("File hash request received\n");

    /* Check if handshake wasn't yet */
    if (fxfer_status.handshake_done_flag == false) {
        log_error("There was no handshake yet\n");
        report_nack(FXFER_NACK_ERR_NO_HANDSHAKE);
        return;
    }

    /* Respond with FXFER_PACK_FILE_HASH_RES */
    fill_preamble();
    fill_msg_id(FXFER_PACK_FILE_HASH_RES);
    fill_len(sizeof(uint32_t));
    uint32_t file_hash;
    if (get_file_hash_cb((const char *)arg, &file_hash) != true) {
        log_error("Can't get hash for file %s\n", (const char *)arg);
        report_nack(FXFER_NACK_ERR_BAD_REQUEST);
        return;
    }
    fill_payload((uint8_t *)&file_hash, sizeof(uint32_t));
    fill_msg_crc();
    send_msg();
    log_debug("File hash response sent, gotten hash 0x%08X for the file %s\n",
            file_hash, (const char *)arg);
}

static void file_hash_res_handler(void* arg) {
    uint32_t crc32 = get_uint32_by_ptr(arg);
    log_debug("File hash response received, with crc32: 0x%08X\n", crc32);
    if (fxfer_status.session_state == FXFER_SSTATE_WAIT_FILEHASH) {
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
        file_hash_gotten_cb(&crc32);
    } else {
        log_error("Packet wasn't awaited\n");
        report_nack(FXFER_NACK_ERR_UNEXPECTED_PACKET);
    }
}

static void file_send_req_handler(void* arg) {
    log_debug("File send request received\n");
    strncpy(fxfer_status.file_name_temp, (const char*)arg, FXFER_FILE_NAME_LEN_MAX);
    log_debug("File name to send: %s\n", (const char*)arg);

    /* Check if handshake wasn't yet */
    if (fxfer_status.handshake_done_flag == false) {
        log_error("There was no handshake yet\n");
        report_nack(FXFER_NACK_ERR_NO_HANDSHAKE);
        return;
    }

    /* Respond with FXFER_PACK_ACK */
    fill_preamble();
    fill_msg_id(FXFER_PACK_ACK);
    fill_len(0);
    fill_msg_crc();

    /* Set state 'waiting for file' */
    fxfer_status.session_state = FXFER_SSTATE_WAIT_FILE;

    send_msg();
    log_debug("ACK sent\n");
}

static void file_receive_req_handler(void* arg) {

}

static void file_data_handler(void* arg) {
    log_debug("File data received\n");
    uint8_t *payload = (uint8_t *)arg;
    uint16_t chunc_len = get_uint16_by_ptr(&rx_buf[FXFER_PACK_LEN_IND]) - sizeof(uint16_t);
    uint16_t seg_ind = get_uint16_by_ptr(payload);
    uint8_t *data = (uint8_t *)&payload[sizeof(uint16_t)];
    bool eof_flag = seg_ind > 0 ? false : true;

    /* Check if handshake wasn't yet */
    if (fxfer_status.handshake_done_flag == false) {
        log_error("There was no handshake yet\n");
        report_nack(FXFER_NACK_ERR_NO_HANDSHAKE);
        return;
    }

    if (fxfer_status.session_state == FXFER_SSTATE_WAIT_FILE) {
        /* Get segment index and data pointer */
        if (file_append_cb(fxfer_status.file_name_temp, chunc_len, data, &eof_flag) != true) {
            fxfer_status.session_state = FXFER_SSTATE_IDLE;
            log_error("File %s data append error\n", fxfer_status.file_name_temp);
            return;
        }
        log_debug("File %s: %u bytes of data appended\n", fxfer_status.file_name_temp, chunc_len);
        if (eof_flag == true) {
            fxfer_status.session_state = FXFER_SSTATE_IDLE;
        }
        report_ack();
    } else {
        log_error("Packet wasn't awaited\n");
        report_nack(FXFER_NACK_ERR_UNEXPECTED_PACKET);
    }
}

static void ack_handler(void* arg) {
    log_debug("ACK received\n");
    if (fxfer_status.session_state == FXFER_SSTATE_WAIT_ACK) {
        fxfer_status.session_state = FXFER_SSTATE_IDLE;
    } else {
        log_error("Packet wasn't awaited\n");
        report_nack(FXFER_NACK_ERR_UNEXPECTED_PACKET);
    }
}

static void nack_handler(void* arg) {
    uint8_t *payload = (uint8_t *)arg;
    uint8_t err = payload[0];
    log_debug("NACK received, with files error: %u\n", err);
    fxfer_status.session_state = FXFER_SSTATE_ERR_RECEIVED;
    fxfer_status.last_error = err;
}

static void default_handler(void* arg) {

}

/* Utility functions for forming message */
static void fill_preamble() {
    fxfer_status.tx_buf_fill_size = 0;
    write_uint32_le(FXFER_PACK_PREAMBLE, tx_buf);
    fxfer_status.tx_buf_fill_size += sizeof(uint32_t);
}

static void fill_msg_id(uint8_t msg_id) {
    tx_buf[FXFER_PACK_MSGID_IND] = msg_id;
    fxfer_status.tx_buf_fill_size += sizeof(uint8_t);
}

static void fill_len(uint16_t msg_len) {
    write_uint16_le(msg_len, &tx_buf[FXFER_PACK_LEN_IND]);
    fxfer_status.tx_buf_fill_size += sizeof(uint16_t);
}

static void fill_payload(uint8_t *data, uint16_t len) {
    memcpy(&tx_buf[FXFER_PACK_PAYLOAD_IND], data, len);
    fxfer_status.tx_buf_fill_size += len;
}

static void fill_msg_crc() {
    /* Calc crc32 */
    uint32_t crc32 = crc32_compute_buf(0, tx_buf, fxfer_status.tx_buf_fill_size);
    write_uint32_le(crc32, &tx_buf[fxfer_status.tx_buf_fill_size]);
    fxfer_status.tx_buf_fill_size += sizeof(uint32_t);
}

static void send_msg() {
    platform_send(tx_buf, fxfer_status.tx_buf_fill_size);
}
