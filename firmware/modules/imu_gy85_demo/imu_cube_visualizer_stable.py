import serial
import time
import math
import numpy as np
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

PORT = "COM5"
BAUD = 115200

# Larger value = smoother but slower response.
SMOOTH_ALPHA = 0.85

ref_g = None
smooth_g = None
cube_obj = None


def parse_line(line):
    parts = line.strip().split(",")
    if len(parts) != 9:
        return None
    try:
        return [float(x) for x in parts]
    except ValueError:
        return None


def normalize(v):
    n = np.linalg.norm(v)
    if n < 1e-6:
        return v
    return v / n


def rotation_from_vectors(a, b):
    """Return rotation matrix that rotates vector a to vector b."""
    a = normalize(a)
    b = normalize(b)

    v = np.cross(a, b)
    c = np.dot(a, b)

    if c > 0.9999:
        return np.eye(3)

    if c < -0.9999:
        axis = np.array([1.0, 0.0, 0.0])
        if abs(a[0]) > 0.9:
            axis = np.array([0.0, 1.0, 0.0])
        v = normalize(np.cross(a, axis))
        K = np.array([
            [0, -v[2], v[1]],
            [v[2], 0, -v[0]],
            [-v[1], v[0], 0]
        ])
        return np.eye(3) + 2 * K @ K

    s = np.linalg.norm(v)

    K = np.array([
        [0, -v[2], v[1]],
        [v[2], 0, -v[0]],
        [-v[1], v[0], 0]
    ])

    R = np.eye(3) + K + K @ K * ((1 - c) / (s ** 2))
    return R


def make_box_vertices():
    L = 2.0
    W = 1.0
    H = 0.35

    return np.array([
        [-L/2, -W/2, -H/2],
        [ L/2, -W/2, -H/2],
        [ L/2,  W/2, -H/2],
        [-L/2,  W/2, -H/2],
        [-L/2, -W/2,  H/2],
        [ L/2, -W/2,  H/2],
        [ L/2,  W/2,  H/2],
        [-L/2,  W/2,  H/2],
    ])


FACES = [
    [0, 1, 2, 3],
    [4, 5, 6, 7],
    [0, 1, 5, 4],
    [2, 3, 7, 6],
    [1, 2, 6, 5],
    [0, 3, 7, 4],
]


def draw_box(ax, R):
    global cube_obj

    if cube_obj is not None:
        cube_obj.remove()

    vertices = make_box_vertices()
    rotated = vertices @ R.T

    face_vertices = [[rotated[i] for i in face] for face in FACES]

    cube_obj = Poly3DCollection(
        face_vertices,
        alpha=0.65,
        edgecolor="black",
        linewidth=1.0
    )

    ax.add_collection3d(cube_obj)


def on_key(event):
    global ref_g, smooth_g

    if event.key == "r" and smooth_g is not None:
        ref_g = smooth_g.copy()
        print("Reference reset.")


def main():
    global ref_g, smooth_g

    print(f"Opening {PORT} at {BAUD} baud...")
    print("Close Arduino Serial Monitor first.")
    print("This stable version uses gravity vector for tilt visualization.")
    print("Yaw is locked to avoid gyro drift.")
    print("Press R in the figure window to reset reference.")
    print()

    ser = serial.Serial(PORT, BAUD, timeout=0.02)
    time.sleep(2)

    plt.ion()
    fig = plt.figure("ThirdHand IMU Stable Cube Visualizer")
    ax = fig.add_subplot(111, projection="3d")
    fig.canvas.mpl_connect("key_press_event", on_key)

    ax.set_xlim(-2, 2)
    ax.set_ylim(-2, 2)
    ax.set_zlim(-2, 2)
    ax.set_xlabel("X")
    ax.set_ylabel("Y")
    ax.set_zlabel("Z")
    ax.set_box_aspect([1, 1, 1])
    ax.view_init(elev=22, azim=35)

    while plt.fignum_exists(fig.number):
        line = ser.readline().decode(errors="ignore").strip()
        data = parse_line(line)

        if data is None:
            plt.pause(0.001)
            continue

        ax_m, ay_m, az_m, gx, gy, gz, mx, my, mz = data

        current_g = normalize(np.array([ax_m, ay_m, az_m], dtype=float))

        if smooth_g is None:
            smooth_g = current_g
        else:
            smooth_g = normalize(SMOOTH_ALPHA * smooth_g + (1 - SMOOTH_ALPHA) * current_g)

        if ref_g is None:
            ref_g = smooth_g.copy()

        R = rotation_from_vectors(ref_g, smooth_g)

        dot_value = float(np.clip(np.dot(ref_g, smooth_g), -1.0, 1.0))
        tilt_angle = math.degrees(math.acos(dot_value))

        draw_box(ax, R)

        ax.set_title(
            f"Stable tilt visualization | tilt={tilt_angle:.1f}° | yaw locked"
        )

        plt.pause(0.001)

    ser.close()


if __name__ == "__main__":
    main()
