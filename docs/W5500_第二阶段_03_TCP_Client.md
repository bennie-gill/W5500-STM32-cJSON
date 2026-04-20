# W5500 第二阶段 Step 3：TCP Client（板子主动连接）

目标：板子作为 TCP Client，主动连接 PC 或服务器（例如 PC 的 5002 端口），连接成功后发送数据、接收响应，并能在断线后自动重连。

参考官方 loopback（TCP client 部分）：
- [loopback_tcpc](file:///i:/AI/w5500/ioLibrary_Driver/Application/loopback/loopback.c#L96-L186)

## 1. TCP Client 状态机（核心）

与 TCP Server 的差别：
- `SOCK_INIT` 阶段是 `connect()`，不是 `listen()`

典型分支：

- `SOCK_CLOSED`：打开 socket（客户端端口可用任意端口）  
  `socket(sn, Sn_MR_TCP, any_port, 0)`
- `SOCK_INIT`：发起连接  
  `connect(sn, destip, destport)`
- `SOCK_ESTABLISHED`：收发数据  
  `send()` / `recv()`
- `SOCK_CLOSE_WAIT`：断开并回到 closed  
  `disconnect(sn)` 或 `close(sn)`

## 2. PC 端准备一个 TCP Server（推荐用 PowerShell）

用 PowerShell 起一个简单的 TCP server（收到后回显）：

```powershell
$port=5002
$listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Any,$port)
$listener.Start()
"LISTEN $port"
$client = $listener.AcceptTcpClient()
"CONNECTED"
$stream = $client.GetStream()
$buf = New-Object byte[] 1024
while($true){
  $n = $stream.Read($buf,0,$buf.Length)
  if($n -le 0){ break }
  $stream.Write($buf,0,$n)
  "RX: " + ([Text.Encoding]::ASCII.GetString($buf,0,$n))
}
$client.Close()
$listener.Stop()
```

PC 的 IP 要能被板子访问到（例如 PC 以太网 IP 为 `192.168.227.10`）。

## 3. 板子侧参数建议

建议你固定几个参数，方便抓包与排障：
- Socket：1（避免和 UDP/TCP server 混用）
- 目标 IP：PC 的以太网 IP，例如 `192.168.227.10`
- 目标端口：5002
- 客户端本地端口：50000 起递增（参考 loopback）

## 4. 断线重连策略（最实用）

你要解决的实际问题是：
- PC 端 server 重启/断网/拔线后，板子不能永远卡死

推荐策略（非阻塞写法）：
- 维护一个 `next_retry_ms`，到点才尝试 connect
- 检测 `SOCK_CLOSED` 或 `SOCK_CLOSE_WAIT`，进入重连流程
- Link Down 时直接 `close(sn)`，Link Up 后重新 `socket()` + `connect()`

## 5. 抓包与判读（Wireshark）

过滤器：
- `tcp.port == 5002`

你应该看到：
- 由板子发起 SYN 到 PC
- 连接建立后，板子发 payload，PC 回显

## 6. Step 3 验收标准

- PC 起/停 server，板子都能在合理时间内自动重连
- Wireshark 能看到多次连接建立与断开过程
- 板子不会卡死在某个状态（例如一直 SOCK_INIT 不动）

完成后进入 Step 4：抓包与定位（把问题定位能力补齐）。

