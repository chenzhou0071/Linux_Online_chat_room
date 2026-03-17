#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <string>
#include <cstdint>

class WebSocket {
public:
    // 处理 WebSocket 握手
    static bool handshake(const char* request, int req_len, std::string& response);

    // 解析 WebSocket 数据帧，返回解析后的消息（空串表示需要更多数据）
    static std::string parseFrame(const char* data, int len, int& out_len);

    // 构建 WebSocket 数据帧
    static std::string buildFrame(const std::string& message);

    // 检查是否是 WebSocket 握手请求
    static bool isHandshakeRequest(const char* data, int len);
};

#endif // WEBSOCKET_H
