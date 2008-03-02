#ifndef _CRC_HPP_
#define _CRC_HPP_
#include "specs.hpp"
#include "macs.hpp"

uint16_t calc_crc(uint8_t *buf, int len);
uint32_t crc_file(bFILE *fp);


#endif