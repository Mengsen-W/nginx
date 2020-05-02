/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-30 17:23:49
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-02 17:37:23
 * @Description: 读事件回调
 */

#include <arpa/inet.h>

#include <cerrno>
#include <cstring>

#include "ngx_c_lockmutex.h"
#include "ngx_c_memory.h"
#include "ngx_c_socket.h"
#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"

/*
 * @ Description: 读数据回调函数
 * @ Parameter: lpngx_connection_t c
 * @ Return: void
 */
void CSocket::ngx_wait_request_handler(lpngx_connection_t c) {
  ssize_t reco = recvproc(c, c->precvbuf, c->irecvlen);
  if (reco <= 0) return;

  //走到这里，说明成功收到了一些字节（>0）就要开始判断收到了多少数据了
  if (c->curStat == _PKG_HD_INIT) {
    /* 连接建立起来时肯定是这个状态 */
    /* 因为在ngx_get_connection()中已经把curStat成员赋值成_PKG_HD_INIT */
    if (reco == (ssize_t)m_iLenPkgHeader) { /* 正好收到完整包头，这里拆解包头 */
      /* 那就调用专门针对包头处理 */
      ngx_wait_request_handler_proc_p1(c);
    } else { /* 收到包头不完整 */
      /* 我们不能预料每个包的长度，也不能预料各种拆包/粘包情况 */
      /* 所以收到不完整包头【也算是缺包】是很可能的；*/
      c->curStat = _PKG_HD_RECVING;     /* 接收包头中，包头不完整 */
      c->precvbuf = c->precvbuf + reco; /* 注意收后续包的内存往后走 */
      c->irecvlen = c->irecvlen - reco; /* 要收的内容当然要减少 */
    }
  } else if (c->curStat == _PKG_HD_RECVING) { /* 接收包头中，包头不完整 */
    if (c->irecvlen == reco) { /* 要求收到的宽度和我实际收到的宽度相等 */
      /* 包头收完整了 */
      ngx_wait_request_handler_proc_p1(c);
    } else {                        /* 仍然不完整 */
      c->curStat = _PKG_HD_RECVING; /* 没必要 */
      c->precvbuf = c->precvbuf + reco;
      c->irecvlen = c->irecvlen - reco;
    }
  } else if (c->curStat == _PKG_BD_INIT) { /* 刚好收完包头 */
    if (reco == c->irecvlen) {
      /* 收到的宽度等于要收的宽度，包体也收完整了 */
      ngx_wait_request_handler_proc_plast(c);
    } else { /* 收到宽度小于提供宽度 */
      c->curStat = _PKG_BD_RECVING;
      c->precvbuf = c->precvbuf + reco;
      c->irecvlen = c->irecvlen - reco;
    }
  } else if (c->curStat == _PKG_BD_RECVING) { /* 包体不完整 */
    if (c->irecvlen == reco) {
      ngx_wait_request_handler_proc_plast(c);
    } else {
      c->precvbuf = c->precvbuf + reco;
      c->irecvlen = c->irecvlen - reco;
    }
  }

  return;
}

/*
 * @ Description: 封装收包函数
 * @ Parameter:
 *    lpngx_connection c(连接池单元)
 *    char *buffer(buffer首地址)
 *    ssize_t bufflen(buffer长度)
 * @ Return: ssize_t 接收长度 -1 返回失败
 */
