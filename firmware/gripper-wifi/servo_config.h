// ===== 舵机配置 / Servo Configuration =====
// 对应 Python 版 gripper_grasp.py 的配置参数

#ifndef SERVO_CONFIG_H
#define SERVO_CONFIG_H

// --- 硬件引脚 / Hardware Pins ---
#define SERVO_RX_PIN  18    // 直连: ESP32 RX ← → Bus Adapter RX
#define SERVO_TX_PIN  17    // 直连: ESP32 TX ← → Bus Adapter TX
#define SERVO_ID       1    // STS3215 舵机 ID

// --- 串口 / UART ---
#define SERVO_BAUDRATE 1000000  // 1M bps (舵机默认)

// --- 夹爪限位 / Gripper Limits (已标定) ---
#define POS_OPEN       0      // 全开 / Fully open (84mm)
#define POS_CLOSE      3800   // 全闭 / Fully closed (0mm)

// --- 运动参数 / Motion Parameters ---
#define SERVO_SPEED    800    // 速度 (0-2400)
#define SERVO_ACC      200    // 加速度
#define TORQUE_LIMIT   500    // 力矩限制 0-1000 (默认1000即100%%, 500=50%%)

// --- 夹取检测 / Grasp Detection ---
#define GRIP_LOAD_THRESHOLD  1320   // 负载超过此值 = 夹到物体 / Object contact
#define GRIP_POS_THRESHOLD   100    // 离目标此范围内 = 完全闭合 / Fully closed
#define GRIP_TIMEOUT_MS      5000   // 夹取超时 (ms)
#define GRIP_POLL_MS         50     // 轮询间隔 (ms)
#define MIN_MOVEMENT         30     // 最小位移量 (认为开始动了)
#define OVERLOAD_THRESHOLD   1350   // 过载保护: 静止时负载>此值自动张开

#endif
