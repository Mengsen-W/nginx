/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-28 19:54:45
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-28 21:53:32
 */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stropts.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "ngx_c_conf.h"
#include "ngx_c_socket.h"
#include "ngx_func.h"
#include "ngx_macro.h"

/*
 * @ Description: 构造函数
 */
CSocket::CSocket() {
  m_ListenPortCount = 1;
  return;
}

/*
 * @ Description: 构造函数
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
 * @ Description: 初始化监听套接字
 * @ Parameter: void
 * @ return: bool
 */
bool CSocket::Initialize() {
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