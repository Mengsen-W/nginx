/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-30 15:30:32
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-08 09:11:07
 * @Description: 处理网络事件和定时器事件
 */

#include "ngx_global.h"

void ngx_process_events_and_timers() {
  g_socket.ngx_epoll_process_events(-1);
  g_socket.printTDInfo();
}
