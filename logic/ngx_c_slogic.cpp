/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-03 11:05:49
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-03 15:52:22
 * @Description: 业务处理类
 */

#include "ngx_c_slogic.h"

#include <arpa/inet.h>

#include <cstring>

#include "ngx_c_crc32.h"
#include "ngx_c_lockmutex.h"
#include "ngx_func.h"
#include "ngx_logiccomm.h"
#include "ngx_macro.h"

/* 回调处理函数指针 */
typedef bool (CLogicSocket::*handler)(lpngx_connection_t pConn,
                                      LPSTRUC_MSG_HEADER pMsgHeader,
                                      char *pPkgBody,
                                      unsigned short iBodyLength);

/* 回调函数类别 */
static const handler statusHandler[] = {
    nullptr,                        /* 下标0 */
    nullptr,                        /* 下标1 */
    nullptr,                        /* 下标2 */
    nullptr,                        /* 下标3 */
    nullptr,                        /* 下标4 */
    &CLogicSocket::_HandleRegister, /* 下标5 */
    &CLogicSocket::_HandleLogin,    /* 下标6 */
};

/* 函数指针总数 编译期绑定 */
#define AUTH_TOTAL_COMMANDS sizeof(statusHandler) / sizeof(handler)

/*
 * @ Description: 构造函数
 */
CLogicSocket::CLogicSocket() {}

/*
 * @ Description: 析构函数
 */
CLogicSocket::~CLogicSocket() {}

/*
 * @ Description: 初始化函数
 * @ Parameter: void
 * @ Return: bool
 */
bool CLogicSocket::Initialize() {
  bool bParentInit = CSocket::Initialize();
  return bParentInit;
}

// 临时业务注册函数
bool CLogicSocket::_HandleRegister(lpngx_connection_t pConn,
                                   LPSTRUC_MSG_HEADER pMsgHeader,
                                   char *pPkgBody, unsigned short iBodyLength) {
  ngx_log_error_core(NGX_LOG_DEBUG, 0,
                     "CLogicSocket::_HandleRegister() successful");
  return true;
}

// 临时业务登录
bool CLogicSocket::_HandleLogin(lpngx_connection_t pConn,
                                LPSTRUC_MSG_HEADER pMsgHeader, char *pPkgBody,
                                unsigned short iBodyLength) {
  if (pPkgBody == nullptr) return false;

  int iRecvLen = sizeof(STRUCT_REGISTER);
  if (iRecvLen != iBodyLength) return false;

  CLock lock(&pConn->logicPorcMutex);

  LPSTRUCT_REGISTER p_RecvInfo = reinterpret_cast<LPSTRUCT_REGISTER>(pPkgBody);

  // 业务逻辑
  ngx_log_error_core(NGX_LOG_DEBUG, 0,
                     "CLogicSocket::_HandleLogin() successful");
  // 业务处理结束

  // 服务端回复消息
  LPCOMM_PKG_HEADER pPkgHeader;
  CMemory *p_memory = CMemory::GetInstance();
  CCRC32 *p_crc32 = CCRC32::GetInstance();
  int iSendLen = sizeof(STRUCT_REGISTER);
  char *p_sendbuf = static_cast<char *>(p_memory->AllocMemory(
      m_iLenMsgHeader + m_iLenPkgHeader + iSendLen, false));
  memcpy(p_sendbuf, pMsgHeader, m_iLenMsgHeader);
  pPkgHeader = reinterpret_cast<LPCOMM_PKG_HEADER>(p_sendbuf + m_iLenMsgHeader);
  pPkgHeader->msgCOde = _CMD_LOGIN;
  pPkgHeader->msgCOde = htons(pPkgHeader->msgCOde);
  pPkgHeader->pkgLen = htons(pPkgHeader->pkgLen);
  LPSTRUCT_REGISTER p_sendInfo = reinterpret_cast<LPSTRUCT_REGISTER>(
      p_sendbuf + m_iLenMsgHeader + m_iLenPkgHeader);
  pPkgHeader->crc32 = p_crc32->Get_CRC((unsigned char *)p_sendInfo, iSendLen);
  pPkgHeader->crc32 = htonl(pPkgHeader->crc32);
  return true;
}

/*
 * @ Description: 处理消息队列
 * @ Paramater: char *pMsgHeader(业务指针--消息头+包头+包体)
 * @ Return: void
 */
void CLogicSocket::threadRecvProcFunc(char *pMsgBuf) {
  LPSTRUC_MSG_HEADER pMsgHeader =
      reinterpret_cast<LPSTRUC_MSG_HEADER>(pMsgBuf); /* 消息头*/
  LPCOMM_PKG_HEADER pPkgHeader =
      reinterpret_cast<LPCOMM_PKG_HEADER>(pMsgBuf + m_iLenMsgHeader); /* 包头 */
  void *pPkgBody; /* 指向包体的指针 */
  unsigned short pkgLen =
      ntohs(pPkgHeader->pkgLen);  //客户端指明的包宽度【包头+包体】

  if (m_iLenPkgHeader == pkgLen) { /* 没有包体 */

    if (pPkgHeader->crc32 != 0) return; /* crc错误 */
    pPkgBody = nullptr;

  } else { /* 有包体 */

    pPkgHeader->crc32 = ntohl(pPkgHeader->crc32);
    pPkgBody = static_cast<void *>(pMsgBuf + m_iLenMsgHeader + m_iLenPkgHeader);
    int calccrc = CCRC32::GetInstance()->Get_CRC((unsigned char *)pPkgBody,
                                                 pkgLen - m_iLenPkgHeader);

    if (calccrc != pPkgHeader->crc32) { /* 校验错误 */
      ngx_log_error_core(NGX_LOG_WARN, 0,
                         "CLogicSocket::threadRecvProcFunc()->CRC error");
      return;
    }
  }

  unsigned short iMsgCode = ntohs(pPkgHeader->msgCOde); /* 业务代码 */

  lpngx_connection_t p_Conn = pMsgHeader->pConn; /* 连接池指针 */

  if (p_Conn->iCurrsequence != pMsgHeader->iCurrsequence) { /* 客户端意外终止 */
    /* 比较发送过来的和连接单元的序号判断是否为废包 */
    ngx_log_error_core(
        NGX_LOG_NOTICE, 0,
        "CLogicSocket::threadRecvProcFunc()-> client terminate unexpectedly");
    return; /* 丢弃 */
  }

  if (iMsgCode >= AUTH_TOTAL_COMMANDS) { /* 业务代码错误 */

    ngx_log_error_core(NGX_LOG_WARN, 0,
                       "CLogicSocket::threadRecvProcFunc()-> client link "
                       "spamming [MsgCode = %d]",
                       iMsgCode);
    return; /* 拒收丢弃 */
  }

  if (statusHandler[iMsgCode] == nullptr) { /* 没有相关处理函数 */
    ngx_log_error_core(NGX_LOG_INFO, 0,
                       "CLogicSocket::statusHandler[] no found call back "
                       "function[MsgCode = %d]",
                       iMsgCode);
    return; /* 丢弃 */
  }

  (this->*(statusHandler[iMsgCode]))(p_Conn, pMsgHeader,
                                     static_cast<char *>(pPkgBody),
                                     pkgLen - m_iLenPkgHeader);

  return;
}
