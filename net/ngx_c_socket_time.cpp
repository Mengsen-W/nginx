/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-07 10:30:01
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-07 12:52:37
 * @Description: 心跳包有关
 */

#include <errno.h>   //errno
#include <fcntl.h>   //open
#include <stdarg.h>  //va_start....
#include <stdint.h>  //uintptr_t
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>  //gettimeofday
#include <time.h>      //localtime_r
#include <unistd.h>    //STDERR_FILENO等
//#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>  //ioctl

#include "ngx_c_conf.h"
#include "ngx_c_lockmutex.h"
#include "ngx_c_memory.h"
#include "ngx_c_socket.h"
#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"

/*
 * @ Description: 套接字加入心跳队列
 * @ Paramater: lpngx_connection_t pConn
 * @ Return: void
 */
void CSocket::AddToTimerQueue(lpngx_connection_t pConn) {
  CMemory *p_memory = CMemory::GetInstance();

  time_t futtime = time(NULL);
  futtime += m_iWaitTime; /* 20秒之后的时间 */

  CLock lock(&m_timequeueMutex);
  LPSTRUC_MSG_HEADER tmpMsgHeader =
      (LPSTRUC_MSG_HEADER)p_memory->AllocMemory(m_iLenMsgHeader, false);
  tmpMsgHeader->pConn = pConn;
  tmpMsgHeader->iCurrsequence = pConn->iCurrsequence;
  m_timerQueuemap.insert(std::make_pair(futtime, tmpMsgHeader));
  m_cur_size_++; /* 增加 */
  m_timer_value_ =
      GetEarliestTime(); /* 计时队列头部时间值保存到m_timer_value_里 */

  // ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocket::AddTOTimeQueue() success");
  return;
}

/*
 * @ Description: 获取时间队列中最早的时间
 * @ Parameters: void
 * @ Return: void
 */
time_t CSocket::GetEarliestTime() {
  std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos;
  pos = m_timerQueuemap.begin();
  return pos->first;
}

/*
 * @ Description: 获取第一个节点并移除
 * @ Paramater: void
 * @ Returns: LPSTRUC_MSG_HEADER
 */
LPSTRUC_MSG_HEADER CSocket::RemoveFirstTimer() {
  std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos;
  LPSTRUC_MSG_HEADER p_tmp;
  if (m_cur_size_ <= 0) {
    return NULL;
  }
  pos = m_timerQueuemap.begin();
  p_tmp = pos->second;
  m_timerQueuemap.erase(pos);
  --m_cur_size_;
  // ngx_log_error_core(NGX_LOG_DEBUG, 0, "CSocket::RemoveFirstTimer() success");
  return p_tmp;
}

/*
 * @ Description: 根据给定的时间找到比当前时间小的时间
 * @ Parameters: time_t cut_time
 * @ Returns: LPSTRUC_MSG_HEADER
 */
LPSTRUC_MSG_HEADER CSocket::GetOverTimeTimer(time_t cur_time) {
  CMemory *p_memory = CMemory::GetInstance();
  LPSTRUC_MSG_HEADER ptmp;

  if (m_cur_size_ == 0 || m_timerQueuemap.empty()) return NULL; /* 队列为空 */

  time_t earliesttime = GetEarliestTime(); /* 获取最早时间 */
  if (earliesttime <= cur_time) {
    ptmp = RemoveFirstTimer();

    if (m_ifkickTimeCount != 1) { /* 不需要在加进去 */
      //因为下次超时的时间我们也依然要判断，所以还要把这个节点加回来
      time_t newinqueutime = cur_time + (m_iWaitTime);
      LPSTRUC_MSG_HEADER tmpMsgHeader =
          (LPSTRUC_MSG_HEADER)p_memory->AllocMemory(sizeof(STRUC_MSG_HEADER),
                                                    false);
      tmpMsgHeader->pConn = ptmp->pConn;
      tmpMsgHeader->iCurrsequence = ptmp->iCurrsequence;
      m_timerQueuemap.insert(std::make_pair(newinqueutime, tmpMsgHeader));
      m_cur_size_++;
    }

    if (m_cur_size_ > 0) {
      //这个判断条件必要，因为以后我们可能在这里扩充别的代码

      m_timer_value_ = GetEarliestTime();
      //计时队列头部时间值保存到m_timer_value_里
    }
    return ptmp;
  }
  return NULL;
}

