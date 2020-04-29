/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-29 21:17:17
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-29 21:22:31
 * @Description: 处理accept
 */

#include <sys/socket.h>
#include <sys/types.h>

#include "ngx_c_socket.h"
/*
 * @ Description: 封装accept
 * @ Parameter: lpngx_connect_t oldc
 * @ Return: void
 */
void CSocket::ngx_event_accept(lpngx_connection_t oldc) {
  struct sockaddr mysockaddr;
  socklen_t socklen;
  int err;
  int level;
  int s;
  static int use_accept4 = 1;
  lpngx_connection_t newc;
}