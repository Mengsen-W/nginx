/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 20:27:51
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-02 17:01:57
 * @Description: 主函数
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#include "ngx_c_conf.h"
#include "ngx_c_socket.h"
#include "ngx_c_threadpool.h"
#include "ngx_func.h"  //头文件路径，已经使用gcc -I参数指定了
#include "ngx_macro.h"
#include "ngx_signal.h"

char **g_os_argv = nullptr;
char **gp_envmem = nullptr;
size_t g_argvneedmem = 0;
size_t g_envneedmen = 0;
int g_environlen = 0;
int g_os_argc;
int g_daemonized;
CSocket g_socket;
CThreadPool g_threadpool;

pid_t ngx_pid;  //当前进程的pid
pid_t ngx_parent;
int ngx_process;
int ngx_reap;

static void freeresource();

int main(int argc, char *argv[]) {
  ngx_pid = getpid();
  ngx_parent = getppid();

  g_os_argv = argv;
  g_argvneedmem = 0;
  for (int i = 0; i < argc; ++i) {
    g_argvneedmem += strlen(argv[i]) + 1;
  }
  for (int i = 0; environ[i]; ++i) {
    g_envneedmen += strlen(environ[i]) + 1;
  }
  g_os_argc = argc;
  g_os_argv = argv;

  ngx_log.fd = -1;
  ngx_process = NGX_PROCESS_MASTER;
  ngx_reap = 0;

  // 初始化配置文件
  CConfig *p_config = CConfig::GetInstance();
  if (p_config->Load("nginx.conf") == false) {
    ngx_log_stderr(0, "configure failed", "nginx.conf");
    exit(1);
  }

  // 初始化函数
  ngx_log_init();
  ngx_init_signals();
  ngx_init_setproctitle();
  g_socket.Initialize();

  // ngx_log_stderr(1, "invalid option: \"%s\"", argv[0]);
  // ngx_log_stderr(2, "invalid option: %10d", 21);
  // ngx_log_stderr(3, "invalid option: %010d", 21);
  // ngx_log_stderr(4, "invalid option: %.6f", 21.378);
  // ngx_log_stderr(5, "invalid option: %.6f", 12.999);
  // ngx_log_stderr(6, "invalid option: %.2f", 12.999);
  // ngx_log_stderr(7, "invalid option: %xd", 1678);
  // ngx_log_stderr(8, "invalid option: %Xd", 1678);
  // ngx_log_stderr(9, "invalid option: %d", 1678);
  // ngx_log_stderr(10, "invalid option: %s , %d", "testInfo", 326);

  //   for (int i = 0; i < 9; ++i) {
  //     ngx_log_error_core(i, i + 1, "this failed xxx, and put out = %s",
  //     "YYYY");
  //   }
  if (p_config->GetIntDefault("Daemon", 0) == 1) {
    int create_daemon_result = ngx_daemon();
    if (create_daemon_result == -1) {
      goto lblexit;
    }
    if (create_daemon_result == 1) {
      freeresource();
      return 0;
    }
    g_daemonized = 1;
  }

  ngx_master_process_cycle();

lblexit:
  freeresource();

  return 0;
}

static void freeresource() {
  //(1)对于因为设置可执行程序标题导致的环境变量分配的内存，我们应该释放
  ngx_deleteEnvironment();

  //(2)关闭日志文件
  if (ngx_log.fd != STDERR_FILENO && ngx_log.fd != -1) {
    close(ngx_log.fd);  //不用判断结果了
    ngx_log.fd = -1;    //标记下，防止被再次close吧
  }
}