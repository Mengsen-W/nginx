/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-28 19:54:45
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-03 15:43:59
 */

#include "ngx_c_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ngx_c_conf.h"
#include "ngx_c_threadpool.h"
#include "ngx_func.h"
#include "ngx_macro.h"

/*
 * @ Description: 构造函数
 */
CSocket::CSocket()
    : m_iLenPkgHeader(sizeof(COMM_PKG_HEADER)),
      m_iLenMsgHeader(sizeof(STRUC_MSG_HEADER)),
      m_ListenPortCount(1),
      m_worker_connections(1),
      m_epollhandle(-1),
      m_connection_n(0),
      m_free_connection_n(0),
      m_pconnections(nullptr),
      m_pfree_connections(nullptr),
      m_total_recyconnection_n(0) {}

/*
 * @ Description: 析构函数
 */
CSocket::~CSocket() {
  std::vector<lpngx_listening_t>::iterator pos;
  for (pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end();
       ++pos) {
    delete (*pos);
  }
  m_ListenSocketList.clear();

  return;
}

/*
 * @ Description: 清理发送消息队列
 * @ Paramater: void
 * @ Return: void
 */
void CSocket::clearMsgSendQueue() {
  char *sTmpMsgBuf;
  CMemory *p_memory = CMemory::GetInstance();

  // 临界问题先不考虑了
  while (!m_MsgSendQueue.empty()) {
    sTmpMsgBuf = m_MsgSendQueue.front();
    m_MsgSendQueue.pop_front();
    p_memory->FreeMemory(sTmpMsgBuf);
  }
}

/*
 * @ Description: 读配置文件
 * @ Parameter: void
 * @ return: void
 */
void CSocket::ReadConf() {
  CConfig *p_config = CConfig::GetInstance();
  m_worker_connections =
      p_config->GetIntDefault("worker_connections", m_worker_connections);
  m_ListenPortCount =
      p_config->GetIntDefault("listen_port_count", m_ListenPortCount);
  m_RecyConnectionWaitTime = p_config->GetIntDefault("Sock_RecyConnectionWait",
                                                     m_RecyConnectionWaitTime);
}

/*
 * @ Description: 初始化监听套接字
 * @ Parameter: void
 * @ return: bool
 */
bool CSocket::Initialize() {
  ReadConf();
  bool reco = ngx_open_listening_sockets();
  return reco;
}

/*
 * @ Description: 回收线程
 */
void CSocket::Shutdown_subproc() {
  /* 通过 shutdown 开关 */
  std::vector<ThreadItem *>::iterator iter;
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++) {
    pthread_join((*iter)->_Handle, NULL);
  }

  //释放一下new出来的ThreadItem
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); iter++) {
    if (*iter) delete *iter;
  }
  m_threadVector.clear();

  //(3)队列相关
  clearMsgSendQueue();
  clearconnection();

  //(4)多线程相关
  pthread_mutex_destroy(&m_connectionMutex);  //连接相关互斥量释放
  pthread_mutex_destroy(&m_sendMessageQueueMutex);  //发消息互斥量释放
  pthread_mutex_destroy(&m_recyconnqueueMutex);  //连接回收队列相关的互斥量释放
  // sem_destroy(&m_semEventSendQueue);  //发消息相关线程信号量释放
}

/*
 * @ Description: 监听端口
 * @ Paramenter: void
 * @ return: bool
 */
bool CSocket::ngx_open_listening_sockets() {
  CConfig *p_config = CConfig::GetInstance();
  m_ListenPortCount =
      p_config->GetIntDefault("ListenPortCount", m_ListenPortCount);

  int isock;                    /* socket */
  struct sockaddr_in serv_addr; /* 服务器配置结构体 */
  int iport;                    /* port */
  char strinfo[100];            /* 临时字符串 */

  // 初始化
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  for (int i = 0; i < m_ListenPortCount; ++i) {
    isock = socket(AF_INET, SOCK_STREAM, 0);
    if (isock == -1) {
      ngx_log_error_core(NGX_LOG_ERR, errno,
                         "CSocket::Initialize()->socket() failed i = %d", i);
      return false;
    }

    int reuseaddr = 1; /* 打开对应的设置项 */
    if (setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuseaddr,
                   sizeof(reuseaddr)) == -1) { /* 设置 bind() TIME_WAIT */
      ngx_log_error_core(NGX_LOG_ERR, errno,
                         "CSocket::Initialize()->setsockopt() failed i = %d",
                         i);
      close(isock);
      return false;
    }

    if (setnonblocking(isock) == false) { /* 设置非阻塞 */
      ngx_log_stderr(errno, "CSocket::Initialize()->setsockopt() failed i = %d",
                     i);
      close(isock);
      return false;
    }

    /* 取端口 */
    strinfo[0] = 0;
    sprintf(strinfo, "ListenPort%d", i);
    iport = p_config->GetIntDefault(strinfo, 100000);
    serv_addr.sin_port = htons((in_port_t)iport);

    if (bind(isock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
      ngx_log_error_core(NGX_LOG_ERR, errno,
                         "CSocket::Initialize()->bind() failed port = %d",
                         iport);
      close(isock);
      return false;
    }

    if (listen(isock, NGX_LISTEN_BACKLOG) == -1) { /* 监听 */
      ngx_log_error_core(NGX_LOG_ERR, errno,
                         "CSocket::Initialize()->listen() failed port = %d",
                         iport);
      close(isock);
      return false;
    }

    // 放入监听队列
    lpngx_listening_t p_listensocketitem = new ngx_listening_t;
    memset(p_listensocketitem, 0, sizeof(ngx_listening_t));
    p_listensocketitem->port = iport;
    p_listensocketitem->fd = isock;
    ngx_log_error_core(NGX_LOG_INFO, 0, "listen port %d success", iport);
    m_ListenSocketList.push_back(p_listensocketitem);
  }
  if (m_ListenSocketList.size() <= 0)  //不可能一个端口都不监听吧
    return false;
  return true;
}

