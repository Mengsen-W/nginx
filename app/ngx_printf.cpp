/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-15 17:41:17
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-15 19:51:40
 * @Description: 打印格式相关
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"

static u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64,
                               u_char zero, uintptr_t hexadecimal,
                               uintptr_t width);

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
    if (*fmt == '%') {         /* 格式化字符 */

      // 格式化必要参数

      zero = (u_char)((*++fmt == '0') ? '0' : ' '); /* 判断是否为 %0 这种情况 */
      /* 判断%后边接的是否是个'0',如果是zero = '0'，否则zero = ' ' */
      /*一般比如你想显示10位而实际数字7位,前头填充三个字符就是这里的zero用于填充*/
      width = 0; /* 格式字符%后边如果是个数字，这个数字最终会弄到width里边来 */
      /* 这东西目前只对数字格式有效，比如%d,%f这种 */
      sign = 1; /* 显示的是否是有符号数，这里给1，表示是有符号数 */
      /* 用%u，这个u表示无符号数 */
      hex = 0; /* 是否以16进制形式显示(比如显示一些地址) */
      /* 0：不是，1：是，并以小写字母显示a-f，2：是，并以大写字母显示A-F */
      frac_width = 0; /*小数点后位数字，*/
      /*一般需要和%.10f配合使用，这里10就是frac_width；*/
      i64 = 0; /* 一般用%d对应的可变参中的实际数字，会保存在这里 */
      ui64 = 0; /*一般用%ud对应的可变参中的实际数字，会保存在这里 */

      //格式化完成

      while (*fmt >= '0' && *fmt <= '9') { /* 这里已经取完前面的第一个0 了 */
        /* 除了第一个0，剩下的0会在这里跳过 */
        width = width * 10 + (*fmt++ - '0'); /* %16 -> width = 1 -> width = 16 */
      }  // while end

      for (;;) { /* 一些特殊标识会在这里给标记位打标记 */
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
            fmt++;  /* 其后面必须跟的是一个数字 */
            while (*fmt >= '0' && *fmt <= '9') /* 把数字提取出来 */
              frac_width = frac_width * 10 + (*fmt++ - '0');
            break;
          default:
            break;
        }  // end switch
        break;
      }  // end for(;;)

      switch (*fmt) { /* 处理实际的占位参数 */
        case '%': /*只有%%时才会遇到这个情形，本意是打印一个% */
          *buf++ = '%';
          fmt++;
          continue;

        case 'd': /*显示整型数据如果和u配合使用也就是%ud则是显示无符号整型数据*/
          if (sign) { /*如果是有符号数 */

            i64 = (int64_t)va_arg(args, int); /* va_arg():遍历可变参数 */
            /*var_arg的第二个参数表示遍历的这个可变的参数的类型*/
          } else { /* 如何是和 %ud配合使用，则本条件就成立 */
            ui64 = (uint64_t)va_arg(args, u_int);
          }
          break; /* 这break掉，直接跳道switch后边的代码去执行 */
          /* 这种凡是break的，都不做fmt++ */

        case 's': /* 一般用于显示字符串 */
          p = va_arg(args, u_char *);

          while (*p && buf < last) {
            /* 没遇到字符串结束标记并且buf值够装得下这个参数*/
            *buf++ = *p++; /* 全部装进来 */
          }

          fmt++;
          continue;  //重新从while开始执行

        case 'P':  //转换一个pid_t类型
          i64 = (int64_t)va_arg(args, pid_t);
          sign = 1;
          break;

        case 'f': /*用于显示double类型数据如果要显示小数部分要加%.5f*/
          f = va_arg(args, double);
          if (f < 0) {    /*负数的处理 */
            *buf++ = '-'; /* 单独搞个负号出来 */
            f = -f;
          }
          /* 走到这里保证f肯定非负 */
          ui64 = (int64_t)f; /* 正整数部分给到ui64里 */
          frac = 0;

          /* 如果要求小数点后显示多少位小数 */
          if (frac_width) { /* 如果是%d.2f，那么frac_width就会是这里的2 */

            scale = 1; /* 缩放从1开始 */
            for (n = frac_width; n; n--) {
              scale *= 10; /* 这可能溢出哦 */
            }

            /* 把小数部分取出来，比如如果是格式%.2f对应的参数是12.537 */
            /* (uint64_t) ((12.537 - (double) 12) * 100 + 0.5); */
            /* = (uint64_t) (0.537 * 100 + 0.5)  = (uint64_t) (53.7 + 0.5) =
             */
            /* (uint64_t) (54.2) = 54 */
            frac = (uint64_t)((f - (double)ui64) * scale + 0.5);
            /* 取得保留的那些小数位数 */
            /*比如%.2f对应的参数是12.537,取小数点后的2位四舍五入，也就是54*/
            /* 如果是"%.6f", 21.378，那么这里frac = 378000 */

            if (frac == scale) { /* 进位 */

              /* 比如 % .2f ，对应的参数是12 .999 */
              /* = (uint64_t) (0.999 * 100 + 0.5) */
              /* = (uint64_t)(99.9 + 0.5) = (uint64_t) (100.4) = 100 */
              /* 而此时scale == 100，两者正好相等 */
              ui64++;   /* 正整数部分进位 */
              frac = 0; /* 小数部分归0 */
            }
          }  // end if (frac_width)

          /* 正整数部分，先显示出来 */
          buf = ngx_sprintf_num(buf, last, ui64, zero, 0, width);
          /* 把一个数字 比如“1234567”弄到buffer中显示 */
          if (frac_width) { /* 指定了显示多少位小数 */
            if (buf < last) {
              /* 因为指定显示多少位小数，先把小数点增加进来 */
              *buf++ = '.';
            }
            buf = ngx_sprintf_num(buf, last, frac, '0', 0, frac_width);
            /* frac这里是小数部分，显示出来，不够的，前边填充0'字符*/
          }
          fmt++;
          continue; /* 重新从while开始执行 */

          //..................................
          //................其他格式符，逐步完善
          //..................................

        default:
          *buf++ = *fmt++; /* 往下移动一个字符 */
          continue;
          //注意这里不break，而是continue;而这个continue其实是continue到外层的while去了，也就是流程重新从while开头开始执行;
      }  // end switch (*fmt)

      //显示%d的，会走下来，其他走下来的格式日后逐步完善......

      //统一把显示的数字都保存到 ui64 里去；
      if (sign) { /*显示的是有符号数 */

        if (i64 < 0) { /* 这可能是和%d格式对应的要显示的数字 */

          *buf++ = '-'; /* 小于0，自然要把负号先显示出来 */
          ui64 = (uint64_t)-i64; /* 成无符号数（正数） */

        } else { /* 显示正数 */
          ui64 = (uint64_t)i64;
        }
      }  // end if (sign)

      /* 比如“1234567”弄到buffer中显示，如果是要求10位，则前边会填充3个空格*/
      /* 注意第5个参数hex，是否以16进制显示 */
      buf = ngx_sprintf_num(buf, last, ui64, zero, hex, width);
      fmt++;
    } else
      /* 正常字符 */
      *buf++ = *fmt++; /* 先赋值后自增 */
                       // end if

  }  // end while

  return buf;
}

