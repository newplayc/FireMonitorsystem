# ST-Link 烧录问题解决方案

## 问题现象
```
Erase Failed!
Error: Flash Download failed - "Cortex-M3"
```

## 解决方法

### 方法1：检查ST-Link连接
1. 检查ST-Link与开发板的连接线（SWDIO, SWCLK, GND, 3.3V）
2. 确保连接牢固，没有松动
3. 检查ST-Link指示灯是否正常

### 方法2：解除Flash保护
在Keil中操作：
1. 点击 "Flash" -> "Configure Flash Tools"
2. 选择 "Debug" 选项卡
3. 点击 "Settings" 按钮
4. 选择 "Flash Download" 选项卡
5. 点击 "Erase Full Chip" 或 "Erase Sectors"
6. 取消勾选 "Program" 和 "Verify"
7. 只保留 "Erase"，然后点击下载
8. 成功后重新勾选所有选项

### 方法3：复位设置
1. 在 "Debug" 设置中
2. 选择 "Reset and Run"
3. 或选择 "Load Application at Startup"

### 方法4：降低时钟速度
1. 在 "Debug" -> "Settings" 中
2. 降低 "Max Clock" 到 1MHz 或更低

### 方法5：检查芯片
1. 检查芯片是否被读保护
2. 使用 ST-Link Utility 工具解除保护
3. 连接ST-Link，点击 "Target" -> "Option Bytes"
4. 检查读保护状态，如果被保护则解除

### 方法6：重新上电
1. 断开开发板电源
2. 等待5秒
3. 重新上电
4. 再次尝试烧录

### 方法7：检查电源
1. 确保开发板供电充足（至少3.3V）
2. 检查USB供电是否足够
3. 尝试使用外部电源供电

## 最可能的原因
STM32F103C8T6 最常见的问题是：
1. Flash被读保护
2. 供电不足
3. ST-Link连接不稳定

## 推荐操作顺序
1. 先尝试方法6（重新上电）
2. 如果不行，尝试方法2（解除Flash保护）
3. 如果还不行，检查方法5（芯片读保护）
4. 最后尝试方法4（降低时钟速度）
