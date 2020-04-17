/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 20:27:51
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-15 19:25:11
 * @Description: 主函数
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "ngx_c_conf.h"
#include "ngx_func.h"  //头文件路径，已经使用gcc -I参数指定了
#include "ngx_signal.h"

char **g_os_argv = nullptr;
char **gp_envmem = nullptr;
int g_environlen = 0;

pid_t ngx_pid;  //当前进程的pid

int main(int argc, char *argv[]) {

  ngx_pid = getpid();
  g_os_argv = argv;

  CConfig *p_config = CConfig::GetInstance();
  if (p_config->Load("nginx.conf") == false) {
    std::cerr << "Open config false" << std::endl;
    exit(1);
  }

  ngx_log_init();
  ngx_init_setproctitle();
  ngx_setproctitle("nginx: master");

  printf("非常高兴，大家和老师一起学习《linux c++通讯架构实战》\n");
  ngx_deleteEnvironment();
  printf("程序退出，再见!\n");
  return 0;
}
