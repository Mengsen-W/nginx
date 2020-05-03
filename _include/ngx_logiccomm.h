/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-03 14:21:14
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-03 14:22:59
 * @Description: 数据包格式
 */

#ifndef __NGX_LOGICCOMM_H__
#define __NGX_LOGICCOMM_H__

//收发命令宏定义
#define _CMD_START 0
#define _CMD_REGISTER _CMD_START + 5 /* 注册 */
#define _CMD_LOGIN _CMD_START + 6    /* 登录 */

//结构定义------------------------------------
#pragma pack(1)

typedef struct _STRUCT_REGISTER {
  int iType;         /* 类型 */
  char username[56]; /* 用户名 */
  char password[40]; /* 密码 */

} STRUCT_REGISTER, *LPSTRUCT_REGISTER;

typedef struct _STRUCT_LOGIN {
  char username[56]; /* 用户名 */
  char password[40]; /* 密码 */

} STRUCT_LOGIN, *LPSTRUCT_LOGIN;

#pragma pack() /* 取消指定对齐，恢复缺省对齐 */

#endif
