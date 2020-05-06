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
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ngx_c_conf.h"
#include "ngx_c_lockmutex.h"
#include "ngx_c_threadpool.h"
#include "ngx_func.h"
#include "ngx_macro.h"

/*
 * @ Description: 构造函数
 */
CSocket::CSocket()
    : m_ListenPortCount(0),
      m_worker_connections(0),
      m_epollhandle(-1),
      m_connection_n(0),
      m_total_connection_n(0),
      m_free_connection_n(0),
      m_RecyConnectionWaitTime(0),
      m_iWaitTime(0),
      m_ifkickTimeCount(0),
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
  m_iWaitTime = p_config->GetIntDefault("Sock_MaxWaitTime", m_iWaitTime);
  m_iWaitTime = (m_iWaitTime > 5) ? m_iWaitTime : 5;
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
  if (sem_post(&m_semEventSendQueue) == -1) {
    ngx_log_error_core(NGX_LOG_ERR, errno,
                       "CSocket::Shutown_subproc()->sem_post() failed");
  }
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
  clearAllFromTimeQueue();

  //(4)多线程相关
  pthread_mutex_destroy(&m_connectionMutex);  //连接相关互斥量释放
  pthread_mutex_destroy(&m_sendMessageQueueMutex);  //发消息互斥量释放
  pthread_mutex_destroy(&m_recyconnqueueMutex);  //连接回收队列相关的互斥量释放
  pthread_mutex_destroy(&m_timequeueMutex);  //时间处理队列相关的互斥量释放
  sem_destroy(&m_semEventSendQueue);  //发消息相关线程信号量释放
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
  //和时间处理队列有关的互斥量初始化
  if (pthread_mutex_init(&m_timequeueMutex, NULL) != 0) {
    ngx_log_stderr(0,
                   "CSocket::Initialize_subproc()中pthread_mutex_init(&m_"
                   "timequeueMutex) failed");
    return false;
  }

  //初始化发消息相关信号量，信号量用于进程/线程 之间的同步，虽然
  //互斥量[pthread_mutex_lock]和
  //条件变量[pthread_cond_wait]都是线程之间的同步手段，但 这里用信号量实现 则
  //更容易理解，更容易简化问题，使用书写的代码短小且清晰；
  //第二个参数=0，表示信号量在线程之间共享，确实如此
  //，如果非0，表示在进程之间共享
  //第三个参数=0，表示信号量的初始值，为0时，调用sem_wait()就会卡在那里卡着
  if (sem_init(&m_semEventSendQueue, 0, 0) == -1) {
    ngx_log_stderr(
        0, "CSocket::Initialize()中sem_init(&m_semEventSendQueue,0,0)失败.");
    return false;
  }

  //创建发送队列管理线程
  int err;
  ThreadItem *pSendQueue;
  m_threadVector.push_back(pSendQueue = new ThreadItem(this));
  err = pthread_create(&pSendQueue->_Handle, NULL, ServerSendQueueThread,
                       pSendQueue);
  if (err != 0) {
    return false;
  }

  // 创建管理回收连接池线程
  ThreadItem *pRecyconn;
  m_threadVector.push_back(pRecyconn = new ThreadItem(this));
  err = pthread_create(&pRecyconn->_Handle, NULL, ServerRecyConnectionThread,
                       pRecyconn);
  if (err != 0) {
    return false;
  }

  if (m_ifkickTimeCount == 1) { /* 是否开启踢人时钟，1：开启   0：不开启 */

    ThreadItem *pTimemonitor;  //专门用来处理到期不发心跳包的用户踢出的线程
    m_threadVector.push_back(pTimemonitor = new ThreadItem(this));
    err = pthread_create(&pTimemonitor->_Handle, NULL,
                         ServerTimerQueueMonitorThread, pTimemonitor);
    if (err != 0) {
      ngx_log_stderr(0,
                     "CSocket::Initialize_subproc()中->thread_create("
                     "ServerTimerQueueMonitorThread) failed");
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
      ev.events = pConn->events;
      if (bcaction == 0) {
        ev.events |= flag;
      } else if (bcaction == 1) {
        ev.events &= ~flag;
      } else {
        ev.events = flag;
      }
    } else {
      //删除红黑树中节点，目前没这个需求，所以将来再扩展
      return 1;  //先直接返回1表示成功
    }

    // 二次确认
    ev.data.ptr = (void *)pConn;

    if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1) {
      ngx_log_error_core(
          NGX_LOG_ERR, errno,
          "CSocket::ngx_epoll_oper_event()中epoll_ctl[%d,%ud,%ud,%d] failed",
          fd, eventtype, flag, bcaction);
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
        } else {
          (this->*(c->whandler))(c);
        }
      }
    }
    return 1;
  }

  /*
   * @ Description: 将数据发送到发送队列中
   */
  void CSocket::msgSend(char *pSendbuf) {
    CLock lock(&m_sendMessageQueueMutex);
    m_MsgSendQueue.push_back(pSendbuf);
    ++m_iSendMsgQueueCount;

    //将信号量的值+1,这样其他卡在sem_wait的就可以走下去
    if (sem_post(&m_semEventSendQueue) ==
        -1)  //让ServerSendQueueThread()流程走下来干活
    {
      ngx_log_stderr(0,
                     "CSocket::msgSend()中sem_post(&m_semEventSendQueue)失败.");
    }
    ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocket::ngx_msgSend() success");
    return;
  }

  /*
   * @ Description: 发送消息队列 单独线程
   */
  void *CSocket::ServerSendQueueThread(void *threadData) {
    ThreadItem *pThread = static_cast<ThreadItem *>(threadData);
    CSocket *pSocketObj = pThread->_pThis;
    int err;
    std::list<char *>::iterator pos, pos2, posend;

    char *pMsgBuf;
    LPSTRUC_MSG_HEADER pMsgHeader;
    LPCOMM_PKG_HEADER pPkgHeader;
    lpngx_connection_t p_Conn;
    unsigned short itmp;
    ssize_t sendsize;

    CMemory *p_memory = CMemory::GetInstance();

    while (g_stopEvent == 0)  //不退出
    {
      if (sem_wait(&pSocketObj->m_semEventSendQueue) == -1) {
        if (errno != EINTR)
          ngx_log_error_core(
              NGX_LOG_ERR, errno,
              "CSocket::ServerSendQueueThread()->sem_wait(&pSocketObj-"
              ">m_semEventSendQueue) failed.");
      }

      if (g_stopEvent != 0) /* 要求整个进程退出 */
        break;

      if (pSocketObj->m_iSendMsgQueueCount > 0) {
        err = pthread_mutex_lock(&pSocketObj->m_sendMessageQueueMutex);
        if (err != 0)
          ngx_log_error_core(
              NGX_LOG_ERR, err,
              "CSocket::ServerSendQueueThread()中pthread_mutex_lock() failed");

        pos = pSocketObj->m_MsgSendQueue.begin();
        posend = pSocketObj->m_MsgSendQueue.end();

        while (pos != posend) {
          pMsgBuf = (*pos);
          pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf; /* 消息头 */
          pPkgHeader = (LPCOMM_PKG_HEADER)(
              pMsgBuf + pSocketObj->m_iLenMsgHeader); /* 包头 */
          p_Conn = pMsgHeader->pConn;

          if (p_Conn->iCurrsequence !=
              pMsgHeader->iCurrsequence) { /* 判断客户端断开 */
            // 注意迭代器失效
            pos2 = pos;
            pos++;
            pSocketObj->m_MsgSendQueue.erase(pos2);
            --pSocketObj->m_iSendMsgQueueCount;
            p_memory->FreeMemory(pMsgBuf);
            continue;
          }

          if (p_Conn->iThrowsendCount > 0) {
            //靠系统驱动来发送消息，所以这里不能再发送
            pos++;
            continue;
          }

          //走到这里，可以发送消息
          p_Conn->psendMemPointer = pMsgBuf;
          //发送后释放用的，因为这段内存是new出来的
          pos2 = pos;
          pos++;
          pSocketObj->m_MsgSendQueue.erase(pos2);
          --pSocketObj->m_iSendMsgQueueCount;

          p_Conn->psendbuf = (char *)pPkgHeader;
          //要发送的数据的缓冲区指针，因为发送数据不一定全部都能发送出去，我们要记录数据发送到了哪里，需要知道下次数据从哪里开始发送
          itmp = ntohs(pPkgHeader->pkgLen); /* 包头+包体 长度 */
          p_Conn->isendlen = itmp;
          ngx_log_error_core(NGX_LOG_DEBUG, 0, "send data [%ud]",
                             p_Conn->isendlen);

          // 发送数据
          ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocket::ServerSend() begin");
          sendsize =
              pSocketObj->sendproc(p_Conn, p_Conn->psendbuf, p_Conn->isendlen);

          if (sendsize > 0) {
            if (sendsize == p_Conn->isendlen) {
              //成功发送的和要求发送的数据相等，说明全部发送成功了
              p_memory->FreeMemory(p_Conn->psendMemPointer); /* 释放内存 */
              /* 初始化 */
              p_Conn->psendMemPointer = NULL;
              p_Conn->iThrowsendCount = 0;
              ngx_log_error_core(
                  NGX_LOG_DEBUG, 0,
                  "CSocket::ServerSendQueueThread()->sendproc() success");
            } else { /* 发送区满 */
              //剩余多少记录
              p_Conn->psendbuf = p_Conn->psendbuf + sendsize;
              p_Conn->isendlen = p_Conn->isendlen - sendsize;

              // epoll_wait()
              ++p_Conn->iThrowsendCount;

              //投递此事件后，我们将依靠epoll驱动调用ngx_write_request_handler()函数发送数据
              if (pSocketObj->ngx_epoll_oper_event(p_Conn->fd, EPOLL_CTL_MOD,
                                                   EPOLLOUT, 0, p_Conn) == -1) {
                //有这情况发生？这可比较麻烦，不过先do nothing
                ngx_log_error_core(
                    NGX_LOG_ERR, errno,
                    "CSocket::ServerSendQueueThread()->ngx_epoll_oper_"
                    "event() in sendsize > 1 failed");
              }

              ngx_log_error_core(NGX_LOG_DEBUG, errno,
                                 "CSocket::ServerSendQueueThread() data no "
                                 "send all[send buffer "
                                 "full] [need to send = %d real send = %d]",
                                 p_Conn->isendlen, sendsize);
            }
            continue;
          }

          //能走到这里，应该是有点问题的
          else if (sendsize == 0) {
            ngx_log_error_core(
                NGX_LOG_INFO, errno,
                "CSocket::ServerSendQueueThread()->sendproc() return0");
            p_memory->FreeMemory(p_Conn->psendMemPointer);  //释放内存
            p_Conn->psendMemPointer = NULL;
            p_Conn->iThrowsendCount = 0;
            continue;
          }

          //能走到这里，继续处理问题
          else if (sendsize == -1) { /* 一个字节都没发出去 */
            ++p_Conn->iThrowsendCount;
            if (pSocketObj->ngx_epoll_oper_event(p_Conn->fd, EPOLL_CTL_MOD,
                                                 EPOLLOUT, 0, p_Conn) == -1) {
              ngx_log_error_core(
                  NGX_LOG_ERR, errno,
                  "CSocket::ServerSendQueueThread()->ngx_epoll_add_"
                  "event() in sendsize == 1 failed");
            }
            continue;
          }

          else { /* 对端断开 */
            p_memory->FreeMemory(p_Conn->psendMemPointer);
            p_Conn->psendMemPointer = NULL;
            p_Conn->iThrowsendCount = 0;
            continue;
          }
        }

        err = pthread_mutex_unlock(&pSocketObj->m_sendMessageQueueMutex);
        if (err != 0)
          ngx_log_error_core(NGX_LOG_ERR, err,
                             "CSocket::ServerSendQueueThread()->pthread_mutex_"
                             "unlock() failed");
      }
    }

    return (void *)0;
  }