ssize_t CSocket::recvproc(lpngx_connection_t c, char *buff, ssize_t buflen) {
  ssize_t n;
  n = recv(c->fd, buff, buflen, 0);

  if (n == 0) { /* 客户端断开 */
    ngx_log_error_core(NGX_LOG_DEBUG, 0, "client close");
    ngx_close_connection(c);
    return -1;
  }
  if (n < 0) { /* 这被认为有错误发生 */

    /* EAGAIN和EWOULDBLOCK[【这个应该常用在惠普系统上】应该是一样的值 */
    /* 表示没收到数据 */
    /* 一般来讲，在ET模式下会出现这个错误 */
    /* 因为ET模式下是不停的recv肯定有一个时刻收到这个errno */
    /* 但LT模式下一般是来事件才收，所以不该出现这个返回值 */
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      ngx_log_stderr(errno,
                     "CSocket::recvproc()中errno == EAGAIN || errno == "
                     "EWOULDBLOCK");
      return -1;
    }

    /* 当阻塞于某个慢系统调用的一个进程捕获某个信号且相应信号处理函数返回时
     */
    if (errno == EINTR) {
      ngx_log_stderr(errno, "CSocket::recvproc()中errno == EINTR");
      return -1;
    }

    //所有从这里走下来的错误，都认为异常：意味着我们要关闭客户端套接字要回收连接池中连接；

    if (errno == ECONNRESET) { /* Connection reset by peer */

      /* 正常关闭socket连接 */
      /*直接给服务器发送rst包而不是4次挥手包完成连接断开 */
      /* 10054(WSAECONNRESET) */
      /* 远程程序正在连接的时候关闭会产生这个错误远程主机强迫关闭了一个现有的连接*/
      /* 算常规错误吧【普通信息型】，日志都不用打印，没啥意思，太普通的错误
       */
      // do nothing
      //....一些大家遇到的很普通的错误信息，也可以往这里增加各种，代码要慢慢完善，一步到位，不可能，很多服务器程序经过很多年的完善才比较圆满；
    } else {
      /* 能走到这里的都表示错误希望知道一下是啥错误，我准备打印到屏幕上 */
      ngx_log_stderr(errno, "CSocket::recvproc() error");
    }

    ngx_log_stderr(0, "errno CSocket::recvproc()->recv() filed");

    /* 这种真正的错误就要，直接关闭套接字，释放连接池中连接了 */
    ngx_close_connection(c);
    return -1;
  }

  /* 能走到这里的，就认为收到了有效数据 */

  return n; /* 返回收到的字节数 */
}

/*
 * @ Description: 收到包头后续处理加入消息头更改状态等
 * @ Parameter: lpngx_connection_t c
 * @ Return: void
 */
void CSocket::ngx_wait_request_handler_proc_p1(lpngx_connection_t c) {
  CMemory *p_memory = CMemory::GetInstance();

  LPCOMM_PKG_HEADER pPkgHeader;
  pPkgHeader = (LPCOMM_PKG_HEADER)c->dataHeadInfo;
  /* 正好收到包头时，包头信息肯定是在dataHeadInfo里 */

  unsigned short e_pkgLen;
  e_pkgLen = ntohs(pPkgHeader->pkgLen);
  /* 注意这里网络序转本机序，所有传输到网络上的2字节数据 */
  /* 都要用htons()转成网络序，所有从网络上收到的2字节数据 */
  /* 都要用ntohs()转成本机序 */
  /* ntohs/htons的目的就是保证不同操作系统数据之间收发的正确性 */
  /* 不管客户端/服务器是什么操作系统，发送的数字是多少，收到的就是多少 */

  //恶意包或者错误包的判断
  if (e_pkgLen < m_iLenPkgHeader) { /* 包长怎么可能比包头还小 */
    /* 整个包长是包头+包体，就算包体为0字节 */
    /* 那么至少e_pkgLen  == m_iLenPkgHeader */
    /* 状态和接收位置都复原 */
    /* 因为有可能在其他状态比如_PKG_HD_RECVING状态调用这个函数 */
    c->curStat = _PKG_HD_INIT;
    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = m_iLenPkgHeader;
  } else if (e_pkgLen > (_PKG_MAX_LENGTH - 1000)) { /* 太大 */
    /* 客户端发来包居然说包长度 > 29000?肯定是恶意包 */
    c->curStat = _PKG_HD_INIT;
    c->precvbuf = c->dataHeadInfo;
    c->irecvlen = m_iLenPkgHeader;
  } else { /* 合法包头 */
    /* 我现在要分配内存开始收包体，因为包体长度并不是固定的 */
    char *pTmpBuffer =
        (char *)p_memory->AllocMemory(m_iLenMsgHeader + e_pkgLen, false);
    /* 分配内存【长度是 消息头长度  + 包头长度 + */
    /* 包体长度】，最后参数先给false，表示内存不需要memset */
    c->ifnewrecvMem = true;
    /* 标记我们new了内存，将来在ngx_free_connection()要回收的 */
    c->pnewMemPointer = pTmpBuffer;  //内存开始指针

    // a)先填写消息头内容
    LPSTRUC_MSG_HEADER ptmpMsgHeader = (LPSTRUC_MSG_HEADER)pTmpBuffer;
    ptmpMsgHeader->pConn = c;
    ptmpMsgHeader->iCurrsequence = c->iCurrsequence;
    /* 收到包时的连接池中连接序号记录到消息头里来，以备将来用 */

    // b)再填写包头内容
    pTmpBuffer += m_iLenMsgHeader; /* 往后跳，跳过消息头，指向包头 */
    memcpy(pTmpBuffer, pPkgHeader, m_iLenPkgHeader);
    /* 直接把收到的包头拷贝进来 */
    if (e_pkgLen == m_iLenPkgHeader) { /* 该报文只有包头无包体 */
      ngx_wait_request_handler_proc_plast(c);
    } else { /* 收包体 */
      c->curStat = _PKG_BD_INIT;
      /* 当前状态发生改变，包头刚好收完，准备接收包体 */
      c->precvbuf = pTmpBuffer + m_iLenPkgHeader;
      /*  pTmpBuffer指向包头，这里 +m_iLenPkgHeader后指向包体位置 */
      c->irecvlen = e_pkgLen - m_iLenPkgHeader;
      /* e_pkgLen是整个包【包头+包体】大小，-m_iLenPkgHeader【包头】= 包体 */
    }
  }

  return;
}

