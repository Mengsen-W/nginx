/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-03 10:47:54
 * @Last Modified by:   Mengsen.Wang
 * @Last Modified time: 2020-05-03 10:47:54
 * @Description: crc32 算法
 */

#ifndef __NGX_C_CRC32_H__
#define __NGX_C_CRC32_H__

#include <stddef.h>  //NULL

class CCRC32 {
 private:
  CCRC32();

 public:
  ~CCRC32();

 private:
  static CCRC32* m_instance;

 public:
  static CCRC32* GetInstance() {
    if (m_instance == NULL) {
      //锁
      if (m_instance == NULL) {
        m_instance = new CCRC32();
        static GC_CRC32 cl;
      }
      //放锁
    }
    return m_instance;
  }

 private:
  class GC_CRC32 {
   public:
    ~GC_CRC32() {
      if (CCRC32::m_instance) {
        delete CCRC32::m_instance;
        CCRC32::m_instance = NULL;
      }
    }
  };

 public:
  void Init_CRC32_Table();
  unsigned int Reflect(unsigned int ref, char ch);
  int Get_CRC(unsigned char* buffer, unsigned int dwSize);

 public:
  unsigned int crc32_table[256];  // Lookup table arrays
};

#endif
