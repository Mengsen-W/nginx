/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 19:00:31
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 19:32:44
 * @Description: 读取配置文件
 */

#ifndef __NGX_CONF_H__
#define __NGX_CONF_H__

#include <vector>

#include "ngx_global.h"

class CConfig {
 private:
  CConfig();

 public:
  ~CConfig();

 private:
  static CConfig *m_instance;

 public:
  static CConfig *GetInstance() {
    if (m_instance == nullptr) {
      if (m_instance == nullptr) {
        m_instance = new CConfig();
        static CGarhuishou c1;
      }
    }
    return m_instance;
  }

  class CGarhuishou {
   public:
    ~CGarhuishou() {
      delete CConfig::m_instance;
      CConfig::m_instance = nullptr;
    }
  };
};

#endif  // CConfig