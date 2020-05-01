/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-01 15:42:43
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-01 18:23:04
 * @Description: 内存分配相关单例类
 */

#ifndef __NGX_C_MEMORY_H__
#define __NGX_C_MEMORY_H__

class CMemory {
 private:
  CMemory() {}

  static CMemory* m_instance;

  class GC_CMemory {
   public:
    ~GC_CMemory() {
      delete CMemory::m_instance;
      CMemory::m_instance = nullptr;
    }
  };

 public:
  static CMemory* GetInstance() {
    if (m_instance == nullptr) {
      if (m_instance == nullptr) {
        m_instance = new CMemory();
        static GC_CMemory gc;
      }
    }
    return m_instance;
  }

  void* AllocMemory(int memCount, bool ifmemset);
  void FreeMemory(void* pointer);
};

#endif
