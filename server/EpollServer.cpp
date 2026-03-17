#include "EpollServer.h"
#include "WebSocket.h"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

EpollServer::EpollServer(int port, int thread_count)
    : port(port), listen_fd(-1), epoll_fd(-1), thread_pool(nullptr), next_user_id(1) {
    thread_pool = new ThreadPool(thread_count);
}

EpollServer::~EpollServer() {
    stop();
    delete thread_pool;
}

void EpollServer::initSocket() {
    // 创建监听套接字
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(1);
    }

    // 设置端口复用
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 绑定地址
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        exit(1);
    }

    // 监听
    if (listen(listen_fd, 128) < 0) {
        perror("listen");
        exit(1);
    }

    // 设置非阻塞
    setNonBlocking(listen_fd);

    // 创建 epoll
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        exit(1);
    }

    // 添加监听fd到epoll
    addEvent(listen_fd, EPOLLIN);

    std::cout << "Server listening on port " << port << std::endl;
}

void EpollServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void EpollServer::addEvent(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl add");
    }
}

void EpollServer::removeEvent(int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr);
    close(fd);
}

void EpollServer::start() {
    initSocket();

    struct epoll_event events[1024];
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, 1024, -1);
        if (nfds < 0) {
            perror("epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // 处理新连接 (ET模式需要循环accept)
                while (true) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);

                    if (client_fd < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break;  // 没有更多连接
                        }
                        perror("accept");
                        break;
                    }

                    // 设置为非阻塞
                    int flags = fcntl(client_fd, F_GETFL, 0);
                    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

                    addEvent(client_fd, EPOLLIN | EPOLLOUT | EPOLLET);

                    // 创建客户端（等待用户 ID 消息）
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    Client* client = new Client(client_fd);
                    client->name = "";  // 初始为空，等收到 user_id 后分配
                    clients[client_fd] = client;

                    std::cout << "New client connected (fd=" << client_fd << "), waiting for auth..." << std::endl;
                }
            } else {
                // 处理客户端数据
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    std::cout << "Client error/hup, fd: " << fd << ", events: " << events[i].events << std::endl;
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    auto it = clients.find(fd);
                    if (it != clients.end()) {
                        std::cout << "Removing client due to error/hup: " << it->second->name << std::endl;
                        // 从 clients_by_id 中移除
                        if (!it->second->user_id.empty()) {
                            clients_by_id.erase(it->second->user_id);
                        }
                        delete it->second;
                        clients.erase(it);
                    }
                    removeEvent(fd);
                    broadcastUserListInPool();  // ← 添加这行！
                    continue;
                }

                if (events[i].events & EPOLLIN) {
                    handleRead(fd);
                }

                // 处理写事件（发送队列中的数据）
                if (events[i].events & EPOLLOUT) {
                    handleWrite(fd);
                }

            }
        }
    }
}

void EpollServer::stop() {
    if (listen_fd >= 0) close(listen_fd);
    if (epoll_fd >= 0) close(epoll_fd);

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& pair : clients) {
        close(pair.first);
        delete pair.second;
    }
    clients.clear();
}

