/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-28 19:54:31
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-02 11:28:34
 * @Description: 监听套接字结构
 */

#ifndef __NGX_C_SOCKET_H__
#define __NGX_C_SOCKET_H__

#include <sys/epoll.h>
#include <sys/socket.h>

#include <cstdint>
#include <list>
#include <vector>

#include "ngx_c_memory.h"
#include "ngx_comm.h"

#define NGX_LISTEN_BACKLOG 511 /* 监听维护队列 */
#define NGX_MAX_EVENTS 512     /* wait 最多返回的fd数目 */

typedef struct ngx_listening_s ngx_listening_t, *lpngx_listening_t;
typedef struct ngx_connection_s ngx_connection_t, *lpngx_connection_t;
typedef class CSocket CSocket;

/* 函数指针 */
typedef void (CSocket::*ngx_event_handler_pt)(lpngx_connection_t c);

// 监听结构
struct ngx_listening_s {
  int port;
  int fd;
  lpngx_connection_t connection;
};

// 连接体结构
struct ngx_connection_s {
  // 套接字
  int fd;                      /* 监听套接字 */
  lpngx_listening_t listening; /* 指向本连接的监听套接字 */

  // 标记位
  unsigned instance : 1;  /* 失效标志位 1 有效 0 失效 */
  uint64_t iCurrsequence; /* 序号，每次分配加1 */

  // 网络地址
  struct sockaddr s_sockaddr; /* 保存对方地址 */
  // char addr_text[100];        /* 地址文本信息 */

  // 读写标记
  // uint8_t r_ready; /* 读准备好标记 */
  uint8_t w_ready; /* 写准备好标记 */

  // 回调事件
  ngx_event_handler_pt rhandler; /* 读事件回调函数指针 */
  ngx_event_handler_pt whandler; /* 写事件函数回调指针 */

  // 收包
  unsigned char curStat;             /* 收包状态 */
  char dataHeadInfo[_DATA_BUFSIZE_]; /* 保存包头信息 */
  char *precvbuf;                    /* 数据缓存区地址 */
  unsigned int irecvlen;             /* 数据缓存长度 */
  bool ifnewrecvMem;                 /* 分配内存 */
  char *pnewMemPointer;              /* 存放内存地址 */

  // 连接池状态
  lpngx_connection_t data; /* 后继指针 */
};

// 消息头结构
typedef struct _STRUC_MSG_HEADER {
  lpngx_connection_t pConn; /* 记录对应连接 */
  uint64_t iCurrsequence;   /* 记录序号 */
} STRUC_MSG_HEADER, *LPSTRUC_MSG_HEADER;

// 管理类
class CSocket {
 public:
  CSocket();
  virtual ~CSocket();

  virtual bool Initialize();
  int ngx_epoll_init();                    /* epoll init */
  int ngx_epoll_process_events(int timer); /* 获取事件消息外部会调用*/

 private:
  bool ngx_open_listening_sockets(); /* 打开监听套接字，支持多个端口 */
  void ngx_close_listening_sockets(); /* 关闭监听套接字 */

  bool setnonblocking(int fd); /* 设置非阻塞模式 */

  void ReadConf(); /* 读配置文件 */

  lpngx_connection_t ngx_get_connection(int isocket); /* 去空闲节点 */
  void ngx_free_connection(lpngx_connection_t c);     /* 加空闲节点 */
  void ngx_close_connection(lpngx_connection_t c);    /* 关闭连接 */

  void ngx_event_accept(lpngx_connection_t oldc);      /* accept回调 */
  void ngx_wait_request_handler(lpngx_connection_t c); /* 可读回调 */

  int ngx_epoll_add_event(int fd, int readevent, int writeevent,
                          uint32_t otherflag, uint32_t eventtype,
                          lpngx_connection_t c); /* 增加事件 */

  size_t ngx_sock_ntop(struct sockaddr *sa, int port, u_char *text,
                       size_t len); /* 转换网络地址 */

  void ngx_close_accepted_connection(lpngx_connection_t c); /* 释放资源 */

  ssize_t recvproc(lpngx_connection_t c, char *buff,
                   ssize_t buflen); /* 封装recv */
  void ngx_wait_request_handler_proc_p1(
      lpngx_connection_t c); /* 接受包头的第一阶段 */
  void ngx_wait_request_handler_proc_plast(
      lpngx_connection_t c); /* 收到一个完整包后处理 */

  void inMsgRecvQueue(char *buf, int &irmqc); /* 收到包头后加入消息队列 */
  char *outMsgRecvQueue();                    /* 出消息队列 */
  void clearMsgRecvQueue();                   /* 清空消息队列 */

  int m_ListenPortCount; /* 监听端口数量 */

  int m_worker_connections; /* worker进程最大连接数 */

  int m_epollhandle;       /* 返回的epoll handle */
  int m_connection_n;      /* 当前连接池中连接总数 */
  int m_free_connection_n; /* 可用连接数 */

  lpngx_connection_t m_pconnections;      /* 连接池首地址 */
  lpngx_connection_t m_pfree_connections; /* 空闲状态链表首地址 */

  size_t m_iLenPkgHeader; /* 包头长度 */
  size_t m_iLenMsgHeader; /* 消息头长度 */

  struct epoll_event
      m_events[NGX_MAX_EVENTS]; /* 用于在epoll_wait()中承载返回的所发生的事件 */

  std::vector<lpngx_listening_t> m_ListenSocketList; /* 监听套接字队列 */

  std::list<char *> m_MsgRecvQueue;        /* 业务队列 */
  int m_iRecvQueueCount;                   /* 接受消息队列大小 */
  pthread_mutex_t m_recvMessageQueueMutex; /* 收消息队列互斥量 */

  // 临时函数
  void tmpoutMsgRecvQueue();
};

#endif