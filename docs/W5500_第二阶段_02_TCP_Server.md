# W5500 第二阶段 Step 2：TCP Server（板子监听端口）

目标：板子作为 TCP Server 监听端口（例如 5001），PC 连接上来后：
- PC 发什么板子回什么（Echo）
- 或板子打印收到的数据

TCP 相比 UDP 的关键差异：
- 有连接建立与断开（状态机必须写好）
- 必须处理 `SOCK_CLOSE_WAIT`、`SOCK_CLOSED` 等边界状态

参考官方 loopback（TCP server 部分）：
- [loopback_tcps](file:///i:/AI/w5500/ioLibrary_Driver/Application/loopback/loopback.c#L17-L93)

## 1. TCP Server 状态机（你必须熟练）

核心就是按 `getSn_SR(sn)` 分支：

- `SOCK_CLOSED`：打开 socket  
  `socket(sn, Sn_MR_TCP, port, 0)`
- `SOCK_INIT`：进入监听  
  `listen(sn)`
- `SOCK_ESTABLISHED`：已建立连接，收发数据  
  `recv()` → `send()`
- `SOCK_CLOSE_WAIT`：对端要断开，执行 `disconnect(sn)` 或 `close(sn)`

注意点：
- `send()`/`recv()` 返回值要检查（<0 是错误）
- `getSn_RX_RSR(sn)` > 0 再 `recv()`，避免空读

## 2. 端口与 socket 选择建议

推荐：
- Socket 号：0（或 1，只要你统一）
- TCP 监听端口：5001

如果你之后还要做 UDP，就把 UDP/TCP 分别用不同 socket（例如 UDP 用 0，TCP 用 1）。

## 3. PC 侧测试方法（选一种）

### 3.1 PowerShell 测 TCP（无需额外软件）

```powershell
$ip="192.168.227.88"
$port=5001
$client = New-Object System.Net.Sockets.TcpClient
$client.Connect($ip,$port)
$stream = $client.GetStream()
$tx = [Text.Encoding]::ASCII.GetBytes("hello-tcp`r`n")
$stream.Write($tx,0,$tx.Length)
$buf = New-Object byte[] 256
$n = $stream.Read($buf,0,$buf.Length)
"RX: " + ([Text.Encoding]::ASCII.GetString($buf,0,$n))
$client.Close()
```

### 3.2 telnet / netcat

- telnet（Windows 需要启用“Telnet 客户端”组件）：  
  `telnet 192.168.227.88 5001`
- ncat：  
  `ncat 192.168.227.88 5001`

## 4. 抓包与判读（Wireshark）

过滤器：
- `tcp.port == 5001`

你应该看到：
- 3 次握手（SYN / SYN-ACK / ACK）
- 数据段（PSH/ACK）来回
- 断开（FIN/ACK）

## 5. 常见问题与快速定位

- PC 连不上（超时/拒绝）：
  - 板子是否真的在 `listen`（串口打印 `getSn_SR`）
  - 端口是否一致（5001）
  - PC 是否走错网卡（多网卡时用 `Test-NetConnection -ComputerName 192.168.227.88 -Port 5001`）
- 连上了但收不到回显：
  - `recv()` 的长度是否正确
  - 是否忘记处理 `SOCK_ESTABLISHED`
  - `send()` 返回值是否为负（错误）
- 断开后无法再次连接：
  - 没处理 `SOCK_CLOSE_WAIT`，导致 socket 卡住
  - 没在 `SOCK_CLOSED` 时重新 `socket()` 打开

## 6. Step 2 验收标准

- PC 能多次连接/断开/重连，板子都能正常 echo
- Wireshark 中能看到完整握手与断开
- 拔网线插回后，板子能恢复监听（允许你检测 link 后 close 并重建）

完成后进入 Step 3：TCP Client。

