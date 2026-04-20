# W5500 第二阶段 Step 1：UDP Echo（最简单，先跑通）

目标：板子打开一个 UDP 端口（例如 5000），PC 发什么板子回什么（Echo）。  
这是最推荐的第一个网络应用，因为：
- 不需要连接建立/维护（比 TCP 简单）
- 抓包容易（Wireshark 直接看 UDP）
- 最容易验证“收发路径都对”

## 1. 必备知识点（只要这几个）

- UDP 是无连接协议：只需要 `socket -> bind(由 W5500 socket() 的 port 参数完成) -> recvfrom/sendto`
- W5500 的 UDP socket 状态通常是 `SOCK_UDP`
- 收包长度来自 `getSn_RX_RSR(sn)`

参考官方 loopback（UDP server 部分）：
- [loopback_udps](file:///i:/AI/w5500/ioLibrary_Driver/Application/loopback/loopback.c#L189-L220)

## 2. 板子侧实现步骤（建议照顺序）

### Step 2.1 选择 socket 与端口

推荐：
- Socket 号：0（先用一个 socket，简单）
- UDP 端口：5000

### Step 2.2 在主循环里跑一个 UDP 状态机

你可以直接按 loopback 的写法：根据 `getSn_SR(sn)` 判断状态：
- `SOCK_CLOSED`：`socket(sn, Sn_MR_UDP, port, 0)`
- `SOCK_UDP`：`recvfrom()` 读数据，读到后 `sendto()` 回发
- 其他状态：`close(sn)` 或忽略

关键 API（都在 `socket.h`）：
- `socket() / close()`
- `recvfrom() / sendto()`
- `getSn_SR() / getSn_RX_RSR()`

## 3. PC 侧测试（选一种就行）

### 3.1 Wireshark（强烈建议同步开着）

过滤器：
- `udp.port == 5000`

你应该看到：
- PC → 板子：UDP 数据包
- 板子 → PC：同 payload 的 UDP 数据包

### 3.2 PowerShell 发送 UDP（无需额外软件）

把下面脚本复制到 PowerShell 执行（改 IP/端口即可）：

```powershell
$ip="192.168.227.88"
$port=5000
$udp = New-Object System.Net.Sockets.UdpClient
$udp.Client.ReceiveTimeout = 1000
$udp.Connect($ip,$port)
$bytes = [Text.Encoding]::ASCII.GetBytes("hello-udp")
[void]$udp.Send($bytes,$bytes.Length)
$remote = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any,0)
$resp = $udp.Receive([ref]$remote)
"RX from $($remote.Address): " + ([Text.Encoding]::ASCII.GetString($resp))
$udp.Close()
```

### 3.3 netcat/ncat（如果你有）

不同版本命令不同，思路是“发送一包，再监听回包”：
- 监听：`ncat -u -l 5000`
- 发送：`echo hello | ncat -u 192.168.227.88 5000`

## 4. 常见问题与快速定位

- PC 发了包板子没回：
  - Wireshark 看板子是否收到（有没有到达板子 IP 的 UDP 包）
  - 板子串口打印 `getSn_SR()` 是否为 `SOCK_UDP`
  - 确认端口一致（5000）
- 板子回了但 PC 收不到：
  - Windows 防火墙可能拦截入站 UDP（尤其你在 PC 上监听端口时）
  - 换 PowerShell 的 UdpClient 测试通常更直观

## 5. Step 1 验收标准

- 连续发送 100 次 UDP，板子 0 丢包回显
- Wireshark 中能看到 request/reply 成对出现
- 拔网线插回后（PHY 恢复），UDP 能重新正常收发（允许你重启 socket）

完成后进入 Step 2：TCP Server。

