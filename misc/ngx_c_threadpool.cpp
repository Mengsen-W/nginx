/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-02 14:28:25
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-03 15:38:18
 * @Description: 线程池实现
 */

#include "ngx_c_threadpool.h"

#include <unistd.h>

#include "ngx_c_memory.h"
#include "ngx_c_socket.h"
#include "ngx_func.h"
#include "ngx_global.h"
#include "ngx_macro.h"

pthread_mutex_t CThreadPool::m_pthreadMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t CThreadPool::m_pthreadCond = PTHREAD_COND_INITIALIZER;
bool CThreadPool::m_shutdown = false;

/*
 * @ Description: 构造函数
 */
CThreadPool::CThreadPool()
    : m_iRunningThreadNUm(0), m_iLastEmgTime(0), m_iRecvMsgQueueCount(0) {}

/*
 * @ Description: 析构函数
 */
CThreadPool::~CThreadPool() { clearMsgRecvQueue(); }

/*
 * @ Description: 创建线程池中的线程
 * @ Parameter: int threadnums
 * @ Return: bool
 */
bool CThreadPool::Create(int threadnums) {
  ThreadItem* pNew;
  int err;

  m_iThreadNUm = threadnums;

  for (int i = 0; i < m_iThreadNUm; ++i) { /* 创建线程 */

    m_threadVector.push_back(pNew = new ThreadItem(this));

    err = pthread_create(&pNew->_Handle, NULL, ThreadFunc, pNew);
    if (err != 0) {
      ngx_log_error_core(NGX_LOG_ERR, err, "[pthread = %d]CThreadPool::Creat()",
                         i);
      return false;
    } else {
    }
  }

  std::vector<ThreadItem*>::iterator iter;

lblfor:
  /* 保证子线程等待到 cond_wait */
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); ++iter) {
    if ((*iter)->ifrunning == false) {
      goto lblfor;
    }
  }
  // ngx_log_error_core(NGX_LOG_DEBUG, 0, "CThreadpool::Create() successful");
  return true;
}

/*
 * @ Description: 线程入口函数
 * @ Parameter: void*
 * @ Return: void*
 */
void* CThreadPool::ThreadFunc(void* threadData) {
  ThreadItem* pthread = static_cast<ThreadItem*>(threadData);
  /* 静态成员函数没有this指针 */
  CThreadPool* pThreadPoolObj = pthread->_pThis;

  CMemory* p_memory = CMemory::GetInstance();
  int err;

  pthread_t tid = pthread_self();
  if (tid != pthread->_Handle) {
    // ngx_log_error_core(NGX_LOG_DEBUG, 0, "[thread = %d]tid != _Handle", tid);
  }

  while (true) {
    err = pthread_mutex_lock(&m_pthreadMutex);
    if (err != 0)
      ngx_log_error_core(
          NGX_LOG_ERR, err,
          "CThreadPool::Threadfunc()->pthread_mutex_lock() failed");

    while ((pThreadPoolObj->m_MsgRecvQueue.size() == 0) &&
           m_shutdown == false) { /* 用while防止虚假唤醒 */
      if (pthread->ifrunning == false) pthread->ifrunning = true;
/*       ngx_log_error_core(NGX_LOG_DEBUG, 0,
                         "CThreadPool [tid = %d] pthread wait()", tid); */
      pthread_cond_wait(&m_pthreadCond, &m_pthreadMutex);
    }

    if (m_shutdown) {
      pthread_mutex_unlock(&m_pthreadMutex);
      break;
    }

    char* jobbuf = pThreadPoolObj->m_MsgRecvQueue.front();
    pThreadPoolObj->m_MsgRecvQueue.pop_front();
    --pThreadPoolObj->m_iRecvMsgQueueCount;
    pthread_mutex_unlock(&m_pthreadMutex);

    ++pThreadPoolObj->m_iRunningThreadNUm;

    g_socket.threadRecvProcFunc(jobbuf);

    // 释放消息资源
    p_memory->FreeMemory(jobbuf);
    --pThreadPoolObj->m_iRunningThreadNUm;
  }
  // ngx_log_error_core(NGX_LOG_DEBUG, 0, "CThreadPool::ThreadFunc() success");
  return static_cast<void*>(0);
}

