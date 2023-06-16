#ifndef FILE_XFER_CONF_H
#define FILE_XFER_CONF_H

/* Actually means message payload size */
#define FXFER_DEFAULT_WINDOW_SIZE        256

/* Size of buffer used to store tx packets */
#define FXFER_TX_BUF_SIZE                256

/* Rx buffer used to store received packets */
#define FXFER_RX_BUF_SIZE                (FXFER_PACK_PREAM_FIELD_LEN +\
                                          FXFER_PACK_MSGID_FIELD_LEN + \
                                          FXFER_PACK_LEN_FIELD_LEN + \
                                          FXFER_DEFAULT_WINDOW_SIZE + \
                                          FXFER_PACK_CRC_FIELD_LEN)

/* Timeout for waiting the response */
#define FXFER_RESPONSE_TIMEOUT_TICKS      1000

/* File name defines */
#define FXFER_FILE_NAME_LEN_MAX           16

#endif /* FILE_XFER_CONF_H */
