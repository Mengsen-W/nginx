/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-02 11:20:04
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-02 11:22:52
 * @Description: 封装mutex lock
 */

#ifndef __NGX_C_LOCKMUTEX_H__
#define __NGX_C_LOCKMUTEX_H__

#include <pthread.h>

class CLock {
 public:
  CLock(pthread_mutex_t *pMutex) : m_pMutex(pMutex) {
    pthread_mutex_lock(m_pMutex);
  }

  ~CLock() { pthread_mutex_unlock(m_pMutex); };

 private:
  pthread_mutex_t *m_pMutex;
};

#endif
