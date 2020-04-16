/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-15 19:51:57
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-15 22:30:08
 * @Description: 和日志相关函数
 */

#include <errno.h>   //einclude <stdarg.h>
#include <fcntl.h>   //open
#include <stdarg.h>  //va_start....
#include <stdint.h>  //uintptr_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>  //gettimeofday
#include <time.h>      //localtime_r
#include <unistd.h>    //STDERR_FILENO等

#include "ngx_func.h"
#include "ngx_macro.h"

static u_char err_levels[][20] = {
    {"stderr"},  // 0: 控制台错误
    {"emerg"},   // 1: 紧急
    {"alert"},   // 2: 警戒
    {"crit"},    // 3: 严重
    {"error"},   // 4: 错误
    {"warn"},    // 5: 警告
    {"notice"},  // 6: 注意
    {"info"},    // 7: 信息
    {"debug"},   // 8: 调试
};

ngx_log_t ngx_log;

/*
 * @ Description: 控制台输出，最高等级错误
 * @ Parameter:
 *         err: 错误代码
 *         fmt: 储存文本信息的格式
 *         ...: 可变参
 */
void ngx_log_stderr(int err, const char *fmt, ...) {
  va_list args;                          //创建一个 va_list 类型变量
  u_char errstr[NGX_MAX_ERROR_STR + 1];  // 2048
  u_char *p, *last;

  memset(errstr, 0, sizeof(errstr));  // 初始化数组

  last = errstr + NGX_MAX_ERROR_STR;     // 指向buffer最后，防止溢出
  p = ngx_cpymem(errstr, "nginx: ", 7);  // 指向第一个写入的位置
  va_start(args, fmt);                   /* 使args指向fmt之后的参数 */
  p = ngx_vslprintf(p, last, fmt, args);
  va_end(args); /* 释放 args */

  if (err) { /* 如果错误代码不是0，表示有错误发生 */
    p = ngx_log_errno(p, last, err);
  }

  if (p >= (last - 1)) { /* 位置不够就放弃 */
    p = (last - 1) - 1;
  }
  /* 换行符必须要插入 */
  *p++ = '\n';

  return;
}
