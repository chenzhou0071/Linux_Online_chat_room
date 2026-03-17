#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <cstring>
#include <mutex>
#include <queue>

// 客户端结构体
struct Client {
    int fd;                     // 文件描述符
    std::string user_id;       // 用户唯一标识
    std::string name;          // 用户名
    bool is_websocket;         // 是否已完成 WebSocket 握手
    char recv_buf[4096];        // 接收缓冲区
    int recv_len;               // 已接收数据长度

    // 发送队列（线程安全）
    std::queue<std::string> send_queue;
    std::mutex send_mutex;

    Client(int fd_) : fd(fd_), is_websocket(false), recv_len(0) {
        user_id = "";
        name = "";
    }
};

#endif // CLIENT_H
