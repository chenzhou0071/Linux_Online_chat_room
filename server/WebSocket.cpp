#include "WebSocket.h"
#include <cstring>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <sstream>
#include <iostream>

// WebSocket 握手密钥盐值
static const std::string WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Base64 编码
static std::string base64Encode(const unsigned char* input, int length) {
    BIO* bio = nullptr;
    BIO* b64 = nullptr;
    BUF_MEM* bufferPtr = nullptr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

bool WebSocket::isHandshakeRequest(const char* data, int len) {
    if (len < 16) return false;
    // 只要包含 Sec-WebSocket-Key 就认为是握手请求
    return std::string(data, len).find("Sec-WebSocket-Key") != std::string::npos;
}

bool WebSocket::handshake(const char* request, int req_len, std::string& response) {
    std::string req(request, req_len);

    // 查找 Sec-WebSocket-Key
    size_t keyPos = req.find("Sec-WebSocket-Key:");
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t keyStart = keyPos + 18;
    // 跳过空格
    while (keyStart < req.length() && req[keyStart] == ' ') keyStart++;
    size_t keyEnd = req.find("\r\n", keyStart);
    std::string key = req.substr(keyStart, keyEnd - keyStart);

    std::cout << "DEBUG: Sec-WebSocket-Key: " << key << std::endl;

    // 拼接 key + GUID
    std::string acceptKey = key + WS_GUID;

    // SHA1 哈希
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(acceptKey.c_str()), acceptKey.length(), hash);

    // Base64 编码
    std::string accept = base64Encode(hash, SHA_DIGEST_LENGTH);

    std::cout << "DEBUG: Sec-WebSocket-Accept: " << accept << std::endl;

    // 构建响应
    std::ostringstream oss;
    oss << "HTTP/1.1 101 Switching Protocols\r\n"
        << "Upgrade: websocket\r\n"
        << "Connection: Upgrade\r\n"
        << "Sec-WebSocket-Accept: " << accept << "\r\n"
        << "\r\n";

    response = oss.str();
    return true;
}

std::string WebSocket::parseFrame(const char* data, int len, int& out_len) {
    if (len < 2) return "";

    // 解析帧头
    unsigned char first = static_cast<unsigned char>(data[0]);
    unsigned char second = static_cast<unsigned char>(data[1]);

    bool fin = (first & 0x80) != 0;
    int opcode = first & 0x0F;

    std::cout << "DEBUG parseFrame: fin=" << fin << ", opcode=" << opcode << std::endl;

    // 处理关闭帧 (0x08)
    if (opcode == 0x08) {
        std::cout << "DEBUG: Received close frame" << std::endl;
        out_len = 2;  // 至少有帧头
        if (len >= 2) {
            bool masked = (second & 0x80) != 0;
            int payloadLen = second & 0x7F;
            int headerLen = 2;
            if (masked) headerLen += 4;
            if (payloadLen == 126) headerLen += 2;
            else if (payloadLen == 127) headerLen += 8;
            out_len = headerLen + payloadLen;
        }
        return "[CLOSE]";  // 特殊标记，表示客户端想关闭
    }

    // 只处理文本帧 (0x01) 和分片帧的后续部分 (0x00)
    if (opcode != 0x01 && opcode != 0x00) {
        std::cout << "DEBUG: Unsupported opcode: " << opcode << std::endl;
        return "";
    }

    bool masked = (second & 0x80) != 0;
    uint64_t payloadLen = second & 0x7F;

    int headerLen = 2;
    if (payloadLen == 126) {
        if (len < 4) return "";
        payloadLen = (static_cast<unsigned char>(data[2]) << 8) |
                    static_cast<unsigned char>(data[3]);
        headerLen = 4;
    } else if (payloadLen == 127) {
        if (len < 10) return "";
        payloadLen = 0;
        for (int i = 0; i < 8; i++) {
            payloadLen = (payloadLen << 8) | static_cast<unsigned char>(data[2 + i]);
        }
        headerLen = 10;
    }

    // 解码掩码
    char maskingKey[4] = {0};
    if (masked) {
        if (len < headerLen + 4) return "";
        std::memcpy(maskingKey, data + headerLen, 4);
        headerLen += 4;
    }

    if (len < headerLen + payloadLen) {
        return "";
    }

    // 解密消息
    std::string message;
    message.resize(payloadLen);
    for (size_t i = 0; i < payloadLen; i++) {
        message[i] = data[headerLen + i] ^ maskingKey[i % 4];
    }

    out_len = headerLen + payloadLen;
    return message;
}

std::string WebSocket::buildFrame(const std::string& message) {
    std::string frame;

    // 第一个字节: FIN + Opcode (0x01 = text)
    frame.push_back(0x81);

    // 负载长度
    size_t len = message.length();
    if (len <= 125) {
        frame.push_back(static_cast<char>(len));
    } else if (len <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<char>((len >> 8) & 0xFF));
        frame.push_back(static_cast<char>(len & 0xFF));
    } else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
    }

    // 添加负载数据
    frame += message;

    return frame;
}
