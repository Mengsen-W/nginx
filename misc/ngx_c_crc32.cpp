/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-03 10:48:11
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-03 10:51:46
 * @Description: CRC32 算法
 */

#include "ngx_c_crc32.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

//类静态变量初始化
CCRC32* CCRC32::m_instance = NULL;

/*
 * @ Description: 构造函数
 */
CCRC32::CCRC32() { Init_CRC32_Table(); }

/*
 * @ Description: 析构函数
 */
CCRC32::~CCRC32() {}
unsigned int CCRC32::Reflect(unsigned int ref, char ch) {
  unsigned int value(0);
  for (int i = 1; i < (ch + 1); i++) {
    if (ref & 1) value |= 1 << (ch - i);
    ref >>= 1;
  }
  return value;
}

/*
 * @ Description: 初始化
 */
void CCRC32::Init_CRC32_Table() {
  unsigned int ulPolynomial = 0x04c11db7;

  for (int i = 0; i <= 0xFF; i++) {
    crc32_table[i] = Reflect(i, 8) << 24;

    for (int j = 0; j < 8; j++) {
      crc32_table[i] = (crc32_table[i] << 1) ^
                       (crc32_table[i] & (1 << 31) ? ulPolynomial : 0);
    }
    crc32_table[i] = Reflect(crc32_table[i], 32);
  }
}

/*
 * @ Description: 用crc32_table 寻找表来产生CRC值
 * @ Parameter: unsigned char* buffer, unsigned int dwSize
 * @ Returns: void
 */
int CCRC32::Get_CRC(unsigned char* buffer, unsigned int dwSize) {
  unsigned int crc(0xffffffff);
  int len;

  len = dwSize;

  while (len--) crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ *buffer++];

  return crc ^ 0xffffffff;
}
