/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 20:27:51
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 20:29:09
 * @Description: 主函数
 */

#include <iostream>

#include "ngx_c_conf.h"
#include "ngx_func.h"  //头文件路径，已经使用gcc -I参数指定了
#include "ngx_signal.h"

int main(int argc, char *const *argv) {
  printf("非常高兴，大家和老师一起学习《linux c++通讯架构实战》\n");
  myconf();
  mysignal();

  CConfig *p_config = CConfig::GetInstance();
  if (p_config->Load("nginx.conf") == false) {
    std::cerr << "Open config false" << std::endl;
    exit(1);
  }
  printf("程序退出，再见!\n");
  return 0;
}
