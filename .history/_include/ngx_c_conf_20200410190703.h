/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 19:00:31
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 19:06:35
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
};

#endif  // CConfig