/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-29 20:42:07
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-30 17:31:23
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

/*
 * @ Description: 从连接池释放
 * @ Parameter: lpngx_connection_t c
 * @ Return: void
 */
void CSocket::ngx_free_connection(lpngx_connection_t c) {
  if (c->ifnewrecvMem == true) {
    CMemory::GetInstance()->FreeMemory(c->pnewMemPointer);
    c->pnewMemPointer = NULL;
    c->ifnewrecvMem = false;  //这行有用？
  }

  c->data = m_pfree_connections;  //回收的节点指向原来串起来的空闲链的链头

  //节点本身也要干一些事
  ++c->iCurrsequence;  //回收后，该值就增加1,以用于判断某些网络事件是否过期【一被释放就立即+1也是有必要的】

  m_pfree_connections = c;  //修改 原来的链头使链头指向新节点
  ++m_free_connection_n;    //空闲连接多1
  return;
}

/*
 * @ Description: 从连接池关闭连接
 * @ Parameter: lpngx_connection_t c
 * @ Return: void
 */
void CSocekt::ngx_close_connection(lpngx_connection_t c) {
  if (close(c->fd) == -1) {
    ngx_log_error_core(NGX_LOG_ALERT, errno,
                       "CSocket::ngx_close_connection()中close(%d)失败!",
                       c->fd);
  }
  c->fd = -1;              //官方nginx这么写，这么写有意义；
  ngx_free_connection(c);  //把释放代码放在最后边，感觉更合适
  return;
}
