/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-26 20:38:45
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-26 22:09:18
 * @Description: Deamon process
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "ngx_c_conf.h"
#include "ngx_func.h"
#include "ngx_macro.h"

int ngx_daemon() {
  switch (fork()) {
    case -1:
      ngx_log_error_core(NGX_LOG_EMERG, errno, "ngx_daemon()->fork() failed");
      return -1;
    case 0: /* 子进程退出并继续 */
      break;
    default: /* 父进程直接退出 */
      return 1;
  }

  ngx_parent = ngx_pid;
  ngx_pid = getpid();

  if (setsid() == -1) {
    ngx_log_error_core(NGX_LOG_EMERG, errno, "ngx_daemon()->setsid() failed");
    return -1;
  }

  umask(0);

  int fd = open("/dev/null", O_RDWR);

  if (fd == -1) {
    ngx_log_error_core(NGX_LOG_EMERG, errno, "ngx_daemon()->open() failed");
    return -1;
  }

  if (dup2(fd, STDIN_FILENO) == -1) {
    ngx_log_error_core(NGX_LOG_EMERG, errno,
                       "ngx_daemon()->dup2()->STDIN_FILENO failed");
    return -1;
  }

  if (dup2(fd, STDOUT_FILENO) == -1) {
    ngx_log_error_core(NGX_LOG_EMERG, errno,
                       "ngx_daemon()->dup2()->STDOUT_FILENO failed");
    return -1;
  }

  if (fd > STDERR_FILENO) {
    if (close(fd) == -1) {
      ngx_log_error_core(NGX_LOG_EMERG, errno, "ngx_daemon()->close() failed");
      return -1;
    }
  }
  return 0;
}