# RFID 外接设备功能对比

以当前应用代码为准，描述应用已接入的能力，不代表芯片的全部硬件能力。✓：已接入；△：有限支持或依赖固件/标签；—：未接入；?：需专项验证。除 PN532Killer 的 USB 连接、MFkey32 及 Staticnested 扇区遍历/密钥恢复外，其余项目不表示本次已完成实机验证。

## LF：低频读卡器

当前应用未接入独立 LF 读卡器驱动。设备的低频模拟能力不等于应用已支持低频读卡。

| 读卡器类型 | 连接方式 | 识别/读卡 | Dump | 写卡 | 模拟 |
|---|---|---|---|---|---|
| 独立 LF 读卡器 | 未接入 | — | — | — | — |

## HF：高频 NFC 读卡器

### 连接与读卡

| 外接设备 | 连接方式 | 扫卡/UID | Mifare Classic Dump | Ultralight/NTAG Dump | ISO15693 Dump | FeliCa | DESFire Dump |
|---|---|---|---|---|---|---|---|
| PN532Killer | USB 串口 / UART，115200 | ✓ | ✓ | ✓ | △ 固件相关 | — | — |
| 标准 PN532（USB 转串口） | USB 串口，115200 | ✓ | ✓ | ✓ | — | — | — |
| 标准 PN532（UART 模块） | UART，115200 | ✓ | ✓ | ✓ | — | — | — |
| M5 NFC Unit（ST25R3916B） | Grove I²C，0x50 | ✓ | ✓ | ✓ | ✓ | — | — |
| GroveNFC | Grove I²C，0x48 | ✓ | ✓ | ✓ | ✓ | △ 识别/读取路径 | — |
| M5 NFC CAP（ST25R3916） | SPI，spidev | ✓ | △ Gen1A 路径 | ✓ | ✓ | — | — |

### 写卡与模拟

| 外接设备 | 写回标签 | 修改 UID | 卡模拟 | NDEF URL 模拟 | 保存/导出 |
|---|---|---|---|---|---|
| PN532Killer | ✓ | Gen1A/2/3/4 | ✓ 硬件槽位 | — | ✓ |
| 标准 PN532（USB 转串口） | △ 按协议 | Gen1A/2/3/4 命令路径，需验证 | — | ✓ | ✓ |
| 标准 PN532（UART 模块） | △ 按协议 | 同标准 PN532 | — | ✓ | ✓ |
| M5 NFC Unit（ST25R3916B） | △ 按协议 | Gen1A/Gen4；Gen3 禁用 | △ NFC-A/NTAG/MFC listener | — | ✓ |
| GroveNFC | △ 按协议 | ? | ✓ NTAG/MFC1K/China2/15693 | — | ✓ |
| M5 NFC CAP（ST25R3916） | △ 按协议 | ? | △ NFC-A listener | — | ✓ |

### Mifare Classic 工具

这些工具统一归入 HF / Mifare Classic，不放在 LF、UHF 的功能表中。此处的支持情况指应用已接入相应执行路径。

| 外接设备 | 字典认证 | MFkey32 | MFkey64 | Darkside | Hardnested | Staticnested |
|---|---|---|---|---|---|---|
| PN532Killer | ✓ | ✓ 无卡 | ✓ 有卡 | — | — | △ 静态 nonce + 已知密钥 |
| 标准 PN532（USB 转串口） | ✓ | — | — | — | — | — |
| 标准 PN532（UART 模块） | ✓ | — | — | — | — | — |
| M5 NFC Unit（ST25R3916B） | ✓ | ✓ 无卡 listener | — | — | — | — |
| GroveNFC | ✓ | — | — | — | — | — |
| M5 NFC CAP（ST25R3916） | △ 按读卡路径 | — | — | — | — | — |

## UHF：超高频读卡器

| 外接设备 | 连接方式 | EPC/TID 识别 | 连续/单次盘点 | 内存读取 | 内存写入 | 保存/导出 |
|---|---|---|---|---|---|---|
| CZUHF / FZUHF / 兼容 UHF Reader | USB 串口 / UART；115200、9600 探测 | ✓ | ✓ | ✓ Reserved/EPC/TID/User | ✓ EPC/TID/User，受标签权限限制 | ✓ JSON/CSV |

## 连接与使用限制

- PN532Killer 与部分 UHF 桥共用 `1A86:55D3`。应用优先使用 USB 产品名称区分，不能只按 VID/PID 判断。
- 标准 PN532 的 USB 接口指 USB 转串口，当前应用不使用原生 USB/libnfc；UART 模块使用同一 PN532 串口协议。
- UART 接线、电平和扩展口供电需匹配设备；UART 配置页可设置端口和波特率。
- I²C 设备按地址和协议识别；SPI 使用 NFC CAP 驱动。ST25R 的 NFC-A listener 不等于完整硬件槽位模拟。
- NFC Unit 和 NFC CAP 的 ISO15693 **标签模拟未接入**；读取 ISO15693 与模拟是不同功能。
- PN532Killer 卡模拟使用硬件槽位；标准 PN532 的 NDEF URL 使用独立目标模式。界面不会把 Killer 当作标准 PN532 的 NDEF 模拟设备。
- Staticnested：PN532Killer 先使用内置默认密钥和启用的 MIFARE Keys（显式禁用项除外）遍历 Mini/1K/2K/4K 的扇区 A/B，再用认证成功的密钥作为种子恢复未知项；候选密钥必须在原 UID 卡片上重新认证。仅支持参考辅助程序识别的静态 nonce，不适用于所有 Classic 卡。
- MFkey32 需要至少两组可配对的真实认证数据；MFkey64 需要有卡认证数据。密钥计算在 CardputerZero 上由辅助程序执行。
- UHF 内存保存会记录各 bank 的完整性和错误状态；受密码保护或读取截断时保留部分数据，不表示完整 Dump。TID 通常只读，写入能力取决于标签。
- 所有 NFC 设备的 DESFire 完整 Dump、Darkside、Hardnested 当前不能仅因菜单或枚举存在就认定支持；本表不将其列为已实现能力。
- Saved 浏览、重命名、删除、十六进制编辑、密钥字典管理属于应用公共功能；写回/上传仍受设备和协议限制。MFC Dump 隐藏 ASCII 栏，Ultralight/ISO15693 保留。

## 代码核对入口

- `main/ui/components/nfc/nfc_transport.hpp`：设备枚举、USB 身份、串口/I²C/SPI 与供电。
- `main/ui/components/nfc/nfc_device_service.hpp`：读写路由、UID 写入限制、模拟、MFkey、UHF bank 操作。
- `main/ui/components/nfc/nfc_i2c_device.hpp`、`nfc_spi_device.hpp`：I²C/SPI 具体读卡和 listener 实现。
- `main/ui/components/page_app/ui_app_nfc.hpp`：用户可进入的功能和设备能力限制。
