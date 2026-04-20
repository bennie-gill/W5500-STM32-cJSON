# W5500 MQTT 全流程使用指南：从初始化到 MQTTYield 维护

本指南详细说明了如何在 W5500 上实现 MQTT 通信。核心点在于弃用复杂的中断累加计时，转而使用 `HAL_GetTick()` 适配底层接口，并通过 `MQTTYield` 函数统一管理所有协议定时任务（如心跳保持、超时重连等）。

---

## 1. 核心定时机制：MQTTYield 的作用

在 Paho MQTT 库中，`MQTTYield` 是整个协议栈的“心脏”。它负责：
1.  **心跳维持**：自动计算 `Keep-alive` 时间，到期发送 `PINGREQ`。
2.  **数据接收**：在设定的 `timeout` 时间内轮询 Socket，解析收到的 `PUBLISH` 消息。
3.  **ACK 处理**：处理订阅确认、发布确认等控制报文。

**移植关键**：底层 `mqtt_interface.c` 必须能正确通过 `HAL_GetTick()` 返回当前时间，`MQTTYield` 才能计算出正确的倒计时。

---

## 2. 第一阶段：底层接口适配 (mqtt_interface.c)

首先，我们需要在 [mqtt_interface.c](file:///i:/AI/w5500/ioLibrary_Driver/Internet/MQTT/mqtt_interface.c) 中实现基于 `HAL_GetTick()` 的计时函数。

```c
/* mqtt_interface.c */
#include "main.h" // 必须包含 HAL 库头文件

// 1. 判断定时器是否到期
char TimerIsExpired(Timer *timer) {
    return (HAL_GetTick() >= timer->end_time);
}

// 2. 设置倒计时 (ms)
void TimerCountdownMS(Timer *timer, unsigned int timeout) {
    timer->end_time = HAL_GetTick() + timeout;
}

// 3. 读函数：MQTTYield 会调用它来轮询 Socket
int w5x00_read(Network *n, unsigned char *buffer, int len, int timeout) {
    uint32_t start_tick = HAL_GetTick();
    int recv_len = 0;
    while (recv_len < len) {
        int ret = recv(n->my_socket, buffer + recv_len, len - recv_len);
        if (ret > 0) recv_len += ret;
        else if (ret != SOCK_BUSY && ret < 0) return ret;
        
        // 如果没有数据，且已超过 MQTTYield 传入的 timeout，则退出循环
        if ((HAL_GetTick() - start_tick) > timeout) break;
    }
    return recv_len;
}
```

---

## 3. 第二阶段：MQTT 初始化与连接 (mqtt_app.c)

在应用层封装初始化逻辑。

```c
/* mqtt_app.c */
#include "mqtt_app.h"

Network n;
MQTTClient c;
uint8_t tx_buf[2048], rx_buf[2048];

// 消息到达后的回调函数 (Echo 功能)
void messageArrived(MessageData* md) {
    MQTTMessage* message = md->message;
    // 数据回传：将收到的 payload 发布到 status 主题
    MQTTMessage pub_msg = { .qos = QOS0, .payload = message->payload, .payloadlen = message->payloadlen };
    MQTTPublish(&c, "w5500/status", &pub_msg);
}

void mqtt_init(void) {
    // 1. 初始化网络接口，关联 W5500 的 Socket 0
    NewNetwork(&n, 0); 
    
    // 2. TCP 连接到服务器 (如 EMQX 公共服务器 116.62.226.174)
    uint8_t server_ip[4] = {116, 62, 226, 174};
    ConnectNetwork(&n, server_ip, 1883);
    
    // 3. 初始化 MQTT 客户端对象
    MQTTClientInit(&c, &n, 1000, tx_buf, 2048, rx_buf, 2048);
    
    // 4. MQTT 协议层连接
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID.cstring = "W5500_User_Device";
    data.keepAliveInterval = 60; // 60秒心跳

    if (MQTTConnect(&c, &data) == SUCCESS) {
        // 5. 连接成功后，订阅控制主题
        MQTTSubscribe(&c, "w5500/control", QOS0, messageArrived);
    }
}
```

---

## 4. 第三阶段：主循环维护 (main.c)

这是使用 `MQTTYield` 的关键点。

```c
/* main.c */
int main(void) {
    // 硬件基础初始化
    HAL_Init();
    w5500_init(); // W5500 静态 IP 配置
    
    // MQTT 应用初始化
    mqtt_init();
    
    while (1) {
        /* 
           核心：持续调用 MQTTYield
           100 表示本次轮询最多等待 100ms
           它内部会自动处理：
           - 发送 Keep-alive PING 包
           - 检查 Socket 接收数据
           - 触发 messageArrived 回调
        */
        MQTTYield(&c, 100); 
        
        // 这里可以执行其他非阻塞任务
    }
}
```

---

## 5. 联调验证：使用 MQTTX

1.  **MQTTX 连接**：连接到 `broker.emqx.io:1883`。
2.  **订阅主题**：订阅 `w5500/status`。
3.  **发送消息**：
    - Topic: `w5500/control`
    - Payload: `Hello W5500`
4.  **验证结果**：
    - W5500 串口应显示收到数据。
    - MQTTX 的 `w5500/status` 窗口应收到 W5500 回传的 `Hello W5500`。

---

## 6. 常见问题：为什么我的 MQTT 会断开？

1.  **主循环阻塞**：如果在 `while(1)` 中使用了 `HAL_Delay(5000)`，`MQTTYield` 无法被及时调用，心跳包会超时，服务器将强制断开连接。
2.  **网关错误**：W5500 无法访问互联网。请检查 `wiz_NetInfo` 中的 `gw` (网关) 是否正确设置为路由器的 IP。
3.  **缓冲区太小**：如果收到的 MQTT 消息超过了 `tx_buf/rx_buf` 的大小（本指南设置为 2048），会导致解析失败。
