# W5500 第一阶段 Step 2：STM32 SPI 与 GPIO 配置（CubeMX/HAL）

目标：确保 STM32 侧 SPI 模式与引脚电气配置正确，让“读 VERSIONR=0x04”成为可重复的结果。

本工程以 STM32F103 + HAL 为例，当前使用 `SPI1 + 软件片选(GPIO)`。

## 1. SPI 必须满足的 4 个条件

- 模式：Master
- 数据位宽：8-bit
- 时钟模式：Mode 0（CPOL=0，CPHA=0）
- 片选：NSS=Soft（CS 由 GPIO 手动拉低/拉高）

## 2. 对照当前工程的 SPI1 配置

SPI1 初始化参数在：
- [spi.c](file:///i:/AI/w5500/Core/Src/spi.c#L30-L60)

关键字段（你现在的配置是正确的）：
- `hspi1.Init.Mode = SPI_MODE_MASTER;`
- `hspi1.Init.Direction = SPI_DIRECTION_2LINES;`
- `hspi1.Init.DataSize = SPI_DATASIZE_8BIT;`
- `hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;`
- `hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;`
- `hspi1.Init.NSS = SPI_NSS_SOFT;`
- `hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;`
- `hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;`

## 3. 引脚复用与电气配置（非常关键）

SPI1 管脚在：
- [spi.c](file:///i:/AI/w5500/Core/Src/spi.c#L74-L89)

当前工程对 GPIO 的配置：
- SCK(PA5) + MOSI(PA7)：复用推挽输出 `GPIO_MODE_AF_PP`
- MISO(PA6)：输入 `GPIO_MODE_INPUT` 且 `GPIO_NOPULL`

建议你理解这个选择：
- MISO 由 W5500 驱动输出，因此 MCU 侧必须是输入
- 不建议给 MISO 上拉/下拉（除非你需要在 MISO 悬空时稳定读到某个值用于自检）

## 4. CS（片选）必须是 GPIO 输出

### 4.1 当前工程的 CS 配置

CS 定义：
- [main.h](file:///i:/AI/w5500/Core/Inc/main.h#L59-L62)

CS GPIO 初始化与默认拉高：
- [gpio.c](file:///i:/AI/w5500/Core/Src/gpio.c#L52-L61)

CS 拉低/拉高函数：
- [bsp_w5500.c](file:///i:/AI/w5500/BSP/bsp_w5500.c#L36-L44)

### 4.2 CS 的验收要点

- 上电后 CS 默认应为高（不选中）
- 每次读写寄存器：
  - CS 拉低 → SPI 事务 → CS 拉高

用逻辑分析仪看 CS 翻转是最快的判定方式（阶段 1 的 Step 6 会细讲）。

## 5. SPI 速率怎么选（先保守后提高）

你当前 `PRESCALER_8` 属于比较稳妥的配置。

如果后续要提高吞吐：
- 先用逻辑分析仪确认边沿/时序余量
- 再逐级提高（例如 8 → 4 → 2）
- 提速后必须回归测试：读 VERSIONR + 连续收发 TCP/UDP

## 6. 常见错误与快速判断

- 错误 1：SPI 模式不对（CPHA/CPOL）
  - 表现：`VERSIONR` 读不到 0x04（经常 0x00/0xFF）
- 错误 2：MOSI/MISO 接反
  - 表现：MISO 一直固定电平，读值固定
- 错误 3：CS 没拉低或拉错脚
  - 表现：SPI 有波形但 W5500 不响应（读值异常）

最硬的判断标准：
- `getVERSIONR()` 必须稳定读到 `0x04`

## 7. Step 2 的验收（你做完要得到什么）

- 读 `VERSIONR` 连续 100 次都等于 `0x04`
- 串口打印稳定：`W5500 VERSIONR=0x04 ...`

达到这个标准再进入 Step 3（ioLibrary 移植与回调注册）。

