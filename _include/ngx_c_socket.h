/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-28 19:54:31
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-08 09:12:03
 * @Description: 监听套接字结构
 */

#ifndef __NGX_C_SOCKET_H__
#define __NGX_C_SOCKET_H__

#include <pthread.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <sys/socket.h>

#include <atomic>
#include <cstdint>
#include <list>
#include <map>
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
  ngx_connection_s();
  virtual ~ngx_connection_s(); /* 析构函数 */

  // 分配连接池单独一个线程
  void GetOneToUse();  /* 分配出去的时候初始化一些内容 */
  void PutOneToFree(); /* 回收回来的时候做一些事情 */

  // 套接字
  int fd;                      /* 监听套接字 */
  lpngx_listening_t listening; /* 指向本连接的监听套接字 */

  // 标记位
  unsigned instance : 1;  /* 失效标志位 1 有效 0 失效 */
  uint64_t iCurrsequence; /* 序号，每次分配加1 */

  // 网络地址
  struct sockaddr s_sockaddr; /* 保存对方地址 */

  uint32_t events;  //和epoll事件有关

  // 回调事件
  ngx_event_handler_pt rhandler; /* 读事件回调函数指针 */
  ngx_event_handler_pt whandler; /* 写事件函数回调指针 */

  // 收包
  unsigned char curStat;             /* 收包状态 */
  char dataHeadInfo[_DATA_BUFSIZE_]; /* 保存包头信息 */
  char *precvbuf;                    /* 数据缓存区地址 */
  unsigned int irecvlen;             /* 数据缓存长度 */
  char *precvMemPointer;             /* 存放数据包内存地址 */

  //和发包有关
  std::atomic<int> iThrowsendCount; /* 发送消息的epoll调用标记 */

  char *psendMemPointer; /* 用于释放 */
  char *psendbuf; /* 发送数据的缓冲区的头指针其实是包头+包体 */

  time_t inRecyTime;   /* 连接池回收时间 */
  time_t lastPingTime; /* 心跳包间隔 */

  unsigned int isendlen; /* 要发送多少数据 */

  // 连接池状态
  lpngx_connection_t data; /* 后继指针 */

  uint64_t FloodkickLastTime; /* 距离上次收到包时间 */
  int FloodAttackCount; /* Flood攻击在该时间内收到包的次数统计 */

  pthread_mutex_t logicPorcMutex;
  std::atomic<int> iSendCount;
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

  virtual bool Initialize(); /* 主进程初始化监听套接字 */

  virtual void threadRecvProcFunc(char *pMsgBuf); /* 业务处理函数 */

  virtual void procPingTimeOutChecking(LPSTRUC_MSG_HEADER tmpmsg,
                                       time_t cur_time); /* 检测 */

  virtual bool Initialize_subproc(); /* 初始化函数[子进程中执行] */
  virtual void Shutdown_subproc();   /* 清理子线程 */

  int ngx_epoll_init();                    /* 子进程epoll init */
  int ngx_epoll_process_events(int timer); /* 获取事件消息外部会调用*/

  void printTDInfo(); /* 打印统计信息 */

 protected:
  void msgSend(char *pSendbuf);                      /* 推入发送队列 */
  void zdClosesocketProc(lpngx_connection_t p_Conn); /* 心跳包超时 */

  size_t m_iLenPkgHeader; /* 包头长度 */
  size_t m_iLenMsgHeader; /* 消息头长度 */

  int m_ifTimeOutKick; /* 控制超时 */
  int m_iWaitTime;     /* 心跳检查间隔 */

 private:
  // 管理线程
  struct ThreadItem {
    pthread_t _Handle;  //线程句柄
    CSocket *_pThis;    //记录线程池的指针
    bool
        ifrunning;  //标记是否正式启动起来，启动起来后，才允许调用StopAll()来释放

    //构造函数
    ThreadItem(CSocket *pthis) : _pThis(pthis), ifrunning(false) {}
    //析构函数
    ~ThreadItem() {}
  };

  bool ngx_open_listening_sockets(); /* 打开监听套接字，支持多个端口 */
  void ngx_close_listening_sockets(); /* 关闭监听套接字 */

  bool setnonblocking(int fd); /* 设置非阻塞模式 */

  void ReadConf(); /* 读配置文件 */

  void initConnection();                             /* 初始化连接池 */
  void clearconnection();                            /* 立即回收连接池 */
  void inRecyConnectQueue(lpngx_connection_t pConn); /* 延迟回收 */

  lpngx_connection_t ngx_get_connection(int isocket); /* 去空闲节点 */
  void ngx_free_connection(lpngx_connection_t c);     /* 加空闲节点 */
  void ngx_close_connection(lpngx_connection_t c);    /* 关闭连接 */

  void ngx_event_accept(lpngx_connection_t oldc);      /* accept回调 */
  void ngx_read_request_handler(lpngx_connection_t c); /* 可读回调 */

  int ngx_epoll_oper_event(int fd, uint32_t eventtype, uint32_t flag,
                           int bcaction,
                           lpngx_connection_t pConn); /* 操作事件 */

  size_t ngx_sock_ntop(struct sockaddr *sa, int port, u_char *text,
                       size_t len); /* 转换网络地址 */

  void ngx_close_accepted_connection(lpngx_connection_t c); /* 释放资源 */

  ssize_t recvproc(lpngx_connection_t c, char *buff,
                   ssize_t buflen); /* 封装recv */
  void ngx_read_request_handler_proc_p1(lpngx_connection_t c,
                                        bool &isflood); /* 接受包头的第一阶段 */
  void ngx_read_request_handler_proc_plast(
      lpngx_connection_t c, bool &isflood); /* 收到一个完整包后处理 */

  void ngx_write_request_handler(lpngx_connection_t pConn); /* 发消息回调函数 */

  ssize_t sendproc(lpngx_connection_t c, char *buff,
                   ssize_t size); /* 发送数据 */

  void clearMsgSendQueue(); /* 清空发送队列 */

  void AddToTimerQueue(lpngx_connection_t pConn); /* 加入心跳队列 */
  time_t GetEarliestTime();                       /* 取连接 */
  LPSTRUC_MSG_HEADER RemoveFirstTimer();          /* 取得最早并删除 */
  LPSTRUC_MSG_HEADER GetOverTimeTimer(time_t cur_time); /* 获取超时 */
  void DeleteFromTimerQueue(lpngx_connection_t pConn);  /* 删除 */
  void clearAllFromTimerQueue();                        /* 清空队列 */

  int m_ListenPortCount;    /* 所监听的端口数量 */
  int m_worker_connections; /* worker进程最大连接数 */

  int m_epollhandle;  /* 返回的epoll handle */
  int m_connection_n; /* 当前连接池中连接总数 */

  std::vector<ThreadItem *> m_threadVector; /* 线程容器*/
  pthread_mutex_t m_sendMessageQueueMutex;  /* 发消息队列互斥量 */
  sem_t m_semEventSendQueue; /* 处理发消息线程相关的信号量 */

  std::list<lpngx_connection_t> m_connectionList;     /* 连接池链表 */
  std::list<lpngx_connection_t> m_freeconnectionList; /* 空闲连接池链表 */

  std::atomic<int> m_total_connection_n; /* 连接池总数 */
  std::atomic<int> m_free_connection_n;  /* 空闲连接池总数 */

  pthread_mutex_t m_connectionMutex;    /* 连接池互斥量 */
  pthread_mutex_t m_recyconnqueueMutex; /* 回收池互斥量 */

  lpngx_connection_t m_pconnections;      /* 连接池首地址 */
  lpngx_connection_t m_pfree_connections; /* 空闲状态链表首地址 */

  std::list<lpngx_connection_t> m_recyconnectionList; /* 回收池队列 */
  std::atomic<int> m_total_recyconnection_n;          /* 回收池数量 */
  int m_RecyConnectionWaitTime;                       /* 回收池等待 */

  int m_ifkickTimeCount; /* 是否开启踢人时钟，1：开启   0：不开启 */
  pthread_mutex_t m_timequeueMutex; /* 和时间队列有关的互斥量 */
  std::multimap<time_t, LPSTRUC_MSG_HEADER> m_timerQueuemap; /* 时间队列 */
  size_t m_cur_size_;    /* 时间队列的尺寸 */
  time_t m_timer_value_; /* 当前计时队列头部时间值 */

  bool TestFlood(lpngx_connection_t pConn); /* 测试是否flood攻击成立 */

  static void *ServerRecyConnectionThread(
      void *threadData); /* 回收队列交给子线程处理 */
  static void *ServerSendQueueThread(void *threadData); /* 发送线程 */
  static void *ServerTimerQueueMonitorThread(void *threadData); /* 监视和处理 */

  std::list<char *> m_MsgSendQueue;      /* 发送数据消息队列 */
  std::atomic<int> m_iSendMsgQueueCount; /* 发消息队列大小 */

  struct epoll_event
      m_events[NGX_MAX_EVENTS]; /* 用于在epoll_wait()中承载返回的所发生的事件 */

  std::vector<lpngx_listening_t> m_ListenSocketList; /* 监听套接字队列 */

  std::atomic<int> m_onlineUserCount; /* 当前用户数 */

  int m_floodAkEnable;              /* 是否检测泛洪攻击 */
  unsigned int m_floodTimeInterval; /* 收发数据包减隔 */
  int m_floodKickCount;             /* 累计多少次踢人 */

  //统计用途
  time_t m_lastprintTime; /* 上次打印统计信息的时间(10秒钟打印一次) */
  int m_iDiscardSendPkgCount; /* 丢弃的发送数据包数量 */
};

#endif