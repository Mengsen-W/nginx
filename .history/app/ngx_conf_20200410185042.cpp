/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 18:48:56
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 18:50:39
 * @Description: 配置文件
 */
#include <stdio.h>

#include "ngx_func.h"
void myconf() {
  printf("执行了myconf()函数,MYVER=%s!\n", MYVER);
  return;
}
