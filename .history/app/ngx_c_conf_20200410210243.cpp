﻿/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 18:48:56
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 21:02:42
 * @Description: 读取配置文件的实现
 */

#include "ngx_c_conf.h"

#include <string.h>

#include <iostream>
#include <vector>

#include "ngx_func.h"

// 初始化尽静态成员
CConfig* CConfig::m_instance = nullptr;

CConfig::CConfig() {}

CConfig::~CConfig() {
  std::vector<LPCConfItem>::iterator pos;
  for (pos = m_ConfigItemList.begin(); pos != m_ConfigItemList.end(); ++pos) {
    delete *pos;
  }
  m_ConfigItemList.clear();
}

bool CConfig::Load(const char* pconfName) {
  FILE* fp = fopen(pconfName, "r");
  if (fp == nullptr) {
    return false; /*放在外面判断出错*/
  }

  char linebuf[512]; /*每一行配置文件读出来放在这里*/

  while (!feof(fp)) {                    /*判断是否读到末尾*/
    if (fgets(linebuf, 500, fp) == NULL) /*读取每一行，出错或者到末尾返回 NULL*/
      continue;
    if (linebuf[0] == 0) continue; /*读到空行返回 0 移除空行*/
    // 处理注释
    if (*linebuf == ';' || *linebuf == ' ' || *linebuf == '#' ||
        *linebuf == '\t' || *linebuf == '\n')
      continue;
  lblprocstring:
    // 截取有效内容
    if (strlen(linebuf) > 0) {
      if (linebuf[strlen(linebuf) - 1] == 10 ||
          linebuf[strlen(linebuf) - 1] == 13 ||
          linebuf[strlen(linebuf) - 1] == 32) {
        linebuf[strlen(linebuf) - 1] = 0;
            }
    }
  }
}