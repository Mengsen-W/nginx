/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-01 15:58:26
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-01 18:19:10
 * @Description: 内存管理相关
 */

#include "ngx_c_memory.h"

#include <cstring>

CMemory* CMemory::m_instance = nullptr;

/*
 * @ Description: 分配数组内存
 * @ Parameter:
 *    int memCount(单元大小),
 *    bool ifmemset(标志位是否把内存初始化为0)
 * @ Return: void *(分配地址的指针)
 */
void* CMemory::AllocMemory(int memCount, bool ifmemset) {
  void* tmpData = (void*)new char[memCount]; /* new 内存 失败就dump吧 */
  if (ifmemset) {
    memset(tmpData, 0, memCount);
  }
  return tmpData;
}

/*
 * @ Description: 释放内存
 * @ Parameter: void * pointer(欲释放数组的指针)
 * @ Return: void
 */
void CMemory::FreeMemory(void* pointer) { delete[]((char*)pointer); }