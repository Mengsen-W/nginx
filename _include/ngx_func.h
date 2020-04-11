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
#define MYVER "1.2"
#endif