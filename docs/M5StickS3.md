# M5StickS3

## 规格参数

| Specification         | Parameter                                                                                                                                     |
| --------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| SoC                   | ESP32-S3-PICO-1-N8R8 @ Dual-core Xtensa LX7 processor, up to 240MHz                                                                           |
| Flash                 | 8MB                                                                                                                                           |
| PSRAM                 | 8MB                                                                                                                                           |
| Wi-Fi                 | 2.4 GHz Wi-Fi                                                                                                                                 |
| Display               | Model: ST7789P3<br>Resolution: 135x240                                                                                                        |
| Input Power           | USB Type-C DC 5V                                                                                                                              |
| Audio Codec           | ES8311: 24-bit resolution, I2S protocol                                                                                                       |
| Microphone            | MEMS microphone, Signal-to-Noise Ratio (SNR): 65 dB                                                                                           |
| Speaker               | AW8737 power amplifier + 8Ω@1W 2011 cavity speaker                                                                                            |
| Operating Temperature | 0 ~ 40°C                                                                                                                                      |
| Battery Capacity      | 250mAh                                                                                                                                        |
| Grove Load Capacity   | No load: 5V<br>Max: <4.88V@0.38A>                                                                                                             |
| Power Consumption     | Power off: <4.2V@14.02uA><br>L1 state: <4.2V@52.47uA><br>L2 state: <4.2V@102.40uA><br>L3A state: <4.2V@36.69mA><br>Full load: <4.2V@519.02mA> |
| Product Size          | 48.0 x 24.0 x 15.0mm                                                                                                                          |
| Product Weight        | 20.0g                                                                                                                                         |
| Package Size          | 110.0 x 65.0 x 15.0mm                                                                                                                         |
| Gross Weight          | 29.3g                                                                                                                                         |

## 按键定义

| 名称 | 位置 | M5Unified API |
|------|------|---------------|
| BtnA（key1） | 正面大按键，屏幕下方 | `M5.BtnA` |
| BtnB（key2） | 侧面小按键，机身右侧 | `M5.BtnB` |

按键通过 M5PM1 电源管理芯片管理，非直连 ESP32S3 GPIO。代码中调用 `M5.update()` 更新状态后，使用 `M5.BtnA.isPressed()` / `M5.BtnB.isPressed()` 读取即可。

## 参考文档

- [M5StickS3 官方文档](https://docs.m5stack.com/en/core/StickS3)
- [原理图 PDF](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/1207/K150_Stick_S3_PRJ_V0.6_20251111_2025_11_17_16_10_24.pdf)
