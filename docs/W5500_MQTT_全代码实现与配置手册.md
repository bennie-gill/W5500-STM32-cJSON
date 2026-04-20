# W5500 MQTT 全代码实现与配置手册

本手册提供了 W5500 移植 Paho MQTT 协议栈的完整代码实现及配置步骤。本方案核心优势在于使用 `HAL_GetTick()` 替代了复杂的中断计数器，极大简化了移植难度。

---

## 1. 接口定义层：mqtt_interface.h

在 `ioLibrary_Driver/Internet/MQTT/mqtt_interface.h` 中，我们定义了适配 HAL 库的结构体和函数原型。

```c
#ifndef __MQTT_INTERFACE_H_
#define __MQTT_INTERFACE_H_

#include <stdint.h>
#include "main.h" // 引用 HAL 库以获取 uint32_t 等定义

/* MQTT 定时器结构体 */
typedef struct Timer Timer;
struct Timer {
    uint32_t end_time; // 使用 HAL_GetTick() 的绝对到期时间
};

/* 网络层接口结构体 */
typedef struct Network Network;
struct Network {
    int my_socket;
    // 适配 HAL 库，将超时参数改为 int 类型 (ms)
    int (*mqttread) (Network*, unsigned char*, int, int);
    int (*mqttwrite) (Network*, unsigned char*, int, int);
    void (*disconnect) (Network*);
};

/* 函数原型声明 */
void TimerInit(Timer*);
char TimerIsExpired(Timer*);
void TimerCountdownMS(Timer*, unsigned int);
void TimerCountdown(Timer*, unsigned int);
int TimerLeftMS(Timer*);

int w5x00_read(Network*, unsigned char*, int, int);
int w5x00_write(Network*, unsigned char*, int, int);
void w5x00_disconnect(Network*);
void NewNetwork(Network* n, int sn);
int ConnectNetwork(Network* n, uint8_t* ip, uint16_t port);

#endif
```

---

## 2. 接口实现层：mqtt_interface.c

在 `ioLibrary_Driver/Internet/MQTT/mqtt_interface.c` 中实现基于 `HAL_GetTick()` 的非阻塞读写逻辑。

```c
#include "mqtt_interface.h"
#include "socket.h"

/* 定时器相关实现 */
void TimerInit(Timer *timer) { timer->end_time = 0; }

char TimerIsExpired(Timer *timer) {
    return (HAL_GetTick() >= timer->end_time);
}

void TimerCountdownMS(Timer *timer, unsigned int timeout) {
    timer->end_time = HAL_GetTick() + timeout;
}

void TimerCountdown(Timer *timer, unsigned int timeout) {
    timer->end_time = HAL_GetTick() + (timeout * 1000);
}

int TimerLeftMS(Timer *timer) {
    uint32_t now = HAL_GetTick();
    if (now >= timer->end_time) return 0;
    return timer->end_time - now;
}

/* 网络读写实现 (带超时保护) */
int w5x00_read(Network *n, unsigned char *buffer, int len, int timeout) {
    uint32_t start_tick = HAL_GetTick();
    int recv_len = 0;
    while (recv_len < len) {
        int ret = recv(n->my_socket, buffer + recv_len, len - recv_len);
        if (ret > 0) recv_len += ret;
        else if (ret != SOCK_BUSY && ret < 0) return ret;
        
        if ((HAL_GetTick() - start_tick) > timeout) break;
    }
    return recv_len;
}

int w5x00_write(Network *n, unsigned char *buffer, int len, int timeout) {
    uint32_t start_tick = HAL_GetTick();
    int sent_len = 0;
    while (sent_len < len) {
        int ret = send(n->my_socket, buffer + sent_len, len - sent_len);
        if (ret > 0) sent_len += ret;
        else if (ret != SOCK_BUSY && ret < 0) return ret;
        
        if ((HAL_GetTick() - start_tick) > timeout) break;
    }
    return sent_len;
}

void NewNetwork(Network *n, int sn) {
    n->my_socket = sn;
    n->mqttread = w5x00_read;
    n->mqttwrite = w5x00_write;
    n->disconnect = w5x00_disconnect;
}

void w5x00_disconnect(Network *n) { disconnect(n->my_socket); }

int ConnectNetwork(Network *n, uint8_t *ip, uint16_t port) {
    if (socket(n->my_socket, Sn_MR_TCP, 12345, 0) != n->my_socket) return SOCK_ERROR;
    if (connect(n->my_socket, ip, port) != SOCK_OK) return SOCK_ERROR;
    return SOCK_OK;
}
```

