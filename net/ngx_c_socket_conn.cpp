/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-29 20:42:07
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-29 22:13:08
 * @Description: 连接池相关函数
 */

#include <cstring>

#include "ngx_c_socket.h"
#include "ngx_func.h"
/*
 * @ Description: 从连接池空闲链表中区节点
 * @ Parameter: int sock
 * @ Return: lpngx_connect_t
 */
lpngx_connection_t CSocket::ngx_get_connection(int isock) {
  lpngx_connection_t c = m_pfree_connections;

  if (c == nullptr) {
    ngx_log_stderr(0, "CSocket::ngx_get_connect() failed");
    return nullptr;
  }

  m_pfree_connections = c->data; /* 指向下一个未用节点 */
  --m_free_connection_n;         /* 空闲链表个数减1 */

  uintptr_t instance = c->instance;          /* 保存失效状态信息 */
  uint64_t iCurrsequence = c->iCurrsequence; /* 保存序号 */

  std::memset(c, 0, sizeof(ngx_connection_t)); /* 初始化连接对象 */
  c->fd = isock;                               /* 绑定监听套接字 */

  c->instance = !instance; /* 更换失效位 */
  c->iCurrsequence = iCurrsequence;
  ++(c->iCurrsequence);

  // wev->write = 1;
  return c;
}
