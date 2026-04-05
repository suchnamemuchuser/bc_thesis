#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <stdint.h>

// magic byte
#define PROTOCOL_MAGIC 0xAA

// packet Types
#define PACKET_TYPE_START    0x01
#define PACKET_TYPE_DATA     0x02
#define PACKET_TYPE_END      0x03
#define PACKET_TYPE_INACTIVE 0x04

#define DATA_PACKET_DEFAULT_SIZE 15000

// no padding
#pragma pack(push, 1)

// header
typedef struct {
    uint8_t magic;   // must be PROTOCOL_MAGIC
    uint8_t type;
    uint32_t value; // size of the payload
} ProtocolHeader;

typedef struct { // legacy, start should put id into value
    uint64_t timestamp; // unix timestamp
    char name[256];
} StartPayload;

#pragma pack(pop) // restore padding

#endif // NETWORK_PROTOCOL_H