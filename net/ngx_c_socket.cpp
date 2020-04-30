/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-28 19:54:45
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-30 18:21:00
 */
#include "ngx_c_socket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ngx_c_conf.h"
#include "ngx_func.h"
#include "ngx_macro.h"

/*
 * @ Description: 构造函数
 */
CSocket::CSocket() : m_ListenPortCount(0), m_worker_connections(0) {}

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
      ngx_log_stderr(errno, "CSocket::Initialize()->socket() failed i = %d", i);
      return false;
    }

    int reuseaddr = 1; /* 打开对应的设置项 */
    if (setsockopt(isock, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuseaddr,
                   sizeof(reuseaddr)) == -1) { /* 设置 bind() TIME_WAIT */
      ngx_log_stderr(errno, "CSocket::Initialize()->setsockopt() failed i = %d",
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
      ngx_log_stderr(errno, "CSocket::Initialize()->bind() failed i = %d", i);
      close(isock);
      return false;
    }

    if (listen(isock, NGX_LISTEN_BACKLOG) == -1) { /* 监听 */
      ngx_log_stderr(errno, "CSocket::Initialize()->listen() failed i = %d", i);
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
  // epoll_creat
  m_epollhandle = epoll_create(m_worker_connections);
  if (m_epollhandle == -1) {
    ngx_log_stderr(errno, "CSocket::ngx_epoll_init()->epoll_creat() failed");
    exit(2);
  }

  // 连接池
  m_connection_n = m_worker_connections; /* 记录当前连接池中的连接总数 */
  /* 每个元素都是一个 ngx_connection_t */
  m_pconnections =
      new ngx_connection_t[m_connection_n]; /* new 失败了直接dump吧 */

  int i = m_connection_n;                /* 连接池数量 */
  lpngx_connection_t next = nullptr;     /* 后继连接 */
  lpngx_connection_t c = m_pconnections; /* 连接池首地址 */
  // m_pread_events = new ngx_event_t[m_connection_n];
  // m_pwrite_events= new ngx_event_t[m_connection_n];
  // for(int i = 0; i < m_connection_n; ++i){
  //   m_pconnections[i].instance = 1
  // }

  do {
    --i; /* i是数组末尾 */

    c[i].data = next;       /* 设置连接对象的next指针 */
    c[i].fd = -1;           /* 初始化套接字 */
    c[i].instance = 1;      /* 设置标志位为1 */
    c[i].iCurrsequence = 0; /* 当前序号统一从0开始 */

    next = &c[i]; /* next 指针前移, 最终移动到连接池首 */
  } while (i);

  m_pfree_connections = next; /* 管理对象的空链表指针指向连接池表头 */
  m_free_connection_n = m_connection_n; /* 空闲池数目 */

  std::vector<lpngx_listening_t>::iterator pos;
  for (pos = m_ListenSocketList.begin(); pos != m_ListenSocketList.end();
       ++pos) {
    c = ngx_get_connection((*pos)->fd);
    if (c == nullptr) { /* 失败致命问题 */
      ngx_log_stderr(errno,
                     "CSocket::ngx_epoll_init()->ngx_get_connect() failed");
      exit(2);
    }
    c->listening = *(pos);  /* 监听对象相互关联 */
    (*pos)->connection = c; /* 连接对象相互关联 */

    // rev->accept = 1;

    c->rhandler = &CSocket::ngx_event_accept; /* 设置回调函数 */

    /* 对listen增加监听事件 */
    if (ngx_epoll_add_event((*pos)->fd, 1, 0, 0, EPOLL_CTL_ADD, c) == -1) {
      ngx_log_stderr(0,
                     "CSocket::ngx_epoll_init()->ngx_epoll_add_event() failed");
      exit(2);
    }
  }
  return 1;
}

/*
 * @ Description: 增加监听事件
 * @ Parameter:
 * int fd, int readevent, int writeevent,uint32_t otherflag, uint32_t
 * eventtype, lpngx_connection_t c
 * @ Return int
 */
int CSocket::ngx_epoll_add_event(int fd, int readevent, int writeevent,
                                 uint32_t otherflag, uint32_t eventtype,
                                 lpngx_connection_t c) {
  struct epoll_event ev;
  memset(&ev, 0, sizeof(epoll_event));

  if (readevent == 1) { /* 注册读事件 */
    ev.events = EPOLLIN | EPOLLHUP;

    // ev.events |= (ev.events | EPOLLET);
  } else { /* 其他事件类型 */
  }

  if (otherflag != 0) {
    ev.events |= otherflag;
  }

  /* 指针的最后一位永远是0 */
  ev.data.ptr = (void *)((uintptr_t)c | c->instance); /* 用失效位来确定 */

  if (epoll_ctl(m_epollhandle, eventtype, fd, &ev) == -1) {
    ngx_log_stderr(
        errno, "CSocket::ngx_epoll_add_event()中epoll_ctl(%d,%d,%d,%u,%u)失败.",
        fd, readevent, writeevent, otherflag, eventtype);
    return -1;
  }
  return 1;
}

/*
 * @ Description: 获取发生的事件消息
 * @ Parameter: int timer
 * @ Return: int success 1 failed 0
 */
int CSocket::ngx_epoll_process_events(int timer) {
  int events = epoll_wait(m_epollhandle, m_events, NGX_MAX_EVENTS, timer);

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
  uintptr_t instance;
  uint32_t revents;
  for (int i = 0; i < events; ++i) {
    c = (lpngx_connection_t)(m_events[i].data.ptr);
    instance = (uintptr_t)c & 1;
    c = (lpngx_connection_t)((uintptr_t)c & (uintptr_t)~1);

    if (c->fd == -1) { /* 关闭连接 */
      ngx_log_error_core(NGX_LOG_DEBUG, 0,
                         "CSocket::ngx_epoll_process_events()->fd = -1 : %p",
                         c);
      continue;
    }

    if (c->instance != instance) { /* 核对标志位 */
      ngx_log_error_core(
          NGX_LOG_DEBUG, 0,
          "CSocket::ngx_epoll_process_events()->instance failed");
      continue;
    }

    revents = m_events[i].events;

    if (revents & (EPOLLERR | EPOLLHUP)) {
      revents |= EPOLLIN | EPOLLOUT;
    }

    if (revents & EPOLLIN) {
      (this->*(c->rhandler))(c); /* 回调 */
    }

    if (revents & EPOLLOUT) {
      ngx_log_stderr(errno, "111111111111debug EPOLLOUT");
    }
  }
  return 1;
}