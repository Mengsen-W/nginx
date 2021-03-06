/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-29 21:17:17
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-07 13:29:25
 * @Description: 处理accept
 */

#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>

#include "ngx_c_socket.h"
#include "ngx_func.h"
#include "ngx_macro.h"

/*
 * @ Description: 封装accept
 * @ Parameter: lpngx_connect_t oldc
 * @ Return: void
 */
void CSocket::ngx_event_accept(lpngx_connection_t oldc) {
  struct sockaddr mysockaddr;
  socklen_t socklen = sizeof(mysockaddr);
  int err;
  int level;
  int s;
  static int use_accept4 = 1;
  lpngx_connection_t newc;

  do {
    if (use_accept4) {
      s = accept4(oldc->fd, &mysockaddr, &socklen, SOCK_NONBLOCK);
    } else {
      s = accept(oldc->fd, &mysockaddr, &socklen);
    }

    if (s == -1) {
      err = errno;
      if (err == EAGAIN) {
        return; /* listen fd 设置非阻塞但是却没有连接 */
        ngx_log_error_core(NGX_LOG_NOTICE, err, "CSocket::ngx_event_accept()");
      }
      /* 监听了listenfd 的可读所以一般不会出现这种问题 */

      level = NGX_LOG_ALERT;

      if (err == ECONNABORTED) /* 连接意外关闭 */
        level = NGX_LOG_ERR;
      else if (err == EMFILE || err == ENFILE) {
        /* EMFILE fd 用尽 ENFILE 也是用尽*/
        level = NGX_LOG_CRIT;

        ngx_log_error_core(level, err,
                           "CSocket::ngx_event_accept()->accept4() failed");
      }
      if (use_accept4 && err == ENOSYS) { /* 没有accept4() 函数 */
        use_accept4 = 0;
        continue;
      }

      if (err == ECONNABORTED) {
        //具体实现先不写
        ngx_log_error_core(level, err,
                           "CSocket::ngx_event_accept()->accept4() failed");
      }

      if (err == EMFILE || err == ENFILE) {
        // 具体实现先不写
        ngx_log_error_core(level, err,
                           "CSocket::ngx_event_accept()->accept4() failed");
      } else {
        ngx_log_error_core(
            level, err,
            "CSocket::ngx_event_accept()->accept4() Parameter[fd = "
            "%d,sockdaddr = %d, socklen = %d, flag = %d ] failed",
            oldc->fd, &mysockaddr, &socklen, SOCK_NONBLOCK);
      }
      return; /* 有错误直接返回 */
    }

    if (m_onlineUserCount >= m_worker_connections) { /* 接入人数过多 */
      ngx_log_error_core(NGX_LOG_INFO, 0, "max online user count close");
      close(s);
      return;
    }

    if (m_connectionList.size() >
        static_cast<size_t>(m_worker_connections * 5)) {
      /* 恶意用户发一条就断并且不断发 */
      if (m_freeconnectionList.size() <
          static_cast<size_t>(m_worker_connections)) {
        /* 连接池太大空闲连接太小 */
        ngx_log_error_core(NGX_LOG_INFO, 0,
                           "user close->accept()-> connection list too number");
        close(s);
        return;
      }
    }

    // 成功 accept4() / accept()
    newc = ngx_get_connection(s); /* 连接池分配 */

    if (newc == nullptr) {
      /* 连接池中连接不够用，那么就得把这个socket直接关闭并返回了 */
      /* 因为在ngx_get_connection()中已经写日志了，所以这里不需要写日志了 */
      if (close(s) == -1) {
        ngx_log_error_core(NGX_LOG_ALERT, errno,
                           "CSocket::ngx_event_accept()中close(%d) failed!", s);
      }
      return;
    }

    //将来这里会判断是否连接超过最大允许连接数，现在，这里可以不处理

    /* 成功的拿到了连接池中的一个连接 */
    memcpy(&newc->s_sockaddr, &mysockaddr, socklen);
    /* 拷贝客户端地址到连接对象【要转成字符串ip地址参考函数ngx_sock_ntop()】 */

    if (!use_accept4) {
      /* 如果不是用accept4()取得的socket，那么就要设置为非阻塞 */
      /* 因为用accept4()的已经被accept4()设置为非阻塞了 */
      if (setnonblocking(s) == false) { /* 设置非阻塞居然失败 */
        ngx_close_connection(newc); /* 回收连接池中的连接关闭socket */
        return;
      }
    }

    newc->listening = oldc->listening; /* 连接对象 */

    newc->rhandler =
        &CSocket::ngx_read_request_handler; /* 设置数据来时的读处理函数 */

    newc->whandler = &CSocket::ngx_write_request_handler; /* 写事件回调函数 */

    /* 客户端应该主动发送第一次的数据，这里将读事件加入epoll监控 */
    if (ngx_epoll_oper_event(s, EPOLL_CTL_ADD, EPOLLIN | EPOLLRDHUP, 0, newc) ==
        -1) {
      /* 增加事件失败 */
      ngx_close_connection(newc); /* 回收连接池中的连接*/
      return;
    }

    if (m_ifkickTimeCount == 1) AddToTimerQueue(newc);

    ++m_onlineUserCount;

    break;  //一般就是循环一次就跳出去
  } while (1);

  // ngx_log_error_core(NGX_LOG_DEBUG, 0, "accept() success");
  return;
}