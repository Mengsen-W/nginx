/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 19:00:31
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-01 15:12:04
 * @Description: 读取配置文件
 */

#ifndef __NGX_CONF_H__
#define __NGX_CONF_H__

#include <vector>
#include <stdlib.h>

#include "ngx_global.h"

class CConfig {
 private:
  CConfig();
  ~CConfig();

 private:
  static CConfig *m_instance;

 public:
  static CConfig *GetInstance() {
    if (m_instance == nullptr) {
      if (m_instance == nullptr) {
        static GC_CConfig c;
        m_instance = new CConfig();
      }
    }
    return m_instance;
  }

  class GC_CConfig {
   public:
    ~GC_CConfig() {
      delete CConfig::m_instance;
      CConfig::m_instance = nullptr;
    }
  };

  // -------------------------------------------------
 public:
  bool Load(const char *pconfName);  // 装载配置文件
  char *GetString(const char *p_itemname);
  int GetIntDefault(const char *p_itemname, const int def);

 public:
  std::vector<LPCConfItem> m_ConfigItemList;  //存储配置信息列表
};

#endif  // CConfig