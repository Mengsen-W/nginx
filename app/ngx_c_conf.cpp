/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 18:48:56
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 22:44:44
 * @Description: 读取配置文件的实现
 */

#include "ngx_c_conf.h"

#include <stdio.h>
#include <string.h>

#include <vector>

#include "ngx_func.h"
#include "ngx_macro.h"

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
    if (fgets(linebuf, 511, fp) == NULL) /*读取每一行，出错或者到末尾返回 NULL*/
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
        goto lblprocstring;
      }
    }
    if (linebuf[0] == 0) continue;
    if (linebuf[0] == '[') continue;

    char* ptmp = strchr(linebuf, '=');
    if (ptmp != nullptr) {
      LPCConfItem p_confitem = new CConfItem;
      memset(p_confitem, 0, sizeof(CConfItem));
      strncpy(p_confitem->ItemName, linebuf, (int)(ptmp - linebuf));
      strcpy(p_confitem->ItemContent, ptmp + 1);

      Rtrim(p_confitem->ItemName);
      Ltrim(p_confitem->ItemName);
      Rtrim(p_confitem->ItemContent);
      Ltrim(p_confitem->ItemContent);

      m_ConfigItemList.push_back(p_confitem);
      printf("[config] %s : %d\n", p_confitem->ItemName, atoi(p_confitem->ItemName));
    }
  }
  fclose(fp);
  return true;
}

char* CConfig::GetString(const char* p_itemname) {
  std::vector<LPCConfItem>::iterator pos;

  for (pos = m_ConfigItemList.begin(); pos != m_ConfigItemList.end(); ++pos) {
    if (strcasecmp((*pos)->ItemName, p_itemname) == 0)
      return (*pos)->ItemContent;
  }
  return NULL;
}

int CConfig::GetIntDefault(const char* p_itemname, const int def) {
  std::vector<LPCConfItem>::iterator pos;
  for (pos = m_ConfigItemList.begin(); pos != m_ConfigItemList.end(); ++pos) {
    if (strcasecmp((*pos)->ItemName, p_itemname) == 0) {
      return atoi((*pos)->ItemContent);
    }
  }

  return def;
}