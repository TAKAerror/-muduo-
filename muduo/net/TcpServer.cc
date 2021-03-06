// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include "muduo/net/TcpServer.h"

#include "muduo/base/Logging.h"
#include "muduo/net/Acceptor.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/SocketsOps.h"

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,                   //tcpserver流程如下：创建server时析构函数主要工作：构造Acceptor，Acceptor绑定newConnectionCallback
                     const InetAddress& listenAddr,     //调用start函数，start函数主要：使Acceptor调用lesting函数，（listening作用见Acceptor.cc），epoll/poll开始关注listenfd
                     const string& nameArg,             //listenfd可读时，Acceptor的channel调用handread，handread调用newConnectionCallback。     
                     Option option)                     //newConnectionCallback第一个参数复制connfd(connfd在Acceptor的handread创建），然后创建TCPconnnectionptr conn，将readcallback、wirtecallback、connectionallback等回调函数        
  : loop_(CHECK_NOTNULL(loop)),                         //绑到conn里面的回调函数，然后用runinloop调用conn里的establishconnection；
    ipPort_(listenAddr.toIpPort()),                    //establishconnection创建持有connfd的channel放入poll/epoll等待pollin事件，触发epollin\epollout等事件时调用handread\handwrite\etc..，handread\handwrite\etc...调用绑到conn里面的writecallback\readcallback回调函数;
    name_(nameArg),                                     
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),//PS：：connectioncallback回调函数会在建立连接和接受连接时调用一次
    threadPool_(new EventLoopThreadPool(loop, name_)),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{
  acceptor_->setNewConnectionCallback(
      std::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (auto& item : connections_)
  {
    TcpConnectionPtr conn(item.second);
    item.second.reset();
    conn->getLoop()->runInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
  }
}

void TcpServer::setThreadNum(int numThreads)//设置线程池线程数
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

void TcpServer::start()
{
  if (started_.getAndSet(1) == 0)
  {
    threadPool_->start(threadInitCallback_);//主线程调用start

    assert(!acceptor_->listening());
    loop_->runInLoop(
        std::bind(&Acceptor::listen, get_pointer(acceptor_)));//get_pointer返回原生指针
  }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)//只在主线程调用
{
  loop_->assertInLoopThread();
  EventLoop* ioLoop = threadPool_->getNextLoop();//获取一个与sub线程绑定的loop，如果线程池为空，返回主线程的loop
  char buf[64];
  snprintf(buf, sizeof buf, "-%s#%d", ipPort_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  conn->setCloseCallback(
      std::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
  ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));  
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);
  EventLoop* ioLoop = conn->getLoop();
  ioLoop->queueInLoop(
      std::bind(&TcpConnection::connectDestroyed, conn));
}

