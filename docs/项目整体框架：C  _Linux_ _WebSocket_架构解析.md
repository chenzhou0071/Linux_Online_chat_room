**项目整体框架：C++ Linux + WebSocket 架构解析**

**一、整体框架（一句话版）**

前后端完全分离：

**前端**：HTML + CSS + JS，负责界面、连接、发消息、显示消息

**后端**：Linux C++，基于 epoll + WebSocket，负责连接管理、消息转发

**通信**：全双工 WebSocket（浏览器唯一能用的长连接）

**二、项目整体架构图（可直接放论文）**

Plain Text  
浏览器前端（Windows）  
↓ WebSocket 长连接  
Linux 服务端（C++）  
├─ 网络层：socket + bind + listen + epoll 高并发模型  
├─ 协议层：WebSocket 握手、解包、封包  
├─ 业务层：客户端管理、消息广播、在线列表  
└─ 工具层：字符串处理、线程安全

**三、项目目录框架（真实可落地）**

你最终项目文件夹就是下面这样，非常干净、标准：

Plain Text  
chatroom/  
├── server/ # 后端 C++  
│ ├── main.cpp # 入口：epoll 主循环  
│ ├── Socket.cpp # socket 封装  
│ ├── Epoll.cpp # epoll 管理  
│ ├── WebSocket.cpp # 握手、解包、封包  
│ ├── ClientManager.cpp # 客户端列表、广播  
│ ├── Makefile # 编译脚本  
│ └── util.cpp # 工具函数  
│  
├── web/ # 前端（Windows 直接打开）  
│ ├── index.html # 仿微信聊天界面  
│ ├── style.css # 样式  
│ └── app.js # JS 连接后端、收发消息  
│  
└── readme.md # 使用说明

**四、后端核心框架（最关键）**

**后端整体运行框架**

Plain Text  
1\. 初始化  
socket() → bind() → listen() → epoll_create()  
将监听 fd 加入 epoll  
<br/>2\. 主循环（死循环）  
epoll_wait() 等待事件  
<br/>3\. 事件分两种  
① 新客户端连接  
accept() → 加入 epoll → 加入客户端列表  
<br/>② 已有客户端发消息  
recv() 读数据  
→ 如果是第一次：做 WebSocket 握手  
→ 如果已握手：解析 WebSocket 帧 → 拿到消息  
→ 调用广播函数发给所有人  
<br/>4\. 关闭  
客户端断开 → 从 epoll 删除 → 从列表移除 → close(fd)

**后端模块分工（论文可直接抄）**

**网络模块**

创建 TCP 服务，处理连接、收发数据。

函数：socket、bind、listen、accept、recv、send

**高并发模块（epoll）**

单线程管理所有客户端。

函数：epoll_create、epoll_ctl、epoll_wait

**协议模块（WebSocket）**

浏览器专用，必须做。

功能：握手、解掩码、解析帧、打包消息

**客户端管理模块**

保存所有在线客户端

功能：添加、删除、遍历、广播消息

**工具模块**

字符串、日志、错误处理

**五、前端框架（极简但完整）**

**前端结构**

Plain Text  
页面布局  
左侧：在线用户  
右侧：聊天区域 + 输入框  
<br/>JS 核心逻辑  
连接后端 WebSocket  
监听消息  
发送消息  
渲染气泡（仿微信）

**前端流程**

Plain Text  
1\. 打开网页  
2\. JS 自动连接 ws://服务器IP:端口  
3\. 连接成功 → 进入聊天室  
4\. 输入消息 → 点发送 → ws.send() 发后端  
5\. 收到后端广播 → 页面显示聊天气泡

**六、数据流转框架（最清晰）**

Plain Text  
用户输入消息  
↓  
前端 JS 打包成 WebSocket 帧  
↓  
传到 Linux C++ 服务器  
↓  
服务器解包得到文本  
↓  
服务器遍历所有客户端  
↓  
打包消息 → 发给所有人  
↓  
所有前端收到 → 显示到界面

**七、总结（论文专用精简版）**

本项目采用**前后端分离架构**。

后端基于 Linux 下 C++ 语言，使用 **epoll I/O 多路复用** 实现高并发，通过 **WebSocket 协议** 与浏览器建立全双工长连接，负责客户端管理与消息广播。

前端使用 HTML + CSS + JS 实现仿微信界面，通过原生 WebSocket API 与服务端通信。

整体结构清晰、耦合低、易扩展、高稳定。

|（注：文档部分内容可能由 AI 生成)