/*
 * @ Description: 停止所有线程
 * @ Paramater: void
 * @ Returns: void
 */
void CThreadPool::StopAll() {
  // shutdown 已经关了
  if (m_shutdown == true) return;

  m_shutdown = true;

  // 唤醒所有让子线程自己退出
  int err = pthread_cond_broadcast(&m_pthreadCond);
  if (err != 0) {
    ngx_log_error_core(
        NGX_LOG_ALERT, err,
        "CThreadPool::StopAll()->pthread_cond_broadcast() failed");
    return;
  }

  // 回收
  std::vector<ThreadItem*>::iterator iter;
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); ++iter) {
    pthread_join((*iter)->_Handle, NULL);
  }

  // 所有线程池中的线程都返回了
  pthread_mutex_destroy(&m_pthreadMutex);
  pthread_cond_destroy(&m_pthreadCond);

  // 释放线程资源
  for (iter = m_threadVector.begin(); iter != m_threadVector.end(); ++iter) {
    if (*iter) delete *iter;
  }

  m_threadVector.clear();

  ngx_log_error_core(NGX_LOG_NOTICE, 0, "CThreadPool::StopAll() successful");
  return;
}

/*
 * @ Description: 激发线程
 * @ Paramater: void
 * @ Return: void
 */
void CThreadPool::Call() {
  int err = pthread_cond_signal(&m_pthreadCond);
  if (err != 0)
    ngx_log_error_core(NGX_LOG_ALERT, err,
                       "CThreadPool::Call()->pthread_cond_signal() failed");

  if (m_iThreadNUm == m_iRunningThreadNUm) { /* 不够用了 */
    time_t currTime = time(NULL);
    if (currTime - m_iLastEmgTime > 10) {
      m_iLastEmgTime = currTime; /* 更新时间 */
      ngx_log_error_core(NGX_LOG_EMERG, 0,
                         "CThreadPool::Call() thread number is not enough");
    }
  }

  // ngx_log_error_core(NGX_LOG_DEBUG, 0, "CThreadPool::Call() success");
  return;
}

/*
 * @ Description: 入消息队列，触发条件变量
 * @ Paramater: char *buf(消息队列地址)
 * @ Return: void
 */

void CThreadPool::inMsgRecvQueueAndSingal(char* buf) {
  // mutex
  int err = pthread_mutex_lock(&m_pthreadMutex);
  if (err != 0) {
    ngx_log_error_core(
        NGX_LOG_ERR, err,
        "CThreadPool::inMsgRecvQueueAndSingal()->pthread_mutex_lock() failed");
  }

  m_MsgRecvQueue.push_back(buf); /* 入消息队列 */
  ++m_iRecvMsgQueueCount;

  // unlock
  err = pthread_mutex_unlock(&m_pthreadMutex);
  if (err != 0) {
    ngx_log_error_core(NGX_LOG_ERR, err,
                       "CThreadPool::inMsgRecvQueueAndSingal()->pthread_mutex_"
                       "unlock() failed");
  }

  Call(); /* 唤醒进程 */
  // ngx_log_error_core(NGX_LOG_DEBUG, 0,
  //                    "CThreadPool::inMsgRecvQueueAndSingal() success");
  return;
}

/*
 * @ Description: 清理消息队列
 * @ Paramater: void
 * @ Return: void
 */
void CThreadPool::clearMsgRecvQueue() {
  char* sTmpMempoint;
  CMemory* p_memory = CMemory::GetInstance();

  // 应该不需要互斥了
  while (!m_MsgRecvQueue.empty()) {
    sTmpMempoint = m_MsgRecvQueue.front();
    m_MsgRecvQueue.pop_front();
    p_memory->FreeMemory(sTmpMempoint);
  }
}