#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "SocketsOps.h"
#include "Callbacks.h"
#include "Buffer.h"
#include "EventLoopThreadPool.h"
#include "Logging.h"

#include <stdio.h>

void defaultConnectionCallback(const TcpConnectionPtr& conn){
    // do some log things
}

void defaultMessageCallback(const TcpConnectionPtr& conn, Buffer*, Timestamp t){
    //
}

TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr, const std::string& nameArg)
    :loop_(loop),
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, 1)),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1),
    threadPool_(new EventLoopThreadPool(loop, nameArg)),
    ipPort_(listenAddr.toIpPort()){
    acceptor_->setNewConnectionCallback(std::bind(&TcpServer::newConnection, this, std::placeholders::_1, std::placeholders::_2));
}

TcpServer::~TcpServer(){
    loop_->assertInLoopThread();
    LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

    for(ConnectionMap::iterator it(connections_.begin()); it != connections_.end(); ++it){
        TcpConnectionPtr conn(it->second);

        it->second.reset();
        conn->getLoop()->runInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
    }
}

void TcpServer::setThreadNum(int numThreads){
    assert(0 <= numThreads);
    threadPool_->setThreadNum(numThreads);
}

void TcpServer::start(){
    if(started_.getAndSet(1) == 0){
        threadPool_->start(threadInitCallback_);

        assert(!acceptor_->listenning());
        loop_->runInLoop(std::bind(&Acceptor::listen, get_pointer(acceptor_)));
    }
}

void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr){
    loop_->assertInLoopThread();
    EventLoop* ioLoop = threadPool_->getNextLoop();
    char buf[32];
    snprintf(buf, sizeof buf, "#%d", nextConnId_);
    ++nextConnId_;
    string connName = name_ + buf;

    LOG_INFO << "TcpServer::newConnection [" << name_
             << "] - new connection [" << connName
             << "] from " << peerAddr.toIpPort();

    InetAddress localAddr(sockets::getLocalAddr(sockfd));

    // TcpConnectionPtr conn(new TcpConnection(ioLoop, connName, sockfd, localAddr, peerAddr));
    TcpConnectionPtr conn = std::make_shared<TcpConnection>(ioLoop, connName, sockfd, localAddr, peerAddr);

    connections_[connName] = conn;
    conn->setConnectionCallback(connectionCallback_);
    conn->setMessageCallback(messageCallback_);
    // unsafe? may be the situation that TcpServer destructs before conn?
    conn->setCloseCallback(std::bind(&TcpServer::removeConnection, this, std::placeholders::_1));
    conn->setWriteCompleteCallback(writeCompleteCallback_);
    // conn->connectEstablished();
    ioLoop->runInLoop(std::bind(&TcpConnection::connectEstablished, conn));
}

void TcpServer::removeConnection(const TcpConnectionPtr& conn){
    // unsafe the same reason as L77?
    // or may double destruct in different thread? L97 double queueInLoop?
    // thus remove duplication is necessary? 
    // however if there exists tasks requring doing duplicate things? 
    // so how to judge and weigh?
    loop_->runInLoop(std::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn){
    loop_->assertInLoopThread();
    LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_ << "] - connection " << conn->name();
    size_t n = connections_.erase(conn->name());
    assert(n == 1);
    (void) n;
    EventLoop* ioLoop = conn->getLoop();
    ioLoop->queueInLoop(std::bind(&TcpConnection::connectDestroyed, conn));
}
