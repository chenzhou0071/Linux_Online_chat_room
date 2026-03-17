# Linux Web 聊天室系统

一个基于 Linux C++ 和 WebSocket 的高性能实时聊天室系统，采用前后端分离架构，支持多用户同时在线聊天。

## 项目简介

本项目实现了类似微信的 Web 聊天室界面，后端使用 Linux C++ 的 epoll 高并发模型，前端使用原生 HTML/CSS/JavaScript。系统支持多用户实时聊天、用户改名、在线用户列表等功能。

## 技术栈

### 后端
- **语言**: C++17
- **网络模型**: epoll (边缘触发 ET 模式)
- **并发处理**: 线程池 + 互斥锁
- **通信协议**: WebSocket
- **构建工具**: CMake
- **依赖库**: pthread, ssl, crypto

### 前端
- **页面结构**: HTML5
- **样式设计**: CSS3 (仿微信界面)
- **交互逻辑**: JavaScript (ES6+)
- **核心 API**: WebSocket API

## 项目结构

```
Linux Web Chat room/
├── server/                    # 后端 C++ 代码
│   ├── main.cpp              # 程序入口
│   ├── EpollServer.cpp/h     # epoll 服务器核心
│   ├── WebSocket.cpp/h       # WebSocket 协议处理
│   ├── ThreadPool.cpp/h      # 线程池实现
│   └── Client.h              # 客户端信息管理
├── web/                       # 前端代码
│   ├── index.html            # 聊天室页面
│   ├── style.css             # 样式文件
│   └── client.js             # WebSocket 客户端逻辑
├── docs/                      # 项目文档
│   ├── 基于Linux的Web聊天室系统设计方案文档.md
│   └── 项目整体框架：C++_Linux_+_WebSocket_架构解析.md
├── CMakeLists.txt            # CMake 构建配置
└── README.md                 # 项目说明文档
```

## 环境要求

### 服务端环境
- **操作系统**: Linux (推荐 Ubuntu 20.04+ / CentOS 7+)
- **编译器**: GCC/G++ 7.0+ (支持 C++17)
- **CMake**: 3.10+
- **开发库**:
  - OpenSSL (用于 WebSocket 握手加密)
  - pthread (线程库)

### 客户端环境
- **浏览器**: Chrome 80+, Firefox 75+, Edge 80+ (支持 WebSocket)
- **网络**: 能够访问服务器 IP 和端口

## 安装部署

### 1. 安装系统依赖

**Ubuntu/Debian:**
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev
```

**CentOS/RHEL:**
```bash
sudo yum groupinstall -y "Development Tools"
sudo yum install -y cmake openssl-devel
```

### 2. 编译服务端

```bash
# 进入项目根目录
cd "Linux Web Chat room"

# 创建构建目录
mkdir -p build
cd build

# 编译项目
cmake ..
make

# 编译成功后会生成可执行文件: chat_server
```

### 3. 运行服务端

```bash
# 默认配置运行 (端口 8080, 4个工作线程)
./chat_server

# 自定义端口运行
./chat_server 9000

# 自定义端口和线程数
./chat_server 9000 8
```

### 4. 部署前端

**方式一: 直接打开 (本地测试)**
```bash
# 直接用浏览器打开 web/index.html 文件
```

**方式二: 使用 HTTP 服务器 (推荐)**

使用 Python 快速启动 HTTP 服务器:
```bash
cd web
python3 -m http.server 8000
```

或使用其他 HTTP 服务器 (Nginx/Apache) 将 `web/` 目录部署到网站根目录。

### 5. 配置防火墙

确保服务器防火墙允许相应端口访问:

```bash
# 开放 8080 端口 (WebSocket)
sudo ufw allow 8080/tcp  # Ubuntu/Debian
sudo firewall-cmd --add-port=8080/tcp --permanent  # CentOS/RHEL
sudo firewall-cmd --reload
```

## 使用说明

### 访问聊天室

1. 打开浏览器访问: `http://服务器IP:8000` (如果使用 HTTP 服务器)
2. 或者直接打开 `web/index.html` 文件
3. 系统会自动连接到 WebSocket 服务器 (`ws://服务器IP:8080`)
4. 连接成功后即可开始聊天

### 功能说明

- **发送消息**: 在输入框输入消息后按回车或点击"发送"按钮
- **修改昵称**: 点击"改名"按钮，输入新昵称确认
- **清空记录**: 点击"清空"按钮可以清空本地聊天历史
- **查看在线用户**: 左侧边栏显示当前在线用户列表
- **自动重连**: 网络断开后系统会自动尝试重连

## 系统架构

### 核心设计模式
- **Reactor 模式**: epoll 事件驱动 + 线程池业务处理
- **生产者-消费者模式**: epoll 主线程接收事件，线程池处理业务

### 数据流转
```
用户输入 → JS封装WebSocket帧 → Linux服务器
→ 解析消息 → 线程池处理 → 广播给所有客户端
→ 前端显示消息
```

### 关键特性
- **高并发**: 使用 epoll 边缘触发模式，支持大量并发连接
- **线程安全**: 使用 mutex 保护共享资源，避免数据竞争
- **长连接**: WebSocket 全双工通信，降低服务器负载
- **会话保持**: 前端使用 localStorage 保存用户信息

## 性能指标

- **并发连接**: 支持 50-100+ 并发连接
- **消息延迟**: < 100ms
- **系统稳定性**: 7x24 小时稳定运行，无内存泄漏

## 常见问题

**Q: 连接不上服务器?**
A: 检查防火墙设置，确认端口 8080 已开放；检查服务器 IP 地址是否正确。

**Q: 编译出现 OpenSSL 相关错误?**
A: 确保已安装 libssl-dev (Ubuntu) 或 openssl-devel (CentOS)。

**Q: 前端连接提示失败?**
A: 检查 client.js 中的服务器地址配置，确保与实际服务器 IP 一致。

**Q: 如何修改默认端口?**
A: 修改 server/main.cpp 中的默认端口变量，或在运行时指定端口参数。

## 未来扩展

- [ ] 实现私聊功能 (点对点消息)
- [ ] 消息持久化 (Redis/数据库)
- [ ] 文件传输功能
- [ ] 移动端响应式优化
- [ ] 集成 AI 智能对话机器人
- [ ] 消息加密传输
- [ ] 聊天室分类管理

## 开发文档

详细的设计文档和架构说明请查看 `docs/` 目录:
- `基于Linux的Web聊天室系统设计方案文档.md` - 系统整体设计
- `项目整体框架：C++_Linux_+_WebSocket_架构解析.md` - 技术架构详解

## 贡献指南

欢迎提交 Issue 和 Pull Request！

## 许可证

MIT License

## 联系方式

如有问题或建议，欢迎通过 Issue 联系。
