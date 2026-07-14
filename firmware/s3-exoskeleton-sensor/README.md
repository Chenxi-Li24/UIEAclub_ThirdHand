# ESP32-S3 六轴外骨骼角度传感器

GPIO1-GPIO6 分别连接 6 个 GT 模拟磁角度传感器。固件把 0-3.3V 映射为 0-360°，再按每路零点和正方向转换为 -180° 到 180° 的关节角，并通过 WiFi/UDP 发送给 P4 控制器。

角度公式：`joint = wrap(direction * (voltage / 3.3 * 360 - zero))`

## 配置

WiFi、S3 静态 IP、P4 地址和网页代理地址位于 `sensor_config.h`。默认 S3 为 `192.168.58.101`，P4 为 `192.168.58.100:20008`，网页代理为 `192.168.58.38:20010`。

## 板载 RGB LED

| 灯色 | 状态 |
|---|---|
| 红色 | 固件启动 |
| 黄色闪烁 | 正在连接 WiFi |
| 蓝色闪烁 | WiFi 已连接，但网页代理未回复 `WEB_ACK` |
| 绿色常亮 | S3 直连网页遥测正常，P4 未确认 |
| 青色常亮 | 网页直连与 P4 链路均正常 |
| 紫色常亮 | 两条链路正常，且 P4 已开启外骨骼驱动机械臂 |

默认使用 ESP32-S3 Dev Module 的 GPIO48 板载 WS2812。串口输入 `net show` 可查看 WiFi IP、RSSI 和 P4 确认包时间。

## 校准

打开 115200 波特率串口，输入：

```text
cal show
cal zero 1
cal zero 1 123.4
cal dir 1 ccw
cal dir 1 cw
cal reset
net show
```

`cal zero 1` 会把第 1 路当前位置保存为零点；`ccw` 表示逆时针为角度增加方向，`cw` 表示顺时针为增加方向。设置保存在 ESP32 NVS，重启后仍有效。

串口状态输出使用非阻塞写入，未打开串口或上位机来不及读取时会跳过日志，不会阻塞 50ms 传感器与 UDP 循环。WiFi 省电已关闭，断线后每 2 秒请求重连；`net show` 会显示当前发送序列、P4/Web 最近确认序列、确认时延和 UDP 发送失败计数。

## 编译烧录

```bash
arduino-cli compile --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi" firmware/s3-exoskeleton-sensor
arduino-cli upload -p COM22 --fqbn "esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=8M,PSRAM=opi" firmware/s3-exoskeleton-sensor
```

同一份 UDP 数据会分别发送给 P4 和网页 Node 代理，因此网页数据显示不依赖 P4。UDP 数据格式：

```text
EXO:sequence,a1,a2,a3,a4,a5,a6,mv1,mv2,mv3,mv4,mv5,mv6
```

串口以 115200 波特率每 200ms 输出一行易读状态，UDP 仍保持 50ms 周期：

```text
[EXO] #123 | J1=-21.60deg (3102mV) | J2=-21.60deg (3102mV) | ... | J6=70.91deg (650mV)
```