void EpollServer::handleRead(int client_fd) {
    char buffer[4096];

    // 循环读取（ET 模式需要）
    while (true) {
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0) {
            if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                // 没有更多数据
                return;
            }
            // 客户端断开
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = clients.find(client_fd);
            if (it != clients.end()) {
                std::cout << "Client disconnected: " << it->second->name << std::endl;
                // 从 clients_by_id 中移除
                if (!it->second->user_id.empty()) {
                    clients_by_id.erase(it->second->user_id);
                }
                delete it->second;
                clients.erase(it);
            }
            removeEvent(client_fd);
            broadcastUserListInPool();
            return;
        }

        buffer[n] = '\0';

        // 调试：打印收到的原始数据
        std::cout << "Raw request (" << n << " bytes):\n" << std::string(buffer, n) << std::endl;

        // 检查客户端是否存在
        bool is_websocket = false;
        int client_fd_for_msg = client_fd;

        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = clients.find(client_fd);
            if (it == clients.end()) {
                return;
            }
            Client* client = it->second;
            is_websocket = client->is_websocket;
        }

        // 如果还没有完成 WebSocket 握手
        if (!is_websocket) {
            // 检查是否是 HTTP 请求
            if (WebSocket::isHandshakeRequest(buffer, n)) {
                // 先获取锁更新状态，然后释放锁再发送
                {
                    std::lock_guard<std::mutex> lock(clients_mutex);
                    auto it = clients.find(client_fd);
                    if (it == clients.end()) return;
                    it->second->is_websocket = true;
                }

                std::cout << "Received handshake request from: " << client_fd << std::endl;
                std::string response;
                if (WebSocket::handshake(buffer, n, response)) {
                    std::cout << "Sending handshake response, length: " << response.length() << std::endl;

                    // 循环发送确保完整发送
                    ssize_t total_sent = 0;
                    ssize_t remaining = response.length();
                    const char* ptr = response.c_str();
                    while (total_sent < remaining) {
                        ssize_t sent = send(client_fd, ptr + total_sent, remaining - total_sent, 0);
                        if (sent < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                continue;
                            }
                            std::cout << "Send error: " << strerror(errno) << std::endl;
                            break;
                        }
                        total_sent += sent;
                    }
                    std::cout << "Sent " << total_sent << " bytes total" << std::endl;
                    std::cout << "Response:\n" << response << std::endl;
                    std::cout << "WebSocket handshake done" << std::endl;

                    // 握手完成后广播用户列表
                    broadcastUserListInPool();
                }
            }
            return;
        }

        // 解析 WebSocket 帧（在锁外）
        std::cout << "Parsing WebSocket frame, n=" << n << std::endl;
        int processed = 0;
        while (processed < n) {
            int out_len = 0;
            std::string message = WebSocket::parseFrame(buffer + processed, n - processed, out_len);

            if (out_len == 0) {
                break;
            }

            processed += out_len;
            std::cout << "Parsed message: '" << message << "', out_len=" << out_len << std::endl;

            if (!message.empty()) {
                // 检查是否是关闭帧
                if (message == "[CLOSE]") {
                    std::cout << "Client requested close, fd: " << client_fd << std::endl;
                    // 清理客户端
                    {
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        auto it = clients.find(client_fd);
                        if (it != clients.end()) {
                            std::cout << "Removing client due to close frame: " << it->second->name << std::endl;
                            if (!it->second->user_id.empty()) {
                                clients_by_id.erase(it->second->user_id);
                            }
                            delete it->second;
                            clients.erase(it);
                        }
                    }
                    removeEvent(client_fd);
                    broadcastUserListInPool();
                    return;
                }

                // 提交到线程池处理（不阻塞主线程）
                std::cout << "Enqueue message to thread pool: " << message << std::endl;
                thread_pool->enqueue([this, client_fd, message]() {
                    processMessageLogic(client_fd, message);
                });
            }
        }
    }
}

void EpollServer::broadcast(const std::string& message, int exclude_fd) {
    std::string frame = WebSocket::buildFrame(message);

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& pair : clients) {
        // 发送给所有已握手的客户端
        if (pair.second->is_websocket && pair.first != exclude_fd) {
            std::cout << "Sending to fd: " << pair.first << std::endl;
            ssize_t sent = send(pair.first, frame.c_str(), frame.length(), 0);
            std::cout << "Sent result: " << sent << ", errno: " << errno << std::endl;
        }
    }
    std::cout << "Broadcast completed" << std::endl;
}

// 注意：调用此函数时需要持有 clients_mutex 锁
void EpollServer::broadcastUserList() {
    std::string json = getUserListJson();
    std::string frame = WebSocket::buildFrame(json);

    for (auto& pair : clients) {
        if (pair.second->is_websocket) {
            send(pair.first, frame.c_str(), frame.length(), 0);
        }
    }
}

// 注意：调用此函数时需要持有 clients_mutex 锁
std::string EpollServer::getUserListJson() {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (auto& pair : clients) {
        if (!first) oss << ",";
        oss << "\"" << pair.second->name << "\"";
        first = false;
    }
    oss << "]";

    return std::string("用户列表:") + oss.str();
}

