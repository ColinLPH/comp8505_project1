#include "commands.h"

#include <stdint.h>
#include <stdio.h>

int encode_ip(char *buffer, uint8_t byte1, uint8_t byte2, uint8_t byte3, uint8_t byte4) {
    return sprintf(buffer, "%u.%u.%u.%u", byte1, byte2, byte3, byte4);
}


int decode_ip(char *buffer, uint8_t *byte1, uint8_t *byte2, uint8_t *byte3, uint8_t *byte4) {
    return sscanf(buffer, "%hhu.%hhu.%hhu.%hhu", byte1, byte2, byte3, byte4);
}
