/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-28 19:54:31
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-28 21:51:50
 * @Description: 监听套接字结构
 */

#ifndef __NGX_C_SOCKET_H__
#define __NGX_C_SOCKET_H__

#include <vector>

#define NGX_LISTEN_BACKLOG 511

typedef struct ngx_listening_s {
  int port;
  int fd;
} ngx_listening_t, *lpngx_listening_t;

class CSocket {
 public:
  CSocket();
  virtual ~CSocket();

  virtual bool Initialize();

 private:
  bool ngx_open_listening_sockets(); /* 打开监听套接字，支持多个端口 */
  void ngx_close_listening_sockets(); /* 关闭监听套接字 */
  bool setnonblocking(int fd);        /* 设置非阻塞模式 */

  int m_ListenPortCount;                             /* 监听端口数量 */
  std::vector<lpngx_listening_t> m_ListenSocketList; /* 监听套接字队列 */
};

#endif