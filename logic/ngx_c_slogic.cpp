/*
 * @Author: Mengsen.Wang
 * @Date: 2020-05-03 11:05:49
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-05-07 12:10:40
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
    &CLogicSocket::_HandlePing,     /* 下标0 */
    nullptr,                        /* 下标1 */
    nullptr,                        /* 下标2 */
    nullptr,                        /* 下标3 */
    nullptr,                        /* 下标4 */
    &CLogicSocket::_HandleRegister, /* 下标5 */
    &CLogicSocket::_HandleLogIn,    /* 下标6 */
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

/*
 * @ Description: 处理消息队列
 * @ Paramater: char *pMsgHeader(业务指针--消息头+包头+包体)
 * @ Return: void
 */
void CLogicSocket::threadRecvProcFunc(char *pMsgBuf) {
  LPSTRUC_MSG_HEADER pMsgHeader = (LPSTRUC_MSG_HEADER)pMsgBuf;  //消息头
  LPCOMM_PKG_HEADER pPkgHeader =
      (LPCOMM_PKG_HEADER)(pMsgBuf + m_iLenMsgHeader);  //包头
  void *pPkgBody;                                      //指向包体的指针
  unsigned short pkglen =
      ntohs(pPkgHeader->pkgLen);  //客户端指明的包宽度【包头+包体】

  if (m_iLenPkgHeader == pkglen) {
    //没有包体，只有包头
    if (pPkgHeader->crc32 != 0)  //只有包头的crc值给0
    {
      return;  // crc错，直接丢弃
    }
    pPkgBody = NULL;
  } else {
    //有包体，走到这里
    pPkgHeader->crc32 =
        ntohl(pPkgHeader->crc32);  //针对4字节的数据，网络序转主机序
    pPkgBody = (void *)(pMsgBuf + m_iLenMsgHeader +
                        m_iLenPkgHeader);  //跳过消息头 以及 包头 ，指向包体

    //计算crc值判断包的完整性
    int calccrc = CCRC32::GetInstance()->Get_CRC(
        (unsigned char *)pPkgBody,
        pkglen - m_iLenPkgHeader);  //计算纯包体的crc值
    if (calccrc !=
        pPkgHeader
            ->crc32)  //服务器端根据包体计算crc值，和客户端传递过来的包头中的crc32信息比较
    {
      ngx_log_stderr(
          0,
          "CLogicSocket::threadRecvProcFunc()中CRC错误，丢弃数据!");  //正式代码中可以干掉这个信息
      return;  // crc错，直接丢弃
    }
  }

  //包crc校验OK才能走到这里
  unsigned short imsgCode = ntohs(pPkgHeader->msgCode);  //消息代码拿出来
  lpngx_connection_t p_Conn =
      pMsgHeader->pConn;  //消息头中藏着连接池中连接的指针

  //我们要做一些判断
  //(1)如果从收到客户端发送来的包，到服务器释放一个线程池中的线程处理该包的过程中，客户端断开了，那显然，这种收到的包我们就不必处理了；
  if (p_Conn->iCurrsequence !=
      pMsgHeader
          ->iCurrsequence)  //该连接池中连接以被其他tcp连接【其他socket】占用，这说明原来的
                            //客户端和本服务器的连接断了，这种包直接丢弃不理
  {
    return;  //丢弃不理这种包了【客户端断开了】
  }

  //(2)判断消息码是正确的，防止客户端恶意侵害我们服务器，发送一个不在我们服务器处理范围内的消息码
  if (imsgCode >= AUTH_TOTAL_COMMANDS)  //无符号数不可能<0
  {
    ngx_log_stderr(
        0, "CLogicSocket::threadRecvProcFunc()中imsgCode=%d消息码不对!",
        imsgCode);  //这种有恶意倾向或者错误倾向的包，希望打印出来看看是谁干的
    return;         //丢弃不理这种包【恶意包或者错误包】
  }

  //能走到这里的，包没过期，不恶意，那好继续判断是否有相应的处理函数
  //(3)有对应的消息处理函数吗
  if (statusHandler[imsgCode] ==
      NULL)  //这种用imsgCode的方式可以使查找要执行的成员函数效率特别高
  {
    ngx_log_stderr(
        0,
        "CLogicSocket::threadRecvProcFunc()中imsgCode=%"
        "d消息码找不到对应的处理函数!",
        imsgCode);  //这种有恶意倾向或者错误倾向的包，希望打印出来看看是谁干的
    return;         //没有相关的处理函数
  }

  //一切正确，可以放心大胆的处理了
  //(4)调用消息码对应的成员函数来处理
  (this->*statusHandler[imsgCode])(p_Conn, pMsgHeader, (char *)pPkgBody,
                                   pkglen - m_iLenPkgHeader);
  return;
}

