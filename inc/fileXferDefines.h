#ifndef FILE_XFER_DEFINES_H
#define FILE_XFER_DEFINES_H

#define FXFER_PACK_PREAMBLE                 0xDEADBEEF

/* Packet fields indexes */
#define FXFER_PACK_MSGID_IND                4
#define FXFER_PACK_LEN_IND                  5
#define FXFER_PACK_PAYLOAD_IND              7

/* Packet field lengths */
#define FXFER_PACK_PREAM_FIELD_LEN          4
#define FXFER_PACK_MSGID_FIELD_LEN          1
#define FXFER_PACK_LEN_FIELD_LEN            2
#define FXFER_PACK_CRC_FIELD_LEN            4

/* Packets IDs */
#define FXFER_PACKS_NUM                     12
#define FXFER_PACK_ID_MIN                   1
#define FXFER_PACK_ID_MAX                   11
#define FXFER_PACK_HANDSHAKE_REQ            1
#define FXFER_PACK_HANDSHAKE_RES            2
#define FXFER_PACK_FILES_LIST_REQ           3
#define FXFER_PACK_FILES_LIST_RES           4
#define FXFER_PACK_FILE_HASH_REQ            5
#define FXFER_PACK_FILE_HASH_RES            6
#define FXFER_PACK_FILE_SEND_REQ            7
#define FXFER_PACK_FILE_RECEIVE_REQ         8
#define FXFER_PACK_FILE_DATA                9
#define FXFER_PACK_ACK                      10
#define FXFER_PACK_NACK                     11

/* NACK error codes */
#define FXFER_NACK_ERR_NO_HANDSHAKE         1
#define FXFER_NACK_ERR_WRONG_CRC            2
#define FXFER_NACK_ERR_UNEXPECTED_PACKET    3
#define FXFER_NACK_ERR_BAD_REQUEST          4
#define FXFER_NACK_ERR_NO_MEMORY            5

#endif /* FILE_XFER_DEFINES_H */
