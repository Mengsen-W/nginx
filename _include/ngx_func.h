#ifndef __NGX_FUNC_H__
#define __NGX_FUNC_H__

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
u_char *ngx_log_errno(u_char *buf, u_char *last, int err);
#define MYVER "1.2"
#endif