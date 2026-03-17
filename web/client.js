// WebSocket 客户端
let ws = null;
let myName = '';
let myUserId = '';

// 生成或获取用户 ID
function getOrCreateUserId() {
    let userId = localStorage.getItem('chatUserId');
    if (!userId) {
        // 生成随机 UUID
        userId = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
            const r = Math.random() * 16 | 0;
            const v = c === 'x' ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
        localStorage.setItem('chatUserId', userId);
        console.log('Generated new userId:', userId);
    } else {
        console.log('Using existing userId:', userId);
    }
    return userId;
}

// 获取保存的用户名
function getSavedName() {
    return localStorage.getItem('chatUserName') || '';
}

// 保存用户名
function saveName(name) {
    localStorage.setItem('chatUserName', name);
    myName = name;
}

// 获取服务器地址
function getServerUrl() {
    // 从当前页面 URL 获取主机名，但端口改为 8080
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const hostname = window.location.hostname;
    return `${protocol}//${hostname}:8080/chat`;
}

// 初始化 WebSocket 连接
function initWebSocket() {
    // 获取或创建用户 ID
    myUserId = getOrCreateUserId();

    const url = getServerUrl();
    console.log('Connecting to:', url, 'with userId:', myUserId);

    ws = new WebSocket(url);

    ws.onopen = function() {
        console.log('WebSocket connected, sending userId...');
        // 获取保存的用户名
        const savedName = getSavedName();
        // 发送用户 ID 和用户名
        ws.send(JSON.stringify({ type: 'auth', userId: myUserId, name: savedName }));
        showConnectionStatus('connected');
    };

    ws.onmessage = function(event) {
        console.log('Received:', event.data);
        handleMessage(event.data);
    };

    ws.onclose = function() {
        console.log('WebSocket disconnected');
        showConnectionStatus('disconnected');
        // 尝试重新连接
        setTimeout(initWebSocket, 3000);
    };

    ws.onerror = function(error) {
        console.error('WebSocket error:', error);
        showConnectionStatus('disconnected');
    };
}

// 显示连接状态
function showConnectionStatus(status) {
    let statusDiv = document.getElementById('connectionStatus');
    if (!statusDiv) {
        statusDiv = document.createElement('div');
        statusDiv.id = 'connectionStatus';
        statusDiv.className = 'connection-status';
        document.body.insertBefore(statusDiv, document.body.firstChild);
    }

    statusDiv.className = 'connection-status ' + status;

    switch(status) {
        case 'connecting':
            statusDiv.textContent = '正在连接...';
            statusDiv.style.display = 'block';
            break;
        case 'connected':
            statusDiv.textContent = '已连接';
            setTimeout(() => { statusDiv.style.display = 'none'; }, 2000);
            break;
        case 'disconnected':
            statusDiv.textContent = '连接已断开，正在重连...';
            statusDiv.style.display = 'block';
            break;
    }
}

// 处理接收到的消息
function handleMessage(data) {
    // 过滤掉 JSON 格式的控制消息（auth 等）
    if (data.startsWith('{') || data.startsWith('[')) {
        console.log('Control message:', data);
        return;
    }

    // 检查是否是用户列表更新
    if (data.startsWith('用户列表:')) {
        const users = JSON.parse(data.substring(5));
        updateUserList(users);
        return;
    }

    // 检查是否是欢迎消息
    if (data.startsWith('欢迎来到聊天室！你的名字是:')) {
        // 从最后一个 : 后面提取名字
        const lastColonIndex = data.lastIndexOf(':');
        myName = data.substring(lastColonIndex + 1).trim();
        console.log('My name:', myName);
        saveName(myName);  // 保存用户名
        updateMyNameDisplay();
        return;  // 不显示欢迎消息到聊天区域
    }

    // 检查名字修改确认
    if (data.startsWith('系统: 你的名字已改为:')) {
        myName = data.split(':')[2].trim();
        console.log('My name updated to:', myName);
        saveName(myName);  // 保存用户名
        updateMyNameDisplay();
    }

    // 检查是否是系统消息（改名等）
    if (data.startsWith('系统:')) {
        addMessage(data.substring(3), 'system');
        return;
    }

    // 解析普通消息格式 "用户名: 消息内容"
    const colonIndex = data.indexOf(':');
    if (colonIndex > 0) {
        const sender = data.substring(0, colonIndex);
        const content = data.substring(colonIndex + 1);

        // 去除消息前可能的空格
        const message = content.startsWith(' ') ? content.substring(1) : content;

        if (sender === myName) {
            addMessage(message, 'self', sender);
        } else {
            addMessage(message, 'other', sender);
        }
    } else {
        addMessage(data, 'other');
    }
}