/*
 * @ Description: 设置非阻塞监听套接字
 * @ Parameter: int sockfd
 * @ return: bool
 */
bool CSocket::setnonblocking(int sockfd) {
  int nb = 1;                              /* 1设置0清除 */
  if (ioctl(sockfd, FIONBIO, &nb) == -1) { /* 设置非阻塞IO */
    return false;
  }
  return true;
}

/*
 * @ Description: 关闭 listen socket
 * @ Parameter: void
 * @ return: void
 */
void CSocket::ngx_close_listening_sockets() {
  for (int i = 0; i < m_ListenPortCount; ++i) {
    close(m_ListenSocketList[i]->fd);
    ngx_log_error_core(NGX_LOG_INFO, 0, "close listen socket %d",
                       m_ListenSocketList[i]->port);
  }
  return;
}

/*
 * @ Description: epoll 初始化
 * @ Parameter: void
 * @ Returns: int
 */
int CSocket::ngx_epoll_init() {
  ngx_log_error_core(NGX_LOG_DEBUG, 0, "begin ngx_epoll_init()");
  // epoll_creat
  m_epollhandle = epoll_create(m_worker_connections);
  if (m_epollhandle == -1) {
    ngx_log_error_core(NGX_LOG_ERR, errno,
                       "CSocket::ngx_epoll_init()->epoll_creat() failed");
    exit(-2);
  }

  initConnection();

  std::vector<lpngx_listening_t>::iterator pos;
  for (pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end();
       ++pos) {
    lpngx_connection_t p_Conn = ngx_get_connection((*pos)->fd);
    if (p_Conn == NULL) {
      ngx_log_error_core(
          NGX_LOG_ERR, errno,
          "CSocket::ngx_epoll_init()->gx_get_connection() failed.");
      exit(2);
    }
    p_Conn->listening =
        (*pos); /* 连接对象 和监听对象关联，方便通过连接对象找监听对象 */
    (*pos)->connection =
        p_Conn; /* 监听对象 和连接对象关联，方便通过监听对象找连接对象 */

    //对监听端口的读事件设置处理方法，因为监听端口是用来等对方连接的发送三路握手的，所以监听端口关心的就是读事件
    p_Conn->rhandler = &CSocket::ngx_event_accept;

    if (ngx_epoll_oper_event((*pos)->fd, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP, 0,
                             p_Conn) == -1) {
      exit(2);
    }
  }  // end for
  return 1;
}

/*
 * @ Description: 初始化子进程
 * @ Paramater: void
 * @ Return: void
 */
