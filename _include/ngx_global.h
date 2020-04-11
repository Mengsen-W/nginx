/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 19:04:00
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-11 21:13:52
 * @Description: 全局变量的设置
 */

#ifndef __NGX_GLOBAL_H__
#define __NGX_GLOBAL_H__

typedef struct {
  char ItemName[50];
  char ItemContent[500];
} CConfItem, *LPCConfItem;

// 保存 argv 数组
extern char **g_os_argv;
// 保存环境表envmen的首地址
//但是不改变 environ 二级指针的指向 使程序可以默认调用
// 也不改变实际指向的常字符串
extern char *gp_envmem;
// 保存 argc
extern int g_environlen;

#endif