/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-29 20:42:07
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-03 15:51:10
 * @Description: 连接池相关函数
 */

#include <unistd.h>

#include <cerrno>
#include <cstring>

#include "ngx_c_lockmutex.h"
#include "ngx_c_memory.h"
#include "ngx_c_socket.h"
#include "ngx_comm.h"
#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"

/*
 * Description: 构造函数
 */
ngx_connection_s::ngx_connection_s() {
  iCurrsequence = 0;
  pthread_mutex_init(&logicPorcMutex, NULL);
}
/*
 * Description: 析构函数
 */
ngx_connection_s::~ngx_connection_s() {
  pthread_mutex_destroy(&logicPorcMutex);
}

/*
 * @ Description: 分配出去一个连接的时候初始化一些内容,原来内容放在
 */
void ngx_connection_s::GetOneToUse() {
  ++iCurrsequence;

  curStat = _PKG_HD_INIT;
  precvbuf = dataHeadInfo;
  irecvlen = sizeof(COMM_PKG_HEADER);

  precvMemPointer = NULL;
  iThrowsendCount = 0;
  psendMemPointer = NULL;
  events = 0;
}

/*
 * @ Description: 回收回来一个连接的时候做一些事
 */
void ngx_connection_s::PutOneToFree() {
  ++iCurrsequence;
  if (precvMemPointer != NULL) {
    CMemory::GetInstance()->FreeMemory(precvMemPointer);
    precvMemPointer = NULL;
  }
  if (psendMemPointer != NULL) {
    CMemory::GetInstance()->FreeMemory(psendMemPointer);
    psendMemPointer = NULL;
  }

  iThrowsendCount = 0;
}

/*
 * @ Description: 初始化连接池
 * @ Paramater: void
 * @ Return: void
 */
void CSocket::initConnection() {
  lpngx_connection_t p_Conn;
  CMemory *p_memory = CMemory::GetInstance();

  int ilenconnpool = sizeof(ngx_connection_t);
  for (int i = 0; i < m_worker_connections; ++i) {
    p_Conn = (lpngx_connection_t)p_memory->AllocMemory(ilenconnpool, true);

    p_Conn = new (p_Conn) ngx_connection_t(); /* 定位new */
    p_Conn->GetOneToUse();
    m_connectionList.push_back(p_Conn);
    m_freeconnectionList.push_back(p_Conn);
  }

  m_free_connection_n = m_total_connection_n = m_connectionList.size();
  return;
}

/*
 * Description: 最终回收连接池，释放内存
 */
void CSocket::clearconnection() {
  lpngx_connection_t p_Conn;
  CMemory *p_memory = CMemory::GetInstance();

  while (!m_connectionList.empty()) {
    p_Conn = m_connectionList.front();
    m_connectionList.pop_front();
    p_Conn->~ngx_connection_t();
    p_memory->FreeMemory(p_Conn);
  }
}

/*
 * @ Description: 从连接池空闲链表中区节点
 * @ Parameter: int sock
 * @ Return: lpngx_connect_t
 */
lpngx_connection_t CSocket::ngx_get_connection(int isock) {
  CLock lock(&m_connectionMutex); /* 锁住连接池链表 */

  if (!m_freeconnectionList.empty()) { /* 空闲列表有位置 */
    lpngx_connection_t p_Conn = m_freeconnectionList.front();
    m_freeconnectionList.pop_front();
    p_Conn->GetOneToUse();
    --m_free_connection_n;
    p_Conn->fd = isock;
    ngx_log_error_core(NGX_LOG_DEBUG, 0,
                       "CSocket::ngx_get_connection() is empty success");
    return p_Conn;
  }

  // 空闲列表没位置
  CMemory *p_memory = CMemory::GetInstance();
  lpngx_connection_t p_Conn =
      (lpngx_connection_t)p_memory->AllocMemory(sizeof(ngx_connection_t), true);
  p_Conn = new (p_Conn) ngx_connection_t();
  p_Conn->GetOneToUse();
  m_connectionList.push_back(p_Conn);
  ++m_total_connection_n;
  p_Conn->fd = isock;
  ngx_log_error_core(NGX_LOG_DEBUG, 0,
                     "CSocket::ngx_get_connection() not empty success");
  return p_Conn;
}

/*
 * @ Description: 从连接池释放
 * @ Parameter: lpngx_connection_t c
 * @ Return: void
 */
void CSocket::ngx_free_connection(lpngx_connection_t pConn) {
  //因为有线程可能要动连接池中连接，所以在合理互斥也是必要的
  CLock lock(&m_connectionMutex);

  //首先明确一点，连接，所有连接全部都在m_connectionList里；
  pConn->PutOneToFree(); /* 初始化 */

  //扔到空闲连接列表里
  m_freeconnectionList.push_back(pConn);

  //空闲连接数+1
  ++m_free_connection_n;

  ngx_log_error_core(NGX_LOG_DEBUG, 0, "free connection success");
  return;
}

/*
 * @ Description: 从连接池关闭连接
 * @ Parameter: lpngx_connection_t c
 * @ Return: void
 */
