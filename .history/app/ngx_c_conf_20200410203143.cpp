/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 18:48:56
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 20:31:43
 * @Description: 读取配置文件的实现
 */

#include "ngx_c_conf.h"

#include <iostream>
#include <string>
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
    return false;  //放在外面判断出错
  }
}