/*
 * @ Description: 收包体
 * @ Parameter: lpngx_connect_t c
 * @ Return: void
 */
void CSocket::ngx_wait_request_handler_proc_plast(lpngx_connection_t c) {
  int irmqc = 0;
  inMsgRecvQueue(c->pnewMemPointer, irmqc); /* 把这段内存放到消息队列中来 */

  g_threadpool.Call(irmqc);

  /* 收包状态机的状态恢复为原始态，为收下一个包做准备 */
  c->ifnewrecvMem = false;
  /* 内存不再需要释放，收完整了包，由inMsgRecvQueue()移入消息队列 */
  /* 那么释放内存就属于业务逻辑去干，不需要回收连接到连接池中干了 */
  c->pnewMemPointer = NULL;
  c->curStat = _PKG_HD_INIT;
  c->precvbuf = c->dataHeadInfo; /* 设置好收包的位置 */
  c->irecvlen = m_iLenPkgHeader; /* 设置好要接收数据的大小 */
  return;
}

/*
 * @ Description: 插入业务队列
 * @ Paramater: char *buf(包的地址)
 * @ Return: void
 */
void CSocket::inMsgRecvQueue(char *buf, int &irmqc) {
  CLock lock(&m_recvMessageQueueMutex);
  m_MsgRecvQueue.push_back(buf);
  ++m_iRecvQueueCount;
  irmqc = m_iRecvQueueCount;

  // 其他功能待扩充，这里要记住一点，这里的内存都是要释放的
  // 而且逻辑处理应该要引入多线程，所以这里要考虑临界问题

  /* 临时在这里调用一下该函数，以防止接收消息队列过大 */
  // tmpoutMsgRecvQueue();  //.....临时，后续会取消这行代码

  //  ngx_log_error_core(NGX_LOG_DEBUG, 0,
  //                      "get it successful message nice"); /* debug */
  return;
}

/*
 * @ Description: 出业务队列
 * @ Paramater: void
 * @ Return: char*(队列头指针)
 */
char *CSocket::outMsgRecvQueue() {
  CLock lock(&m_recvMessageQueueMutex);
  if (m_MsgRecvQueue.empty()) {
    return nullptr;
  }
  char *sTmpMsgBuf = m_MsgRecvQueue.front();
  m_MsgRecvQueue.pop_front();
  --m_iRecvQueueCount;
  return sTmpMsgBuf;
}

//临时函数，用于将Msg中消息干掉
void CSocket::tmpoutMsgRecvQueue() {
  if (m_MsgRecvQueue.empty()) { /* 没有消息就 */
    return;
  }

  int size = m_MsgRecvQueue.size();

  if (size < 1000) { /* 消息不超过1000条就不处理先 */
    return;
  }

  //消息达到1000条
  CMemory *p_memory = CMemory::GetInstance();
  int cha = size - 500;
  for (int i = 0; i < cha; ++i) {
    /* 一次干掉一堆 */
    char *sTmpMsgBuf = m_MsgRecvQueue.front();
    /* 返回第一个元素但不检查元素存在与否 */
    m_MsgRecvQueue.pop_front();
    /* 移除第一个元素但不返回 */
    p_memory->FreeMemory(sTmpMsgBuf); /* 释放 */
  }
  return;
}