// 添加消息到界面
function addMessage(text, type, sender, shouldSave = true) {
    const messagesDiv = document.getElementById('messages');
    const messageDiv = document.createElement('div');
    messageDiv.className = 'message ' + type;

    if (type !== 'system' && sender) {
        const senderDiv = document.createElement('div');
        senderDiv.className = 'sender';
        senderDiv.textContent = sender;
        messageDiv.appendChild(senderDiv);
    }

    const contentDiv = document.createElement('div');
    contentDiv.textContent = text;
    messageDiv.appendChild(contentDiv);

    messagesDiv.appendChild(messageDiv);

    // 滚动到底部
    messagesDiv.scrollTop = messagesDiv.scrollHeight;

    // 保存到历史记录
    if (shouldSave) {
        saveToHistory(text, type, sender);
    }
}

// 保存消息到历史记录
function saveToHistory(text, type, sender) {
    if (type === 'system') return;

    const history = JSON.parse(localStorage.getItem('chatHistory') || '[]');
    history.push({ text, type, sender, timestamp: Date.now() });
    localStorage.setItem('chatHistory', JSON.stringify(history));
}

// 加载历史记录
function loadHistory() {
    const history = JSON.parse(localStorage.getItem('chatHistory') || '[]');
    const messagesDiv = document.getElementById('messages');

    // 清空除欢迎消息外的所有消息
    const welcomeMsg = messagesDiv.querySelector('.message.system');
    messagesDiv.innerHTML = '';
    if (welcomeMsg) {
        messagesDiv.appendChild(welcomeMsg);
    }

    // 重新添加历史消息（不重复保存）
    history.forEach(msg => {
        addMessage(msg.text, msg.type, msg.sender, false);
    });
}

// 更新用户列表
function updateUserList(users) {
    const userListDiv = document.getElementById('userList');
    userListDiv.innerHTML = '';

    users.forEach(user => {
        const userItem = document.createElement('div');
        userItem.className = 'user-item';

        // 首字母作为头像
        const avatar = document.createElement('div');
        avatar.className = 'avatar';
        avatar.textContent = user.charAt(0).toUpperCase();

        const name = document.createElement('div');
        name.className = 'name';
        name.textContent = user;

        userItem.appendChild(avatar);
        userItem.appendChild(name);
        userListDiv.appendChild(userItem);
    });
}

// 发送消息
function sendMessage() {
    const input = document.getElementById('messageInput');
    const message = input.value.trim();

    if (message && ws && ws.readyState === WebSocket.OPEN) {
        ws.send(message);
        input.value = '';
    }
}

// 处理回车键
function handleKeyPress(event) {
    if (event.key === 'Enter') {
        sendMessage();
    }
}

// 显示改名输入框
function showRenameInput() {
    const renameArea = document.getElementById('renameArea');
    const renameInput = document.getElementById('renameInput');
    renameInput.value = myName;
    renameArea.style.display = 'flex';
    renameInput.focus();
}

// 确认改名
function changeName() {
    const renameInput = document.getElementById('renameInput');
    const newName = renameInput.value.trim();

    if (newName && newName !== myName && ws && ws.readyState === WebSocket.OPEN) {
        ws.send('/nick ' + newName);
    }

    cancelRename();
}

// 取消改名
function cancelRename() {
    const renameArea = document.getElementById('renameArea');
    renameArea.style.display = 'none';
}

// 清空聊天记录
function clearHistory() {
    if (confirm('确定要清空聊天记录吗？')) {
        localStorage.removeItem('chatHistory');
        // 刷新页面显示
        location.reload();
    }
}

// 更新当前用户名字显示
function updateMyNameDisplay() {
    const nameDisplay = document.getElementById('myNameDisplay');
    if (nameDisplay && myName) {
        nameDisplay.textContent = myName;
    }
}

// 页面加载完成后初始化
document.addEventListener('DOMContentLoaded', function() {
    // 先加载历史记录
    loadHistory();

    initWebSocket();

    // 输入框自动聚焦
    document.getElementById('messageInput').focus();
});
