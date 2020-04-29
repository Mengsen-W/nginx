/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-28 19:54:31
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-29 22:14:43
 * @Description: 监听套接字结构
 */

#ifndef __NGX_C_SOCKET_H__
#define __NGX_C_SOCKET_H__

#include <sys/socket.h>

#include <cstdint>
#include <vector>

#define NGX_LISTEN_BACKLOG 511 /* 监听维护队列 */
#define NGX_MAX_EVENT 512      /* wait 最多返回的fd数目 */

typedef struct ngx_listening_s ngx_listening_t, *lpngx_listening_t;
typedef struct ngx_connection_s ngx_connection_t, *lpngx_connection_t;
typedef class CSocket CSocket;

/* 函数指针 */
typedef void (CSocket::*ngx_event_handler_pt)(lpngx_connection_t c);

struct ngx_listening_s {
  int port;
  int fd;
  lpngx_connection_t connection;
};

struct ngx_connection_s {
  int fd;                      /* 监听套接字 */
  lpngx_listening_t listening; /* 指向本链接的监听套接字 */

  unsigned instance : 1;  /* 失效标志位 0 有效 1 失效 */
  uint64_t iCurrsequence; /* 序号，每次分配加1 */

  struct sockaddr s_sockaddr; /* 保存对方地址 */
  // char addr_text[100];        /* 地址文本信息 */

  // uint8_t r_ready; /* 读准备好标记 */
  uint8_t w_ready; /* 写准备好标记 */

  ngx_event_handler_pt rhandler; /* 读事件回调函数指针 */
  ngx_event_handler_pt whandler; /* 写事件函数回调指针 */

  lpngx_connection_t data; /* 后继指针 */
};

class CSocket {
 public:
  CSocket();
  virtual ~CSocket();

  virtual bool Initialize();
  int ngx_epoll_init(); /* epoll init */

 private:
  bool ngx_open_listening_sockets(); /* 打开监听套接字，支持多个端口 */
  void ngx_close_listening_sockets(); /* 关闭监听套接字 */
  bool setnonblocking(int fd);        /* 设置非阻塞模式 */
  void ReadConf();                    /* 读配置文件 */
  lpngx_connection_t ngx_get_connection(int isocket); /* 去空闲节点 */
  void ngx_event_accept(lpngx_connection_t oldc);     /* accept */
  int ngx_epoll_add_event(int fd, int readevent, int writeevent,
                          uint32_t otherflag, uint32_t eventtype,
                          lpngx_connection_t c); /* 增加事件 */

  int m_ListenPortCount; /* 监听端口数量 */

  int m_worker_connections; /* worker进程最大连接数 */

  int m_epollhandle;       /* 返回的epoll handle */
  int m_connection_n;      /* 当前连接池中连接总数 */
  int m_free_connection_n; /* 可用连接数 */

  lpngx_connection_t m_pconnections;      /* 连接池首地址 */
  lpngx_connection_t m_pfree_connections; /* 空闲状态链表首地址 */

  std::vector<lpngx_listening_t> m_ListenSocketList; /* 监听套接字队列 */
};

#endif