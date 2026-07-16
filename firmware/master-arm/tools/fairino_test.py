#!/usr/bin/env python3
"""Fairino Robot Direct UDP Test
Sends ServoJ frames directly to robot at 192.168.58.2:20007

Usage:
  python fairino_test.py zero        # Go to master-arm zero pose
  python fairino_test.py home        # Go to home (all zeros)
  python fairino_test.py j1 <deg>    # Move single joint 1
  python fairino_test.py raw "ServoJ({0,0,0,0,0,0},{0,0,0,0},0.1,0.1,0.05,1.0,1.0,0)"
  python fairino_test.py stop        # Stop motion
"""

import socket
import sys
import time

ROBOT_IP = "192.168.58.2"
ROBOT_PORT = 20007

FR_HEAD = "/f/bIII"
FR_DELIM = "III"
FR_TAIL = "/b/f"

CMD_SERVO_J = 376
CMD_SERVO_MOVE_START = 689
CMD_SERVO_MOVE_END = 690
CMD_STOP = 102
CMD_MODE = 303       # Mode(0) = auto, Mode(1) = manual

_count = 0


def pack_frame(cmd_id: int, content: str) -> str:
    global _count
    _count += 1
    frame = f"{FR_HEAD}{_count}{FR_DELIM}{cmd_id}{FR_DELIM}{len(content)}{FR_DELIM}{content}{FR_DELIM}{FR_TAIL}"
    return frame


def send_frame(cmd_id: int, content: str) -> bool:
    frame = pack_frame(cmd_id, content)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(1.0)
    try:
        sock.sendto(frame.encode(), (ROBOT_IP, ROBOT_PORT))
        print(f"[SEND] cmdID={cmd_id} len={len(frame)}")
        print(f"       {content}")
        try:
            data, _ = sock.recvfrom(4096)
            print(f"[RECV] {data.decode('gbk', errors='replace').strip()}")
        except socket.timeout:
            print("[RECV] (no response — normal for ServoJ)")
        return True
    except Exception as e:
        print(f"[ERR] {e}")
        return False
    finally:
        sock.close()


def servo_j(j1, j2, j3, j4, j5, j6, acc=0.0, vel=0.0, cmdT=0.5, filterT=0.0, gain=0.0):
    content = (f"ServoJ({{{j1:.3f},{j2:.3f},{j3:.3f},{j4:.3f},{j5:.3f},{j6:.3f}}},"
               f"{{0.000,0.000,0.000,0.000}},"
               f"{acc:.6f},{vel:.6f},{cmdT:.6f},{filterT:.6f},{gain:.6f},0)")
    return send_frame(CMD_SERVO_J, content)


def stop_motion():
    return send_frame(CMD_STOP, "STOP")


def servo_start():
    return send_frame(CMD_SERVO_MOVE_START, "ServoMoveStart()")


def servo_end():
    return send_frame(CMD_SERVO_MOVE_END, "ServoMoveEnd()")


def set_mode(mode=0):
    """Set robot mode: 0=Auto, 1=Manual"""
    return send_frame(CMD_MODE, f"Mode({mode})")


def move(j1, j2, j3, j4, j5, j6, acc=0.0, vel=0.0, cmdT=0.5):
    """Full ServoJ sequence: Mode → Start → ServoJ → End"""
    print("→ Setting Mode(0) Auto...")
    set_mode(0)
    time.sleep(0.2)
    servo_start()
    time.sleep(0.1)
    servo_j(j1, j2, j3, j4, j5, j6, acc, vel, cmdT)
    time.sleep(cmdT + 0.5)
    servo_end()


# ── Pre-defined poses ──────────────────────────────────────

# Master-arm zero → Fairino target: J1=0, J2=0, J3=-150, J4=0, J5=90, J6=0
# Mapping: M1→FJ1, M2→FJ2, M3→FJ3, M4→FJ5, M5→FJ6, M6→FJ4
ZERO_POSE = [0, 0, -150, 0, 90, 0]

# All zeros
HOME_POSE = [0, 0, 0, 0, 0, 0]


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(0)

    cmd = sys.argv[1].lower()

    if cmd == "zero":
        j = ZERO_POSE
        print(f"→ Zero pose: {j}")
        move(*j)

    elif cmd == "home":
        j = HOME_POSE
        print(f"→ Home: {j}")
        move(*j)

    elif cmd == "stop":
        stop_motion()

    elif cmd == "mode":
        m = 0 if len(sys.argv) < 3 else int(sys.argv[2])
        print(f"→ Set Mode({m}) (0=Auto, 1=Manual)")
        set_mode(m)

    elif cmd == "start":
        servo_start()

    elif cmd == "end":
        servo_end()

    elif cmd == "j1" and len(sys.argv) >= 3:
        j = ZERO_POSE.copy()
        j[0] = float(sys.argv[2])
        print(f"→ J1={j[0]} (rest at zero pose)")
        move(*j)

    elif cmd == "j2" and len(sys.argv) >= 3:
        j = ZERO_POSE.copy()
        j[1] = float(sys.argv[2])
        print(f"→ J2={j[1]} (rest at zero pose)")
        move(*j)

    elif cmd == "j3" and len(sys.argv) >= 3:
        j = ZERO_POSE.copy()
        j[2] = float(sys.argv[2])
        print(f"→ J3={j[2]} (rest at zero pose)")
        move(*j)

    elif cmd == "j4" and len(sys.argv) >= 3:
        j = ZERO_POSE.copy()
        j[3] = float(sys.argv[2])
        print(f"→ J4={j[3]} (rest at zero pose)")
        move(*j)

    elif cmd == "j5" and len(sys.argv) >= 3:
        j = ZERO_POSE.copy()
        j[4] = float(sys.argv[2])
        print(f"→ J5={j[4]} (rest at zero pose)")
        move(*j)

    elif cmd == "j6" and len(sys.argv) >= 3:
        j = ZERO_POSE.copy()
        j[5] = float(sys.argv[2])
        print(f"→ J6={j[5]} (rest at zero pose)")
        move(*j)

    elif cmd == "wave":
        print("→ Waving J1 ±30° (Ctrl+C to stop)")
        servo_start()
        time.sleep(0.1)
        try:
            while True:
                servo_j(30, 0, -150, 0, 90, 0)
                time.sleep(1.0)
                servo_j(-30, 0, -150, 0, 90, 0)
                time.sleep(1.0)
        except KeyboardInterrupt:
            print("\nStopping...")
            servo_end()
            print("Stopped.")

    elif cmd == "raw" and len(sys.argv) >= 3:
        content = sys.argv[2]
        send_frame(CMD_SERVO_J, content)

    else:
        print(f"Unknown command: {cmd}")
        print(__doc__)
