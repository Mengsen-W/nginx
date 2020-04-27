#ifndef __NGX_FUNC_H__
#define __NGX_FUNC_H__
#include <stdlib.h>

//函数声明
void myconf();
// 修剪字符串
void Rtrim(char *string);
// 修剪字符串
void Ltrim(char *string);

// 初始化拷贝环境列表
void ngx_init_setproctitle();
// 设置程序标题
void ngx_setproctitle(const char *);
// 删除环境列表
void ngx_deleteEnvironment();
// 格式化字符串
u_char *ngx_vslprintf(u_char *buf, u_char *last, const char *fmt, va_list args);
// 封装 ngx_vslprintf
u_char *ngx_slprintf(u_char *buf, u_char *last, const char *fmt, ...);
// 初始化log文件
void ngx_log_init();
// 标准错误打印
void ngx_log_stderr(int err, const char *fmt, ...);
// 写日志核心函数
void ngx_log_error_core(int level, int err, const char *fmt, ...);
void ngx_master_process_cycle();
int ngx_daemon();
#define MYVER "1.2"
#endif