/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 19:04:00
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-10 22:56:26
 * @Description: 全局变量的设置
 */

#ifndef __NGX_GLOBAL_H__
#define __NGX_GLOBAL_H__

typedef struct {
  char ItemName[50];
  char ItemContent[500];
} CConfItem, *LPCConfItem;

extern char **g_os_argv;
extern char *gp_envmem;
extern int g_environlen;

#endif