void CSocket::ngx_close_connection(lpngx_connection_t c) {
  ngx_free_connection(c);  //把释放代码放在最后边，感觉更合适
  if (close(c->fd) == -1) {
    ngx_log_error_core(NGX_LOG_ALERT, errno,
                       "CSocket::ngx_close_connection()中close(%d)失败!",
                       c->fd);
  }
  return;
}

/*
 * @ Description: 延迟回收连接单元
 * @ Parameters: lpngx_connection_t pConn
 * @ Return: void *
 */
void CSocket::inRecyConnectQueue(lpngx_connection_t pConn) {
  ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocket::inRecyConnectQueue().");

  CLock lock(&m_recyconnqueueMutex);

  pConn->inRecyTime = time(NULL); /* 记录回收时间 */
  ++pConn->iCurrsequence;
  m_recyconnectionList.push_back(
      pConn); /* 等待ServerRecyConnectionThread线程自会处理 */
  ++m_total_recyconnection_n; /* 待释放连接队列大小+1 */

  return;
}

/*
 * @ Description: 单独一个线程处理延迟回收池
 * @ Paramater void *threadDate
 * @ Return: void *
 */
void *CSocket::ServerRecyConnectionThread(void *threadData) {
  ThreadItem *pThread = static_cast<ThreadItem *>(threadData);
  CSocket *pSocketObj = pThread->_pThis;

  time_t currtime;
  int err;
  std::list<lpngx_connection_t>::iterator pos, posend;
  lpngx_connection_t p_Conn;

  while (1) {
    //为简化问题，我们直接每次休息200毫秒
    usleep(200 * 1000);

    //不管啥情况，先把这个条件成立时该做的动作做了
    if (pSocketObj->m_total_recyconnection_n > 0) {
      currtime = time(NULL);
      err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);
      if (err != 0)
        ngx_log_error_core(
            NGX_LOG_ERR, err,
            "CSocket::ServerRecyConnectionThread()->pthread_mutex_"
            "lock() failed!");

    lblRRTD:
      pos = pSocketObj->m_recyconnectionList.begin();
      posend = pSocketObj->m_recyconnectionList.end();
      for (; pos != posend; ++pos) {
        p_Conn = (*pos);
        if (((p_Conn->inRecyTime + pSocketObj->m_RecyConnectionWaitTime) >
             currtime) &&
            (g_stopEvent == 0)) {
          //如果不是要整个系统退出，你可以continue，否则就得要强制释放
          continue; /* 没到释放的时间 */
        }
        if (p_Conn->iThrowsendCount != 0) {
          //这确实不应该，打印个日志吧；
          ngx_log_error_core(
              NGX_LOG_INFO, 0,
              "CSocket::ServerRecyConnectionThread()中到释放时间却发现p_Conn."
              "iThrowsendCount!=0，这个不该发生");
          //其他先暂时啥也不敢，路程继续往下走，继续去释放吧。
        }

        //到释放的时间了:
        //......这将来可能还要做一些是否能释放的判断[在我们写完发送数据代码之后吧]，先预留位置
        //....

        //流程走到这里，表示可以释放，那我们就开始释放
        --pSocketObj->m_total_recyconnection_n; /* 待释放连接队列大小-1 */
        pSocketObj->m_recyconnectionList.erase(pos); /* 删除节点 */

        ngx_log_error_core(
            NGX_LOG_DEBUG, 0,
            "CSocket::ServerRecyConnectionThread() link[%d] return pool",
            p_Conn->fd);

        pSocketObj->ngx_free_connection(p_Conn);
        /* 归还参数pConn所代表的连接到到连接池中 */
        goto lblRRTD;
      }

      err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex);
      if (err != 0)
        ngx_log_error_core(NGX_LOG_ERR, err,
                           "CSocket::ServerRecyConnectionThread()pthread_mutex_"
                           "unlock()filed");
    }  // end if

    if (g_stopEvent == 1) { /* 要退出整个程序，那么肯定要先退出这个循环 */

      if (pSocketObj->m_total_recyconnection_n > 0) { /* 程序退出硬释放 */
        err = pthread_mutex_lock(&pSocketObj->m_recyconnqueueMutex);
        if (err != 0)
          ngx_log_error_core(
              NGX_LOG_ERR, err,
              "CSocket::ServerRecyConnectionThread() hard -> pthread_mutex_"
              "lock() failed");

      lblRRTD2:
        pos = pSocketObj->m_recyconnectionList.begin();
        posend = pSocketObj->m_recyconnectionList.end();
        for (; pos != posend; ++pos) {
          p_Conn = (*pos);
          --pSocketObj->m_total_recyconnection_n;
          pSocketObj->m_recyconnectionList.erase(pos);
          pSocketObj->ngx_free_connection(p_Conn);
          goto lblRRTD2;
        }  // end for

        err = pthread_mutex_unlock(&pSocketObj->m_recyconnqueueMutex);
        if (err != 0)
          ngx_log_error_core(
              NGX_LOG_ERR, err,
              "CSocket::ServerRecyConnectionThread() hard -> pthread_mutex_"
              "unlock() failed");
      }
      break;
    }
  }

  return nullptr;
}