/*
 * @ Description: 格式化输出 封装vslprintf
 * @ Parameter:
 *        buf:  指向buffer能够开始写的位置
 *        last: buffer末尾指针防止溢出
 *        fmt:  存储文本信息的格式
 *        args: 格式化中用到的数量不固定的可变参数
 */
u_char *ngx_slprintf(u_char *buf, u_char *last, const char *fmt, ...) {
  va_list args;
  u_char *p;

  va_start(args, fmt); /* 使args指向起始的参数 */
  p = ngx_vslprintf(buf, last, fmt, args);
  va_end(args); /* 释放args */
  return p;
}

/*
 * @ Description: 格式化后追加到buf上
 * @ Parameter:
 *        buf:  指向buffer能够开始写的位置
 *        last: buffer末尾指针防止溢出
 *    uint64_t: 预计追加的字符
 *        zero: 补位标记
 *   uintptr_t: 16进制标记
 *       width: 补位字符长度
 */
static u_char *ngx_sprintf_num(u_char *buf, u_char *last, uint64_t ui64,
                               u_char zero, uintptr_t hexadecimal,
                               uintptr_t width) {
  // temp[21]
  u_char *p, temp[NGX_INT64_LEN + 1];
  size_t len;
  uint32_t ui32;

  static u_char hex[] = "0123456789abcdef";
  //跟把一个10进制数显示成16进制有关，换句话说和
  //%xd格式符有关，显示的16进制数中a-f小写
  static u_char HEX[] = "0123456789ABCDEF";
  //跟把一个10进制数显示成16进制有关，换句话说和
  //%Xd格式符有关，显示的16进制数中A-F大写

  p = temp + NGX_INT64_LEN;
  /* NGX_INT64_LEN = 20 */
  /* 所以p指向的是temp[20]那个位置，也就是数组最后一个元素位置 */

  if (hexadecimal == 0) { /* 10进制模式 */
    if (ui64 <= (uint64_t)NGX_MAX_UINT32_VALUE) {
      ui32 = (uint32_t)ui64; /* 能保存下 */
      do {
        /* 7654321这个数字保存成：temp[13]=7,temp[14]=6,temp[15]=5 */
        /* temp[16]=4,temp[17]=3,temp[18]=2,temp[19]=1 */
        /* 除这些值之外的temp[0..12]以及temp[20]都是不确定的值 */
        /* 转换数字只能在 [0-9] 内 */
        *--p = (u_char)(ui32 % 10 + '0');
        /* 把屁股后边这个数字拿出来往数组里装，并且是倒着装 */
      } while (ui32 /= 10); /* 每次缩小10倍等于去掉屁股后边这个数字 */
    } else {                /* 32位保存不下 */
      do {
        *--p = (u_char)(ui64 % 10 + '0');
      } while (ui64 /= 10);
      /* 每次缩小10倍等于去掉屁股后边这个数字 */
    }
  } else if (hexadecimal == 1) {
    /* 如果显示一个十六进制数字，格式符为：%xd，则这个条件成立 */
    /* 要以16进制数字形式显示出来这个十进制数,a-f小写 */
    /* 比如我显示一个1,234,567(10)，他对应的16进制数实际是 12D687(16) */
    //，那怎么显示出这个12D687来呢？
    do {
      /* 0xf就是二进制的1111,大家都学习过位运算 */
      /* ui64 & 0xf，就等于把一个数的最末尾的4个二进制位拿出来 */
      /* ui64 & 0xf  其实就能分别得到这个16进制数也就是 7,8,6,D,2,1这个数字 */
      /* 转成(uint32_t)然后以这个为hex的下标 */
      /* 找到这几个数字的对应的能够显示的字符 */
      *--p = hex[(uint32_t)(ui64 & 0xf)];
    } while (ui64 >>= 4);
    /* 相当于把该16进制数的最末尾一位去掉 */
    /* 如此反复，最终肯定有=0时导致while不成立退出循环 */
    /* 剩余的数一定小于16，会在do里面变为16进制 */
    /* 最后在赋值给p后变为0，退出循环 */
  } else { /* hexadecimal == 2 */
    /* hexadecimal ==1 */
    do {
      *--p = HEX[(uint32_t)(ui64 & 0xf)];
    } while (ui64 >>= 4);
  }

  len = (temp + NGX_INT64_LEN) - p;
  /* 得到这个数字的宽度，比如 “7654321”这个数字 ,len = 7 */

  while (len++ < width && buf < last) {
    /* 如果你希望显示的宽度是10个宽度[%12f] */
    /* 而实际想显示的是7654321，只有7个宽度 */
    /* 那么这里要填充5个0进去到末尾，凑够要求的宽度 */
    *buf++ = zero;
    /* 填充0进去到buffer中（往末尾增加），比如你用格式 */
    /* ngx_log_stderr(0, "invalid option: %10d\n", 21) */
    /* 显示的结果是：nginx: invalid option:         21 */
    /* ---21前面有8个空格，这8个弄个，就是在这里添加进去的 */
  }

  len = (temp + NGX_INT64_LEN) - p;
  /*还原这个len，因为上边这个while循环改变了len的值 */
  /* 现在还没把实际的数字比如“7654321”往buf里拷贝呢 */

  //如下这个等号是我加的[我认为应该加等号]，nginx源码里并没有加;***********************************************
  if ((buf + len) >= last) {
    /* 发现如果往buf里拷贝“7654321”后 */
    /* 会导致buf不够长[剩余的空间不够拷贝整个数字] */
    len = last - buf;  //剩余的buf有多少我就拷贝多少
  }

  return ngx_cpymem(buf, p, len);
  /* 构造p的时候已经把int转为char了 */
  /* 把最新p写回到buf去 */
}