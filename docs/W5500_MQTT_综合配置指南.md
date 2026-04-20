# W5500 MQTT 通信全流程配置指南 (基于 HAL 库)

本指南详细介绍了如何在 W5500 上从零开始配置 MQTT 协议，实现与云端/上位机的高效双向通信。本次配置采用了 STM32 HAL 库原生的 `HAL_GetTick()` 计时机制，无需手动注册中断回调，架构更简洁、稳定。

---

## 1. 工程结构与库准备

### 1.1 核心库文件
确保工程中包含 `ioLibrary_Driver/Internet/MQTT` 目录下的以下核心文件：
- `MQTTClient.c/h`：Paho MQTT 协议核心逻辑。
- `mqtt_interface.c/h`：W5500 与 MQTT 库的对接层（**关键移植点**）。
- `MQTTPacket/src/`：MQTT 报文封装与解析库。

### 1.2 Include 路径配置
在 IDE 中添加以下包含路径：
- `ioLibrary_Driver/Internet/MQTT`
- `ioLibrary_Driver/Internet/MQTT/MQTTPacket/src`

---

## 2. 接口层适配 (mqtt_interface.c)

为了让 MQTT 库能在 W5500 上运行，我们需要适配计时器和 Socket 读写。

### 2.1 计时器逻辑 (使用 HAL_GetTick)
移除传统的 `MilliTimer_Handler`，直接利用 HAL 库的毫秒计数器。

```c
/* mqtt_interface.c 核心逻辑 */
#include "main.h"

// 判断定时器是否到期
char TimerIsExpired(Timer *timer) {
    return (HAL_GetTick() >= timer->end_time);
}

// 倒计时设置 (ms)
void TimerCountdownMS(Timer *timer, unsigned int timeout) {
    timer->end_time = HAL_GetTick() + timeout;
}

// 获取剩余时间 (ms)
int TimerLeftMS(Timer *timer) {
    uint32_t now = HAL_GetTick();
    if (now >= timer->end_time) return 0;
    return timer->end_time - now;
}
```

### 2.2 非阻塞读写函数 (带超时保护)
针对 W5500 的硬件特性，在读取 MQTT 报文时增加超时处理，防止网络波动导致死锁。

```c
/* mqtt_interface.c 读函数 */
int w5x00_read(Network *n, unsigned char *buffer, int len, int timeout) {
    uint32_t start_tick = HAL_GetTick();
    int recv_len = 0;
    while (recv_len < len) {
        int ret = recv(n->my_socket, buffer + recv_len, len - recv_len);
        if (ret > 0) {
            recv_len += ret;
        } else if (ret != SOCK_BUSY && ret < 0) {
            return ret; // 硬件或连接错误
        }
        if ((HAL_GetTick() - start_tick) > timeout) break; // 超时退出
    }
    return recv_len;
}
```

---

## 3. 应用层业务实现 (mqtt_app.c)

封装 MQTT 的连接、订阅及数据回传（Echo）逻辑。

### 3.1 核心业务代码
```c
/* mqtt_app.c */
#include "mqtt_app.h"

static Network n;
static MQTTClient c;
static uint8_t target_ip[4] = {116, 62, 226, 174}; // broker.emqx.io
static uint16_t target_port = 1883;

// 消息到达回调：实现 Echo 功能
static void messageArrived(MessageData* md) {
    MQTTMessage* message = md->message;
    printf("Recv Topic: %.*s, Payload: %.*s\r\n", 
           md->topicName->lenstring.len, md->topicName->lenstring.data,
           (int)message->payloadlen, (char*)message->payload);

    // 将收到的数据原样回传到 status 主题
    MQTTMessage pub_msg = { .qos = QOS0, .payload = message->payload, .payloadlen = message->payloadlen };
    MQTTPublish(&c, "w5500/status", &pub_msg);
}

void mqtt_init(void) {
    NewNetwork(&n, MQTT_SOCKET); // 绑定 Socket 0
    ConnectNetwork(&n, target_ip, target_port); // TCP 握手
    
    MQTTClientInit(&c, &n, 1000, tx_buf, BUF_SIZE, rx_buf, BUF_SIZE);
    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID.cstring = "W5500_STM32_Final";
    data.keepAliveInterval = 60;

    if (MQTTConnect(&c, &data) == SUCCESS) {
        printf("MQTT Connected!\r\n");
        MQTTSubscribe(&c, "w5500/control", QOS0, messageArrived);
    }
}

void mqtt_loop(void) {
    MQTTYield(&c, 100); // 维持心跳并处理收包
}
```

---

## 4. 主程序集成 (main.c)

在 `main` 函数中启动 MQTT 并持续维护。

```c
/* main.c */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    w5500_init(); // 初始化 W5500 静态 IP
    
    mqtt_init();  // 启动 MQTT
    
    while (1) {
        mqtt_loop(); // 核心：持续调用 MQTTYield
    }
}
```

---

## 5. MQTTX 联调测试步骤

### 5.1 配置 MQTTX
1.  **连接设置**：Host 选择 `mqtt://broker.emqx.io`，Port `1883`。
2.  **订阅主题**：在 MQTTX 中订阅 `w5500/status`。
3.  **发送测试**：
    - Topic: `w5500/control`
    - Payload: `{"msg": "TRAE Test Success"}`
    - 点击发送。

### 5.2 预期结果 (Echo 验证)
- **W5500 侧**：串口打印接收到的 Payload。
- **MQTTX 侧**：在 `w5500/status` 窗口立即收到 W5500 回传的相同消息。

---

## 6. 排障清单

| 问题 | 原因 | 解决 |
| :--- | :--- | :--- |
| 连接失败 | 网关或 DNS 配置错误 | 检查 `network_init` 中的 GW 地址是否正确。 |
| 频繁重连 | Keep-alive 失败 | 确保 `mqtt_loop()` 在主循环中未被阻塞。 |
| 读写超时 | W5500 缓冲区溢出 | 检查 `w5500_init` 中分配给 MQTT Socket 的缓冲区大小。 |
