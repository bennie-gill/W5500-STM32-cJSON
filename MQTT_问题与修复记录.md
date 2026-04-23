
# STM32F103 + W5500 + MQTT（MQTTX 测试）问题与修复记录（本工程版）

本文档用于记录本工程 `i:\AI\w5500` 在移植/联调 MQTT（Paho Embedded C / WIZnet ioLibrary）过程中出现过的典型问题、原因定位与解决办法，并给出对应的代码位置，便于后续复现与排查。

涉及的核心文件：
- 网络/MQTT 适配层：[mqtt_interface.c](file:///i:/AI/w5500/ioLibrary_Driver/Internet/MQTT/mqtt_interface.c)
- MQTT 应用层（连接/订阅/循环/JSON）：[mqtt_app.c](file:///i:/AI/w5500/BSP/mqtt_app.c)
- 传感器与周期发布逻辑：[sensor.c](file:///i:/AI/w5500/BSP/sensor.c)
- 按键扫描（GPIO 读取）：[control.c](file:///i:/AI/w5500/BSP/control.c)
- Keil 启动文件（堆/栈大小）：[startup_stm32f103xe.s](file:///i:/AI/w5500/MDK-ARM/startup_stm32f103xe.s)

---

## 1）链接错误：Undefined symbol MQTTDeserialize_ack / MQTTSerialize_xxx 等

**现象**
- Keil 编译通过但链接报错，例如 `Undefined symbol MQTTDeserialize_ack (referred from MQTTClient.o)` 等。

**根因**
- `MQTTClient.c` 依赖的 MQTTPacket 实现文件没有加入工程编译（只包含了头文件或只加了部分 `.c`）。
- `MQTTDeserialize_ack`、`MQTTSerialize_ack` 等函数实现位于 `MQTTPacket/src` 目录中。

**解决办法（Keil 工程配置）**
- 在工程中把以下目录的 `.c` 文件全部加入编译：
  - `i:\AI\w5500\ioLibrary_Driver\Internet\MQTT\MQTTPacket\src\`
- 该目录下典型需要加入的源文件包括（以本工程目录为准）：
  - `MQTTPacket.c`
  - `MQTTConnectClient.c / MQTTConnectServer.c`
  - `MQTTSubscribeClient.c / MQTTSubscribeServer.c`
  - `MQTTUnsubscribeClient.c / MQTTUnsubscribeServer.c`
  - `MQTTSerializePublish.c / MQTTDeserializePublish.c`
  - `MQTTFormat.c`

**验证方法**
- 重新全量编译并链接，通过后再下载运行。

---

## 2）移植定时器问题：工程里没有 MilliTimer_Handler（但库需要计时）

**现象**
- 移植示例时发现“文档/代码里提到 `MilliTimer_Handler()`”，但本工程并不存在该函数。
- 进一步表现为：超时控制不稳定、MQTT 等待包/心跳逻辑异常等。

**根因**
- 部分 WIZnet/Paho 移植例程使用自定义的 1ms 定时中断与 `MilliTimer_Handler()`。
- 本工程采用 STM32 HAL，已有统一的 `HAL_GetTick()` 1ms 计时源，因此不需要额外的 `MilliTimer_Handler()`。

**解决办法（使用 HAL_GetTick 替代）**
- 在 MQTT 适配层实现 Paho 需要的 Timer 接口，全部基于 `HAL_GetTick()`：
  - `TimerIsExpired / TimerCountdownMS / TimerCountdown / TimerLeftMS`
- 代码位置：[mqtt_interface.c:L53-L95](file:///i:/AI/w5500/ioLibrary_Driver/Internet/MQTT/mqtt_interface.c#L53-L95)

**为什么这样能工作**
- `MQTTYield()` 内部会创建一个 `Timer` 并调用 `cycle()` 读取网络数据；`cycle()` 再通过 `TimerLeftMS()` 控制读超时。
- 代码位置：[MQTTYield](file:///i:/AI/w5500/ioLibrary_Driver/Internet/MQTT/MQTTClient.c#L282-L294)

---

## 3）MQTTX 下发控制消息后，板子不打印/不处理；SUBACK 长时间收不到

**现象**
- MQTTX 已显示发送到订阅主题（如 `device/control`），但板子串口没有打印“收到消息/解析成功”等。
- MQTTX 侧可能看到类似 “Sending SUBACK to ...” 的日志，但板子侧迟迟没有进入订阅成功流程，或者订阅后消息处理不及时。

**根因（常见且最关键）**
- Paho 的 `readPacket()` 会多次调用 `mqttread()`：先读 1 字节头，再读剩余长度，再读 payload。
- 如果底层 `mqttread()` 在“无数据时”调用了阻塞式 `recv()`，就会把整个 `MQTTYield()` 卡住，导致：
  - `SUBACK`/`PUBLISH` 等包无法被及时读取与解析
  - 看起来像“必须等对端再发点东西/板子再主动发点东西才会动”

**解决办法（核心改动：硬件 RSR 预检查，避免无数据时调用 recv）**
- 在 W5500 的 `w5x00_read()` 中，先读取接收缓冲区寄存器 `Sn_RX_RSR`，确认确实有数据后再调用 `recv()`：
  - 代码位置：[mqtt_interface.c:L120-L149](file:///i:/AI/w5500/ioLibrary_Driver/Internet/MQTT/mqtt_interface.c#L120-L149)
- 这样即使 socket 处于阻塞模式，也能在“硬件缓冲区没有数据时”快速返回，让 `MQTTYield()` 周期性地继续运行。

**配套要求：主循环必须周期调用 MQTTYield**
- 本工程在 `mqtt_loop()` 中周期调用 `MQTTYield(&g_MQTTclient, 50)` 来驱动协议栈：
  - 代码位置：[mqtt_app.c:L336-L356](file:///i:/AI/w5500/BSP/mqtt_app.c#L336-L356)

**验证方法（MQTTX）**
- 订阅/发布主题：
  - 板子订阅：`SUB_TOPIC`（默认 `device/control`）
  - 板子发布：`PUB_TOPIC`（默认 `device/data`）
- MQTTX 向 `device/control` 发送示例：
  - `{"lamp":true,"id":0}`
  - `{"fun":false,"id":1}`
- 期望现象：
  - 板子串口打印 `Copied payload: [...]`（见 [mqtt_app.c:L257-L285](file:///i:/AI/w5500/BSP/mqtt_app.c#L257-L285)）
  - 成功解析后打印 `JSON Parse SUCCESS!` 并执行控制（见 [mqtt_app.c:L187-L255](file:///i:/AI/w5500/BSP/mqtt_app.c#L187-L255)）

---

## 4）TCP 频繁重连/连接循环（看起来一直在断开重连）

**现象**
- 程序运行一段时间后，串口周期性打印重连日志（或重复出现 TCP Connected / MQTT Connected）。

**根因（与第 3 点常伴生）**
- 如果底层读操作阻塞、或连接阶段把 socket 强制切到非阻塞导致握手/收包时序不稳定，会触发 MQTT 的超时与状态机回退，进而进入“重连循环”。

**解决办法（本工程当前策略）**
- TCP 连接阶段保持阻塞，确保握手稳定：
  - 代码位置：[mqtt_interface.c:L194-L206](file:///i:/AI/w5500/ioLibrary_Driver/Internet/MQTT/mqtt_interface.c#L194-L206)
- MQTT 协议阶段不依赖强制切非阻塞，而是依赖第 3 点的 “RSR 预检查 + 超时返回” 达到软件层面的“非阻塞效果”：
  - `mqtt_init()` 中明确说明移除强制切换逻辑：[mqtt_app.c:L330-L333](file:///i:/AI/w5500/BSP/mqtt_app.c#L330-L333)
- TCP 连接后增加一个短延时，让链路稳定再开始 MQTT 握手：
  - 代码位置：[mqtt_app.c:L300-L306](file:///i:/AI/w5500/BSP/mqtt_app.c#L300-L306)

**验证方法**
- 观察串口：`[RECONNECT]` 不应频繁出现（见 [mqtt_app.c:L349-L356](file:///i:/AI/w5500/BSP/mqtt_app.c#L349-L356)）。
- MQTTX 侧连接应稳定，订阅/发布延迟正常。

---

## 5）CO2 数据很长时间不出现（或只在报警变化时出现）

**现象**
- MQTTX 上能看到温湿度/状态，但 CO2 长时间没有更新。
- 或者 CO2 只在“超过阈值/恢复正常”的瞬间上报一次。

**根因**
- 早期逻辑只在报警状态变化时上报（`CO2_Alert_Check()` 里做了“状态变化才发布”的判断），因此“数值在阈值同侧变化”不会上报。

**解决办法（周期性发布 CO2，不再依赖报警翻转）**
- 在 `Sensor_task()` 中每 1s 都读取并发布一次 CO2（同时保留报警判断与控制逻辑）：
  - 代码位置：[sensor.c:L67-L93](file:///i:/AI/w5500/BSP/sensor.c#L67-L93)

**验证方法**
- MQTTX 订阅 `device/data`，应每秒都能看到包含 `co2` 字段的消息（来自 `MQTT_Publish_Alert()`）。

---

## 6）GPIO 读取函数参数错误：HAL_GPIO_ReadPin 参数顺序写反

**现象**
- 编译报错或告警，提示实参类型不匹配；或者运行逻辑异常（按键状态读取不对）。

**根因**
- `HAL_GPIO_ReadPin(GPIOx, GPIO_Pin)` 的参数顺序应为 “端口在前、引脚在后”。
- 写反后会导致类型不匹配（或在强转后出现不可预期行为）。

**解决办法**
- 修正为：`HAL_GPIO_ReadPin(key_GPIO_Port, key_Pin);`
- 代码位置：[control.c:L111-L116](file:///i:/AI/w5500/BSP/control.c#L111-L116)

---

## 7）cJSON 报错：JSON string creation failed (Low Memory?)

**现象**
- 串口打印：
  - `[ERROR] Send_All_Device_Status: JSON string creation failed (Low Memory?)`
  - 代码位置：[mqtt_app.c:L71-L115](file:///i:/AI/w5500/BSP/mqtt_app.c#L71-L115)

**根因**
- `cJSON_PrintUnformatted()` 会动态申请内存生成 JSON 字符串。
- 当前 Keil 启动文件中 `Heap_Size` 只有 `0x200`（512B），在频繁 JSON 组包、同时还有其他 `malloc/newlib` 分配时，非常容易失败。
  - 代码位置：[startup_stm32f103xe.s:L38-L47](file:///i:/AI/w5500/MDK-ARM/startup_stm32f103xe.s#L38-L47)

**解决办法（推荐优先级从高到低）**
1. **增大堆（Heap）**
   - 把 `Heap_Size EQU 0x200` 调整为更大值（例如 `0x2000`=8KB，具体取决于 RAM 余量与业务分配峰值）。
   - 修改位置：[startup_stm32f103xe.s:L42](file:///i:/AI/w5500/MDK-ARM/startup_stm32f103xe.s#L42)
2. **降低动态分配压力**
   - 减少发布频率、缩短 JSON、避免同时生成多个 JSON 字符串。
3. **改为预分配打印（长期方案）**
   - 使用 `cJSON_PrintPreallocated()`（需要你提供静态 buffer），从根源上避免 `malloc` 失败导致发布中断。

**验证方法**
- 在“每 2 秒发送全量状态”（见 [sensor.c:L88-L93](file:///i:/AI/w5500/BSP/sensor.c#L88-L93)）的场景下，串口不再出现 Low Memory 报错，MQTTX 能持续收到状态 JSON。

---

## 8）MQTTX 侧与板端联调的最小验收清单（复测用）

1. 板子启动后能看到：
   - `TCP Connected.`
   - `MQTT Protocol Connected!`
   - `Subscribed to topic: device/control`
   - 代码位置：[mqtt_init](file:///i:/AI/w5500/BSP/mqtt_app.c#L287-L334)
2. MQTTX 向 `device/control` 发送控制 JSON 后，板子串口能打印 `Copied payload`，并执行控制逻辑。
3. MQTTX 订阅 `device/data`，能看到：
   - 控制消息的 Echo（如果 payload 不是命令或命令解析失败时会回传，见 [mqtt_app.c:L276-L285](file:///i:/AI/w5500/BSP/mqtt_app.c#L276-L285)）
   - 周期性的温湿度/CO2/全状态上报（见 [sensor.c](file:///i:/AI/w5500/BSP/sensor.c) 与 [mqtt_app.c](file:///i:/AI/w5500/BSP/mqtt_app.c)）

