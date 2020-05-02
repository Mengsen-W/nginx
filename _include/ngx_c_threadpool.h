/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-02 14:32:37
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-02 16:04:02
 * @Description: 线程池
 */

#ifndef __NGX_C_THREADPOOL_H__
#define __NGX_C_THREADPOOL_H__

#include <pthread.h>

#include <atomic>
#include <vector>

class CThreadPool {
 public:
  CThreadPool();
  ~CThreadPool();

  bool Create(int threadNum);
  void StopAll();
  void Call(int irmqc);

 private:
  static void *ThreadFunc(void *threadData);

  struct ThreadItem {
    pthread_t _Handle;   /* 线程id */
    CThreadPool *_pThis; /* 线程池指针 */
    bool ifrunning;      /* 判断是否启动 */

    ThreadItem(CThreadPool *pThis) : _pThis(pThis), ifrunning(false) {}
    ~ThreadItem(){};
  };

  static pthread_mutex_t m_pthreadMutex; /* 互斥量 */
  static pthread_cond_t m_pthreadCond;   /* 条件变量 */
  static bool m_shutdown;                /* 线程退出 */

  int m_iThreadNUm;                     /* 线程池中线程数量 */
  std::atomic<int> m_iRunningThreadNUm; /* 运行线程数 */

  time_t m_iLastEmgTime; /* 上次发生线程不够用的时间 */

  std::vector<ThreadItem *> m_threadVector; /* 线程容器 */
};

#endif
