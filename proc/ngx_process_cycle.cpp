/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-24 19:50:07
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-02 20:41:21
 * @Description: 创建子进程
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ngx_c_conf.h"
#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"
#include "ngx_signal.h"

static void ngx_start_worker_processes(int threadnums);
static int ngx_spawn_process(int threadnums, const char *pprocname);
static void ngx_worker_process_cycle(int inum, const char *pprocname);
static void ngx_worker_process_init(int inum);

static u_char master_process[] = "master process";

void ngx_master_process_cycle() {
  sigset_t set;

  sigemptyset(&set);

  sigaddset(&set, SIGCHLD);  /* 子进程状态改变 */
  sigaddset(&set, SIGALRM);  /* 定时器超时 */
  sigaddset(&set, SIGIO);    /* 异步IO */
  sigaddset(&set, SIGINT);   /* 终端中断 */
  sigaddset(&set, SIGHUP);   /* 连接断开 */
  sigaddset(&set, SIGUSR1);  /* 用户定义信号 */
  sigaddset(&set, SIGUSR2);  /* 用户定义信号 */
  sigaddset(&set, SIGWINCH); /* 终端窗口改变 */
  sigaddset(&set, SIGTERM);  /* 终端终止 */
  sigaddset(&set, SIGQUIT);  /* 终端退出 */

  if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) { /* 创建的过程中不要打扰 */
    ngx_log_error_core(NGX_LOG_ALERT, errno,
                       "ngx_master_process_cycle() sigprocmask filed");
  }

  size_t size;

  size = sizeof(master_process);
  size += g_argvneedmem; /* 需要长度 */
  if (size < 1000) {
    char title[1000] = {0};
    strcpy(title, (const char *)master_process);
    strcat(title, " ");
    for (int i = 0; i < g_os_argc; ++i) {
      strcat(title, g_os_argv[i]); /* 把其他参数放在后面 */
    }
    ngx_setproctitle(title);
    ngx_log_error_core(NGX_LOG_INFO, 0, "[%s : %P] start running...", title,
                       ngx_pid);
  }

  CConfig *p_config = CConfig::GetInstance();
  int workprocess = p_config->GetIntDefault("WorkerProcesses", 1);
  ngx_start_worker_processes(workprocess);

  sigemptyset(&set); /* 结束创建子进程过程 */

  // 父进程继续
  for (;;) {
    sigsuspend(&set); /* 阻塞在这里等待信号 */
  }

  return;
}

static void ngx_start_worker_processes(int threadnums) {
  for (int i = 0; i < threadnums; ++i) {
    ngx_spawn_process(i, "work process");
  }
  return;
}

static int ngx_spawn_process(int inum, const char *pprocname) {
  pid_t pid;
  pid = fork();

  switch (pid) {
    case -1:
      ngx_log_error_core(
          NGX_LOG_ALERT, errno,
          "ngx_spawn_process() fork() filed num = %d, procname = \"%s\"", pid,
          pprocname);
    case 0:
      ngx_parent = ngx_pid;
      ngx_pid = getpid();
      ngx_worker_process_cycle(inum, pprocname);
      break;
    default:
      break;
  }

  return pid; /* 父进程 */
}

static void ngx_worker_process_cycle(int inum, const char *pprocname) {
  ngx_process = NGX_PROCESS_WORKER;

  ngx_worker_process_init(inum);
  ngx_setproctitle(pprocname);

  ngx_log_error_core(NGX_LOG_NOTICE, 0, "[%s : %P] start running...", pprocname,
                     ngx_pid);

  // setvbuf(stdout, NULL, _IONBF, 0);

  for (;;) {
    ngx_process_events_and_timers(); /* 处理网络和定时器 */
    // printf("%d sleep\n", inum);
  }

  g_threadpool.StopAll();

  return;
}

static void ngx_worker_process_init(int inum) {
  sigset_t set;

  sigemptyset(&set); /* 恢复信号 */

  if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
    ngx_log_error_core(NGX_LOG_ALERT, errno,
                       "ngx_worker_process_init() sigprocmask() filed");
  }

  CConfig *p_config = CConfig::GetInstance();
  int threadnums = p_config->GetIntDefault("ProcMsgRecvWorkThreadCount", 1);
  if (g_threadpool.Create(threadnums) == false) exit(-2);

  g_socket.ngx_epoll_init();

  return;
}