// 根据 user_id 查找客户端（需要在持有锁时调用）
Client* EpollServer::findClientByUserId(const std::string& user_id) {
    for (auto& pair : clients) {
        if (pair.second->user_id == user_id) {
            return pair.second;
        }
    }
    return nullptr;
}

// 处理写事件（发送队列中的数据）
void EpollServer::handleWrite(int client_fd) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = clients.find(client_fd);
    if (it == clients.end()) return;

    Client* client = it->second;
    std::lock_guard<std::mutex> send_lock(client->send_mutex);

    while (!client->send_queue.empty()) {
        const std::string& data = client->send_queue.front();
        ssize_t sent = send(client_fd, data.c_str(), data.length(), 0);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 发送缓冲区满，下次再发
                return;
            }
            // 发送错误，关闭连接
            delete client;
            clients.erase(it);
            removeEvent(client_fd);
            return;
        }

        client->send_queue.pop();
    }
}

// 线程池中处理消息的入口
void EpollServer::processMessageLogic(int client_fd, const std::string& message) {
    std::cout << "[Thread " << std::this_thread::get_id() << "] Processing message: " << message << std::endl;

    // 检查是否是 JSON 认证消息
    if (message.length() > 0 && message[0] == '{') {
        processAuthMessage(client_fd, message);
    } else {
        processChatMessage(client_fd, message);
    }
}

// 处理认证消息（在线程池中执行）
void EpollServer::processAuthMessage(int client_fd, const std::string& message) {
    size_t typePos = message.find("\"type\":\"auth\"");
    if (typePos == std::string::npos) return;

    size_t userIdPos = message.find("\"userId\":");
    if (userIdPos == std::string::npos) return;

    size_t start = message.find("\"", userIdPos + 9);
    while (start < message.length() && message[start] == ' ') start++;
    size_t end = message.find("\"", start + 1);
    if (start == std::string::npos || end == std::string::npos) return;

    std::string user_id = message.substr(start + 1, end - start - 1);

    // 解析名字
    std::string client_provided_name;
    size_t namePos = message.find("\"name\":");
    if (namePos != std::string::npos) {
        size_t nameStart = message.find("\"", namePos + 7);
        while (nameStart < message.length() && message[nameStart] == ' ') nameStart++;
        size_t nameEnd = message.find("\"", nameStart + 1);
        if (nameStart != std::string::npos && nameEnd != std::string::npos) {
            client_provided_name = message.substr(nameStart + 1, nameEnd - nameStart - 1);
        }
    }

    // 查找并更新客户端（需要加锁）
    std::string client_name;
    {
        std::lock_guard<std::mutex> lock(clients_mutex);
        auto it = clients.find(client_fd);
        if (it == clients.end()) return;

        Client* existing = findClientByUserId(user_id);
        if (existing && existing->fd != client_fd) {
            // 删除旧连接
            std::cout << "Removing old connection for userId: " << user_id << ", old fd: " << existing->fd << std::endl;
            clients_by_id.erase(user_id);  // 关键：从 clients_by_id 中移除
            delete existing;
            clients.erase(existing->fd);
        }

        it->second->user_id = user_id;

        if (!client_provided_name.empty()) {
            it->second->name = client_provided_name;
        } else if (it->second->name == "访客" || it->second->name.empty()) {
            it->second->name = "用户" + std::to_string(next_user_id++);
        }

        client_name = it->second->name;

        // 加入 clients_by_id 映射
        clients_by_id[user_id] = it->second;
    }

    // 发送欢迎消息（线程安全）
    std::string welcome = "欢迎来到聊天室！你的名字是: " + client_name;
    sendToClient(client_fd, welcome);

    // 广播用户列表（在线程池中执行）
    broadcastUserListInPool();
}

