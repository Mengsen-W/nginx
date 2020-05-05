/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-03 11:07:23
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-03 14:41:25
 * @Description: 业务处理类
 */

#ifndef __NGX_C_SLOGIC_H__
#define __NGX_C_SLOGIC_H__

#include "ngx_c_socket.h"

class CLogicSocket : public CSocket {
 public:
  CLogicSocket();
  virtual ~CLogicSocket();

  virtual bool Initialize() override;
  virtual void threadRecvProcFunc(char *pMsgBuf) override;

  bool _HandleRegister(lpngx_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader,
                       char *pPkgBody, unsigned short size);
  bool _HandleLogIn(lpngx_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader,
                    char *pPkgBody, unsigned short size);
};

#endif