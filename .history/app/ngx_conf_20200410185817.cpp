/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 18:48:56
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 18:58:16
 * @Description: 读取配置文件的实现
 */
#include <stdio.h>
#include <stdlib.h>

#include "ngx_func.h"
void myconf() {
  printf("执行了myconf()函数,MYVER=%s!\n", MYVER);
  return;
}
