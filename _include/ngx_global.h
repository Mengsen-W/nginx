/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 19:04:00
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-15 19:34:58
 * @Description: 全局变量的设置
 */

#ifndef __NGX_GLOBAL_H__
#define __NGX_GLOBAL_H__

typedef struct {
  char ItemName[50];
  char ItemContent[500];
} CConfItem, *LPCConfItem;

// 保存指向 argv 数组
extern char **g_os_argv;
// 环境表envmen数组的首地址
extern char **gp_envmem;
// 保存 argc
extern int g_environlen;

#endif