bool CSocket::Initialize_subproc() {
  //发消息互斥量初始化
  if (pthread_mutex_init(&m_sendMessageQueueMutex, NULL) != 0) {
    ngx_log_error_core(NGX_LOG_ERR, errno,
                       "CSocket::Initialize()->pthread_mutex_init(&m_"
                       "sendMessageQueueMutex)failed.");
    return false;
  }
  //连接相关互斥量初始化
  if (pthread_mutex_init(&m_connectionMutex, NULL) != 0) {
    ngx_log_error_core(
        NGX_LOG_ERR, errno,
        "CSocket::Initialize()->pthread_mutex_init(&m_connectionMutex)failed.");
    return false;
  }
  //连接回收队列相关互斥量初始化
  if (pthread_mutex_init(&m_recyconnqueueMutex, NULL) != 0) {
    ngx_log_error_core(NGX_LOG_ERR, errno,
                       "CSocket::Initialize()->pthread_mutex_init(&m_"
                       "recyconnqueueMutex)failed.");
    return false;
  }

  //初始化发消息相关信号量，信号量用于进程/线程 之间的同步，虽然
  //互斥量[pthread_mutex_lock]和
  //条件变量[pthread_cond_wait]都是线程之间的同步手段，但 这里用信号量实现 则
  //更容易理解，更容易简化问题，使用书写的代码短小且清晰；
  //第二个参数=0，表示信号量在线程之间共享，确实如此
  //，如果非0，表示在进程之间共享
  //第三个参数=0，表示信号量的初始值，为0时，调用sem_wait()就会卡在那里卡着
  // if (sem_init(&m_semEventSendQueue, 0, 0) == -1) {
  //   ngx_log_stderr(
  //       0, "CSocekt::Initialize()中sem_init(&m_semEventSendQueue,0,0)失败.");
  //   return false;
  // }

  //创建线程
  int err;
  /*ThreadItem *pSendQueue;
  m_threadVector.push_back(pSendQueue = new ThreadItem(this)); //创建
  一个新线程对象 并入到容器中 err = pthread_create(&pSendQueue->_Handle,
  NULL, ServerSendQueueThread,pSendQueue);
  //创建线程，错误不返回到errno，一般返回错误码 if(err != 0)
  {
      return false;
  }*/
  //---
  ThreadItem *pRecyconn;
  m_threadVector.push_back(pRecyconn = new ThreadItem(this));
  err = pthread_create(&pRecyconn->_Handle, NULL, ServerRecyConnectionThread,
                       pRecyconn);
  if (err != 0) {
    return false;
  }
  ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocket::Initialize_subProc success");
  return true;
}

/*
 * @ Description: 增加事件
 * @ Parameter:
 * int fd(socket fd), uint32_t eventtype, uint32_t flag, int bcaction,
 * int bcaction, lpngx_connection_t pConn
 * @ Return int
 */
int CSocket::ngx_epoll_oper_event(int fd, uint32_t eventtype, uint32_t flag,
                                  int bcaction, lpngx_connection_t pConn) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(ev));

  if (eventtype == EPOLL_CTL_ADD) {
    //红黑树从无到有增加节点
    ev.events = flag;
    pConn->events = flag;
  } else if (eventtype == EPOLL_CTL_MOD) {
    //节点已经在红黑树中，修改节点的事件信息
  } else {
    //删除红黑树中节点，目前没这个需求，所以将来再扩展
    return 1;  //先直接返回1表示成功
  }

  ev.data.ptr = (void *)pConn;

  if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1) {
    ngx_log_error_core(
        NGX_LOG_ERR, errno,
        "CSocket::ngx_epoll_oper_event()中epoll_ctl[%d,%ud,%ud,%d] failed", fd,
        eventtype, flag, bcaction);
    return -1;
  }
  ngx_log_error_core(NGX_LOG_DEBUG, 0, "ngx_epoll_ctl() success");
  return 1;
}

/*
 * @ Description: 获取发生的事件消息
 * @ Parameter: int timer
 * @ Return: int success 1 failed 0
 */
int CSocket::ngx_epoll_process_events(int timer) {
  int events = epoll_wait(m_epollhandle, m_events, NGX_MAX_EVENTS, timer);
  ngx_log_error_core(NGX_LOG_DEBUG, 0, "epoll_wait()");

  if (events == -1) {     /* 产生错误 */
    if (errno == EINTR) { /* 信号过来 */
      ngx_log_error_core(
          NGX_LOG_INFO, errno,
          "CSocket::ngx_epoll_process_events()->epoll_wait()->EINTER failed");
      return 1;
    } else {
      ngx_log_error_core(
          NGX_LOG_ALERT, errno,
          "CSocket::ngx_epoll_process_events()->epoll_wait()->ERROR failed");
      return 0;
    }
  }

  if (events == 0) {   /* 超时返回 */
    if (timer != -1) { /* 永久等待 */
      return 1;        /* 成功 */
    }
    ngx_log_error_core(
        NGX_LOG_ALERT, 0,
        "CSocket::ngx_epoll_process_events()->epoll_wait()->timeout");
  }

  lpngx_connection_t c;
  uint32_t revents;
  for (int i = 0; i < events; ++i) {
    c = (lpngx_connection_t)(m_events[i].data.ptr);

    revents = m_events[i].events;

    if (revents & EPOLLIN) {
      (this->*(c->rhandler))(c); /* 回调 */
    }

    if (revents & EPOLLOUT) {
      if (revents & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
        --c->iThrowsendCount;
        ngx_log_error_core(NGX_LOG_INFO, errno, "EPOLLOUT");
        epoll_ctl(m_epollhandle,EPOLL_CTL_DEL,revents,&m_events[i]);
      } else {
      }
    }
  }
  return 1;
}
