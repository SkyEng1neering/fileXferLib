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
-

## Protocol description
#
#### Common packets structure
All the packets in protocol have the following structure
| | PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| - | ------ | ------ | ------ |------ |------ |
| **Field type** | uint32_t | uint8_t | uint16_t | uint8_t* | uint32_t |
| **Description** | 0xDEADBEEF | Msg IDs (see below) | Length of payload, bytes | File or hash data in specific format, if present in the packet | Checksum covering full packet from preamble to payload |
#
#### Packets list
Below you can find total messages list used by protocol:
| Msg name | Msg ID | Description |
| - | - | - |
| HANDSHAKE_REQ | 1 | Handshake request, used to initiate connection and to give to the respondent info about working buffer size. The respondent will use this info to limit payload length of its packets |
| HANDSHAKE_RES | 2 | Handshake response, used to accept connection and to give to the respondent info about working buffer size. The respondent will use this info to limit payload length of its packets |
| FILES_LIST_REQ | 3 | Request list of available files |
| FILES_LIST_RES | 4 | Response with list of available files |
| FILE_HASH_REQ | 5 | Request of hashsum for specific file |
| FILE_HASH_RES | 6 | Response with hashsum for specific file |
| FILE_SEND_REQ | 7 | Request of starting file send procedure |
| FILE_RECEIVE_REQ |  8 | Request of starting file receive procedure |
| FILE_DATA | 9 | File data, if file size is more than maximum allowed payload size, it sends by fragments |
| ACK | 10 | Acknowledgement of received packet |
| NACK | 11 | Signalize about some error, contains several error codes |
#
#### Packets description
**HANDSHAKE_REQ**
Used to initiate "connection" prodedure. The purpose of this packet is not only initiation of connection, but also giving to the respondend info about maximum payload that should be used while data xfer. This parameter is called WINDOW_SIZE.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 1 | 2 | WINDOW_SIZE (4 to sizeof(uint16_t) bytes) | crc32 |
---
**HANDSHAKE_RES**
Used to accept "connection" prodedure. The purpose of this packet is not only acception of connection, but also giving to the respondend info about maximum payload that should be used while data xfer. This parameter is called WINDOW_SIZE.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 2 | 2 | WINDOW_SIZE (4 to sizeof(uint16_t) bytes) | crc32 |
---
**FILES_LIST_REQ**
Used to request list of files available in respondent's storage. There is no payload in the packet.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 3 | 0 | - | crc32 |
---

**FILES_LIST_RES**
Response to the files list request. Contains array of filenames.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 4 | 0 to WINDOW_SIZE | PAYLOAD (see below) | crc32 |

PAYLOAD format:
| FILES_NUM | NAMESTRUCT_ARR |
| -- | -- |
| Number of files available | Array of structures with file names: { NAME_LEN (uint8_t), NAME (uint8_t *) }
---
**FILE_HASH_REQ**
Used to request hash of specific file.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 5 | 2 to WINDOW_SIZE | { NAME_LEN (uint8_t), NAME (uint8_t *) } | crc32 |
---

**FILE_HASH_RES**
Used to response on hash request.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 6 | 4 | FILE_HASH | crc32 |
---

**FILE_SEND_REQ**
Used to request of send specific file.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 7 | 2 to WINDOW_SIZE | { NAME_LEN (uint8_t), NAME (uint8_t *) } | crc32 |
---

**FILE_RECEIVE_REQ**
Used to request specific file.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 8 | 2 to WINDOW_SIZE | { NAME_LEN (uint8_t), NAME (uint8_t *) } | crc32 |
---

**FILE_DATA**
Used to send file data.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 9 | 2 to WINDOW_SIZE | PAYLOAD (see below) | crc32 |

PAYLOAD format:
| CURRENT_SEGMENT_IND | SEGMENT_DATA |
| -- | -- |
| Index of current data segment. Decrements from N to 0, index 0 means that it's the last segment (uint32_t) | uint8_t* |
---

**ACK**
Used to accept operation or received data segment. There is no payload in the packet.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 10 | 0 | - | crc32 |
---

**NACK**
Used to report an error.

**Packet format:**
| PREAMBLE | MSG_ID | LEN | PAYLOAD | CRC |
| ------ | ------ | ------ |------ |------ |
| 0xDEADBEEF | 10 | 1 | ERROR_CODE (see below) | crc32 |

**NACK error codes:**
| ERROR_CODE | Description |
| -----------| ------------|
| 1 | NO_HANDSHAKE |
| 2 | WRONG_CRC |
| 3 | UNEXPECTED_PACKET |
| 4 | BAD_REQUEST |
| 5 | NO_MEMORY |
---
#
#
#
#### Data flow
**Handshake**
Session starts with connection procedure, the one device should send **HANDSHAKE_REQ** packet and wait for response of another device with packet **HANDSHAKE_RES**. Connection is considered established on the condition that the response received within the timeout.
In the handshake messages devices gives to each othe the info about maximum value of message payload that they can process. For example it will affect the size of data segments while transfer the file with bigger size than amount of working RAM of the device.
#
Before the handshake procedure happened any other request should be responded with **NACK** packet with error code **NO_HANDSHAKE**.
The handshake procedure can be done several times per session if some of devices needed for example dinamycally change its message payload size.

**Hash request**
Protocol supports request of hash for specific file by it's name. CRC32 is used as a hash function, it is on the library implementation side - to decide which polynome to use.
To request hash of file device should send **FILE_HASH_REQ** packet and wait the response within the timeout value (timeout value defined in library implementation).
In case of request the hash of file that doesn't exist - respondent responds with **NACK** packet with error code **BAD_REQUEST**

**Send the file**
To send file, the device that initiate this process should send the packet **FILE_SEND_REQ** to get permission to start file send session. If respondent is ready to receive the file it responds with **ACK** packet.
After this file data should be send with **FILE_DATA** packet. If the file size is more than payload size that should be used for respondent - file sent by fragments. Each fragment of file has it index that decrements from N to 0. The last data segment has index 0.

**Request the file**
To send file, the device that initiate this process should send the packet **FILE_RECEIVE_REQ** to start file send session. If respondent is ready to send the file it responds with **ACK** packet.
After this file data should be send with **FILE_DATA** packet. If the file size is more than payload size that should be used for respondent - file sent by fragments. Each fragment of file has it index that decrements from N to 0. The last data segment has index 0.

**Request of files list**
**FILES_LIST_REQ** used to get list of available files in the respondent's storage. The packet **FILES_LIST_RES** that should be sent as a response contains names of available files.
