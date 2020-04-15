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
 * @ Description: 格式化输出
 * @ Parameter:
 *        buf:  指向buffer能够开始写的位置
 *        last: buffer末尾指针防止溢出
 *        fmt:  存储文本信息的格式
 *        args: 格式化中用到的数量不固定的可变参数
 */
u_char *ngx_vslprintf(u_char *buf, u_char *last, const char *fmt,
                      va_list args) {
  u_char zero;
  uintptr_t width, sign, hex, frac_width, scale, n;  // 临时变量

  int64_t i64;    // 保存 %d 的可变参
  uint64_t ui64;  // 保存 %ud 的可变参，临时作为 %f 的整数部分
  u_char *p;      // 保存 %s 的可变参
  double f;       // 保存 %f 的可变参
  uint64_t frac;  // %f可变参数，根据 %.2f 等，去小数点前2位

  while (*fmt && buf < last) { /* 每次处理一个字符 */
    if (*fmt == "%") {         /* 格式化字符 */
                               /* 格式化必要参数 */
      zero = (u_char)((*++fmt == '0') ? '0' : ' '); /* 判断是否为 %0 这种情况 */
      width = 0;
      sign = 1;
      hex = 0;
      frac_width = 0;
      i64 = 0;
      ui64 = 0;
      while (*fmt >= '0' && *fmt <= '9')
        // %16 -> width = 1 -> width = 16
        // 取宽度
        width = width * 10 + (*fmt - '0');
      // while end

      for (;;) {
        switch (*fmt) {
          case 'u': /* 无符号 */
            sign = 0;
            fmt++;
            continue;
          case 'X': /* 大写16进制 */
            hex = 2;
            sign = 0;
            fmt++;
            continue;
          case 'x': /* 小写16进制 */
            hex = 1;
            sign = 0;
            fmt++;
            continue;
          case '.': /* 小数位数 */
            fmt++;
            while (*fmt >= '0' && *fmt <= '9')
              frac_width = frac_width * 10 + (*fmt++ - '0');
            break;
          default:
            break;
        }  // end switch
        break;
      }      // end for
    } else { /* 正常字符 */
      *buf++ = *fmt++;
    }  // end if
  }    // end while

  return buf;
}

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
  va_start(args, fmt);                   // 使args指向fmt之后的参数
  p = ngx_vslprintf(p, last, fmt, args);
  va_end(args);
  if (err) {
  }
}