/*
 * @ Description: 处理注册信息
 * @ Paramater: lpngx_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader,
 * char *pPkgBody, unsigned short iBodyLength
 * @ Return: void
 */
bool CLogicSocket::_HandleRegister(lpngx_connection_t pConn,
                                   LPSTRUC_MSG_HEADER pMsgHeader,
                                   char *pPkgBody, unsigned short iBodyLength) {
  if (pPkgBody == nullptr) return false;

  int iRecvLen = sizeof(STRUCT_REGISTER);
  if (iRecvLen != iBodyLength) return false;

  CLock lock(&pConn->logicPorcMutex);

  // 业务逻辑
  LPSTRUCT_REGISTER p_RecvInfo = (LPSTRUCT_REGISTER)pPkgBody;
  p_RecvInfo->iType = ntohl(p_RecvInfo->iType);
  /* 防止客户端发送过来畸形包 */
  p_RecvInfo->username[sizeof(p_RecvInfo->username) - 1] = 0;
  p_RecvInfo->password[sizeof(p_RecvInfo->password) - 1] = 0;
  ngx_log_error_core(NGX_LOG_DEBUG, 0,
                     "CLogicSocket::_HandleRegister() successful");
  // 业务处理结束

  // 服务端回复消息
  LPCOMM_PKG_HEADER pPkgHeader;
  CMemory *p_memory = CMemory::GetInstance();
  CCRC32 *p_crc32 = CCRC32::GetInstance();
  int iSendLen = sizeof(STRUCT_REGISTER);

  // iSendLen = 65000;

  char *p_sendbuf = static_cast<char *>(p_memory->AllocMemory(
      m_iLenMsgHeader + m_iLenPkgHeader + iSendLen, false));
  memcpy(p_sendbuf, pMsgHeader, m_iLenMsgHeader);
  pPkgHeader = (LPCOMM_PKG_HEADER)(p_sendbuf + m_iLenMsgHeader);
  pPkgHeader->msgCode = _CMD_REGISTER;
  pPkgHeader->msgCode = htons(pPkgHeader->msgCode);
  pPkgHeader->pkgLen = htons(m_iLenPkgHeader + iSendLen);
  LPSTRUCT_REGISTER p_sendInfo =
      (LPSTRUCT_REGISTER)(p_sendbuf + m_iLenMsgHeader + m_iLenPkgHeader);
  pPkgHeader->crc32 = p_crc32->Get_CRC((unsigned char *)p_sendInfo, iSendLen);
  pPkgHeader->crc32 = htonl(pPkgHeader->crc32);

  ngx_log_error_core(NGX_LOG_DEBUG, 0,
                     "CLogicSocket::_HandleRegister() begin to send data");
  // send 先不写，防止泄漏
  msgSend(p_sendbuf);
  return true;
}

/*
 * @ Description: 处理登录信息
 * @ Paramater: lpngx_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader,
 * char *pPkgBody, unsigned short iBodyLength
 * @ Return: void
 */
bool CLogicSocket::_HandleLogIn(lpngx_connection_t pConn,
                                LPSTRUC_MSG_HEADER pMsgHeader, char *pPkgBody,
                                unsigned short iBodyLength) {
  if (pPkgBody == nullptr) return false;

  int iRecvLen = sizeof(STRUCT_LOGIN);
  if (iRecvLen != iBodyLength) return false;

  CLock lock(&pConn->logicPorcMutex);

  // 业务逻辑
  LPSTRUCT_LOGIN p_RecvInfo = (LPSTRUCT_LOGIN)pPkgBody;
  /* 防止客户端发送过来畸形包 */
  p_RecvInfo->username[sizeof(p_RecvInfo->username) - 1] = 0;
  p_RecvInfo->password[sizeof(p_RecvInfo->password) - 1] = 0;
  ngx_log_error_core(NGX_LOG_DEBUG, 0,
                     "CLogicSocket::_HandleLogin() successful");
  // 业务处理结束

  // 服务端回复消息
  LPCOMM_PKG_HEADER pPkgHeader;
  CMemory *p_memory = CMemory::GetInstance();
  CCRC32 *p_crc32 = CCRC32::GetInstance();
  int iSendLen = sizeof(STRUCT_LOGIN);
  char *p_sendbuf = static_cast<char *>(p_memory->AllocMemory(
      m_iLenMsgHeader + m_iLenPkgHeader + iSendLen, false));
  memcpy(p_sendbuf, pMsgHeader, m_iLenMsgHeader);
  pPkgHeader = reinterpret_cast<LPCOMM_PKG_HEADER>(p_sendbuf + m_iLenMsgHeader);
  pPkgHeader->msgCode = _CMD_LOGIN;
  pPkgHeader->msgCode = htons(pPkgHeader->msgCode);
  pPkgHeader->pkgLen = htons(pPkgHeader->pkgLen);
  LPSTRUCT_LOGIN p_sendInfo = reinterpret_cast<LPSTRUCT_LOGIN>(
      p_sendbuf + m_iLenMsgHeader + m_iLenPkgHeader);
  pPkgHeader->crc32 = p_crc32->Get_CRC((unsigned char *)p_sendInfo, iSendLen);
  pPkgHeader->crc32 = htonl(pPkgHeader->crc32);
  // send 先不写，防止泄漏
  msgSend(p_sendbuf);
  p_memory->FreeMemory(p_sendbuf);
  return true;
}

