# W5500 第一阶段 Step 3：ioLibrary 移植与回调注册（CS/SPI/临界区）

目标：让 WIZnet ioLibrary 知道“怎么在你的 MCU 上操作 W5500”。核心就是注册 3 类回调：
- CS 选择/取消选择（片选）
- SPI 单字节读写（或 burst 读写）
- 临界区 enter/exit（避免 CS/SPI 事务被中断打断）

完成本步骤后，你应该能稳定完成：
- `wizchip_init()` 正常返回
- `getVERSIONR() == 0x04`
- `wizchip_setnetinfo()` 写入后 `wizchip_getnetinfo()` 读回一致

## 1. ioLibrary 的“平台无关”和“平台相关”

### 1.1 平台无关部分

这部分是 WIZnet 提供的通用逻辑：
- Socket API（TCP/UDP）
- DHCP/DNS/SNTP/HTTP 等协议模块
- 寄存器宏定义、状态机、缓冲区读写流程

### 1.2 平台相关部分

这部分必须由你提供：
- 用什么 SPI 外设发字节
- 用哪个 GPIO 当 CS
- 临界区怎么做（关中断/互斥锁）

## 2. 本工程的回调注册位置（你现在用的实现）

回调注册集中在：
- [bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L40-L58)

### 2.1 CS 回调

实现：
- `CS_LOW()` / `CS_HIGH()` 控制 PA3
- 位置：[bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L36-L44)

注册：
- `reg_wizchip_cs_cbfunc(CS_LOW, CS_HIGH);`
- 位置：[bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L52-L53)

验收：
- 逻辑分析仪上能看到每个 SPI 事务开始 CS 拉低、结束 CS 拉高

### 2.2 SPI 回调（单字节读写）

底层字节收发：
- `Spi1_read_write_byte()` 使用 `HAL_SPI_TransmitReceive()`
- 位置：[bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L47-L53)

ioLibrary 需要的回调形式：
- `uint8_t (*spi_rb)(void)`：读 1 字节
- `void (*spi_wb)(uint8_t)`：写 1 字节

本工程做法：
- `wiz_spi_readbyte()`：发送 dummy `0x00`，读回 1 字节
- `wiz_spi_writebyte()`：发送 1 字节（读回丢弃）
- 位置：[bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L27-L35)

注册：
- `reg_wizchip_spi_cbfunc(wiz_spi_readbyte, wiz_spi_writebyte);`
- 位置：[bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L54-L55)

验收：
- `getVERSIONR()` 稳定为 `0x04`

### 2.3 临界区回调（强烈建议）

为什么需要临界区：
- W5500 的一次寄存器读写是“CS 拉低 → 连续发/收多字节 → CS 拉高”
- 如果中途被中断抢占且中断里也访问 W5500，就可能导致 CS/时序混乱

本工程实现：
- `wiz_cris_enter()`：保存 PRIMASK，禁用全局中断；支持嵌套计数
- `wiz_cris_exit()`：嵌套归零后按原状态恢复
- 位置：[bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L4-L25)

注册：
- `reg_wizchip_cris_cbfunc(wiz_cris_enter, wiz_cris_exit);`
- 位置：[bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L51-L51)

验收：
- 高频连续读写寄存器时不会偶发读错（比如连续读 1000 次 VERSIONR 都正确）

## 3. wizchip_init(txsize, rxsize) 的意义（必须理解）

W5500 有 8 个 socket，每个 socket 的 TX/RX buffer 需要分配大小（单位 KB）。

本工程分配为每个 socket：
- TX 2KB * 8
- RX 2KB * 8

代码位置：
- [bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L40-L58)

建议理解：
- buffer 分配会影响吞吐与并发能力
- 不同项目可不同分配（例如只用 1 个 socket 就可以把更多 buffer 给它）

## 4. 读寄存器的“最终落点”（你要能追到这里）

当你调用 `getVERSIONR()`：
- 最终会走到 `WIZCHIP_READ()`（它会操作 CS，并通过 SPI 回调发 3 字节头再读数据）
- 位置：[w5500.c](file:///i:/AI/w5500/BSP/w5500.c#L65-L89)

验收：
- 你能用逻辑分析仪对应到这一次事务（阶段 1 Step 6 会做）

## 5. Step 3 的验收（你做完要得到什么）

- 读 `VERSIONR` 稳定为 `0x04`
- `ctlwizchip(CW_GET_PHYLINK, ...)` 返回 Link ON
- `wizchip_setnetinfo()` 写入后，`wizchip_getnetinfo()` 读回一致

通过后进入 Step 4（静态 IP 配置与寄存器对应）。

