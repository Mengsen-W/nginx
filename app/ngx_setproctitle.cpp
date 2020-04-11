/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-11 20:20:39
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-11 22:20:53
 * @Description: 设置进程名，将环境变量拷贝到新内存中
 */

// 重新把环境变量保存到自己的内存上，然后修改argv
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ngx_global.h"

void ngx_init_setproctitle() {
  // 统计环境变量所占的内存。注意判断方法是 environ[i]
  // 是否为空作为环境变量的结束标记
  for (int i = 0; environ[i]; ++i) {
    g_environlen +=
        strlen(environ[i]) + 1;  // 加1是因为末尾有 /0 是占用实际内存的要算进来
  }

  // 这里无需判断 penvmen == NULL
  gp_envmem = new char[g_environlen];
  // 清零内存
  memset(gp_envmem, 0, g_environlen);

  char* ptmp = gp_envmem;

  // 填充内存
  for (int i = 0; environ[i]; ++i) {
    size_t size = strlen(environ[i]) + 1;  // 环境表是没有变化的
    strcpy(ptmp, environ[i]);  // 把原环境变量内容拷贝到新内存
    environ[i] = ptmp;         // 将原来新环境变量指向新内存
    ptmp += size;              // 为下次分配准备
  }
  return;
}

// 设置程序标题
void ngx_setproctitle(const char* title) {
  // 假设所有命令行参数都不需要使用了，可以被随意覆盖
  // 标题长度不能长到原始长度+环境变量长度都不够用

  // 计算新标题长度
  size_t ititlelen = strlen(title);

  // 计算argv总长度
  size_t e_environlen = 0;
  for (int i = 0; g_os_argv[i]; ++i) {
    e_environlen += strlen(g_os_argv[i]) + 1;
  }

  size_t esy = e_environlen + g_environlen;
  if (esy <= ititlelen) return;  // 核实长度

  // 设置后续命令行参数为空，学的源码可能是冗余设置
  g_os_argv[1] = nullptr;

  // 拷贝参数
  char* ptmp = g_os_argv[0];
  strcpy(ptmp, title);
  ptmp += ititlelen;  // 跳过标题

  // 将剩余空间全部清理
  size_t cha = esy - ititlelen;
  memset(ptmp, 0, cha);
  return;
}

// 释放环境变量
void ngx_deleteEnvironment() {
  if (gp_envmem) {
    delete[] gp_envmem;
    gp_envmem = nullptr;
  }
  return;
}