/*
 * @ Description: 处理心跳包
 * @ Paramater: lpngx_connection_t pConn, LPSTRUC_MSG_HEADER pMsgHeader,
 * char *pPkgBody, unsigned short iBodyLength
 * @ Return: void
 */
bool CLogicSocket::_HandlePing(lpngx_connection_t pConn,
                               LPSTRUC_MSG_HEADER pMsgHeader, char *pPkgBody,
                               unsigned short iBodyLength) {
  if (iBodyLength != 0) /* 心跳包包体应该为0 */
    return false;

  CLock lock(&pConn->logicPorcMutex);
  pConn->lastPingTime = time(NULL);

  SendNoBodyPkgToClient(pMsgHeader, _CMD_PING);

  ngx_log_error_core(NGX_LOG_DEBUG, 0,
                     "CLogicSocket::_HandlePing() successful");
  return true;
}

/*
 * @ Description: 发送没有包体的数据包
 * @ Paramater: LPSTRUC_MSG_HEADER pMsgHeader, unsigned short iMsgCode
 * @ Returns: void
 */
void CLogicSocket::SendNoBodyPkgToClient(LPSTRUC_MSG_HEADER pMsgHeader,
                                         unsigned short iMsgCode) {
  CMemory *p_memory = CMemory::GetInstance();
  char *p_sendbuf =
      (char *)p_memory->AllocMemory(m_iLenMsgHeader + m_iLenPkgHeader, false);
  char *p_tmpbuf = p_sendbuf;

  memcpy(p_tmpbuf, pMsgHeader, m_iLenMsgHeader);
  p_tmpbuf += m_iLenMsgHeader;

  /* 指向包头 */
  LPCOMM_PKG_HEADER pPkgHeader = (LPCOMM_PKG_HEADER)p_tmpbuf;
  pPkgHeader->msgCode = htons(iMsgCode);
  pPkgHeader->pkgLen = htons(m_iLenPkgHeader);
  pPkgHeader->crc32 = 0;

  msgSend(p_sendbuf);
  return;
}

/*
 * @ Description: 处理心跳包
 * @ Paramater: LPSTRUC_MSG_HEADER tmpmsg, time_t cur_time
 * @ Return: void
 */
void CLogicSocket::procPingTimeOutChecking(LPSTRUC_MSG_HEADER tmpmsg,
                                           time_t cur_time) {
  CMemory *p_memory = CMemory::GetInstance();

  if (tmpmsg->iCurrsequence == tmpmsg->pConn->iCurrsequence) { /* 连接没断 */
    lpngx_connection_t p_Conn = tmpmsg->pConn;
    if (m_ifTimeOutKick == 1) {
      zdClosesocketProc(p_Conn);
    } else if ((cur_time - p_Conn->lastPingTime) >
               (m_iWaitTime * 3 + 10)) { /* 计算方法 */

      //踢出去【如果此时此刻该用户正好断线，则这个socket可能立即被后续上来的连接复用
      //如果真有人这么倒霉，赶上这个点了，那么可能错踢，错踢就错踢】
      ngx_log_error_core(NGX_LOG_INFO, 0, "no ping pack take away");
      zdClosesocketProc(p_Conn);
    }

    // 释放
    p_memory->FreeMemory(tmpmsg);
  } else {
    p_memory->FreeMemory(tmpmsg);
  }
  return;
}