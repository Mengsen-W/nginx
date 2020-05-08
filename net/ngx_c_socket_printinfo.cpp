/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-08 08:58:52
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-08 09:17:50
 * @Description: 打印信息
 */

#include "ngx_c_socket.h"
#include "ngx_func.h"
#include "ngx_global.h"

void CSocket::printTDInfo() {
  // return;
  time_t currtime = time(NULL);
  if ((currtime - m_lastprintTime) > 10) { /* 超过10秒打印一次 */
    int tmprmqc = g_threadpool.getRecvMsgQueueCount(); /* 收消息队列 */

    // 不能直接打印原子类型
    m_lastprintTime = currtime;
    int tmpoLUC = m_onlineUserCount;
    int tmpsmqc = m_iSendMsgQueueCount;
    ngx_log_stderr(0, "begin--------------------------------------");
    ngx_log_stderr(0, "当前在线人数/总人数(%d/%d)。", tmpoLUC,
                   m_worker_connections);
    ngx_log_stderr(0, "连接池中空闲连接/总连接/要释放的连接(%d/%d/%d)。",
                   m_freeconnectionList.size(), m_connectionList.size(),
                   m_recyconnectionList.size());
    ngx_log_stderr(0, "当前时间队列大小(%d)。", m_timerQueuemap.size());
    ngx_log_stderr(0,
                   "当前收消息队列/发消息队列大小分别为(%d/"
                   "%d)，丢弃的待发送数据包数量为%d。",
                   tmprmqc, tmpsmqc, m_iDiscardSendPkgCount);
    if (tmprmqc > 100000) {
      //接收队列过大，报一下，这个属于应该 引起警觉的，考虑限速等等手段
      ngx_log_stderr(0,
                     "接收队列条目数量过大(%d)"
                     "，要考虑限速或者增加处理线程数量了！！！！！！",
                     tmprmqc);
    }
    ngx_log_stderr(0, "end--------------------------------------");
  }
  return;
}