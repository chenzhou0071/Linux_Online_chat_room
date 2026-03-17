#ifndef EPOLLSERVER_H
#define EPOLLSERVER_H

#include <map>
#include <string>
#include <mutex>
#include "Client.h"
#include "ThreadPool.h"

class EpollServer {
public:
    EpollServer(int port, int thread_count = 4);
    ~EpollServer();

    void start();
    void stop();

private:
    int port;
    int listen_fd;
    int epoll_fd;
    ThreadPool* thread_pool;

    std::map<int, Client*> clients;
    std::map<std::string, Client*> clients_by_id;  // 根据 user_id 查找用户
    std::mutex clients_mutex;

    int next_user_id;

    void initSocket();
    void setNonBlocking(int fd);
    void addEvent(int fd, uint32_t events);
    void removeEvent(int fd);
    void handleRead(int client_fd);
    void handleWrite(int client_fd);  // 处理写事件（发送队列）

    // 线程池处理函数
    void processMessageLogic(int client_fd, const std::string& message);
    void processAuthMessage(int client_fd, const std::string& message);
    void processChatMessage(int client_fd, const std::string& message);

    // 线程安全的发送和广播
    void sendToClient(int client_fd, const std::string& message);
    void broadcastInPool(const std::string& message, int exclude_fd = -1);
    void broadcastUserListInPool();

    // 原有函数（保留但内部调用线程池版本）
    void broadcast(const std::string& message, int exclude_fd = -1);
    void broadcastUserList();
    std::string getUserListJson();
    Client* findClientByUserId(const std::string& user_id);
};

#endif // EPOLLSERVER_H
