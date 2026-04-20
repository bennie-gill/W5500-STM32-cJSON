# W5500 使用 MQTT 详细步骤说明手册 (含全量代码)

本手册详细介绍了如何在 W5500 上实现 MQTT 协议，涵盖了从底层接口适配到应用层业务逻辑的全过程，并提供了完整的代码实现。

---

## 1. MQTT 函数功能字典

### 1.1 核心客户端函数 (MQTTClient.c)
| 函数名 | 作用描述 |
| :--- | :--- |
| **MQTTClientInit** | 初始化客户端对象，绑定缓冲区与底层 Network 接口。 |
| **MQTTConnect** | 发送 CONNECT 报文并等待服务器返回 CONNACK。 |
| **MQTTPublish** | 发布消息到指定主题。 |
| **MQTTSubscribe** | 订阅主题并绑定回调函数。 |
| **MQTTYield** | **核心循环**：维持心跳并处理后台事务，必须在 `while(1)` 中调用。 |

### 1.2 底层接口适配函数 (mqtt_interface.c)
| 函数名 | 作用描述 |
| :--- | :--- |
| **NewNetwork** | 初始化网络层结构体，关联 Socket 编号。 |
| **ConnectNetwork** | 完成与服务器的 TCP 三次握手。 |
| **w5x00_read/write** | 底层读写适配，基于 `HAL_GetTick()` 实现超时保护。 |

---

## 2. 核心代码实现

### 2.1 底层适配：mqtt_interface.h
```c
/* ioLibrary_Driver/Internet/MQTT/mqtt_interface.h */
#ifndef __MQTT_INTERFACE_H_
#define __MQTT_INTERFACE_H_

#include <stdint.h>
#include "main.h"

typedef struct Timer Timer;
struct Timer {
  unsigned long end_time; // 到期时刻
};

typedef struct Network Network;
struct Network {
  int my_socket;
  int (*mqttread)(Network *, unsigned char *, int, int);
  int (*mqttwrite)(Network *, unsigned char *, int, int);
  void (*disconnect)(Network *);
};

void TimerInit(Timer *);
char TimerIsExpired(Timer *);
void TimerCountdownMS(Timer *, unsigned int);
void TimerCountdown(Timer *, unsigned int);
int TimerLeftMS(Timer *);

int w5x00_read(Network *, unsigned char *, int, int);
int w5x00_write(Network *, unsigned char *, int, int);
void w5x00_disconnect(Network *);
void NewNetwork(Network *n, int sn);
int ConnectNetwork(Network *n, uint8_t *ip, uint16_t port);

#endif
```

### 2.2 底层适配：mqtt_interface.c
```c
/* ioLibrary_Driver/Internet/MQTT/mqtt_interface.c */
#include "mqtt_interface.h"
#include "socket.h"

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
  return (now >= timer->end_time) ? 0 : (timer->end_time - now);
}

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
  return connect(n->my_socket, ip, port);
}
```

### 2.3 应用逻辑：mqtt_app.c
```c
/* BSP/mqtt_app.c */
#include "mqtt_app.h"
#include <stdio.h>

static uint8_t mqtt_tx_buf[MQTT_BUF_SIZE], mqtt_rx_buf[MQTT_BUF_SIZE];
static Network n;
static MQTTClient c;

// 数据回传回调
static void messageArrived(MessageData* md) {
    MQTTMessage* message = md->message;
    printf("Recv Topic: %.*s, Payload: %.*s\r\n", 
           md->topicName->lenstring.len, md->topicName->lenstring.data,
           (int)message->payloadlen, (char*)message->payload);
    
    // Echo: 发回到发布主题
    MQTTMessage pub_msg = { .qos = QOS0, .payload = message->payload, .payloadlen = message->payloadlen };
    MQTTPublish(&c, PUB_TOPIC, &pub_msg);
    printf("Echo sent to topic: %s\r\n", PUB_TOPIC);
}

void mqtt_init(void) {
    // 192.168.1.200 (根据宏定义转换)
    uint8_t server_ip[4] = {192, 168, 1, 200}; 
    
    NewNetwork(&n, MQTT_SOCKET); // 使用定义的 Socket 2
    if (ConnectNetwork(&n, server_ip, MQTT_BROKER_PORT) == SOCK_OK) {
        MQTTClientInit(&c, &n, 1000, mqtt_tx_buf, MQTT_BUF_SIZE, mqtt_rx_buf, MQTT_BUF_SIZE);
        
        MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
        data.clientID.cstring = CLIENT_ID;
        data.keepAliveInterval = 60;
        data.cleansession = 1;

        if (MQTTConnect(&c, &data) == SUCCESS) {
            printf("MQTT Connected!\r\n");
            MQTTSubscribe(&c, SUB_TOPIC, QOS0, messageArrived);
            printf("Subscribed to topic: %s\r\n", SUB_TOPIC);
        }
    }
}

void mqtt_loop(void) {
    MQTTYield(&c, 100); // 100ms 轮询处理
}
```

---

## 3. 使用流程说明

1.  **硬件准备**：确保 W5500 SPI 通信正常，`w5500_init()` 静态 IP 配置无误。**注意：W5500 的 IP 必须与 Broker IP (192.168.1.200) 在同一网段。**
2.  **配置宏定义**：在 `mqtt_app.h` 中修改以下宏：
    - `MQTT_SOCKET`: 2
    - `MQTT_BROKER_IP`: "192.168.1.200"
    - `PUB_TOPIC`: "device/data"
    - `SUB_TOPIC`: "device/control"
3.  **代码移植**：将底层适配代码和应用代码复制到工程，并在 `main.c` 中初始化。
4.  **联调测试**：
    - 打开 **MQTTX**，连接 `192.168.1.200:1883`。
    - 订阅主题 `device/data`。
    - 向主题 `device/control` 发送消息。
    - 验证 W5500 串口打印接收内容，并检查 MQTTX 是否收到回传。

---

## 4. 常见问题排查 (Troubleshooting)

### 4.1 链接报错：Undefined symbol MQTTDeserialize_...
如果你在编译时遇到 `Error: L6218E: Undefined symbol MQTTDeserialize_ack (referred from mqttclient.o)` 等错误，是因为你虽然包含了头文件，但**没有将对应的源文件添加到 Keil 项目中**。

**解决方法：**
1.  在 Keil 的 Project 窗口中，找到你的项目群组（例如 `ioLibrary/MQTT`）。
2.  右键点击该群组 -> **Add Existing Files to Group...**
3.  导航到以下路径：`ioLibrary_Driver/Internet/MQTT/MQTTPacket/src/`
4.  选中该目录下**所有的 `.c` 文件**并添加：
    - `MQTTConnectClient.c`
    - `MQTTConnectServer.c`
    - `MQTTDeserializePublish.c`
    - `MQTTFormat.c`
    - `MQTTPacket.c`
    - `MQTTSerializePublish.c`
    - `MQTTSubscribeClient.c`
    - `MQTTSubscribeServer.c`
    - `MQTTUnsubscribeClient.c`
    - `MQTTUnsubscribeServer.c`
5.  重新编译 (Build) 即可解决。

### 4.2 无法连接到 Broker
-   **网段检查**：确保 W5500 的 IP 地址与 MQTT Broker (192.168.1.200) 在同一个子网内。
-   **防火墙**：如果 Broker 运行在 PC 上，请确保 PC 防火墙已允许 1883 端口的入站连接。
-   **Ping 测试**：先确保 PC 能够 Ping 通 W5500。
