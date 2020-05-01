/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-01 14:29:10
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-01 15:34:55
 * @Description: 处理数据包
 */

#ifndef __NGX_COMM_H__
#define __NGX_COMM_H__

#define _PKG_MAX_LENGTH 30000 /* 最大包长度 */

// 通信包状态
#define _PKG_HD_INIT 0     /* 准备接受包头 */
#define _PKG_HD_RECVING 1  /* 接受包头中 */
#define _PKG_BD_INIT 2     /* 准备接收包体 */
#define _PKG_BD_RECVING 3  /* 接受包体中 */
#define _PKG_RV_FINISHED 4 /* 接受完成 */

#define _DATA_BUFSIZE_ 20 /* 包头数据大小 */

// 结构定义
#pragma pack(1) /* 1字节对齐方式 */

// 包头结构
typedef struct _COMM_PKG_HEADER {
  unsigned short pkgLen;  /* 报文总长度 */
  unsigned short msgCOde; /* 消息类型 */
  int crc32;              /* crc 校验 */
  // uint8_t itest;
} COMM_PKG_HEADER, *LPCOMM_PKG_HEADER;

#pragma pack(0) /* 恢复默认配置 */

#endif
