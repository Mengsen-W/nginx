/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 22:09:26
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 22:52:21
 * @Description: 定义一些字符操作
 */

#include <string.h>

#include <iostream>

// 截取字符串尾部空格
void Rtrim(char* string) {
  size_t len = 0;
  if (string == NULL) return;

  len = strlen(string);
  while (len > 0 && string[len - 1] == ' ') string[len--] = 0;

  return;
}

// 截取字符串首部空格
void Ltrim(char* string) {
  size_t len = strlen(string);
  if (len == 0) return;
  char* p_tmp = string;

  // 不是以空格开头
  if ((*p_tmp) != ' ') return;

  // 找到第一个不为空格
  while ((*p_tmp) != '\0') {
    if ((*p_tmp) == ' ')
      ++p_tmp;
    else
      break;
  }

  if ((*p_tmp) == '\0') { /* 全是空格 */
    *string = '\0';
    return;
  }

  char* p_tmp2 = string;  // 拷贝到string
  while ((*p_tmp) != '\0') {
    (*p_tmp2) = (*p_tmp);
    ++p_tmp;
    ++p_tmp2;
  }
  (*p_tmp2) = '\0';
  return;
}