// 处理聊天消息（在线程池中执行）
void EpollServer::processChatMessage(int client_fd, const std::string& message) {
    // 检查是否是 /nick 命令
    if (message.length() > 6 && message.substr(0, 5) == "/nick") {
        std::string new_name = message.substr(6);
        size_t start = new_name.find_first_not_of(" \t");
        size_t end = new_name.find_last_not_of(" \t");

        if (start != std::string::npos) {
            new_name = new_name.substr(start, end - start + 1);

            std::string old_name;
            {
                std::lock_guard<std::mutex> lock(clients_mutex);
                auto it = clients.find(client_fd);
                if (it == clients.end()) return;
                old_name = it->second->name;
                it->second->name = new_name;
            }

            // 发送确认
            sendToClient(client_fd, "系统: 你的名字已改为: " + new_name);

            // 广播改名消息
            std::string rename_msg = "系统: " + old_name + " 改名为 " + new_name;
            broadcastInPool(rename_msg);
            broadcastUserListInPool();
        }
    } else {
        // 普通消息
        std::string client_name;
        {
            std::lock_guard<std::mutex> lock(clients_mutex);
            auto it = clients.find(client_fd);
            if (it == clients.end()) return;
            client_name = it->second->name;
        }

        std::string full_msg = client_name + ": " + message;
        broadcastInPool(full_msg);
    }
}

// 线程安全的发送函数（加入队列并立即尝试发送）
void EpollServer::sendToClient(int client_fd, const std::string& message) {
    std::string frame = WebSocket::buildFrame(message);

    std::lock_guard<std::mutex> lock(clients_mutex);
    auto it = clients.find(client_fd);
    if (it == clients.end()) return;

    Client* client = it->second;
    std::lock_guard<std::mutex> send_lock(client->send_mutex);

    client->send_queue.push(frame);

    // 立即尝试发送，不要只依赖 EPOLLOUT 事件
    while (!client->send_queue.empty()) {
        const std::string& data = client->send_queue.front();
        ssize_t sent = send(client_fd, data.c_str(), data.length(), 0);

        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 发送缓冲区满，等待下次 EPOLLOUT 事件
                return;
            }
            // 发送错误，标记客户端为无效状态
            // 注意：不在这里删除，让主线程的 recv 返回 0 来清理
            std::cout << "Send error to fd " << client_fd << ", errno: " << errno << std::endl;
            client->send_queue.pop();
            return;  // 直接返回，下次 recv 会检测到连接断开
        }

        client->send_queue.pop();
    }
}

// 在线程池中广播消息
void EpollServer::broadcastInPool(const std::string& message, int exclude_fd) {
    std::string frame = WebSocket::buildFrame(message);

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& pair : clients) {
        if (pair.second->is_websocket && pair.first != exclude_fd) {
            std::lock_guard<std::mutex> send_lock(pair.second->send_mutex);
            pair.second->send_queue.push(frame);

            // 立即尝试发送
            while (!pair.second->send_queue.empty()) {
                const std::string& data = pair.second->send_queue.front();
                ssize_t sent = send(pair.first, data.c_str(), data.length(), 0);

                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }
                    // 发送失败，清空队列继续下一个客户端
                    std::cout << "Broadcast failed to fd " << pair.first << std::endl;
                    while (!pair.second->send_queue.empty()) {
                        pair.second->send_queue.pop();
                    }
                    break;
                }
                pair.second->send_queue.pop();
            }
        }
    }
}

// 在线程池中广播用户列表
void EpollServer::broadcastUserListInPool() {
    std::string json = getUserListJson();
    std::string frame = WebSocket::buildFrame(json);

    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& pair : clients) {
        if (pair.second->is_websocket) {
            std::lock_guard<std::mutex> send_lock(pair.second->send_mutex);
            pair.second->send_queue.push(frame);

            // 立即尝试发送
            while (!pair.second->send_queue.empty()) {
                const std::string& data = pair.second->send_queue.front();
                ssize_t sent = send(pair.first, data.c_str(), data.length(), 0);

                if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        return;
                    }
                    // 发送失败，清空队列继续下一个客户端
                    std::cout << "BroadcastUserList failed to fd " << pair.first << std::endl;
                    while (!pair.second->send_queue.empty()) {
                        pair.second->send_queue.pop();
                    }
                    break;
                }
                pair.second->send_queue.pop();
            }
        }
    }
}
