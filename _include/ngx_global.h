/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-10 19:04:00
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-01 18:12:21
 * @Description: 全局变量的设置
 */

#ifndef __NGX_GLOBAL_H__
#define __NGX_GLOBAL_H__

#include "ngx_c_socket.h"

typedef struct {
  char ItemName[50];
  char ItemContent[500];
} CConfItem, *LPCConfItem;

//和运行日志相关
typedef struct {
  int log_level; /* 日志级别 或者日志类型，ngx_macro.h里分0-8共9个级别 */
  int fd; /* 日志文件描述符 */
} ngx_log_t;

// 保存指向 argv 数组
extern char **g_os_argv;
// 环境表envmen数组的首地址
extern char **gp_envmem;
// 保存 argc
extern int g_environlen;
extern size_t g_argvneedmem;
extern size_t g_envneedmen;
extern int g_os_argc;
// 保存进程id
extern pid_t ngx_pid;
extern pid_t ngx_parent;
extern int ngx_process;
extern int ngx_reap;
// 保存结构体
extern ngx_log_t ngx_log;
extern int g_daemonize;

extern CSocket g_socket;

#endif