/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-30 17:23:49
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-30 17:25:07
 * @Description: 读事件回调
 */

#include "ngx_c_socket.h"
#include "ngx_func.h"

void CSocket::ngx_wait_request_handler(lpngx_connection_t c) {
  // 先不写
  ngx_log_stderr(errno, "222222222222222");
  return;
}