## 3. 核心库初始化逻辑：MQTTClientInit

为了深入理解初始化过程，这里展示了 Paho MQTT 库中 `MQTTClientInit` 的核心实现。它负责将底层 Network 接口与应用层的发送/接收缓冲区绑定。

```c
/* MQTTClient.c 中的核心实现 */
void MQTTClientInit(MQTTClient* c, Network* network, unsigned int command_timeout_ms,
                    unsigned char* sendbuf, size_t sendbuf_size, 
                    unsigned char* readbuf, size_t readbuf_size) {
    c->ipstack = network; // 关联 w5x00 读写函数
    c->command_timeout_ms = command_timeout_ms; // 指令执行超时 (ms)
    c->buf = sendbuf; // 发送缓冲区地址
    c->buf_size = sendbuf_size;
    c->readbuf = readbuf; // 接收缓冲区地址
    c->readbuf_size = readbuf_size;
    c->isconnected = 0;
    c->ping_outstanding = 0;
    c->defaultMessageHandler = NULL;
    c->next_packetid = 1;
    TimerInit(&c->ping_timer); // 初始化心跳定时器
}
```

---

## 4. 应用逻辑层：mqtt_app.c

在 `BSP/mqtt_app.c` 中实现业务代码，包括初始化、消息回传 (Echo) 和心跳维护。

```c
#include "mqtt_app.h"
#include <stdio.h>

static uint8_t mqtt_tx_buf[2048];
static uint8_t mqtt_rx_buf[2048];
static Network n;
static MQTTClient c;

/* 消息回调：实现 Echo 功能 */
static void messageArrived(MessageData* md) {
    MQTTMessage* message = md->message;
    printf("Topic: %.*s, Payload: %.*s\r\n", 
           md->topicName->lenstring.len, md->topicName->lenstring.data,
           (int)message->payloadlen, (char*)message->payload);

    // 回传数据到 status 主题
    MQTTMessage pub_msg = { .qos = QOS0, .payload = message->payload, .payloadlen = message->payloadlen };
    MQTTPublish(&c, "w5500/status", &pub_msg);
}

void mqtt_init(void) {
    uint8_t server_ip[4] = {116, 62, 226, 174}; // EMQX 公共 Broker
    
    NewNetwork(&n, 0); // 使用 Socket 0
    if (ConnectNetwork(&n, server_ip, 1883) != SOCK_OK) {
        printf("TCP Connect Failed\r\n");
        return;
    }

    MQTTClientInit(&c, &n, 1000, mqtt_tx_buf, 2048, mqtt_rx_buf, 2048);
    
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.clientID.cstring = "W5500_STM32_TRAE";
    data.keepAliveInterval = 60;

    if (MQTTConnect(&c, &data) == SUCCESS) {
        printf("MQTT Connected!\r\n");
        MQTTSubscribe(&c, "w5500/control", QOS0, messageArrived);
    }
}

void mqtt_loop(void) {
    // 维持心跳并处理收发，内部依赖底层 TimerIsExpired 函数
    MQTTYield(&c, 100); 
}
```

---

## 5. 主程序集成：main.c

在 `Core/Src/main.c` 中完成最终调用。

```c
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();

    w5500_init(); // 第一阶段：硬件与静态 IP 初始化
    mqtt_init();  // 第二阶段：MQTT 初始化与连接

    while (1) {
        mqtt_loop(); // 核心：通过 MQTTYield 持续处理协议逻辑
    }
}
```

---

## 6. 测试与验证流程

1.  **MQTTX 配置**：连接 `broker.emqx.io`，订阅 `w5500/status`。
2.  **发送消息**：向 `w5500/control` 发送任意文本。
3.  **验证回传**：在 MQTTX 的 `w5500/status` 窗口查看回传内容，并观察串口输出。
