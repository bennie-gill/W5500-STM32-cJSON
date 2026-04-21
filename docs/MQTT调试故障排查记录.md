# STM32 + W5500 MQTT 调试故障排查与修复记录

## 1. 问题描述 (Symptoms)
在开发过程中，系统出现了以下几个主要问题：
- **连接不稳定**：TCP 显示连接成功，但 MQTT 握手（Connect）频繁失败。
- **任务阻塞**：必须在 MQTTX 发送指令后，W5500 才会返回温湿度数据。
- **内存报错**：串口频繁输出 `[ERROR] Failed to create JSON string`。
- **逻辑 Bug**：CO2 浓度显示正常但频繁触发报警。

---

## 2. 核心原因分析 (Root Causes)

### 2.1 Socket 阻塞与非阻塞切换时机错误
- **原现象**：为了保证主循环不卡死，我们将 Socket 设为了非阻塞模式。
- **原因**：在 MQTT 初始化阶段（握手阶段），发送 `CONNECT` 报文后需要等待服务器返回 `CONNACK`。由于网络存在毫秒级的延迟，非阻塞模式下的 `recv` 会立即返回 `SOCK_BUSY`（无数据）。
- **结果**：MQTT 库误认为“读不到响应”而判定连接超时，导致 `MQTT Connect Failed!`。
```C
int w5x00_read(Network *n, unsigned char *buffer, int len, int timeout) {
  // ...
  uint16_t rsr = getSn_RX_RSR(n->my_socket); // 先读硬件寄存器
  if (rsr > 0) {
    // 只有确定有数据，才调用 recv，保证立即拿到数据
    int ret = recv(n->my_socket, buffer + recv_len, can_read);
  } else {
    // 没数据就在 timeout 内循环等待，给网线传输留缓冲时间
  }
  // ...
}
```
### 2.2 堆内存 (Heap) 空间不足
- **原现象**：`cJSON` 无法生成 JSON 字符串，且程序运行一段时间后可能挂死。
- **原因**：STM32 默认的 Heap 只有 512 字节。`cJSON` 在解析和打印 JSON 时需要动态分配大量内存（解析树+字符串缓冲区），导致内存溢出。
```C
Heap_Size       EQU     0x00002000  ; 增加到 8KB 解决 cJSON 报错
```
### 2.3 传感器逻辑与数据类型错误
- **原因**：在 CO2 报警判断中，将 1000 (uint16_t) 强制转换为了 (uint8_t)，导致阈值变成了 232，造成误报。

---
## 3. 修复方案与步骤 (Solutions)

## 3. 详细修改步骤与核心代码

### 第一步：重构底层读取接口 (解决指令丢失)
**修改文件**：`ioLibrary_Driver/Internet/MQTT/mqtt_interface.c`
**修改点**：重构 `w5x00_read`，引入硬件 RSR 寄存器检查。
```c
int w5x00_read(Network *n, unsigned char *buffer, int len, int timeout) {
  uint32_t start_tick = HAL_GetTick();
  int recv_len = 0;
  while (recv_len < len) {
    // 1. 先查硬件寄存器，确定缓冲区是否有数据
    uint16_t rsr = getSn_RX_RSR(n->my_socket);
    if (rsr > 0) {
      // 2. 有数据才调用 recv，保证立即拿到数据
      uint16_t can_read = (rsr > (len - recv_len)) ? (len - recv_len) : rsr;
      int ret = recv(n->my_socket, buffer + recv_len, can_read);
      if (ret > 0) recv_len += ret;
      else if (ret < 0) return ret; // 连接断开
    }
    // 3. 没数据就在 timeout 内循环，给网线传输留时间
    if ((HAL_GetTick() - start_tick) > timeout) break;
  }
  return recv_len;
}
```

### 第二步：优化应用初始化与连接 (解决连接失败)
**修改文件**：`BSP/mqtt_app.c`
**修改点**：
1. 移除 `ctlsocket` 的非阻塞切换，保持默认阻塞模式进行握手。
2. 明确协议版本为 `4` (v3.1.1)。
3. 增加具体错误码打印。
```c
void mqtt_init(void) {
  // ... 前置 TCP 连接 ...
  MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
  data.MQTTVersion = 4; // 必须设为 4 (v3.1.1)
  data.clientID.cstring = CLIENT_ID;
  
  // 握手期间保持阻塞，保证 100% 成功
  int rc = MQTTConnect(&g_MQTTclient, &data);
  if (rc != SUCCESSS) {
    printf("MQTT Connect Failed! Code: %d\n", rc);
    return;
  }
  // 握手成功后再由 w5x00_read 的软件逻辑实现非阻塞
}
```

### 第三步：调整启动文件堆内存 (解决内存报错)
**修改文件**：`MDK-ARM/startup_stm32f103xe.s`
**修改点**：将 Heap 空间从 512 字节提升至 8KB。
```assembly
Heap_Size       EQU     0x00002000  ; 解决 cJSON 运行时的内存分配失败
```

### 第四步：修正传感器报警 Bug (解决逻辑误报)
**修改文件**：`BSP/sensor.c`
**修改点**：修正 `CO2_Alert_Check` 的参数类型和判断逻辑。
```c
static void CO2_Alert_Check(uint16_t co2) { // 使用 uint16_t
    // 移除错误的 (uint8_t) 强转，直接比较 1000
    uint8_t current_alert = (co2 > CO2_THRESHOLD) ? 1 : 0; 
    // ...
}
```

---

## 4. 经验总结 (Lessons Learned)
1. **嵌入式网络编程**中，初始化阶段建议使用**阻塞模式**提高成功率，运行阶段使用**非阻塞模式**保证实时性。
2. **cJSON 库**是内存大户，必须配套足够的堆（Heap）空间。
3. **硬件层检查**：通过 `getVERSIONR()` 定期检查 W5500 硬件状态，可以有效处理 SPI 掉线等极端物理故障。