/*
 * @ Description: 从时钟队列删除
 * @ Paramater: lpngx_connection_t pConn
 * @ Return: void
 */
void CSocket::DeleteFromTimerQueue(lpngx_connection_t pConn) {
  std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos, posend;
  CMemory *p_memory = CMemory::GetInstance();

  CLock lock(&m_timequeueMutex);

  //因为实际情况可能比较复杂，将来可能还扩充代码等等，所以如下我们遍历整个队列找
  //一圈，而不是找到一次就拉倒，以免出现什么遗漏
lblMTQM:
  pos = m_timerQueuemap.begin();
  posend = m_timerQueuemap.end();
  for (; pos != posend; ++pos) {
    if (pos->second->pConn == pConn) {
      p_memory->FreeMemory(pos->second);  //释放内存
      m_timerQueuemap.erase(pos);
      --m_cur_size_;  //减去一个元素，必然要把尺寸减少1个;
      goto lblMTQM;
    }
  }
  if (m_cur_size_ > 0) {
    m_timer_value_ = GetEarliestTime();
  }
  return;
}

/*
 * @ Description: 清理时间队列中所有内容
 */
void CSocket::clearAllFromTimerQueue() {
  std::multimap<time_t, LPSTRUC_MSG_HEADER>::iterator pos, posend;

  CMemory *p_memory = CMemory::GetInstance();
  pos = m_timerQueuemap.begin();
  posend = m_timerQueuemap.end();
  for (; pos != posend; ++pos) {
    p_memory->FreeMemory(pos->second);
    --m_cur_size_;
  }
  m_timerQueuemap.clear();
}

/*
 * @ Description: 监视处理时间队列的线程
 * @ Paramater: void *threadData
 * @ Return: void *
 */
void *CSocket::ServerTimerQueueMonitorThread(void *threadData) {
  ThreadItem *pThread = static_cast<ThreadItem *>(threadData);
  CSocket *pSocketObj = pThread->_pThis;

  time_t absolute_time, cur_time;
  int err;

  while (g_stopEvent == 0) {
    //这里没互斥判断，所以只是个初级判断，目的至少是队列为空时避免系统损耗
    if (pSocketObj->m_cur_size_ > 0) { /* 队列不为空 */

      //时间队列中最近发生事情的时间放到 absolute_time里；
      absolute_time = pSocketObj->m_timer_value_;
      cur_time = time(NULL);

      if (absolute_time < cur_time) { /* 时间到 */

        std::list<LPSTRUC_MSG_HEADER> m_lsIdleList; /* 保存要处理的内容 */
        LPSTRUC_MSG_HEADER result;

        err = pthread_mutex_lock(&pSocketObj->m_timequeueMutex); /* 锁 */
        if (err != 0)
          ngx_log_error_core(
              NGX_LOG_ERR, err,
              "CSocket::ServerTimerQueueMonitorThread()->pthread_"
              "mutex_lock() failed");

        while ((result = pSocketObj->GetOverTimeTimer(cur_time)) !=
               NULL) { /* 一次性的把所有可能超时节点都拿过来 */

          m_lsIdleList.push_back(result); /* 预处理队列 */
        }

        err = pthread_mutex_unlock(&pSocketObj->m_timequeueMutex);
        if (err != 0)
          ngx_log_error_core(NGX_LOG_ERR, err,
                             "CSocket::ServerTimerQueueMonitorThread()pthread_"
                             "mutex_unlock() failed");

        LPSTRUC_MSG_HEADER tmpmsg;
        while (!m_lsIdleList.empty()) {
          tmpmsg = m_lsIdleList.front();
          m_lsIdleList.pop_front();
          ngx_log_error_core(NGX_LOG_INFO, 0,
                             "CSocket::Begin procPingTimeOutChecking()");
          pSocketObj->procPingTimeOutChecking(
              tmpmsg, cur_time); /* 这里需要检查心跳超时问题 */
        }
      }
    }

    usleep(500 * 1000);
  }

  return (void *)0;
}

/*
 * @ Description: 检测心跳包函数 父类只是释放内存 子类干活
 * @ Parameter: LPSTRUC_MSG_HEADER tmpmsg, time_t cur_time
 * @ Return: void
 */
void CSocket::procPingTimeOutChecking(LPSTRUC_MSG_HEADER tmpmsg,
                                      time_t cur_time) {
  CMemory *p_memory = CMemory::GetInstance();
  p_memory->FreeMemory(tmpmsg);
}
