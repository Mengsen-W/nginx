/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-23 21:40:07
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-24 21:17:47
 * @Description: 信号处理相关
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"

typedef struct {
  int signo;           /* 对应信号的数字编号 */
  const char *signame; /* 对应信号的名称 */
  /* 信号处理函数指针 */
  void (*handler)(int signo, siginfo_t *siginfo, void *ucontext);

} ngx_signal_t;

static void ngx_signal_handler(int signo, siginfo_t *siginfo, void *ucontext);
static void ngx_process_get_status(void);

// 需要处理信号的数组
ngx_signal_t signals[] = {
    {SIGHUP, "SIGHUP", ngx_signal_handler},   /* 挂起信号 */
    {SIGINT, "SIGINT", ngx_signal_handler},   /* 终止信号 */
    {SIGTERM, "SIGTERM", ngx_signal_handler}, /* 程序结束 */
    {SIGCHLD, "SIGCHLD", ngx_signal_handler}, /* 子进程结束 */
    {SIGQUIT, "SIGQUIT", ngx_signal_handler}, /* 程序退出 */
    {SIGIO, "SIGIO", ngx_signal_handler},     /* IO准备完成 */
    {SIGSYS, "SIGSYS, SIG_IGN", NULL},        /* 非法系统调用 */
    // 继续添加
    {0, NULL, NULL} /* 信号编号从1开始，0表示结束标志 */
};

/*
 * @ Description: 注册信号处理程序
 * @ Parameter: void
 */
int ngx_init_signals() {
  ngx_signal_t *sig;   /* 指向自定义结构数组的指针 */
  struct sigaction sa; /* 定义 sigaction 结构体 */

  for (sig = signals; sig->signo != 0; ++sig) { /* 其他信号的编号都不为0 */
    /* 初始化 sa */
    memset(&sa, 0, sizeof(struct sigaction));

    if (sig->handler) {               /* 如果信号处理函数不为空 */
      sa.sa_sigaction = sig->handler; /* 指定信号处理函数 */
      sa.sa_flags = SA_SIGINFO; /* 信号附带参数可以传入信号处理函数 */

    } else { /* 如果没有信号处理函数即忽略该信号 */
      sa.sa_handler = SIG_IGN;
    }

    sigemptyset(&sa.sa_mask); /* 初始化信号屏蔽集 */

    if (sigaction(sig->signo, &sa, NULL) == -1) { /* 注册失败 */
      ngx_log_error_core(NGX_LOG_EMERG, errno, "sigaction(%s) failed",
                         sig->signame);
      return -1;
    } else {
      // ngx_log_stderr(0, "sigaction(%s)", sig->signame); /* debug */
    }
  }
  return 0;
}

/*
 * @ Description: 信号处理函数
 * @ Parameter:
 *       signo: 信号号
 *     siginfo: 信号信息
 *        void: 自定义指针
 */
static void ngx_signal_handler(int signo, siginfo_t *siginfo, void *ucontext) {
  ngx_signal_t *sig;
  char *action;

  for (sig = signals; sig->signo != 0; ++sig) {
    if (sig->signo == signo) break;
  }

  action = (char *)"";

  if (ngx_process == NGX_PROCESS_MASTER) {
    switch (signo) {
      case SIGCHLD:
        ngx_reap = 1;
        break;
      default:
        break;
    }
  } else if (ngx_process == NGX_PROCESS_WORKER) {
    // ...
  } else {
    // ...
  }

  // log
  if (siginfo && siginfo->si_pid) {
    ngx_log_error_core(NGX_LOG_NOTICE, 0, "signal %d (%s) received from %P%s",
                       signo, sig->signame, siginfo->si_pid, action);
  } else {
    ngx_log_error_core(NGX_LOG_NOTICE, 0, "signal %d (%s) received from %s",
                       signo, sig->signame, action);
  }

  if (signo == SIGCHLD) {
    ngx_process_get_status();
  }
  return;
}

static void ngx_process_get_status() {
  pid_t pid;
  int status;
  int err;
  int one = 0;

  for (;;) {
    pid = waitpid(-1, &status, WNOHANG);

    if (pid == 0) return; /* 非阻塞立即返回 */

    if (pid == -1) { /* wait被中断 */
      err = errno;
      if (err == EINTR) {
        continue;
      }
      if (err == ECHILD && one) return; /* 没有子进程 */
      if (err == ECHILD) {              /* 没有子进程 */
        ngx_log_error_core(NGX_LOG_INFO, err, "waitpid() failed");
        return;
      }
      ngx_log_error_core(NGX_LOG_ALERT, err, "waitpid() failed");
    }
    // 收尸成功
    one = 1;
    if (WTERMSIG(status)) { /* 获取使子进程终止的信号编号 */
      ngx_log_error_core(NGX_LOG_ALERT, 0, "pid = %P exited on signal %d", pid,
                         WTERMSIG(status));
    } else {
      ngx_log_error_core(NGX_LOG_NOTICE, 0, "pid = %P exited with code %d", pid,
                         WEXITSTATUS(status));
    }
  }

  return;
}