import serial
import csv
import time

PORT = "COM5"
BAUD = 115200
DURATION = 30
OUTPUT_FILE = "imu_log.csv"


def main():
    print(f"Opening {PORT} at {BAUD} baud...")
    ser = serial.Serial(PORT, BAUD, timeout=1)
    time.sleep(2)

    start_time = time.time()

    with open(OUTPUT_FILE, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "time_s",
            "ax", "ay", "az",
            "gx_cal", "gy_cal", "gz_cal",
            "mx", "my", "mz"
        ])

        print(f"Recording {DURATION} seconds to {OUTPUT_FILE}...")
        print("Keep IMU still if you are doing static test.")

        while time.time() - start_time < DURATION:
            line = ser.readline().decode(errors="ignore").strip()

            if not line:
                continue

            if (
                line.startswith("XIAO")
                or line.startswith("Gyro")
                or line.startswith("Keep")
                or line.startswith("Calibration")
                or line.startswith("gyroBias")
                or line.startswith("ax")
            ):
                print(line)
                continue

            parts = line.split(",")

            if len(parts) != 9:
                continue

            try:
                values = [float(x) for x in parts]
            except ValueError:
                continue

            t = time.time() - start_time
            writer.writerow([round(t, 3)] + values)
            print(round(t, 3), values)

    ser.close()
    print("Done.")
    print(f"Saved file: {OUTPUT_FILE}")


if __name__ == "__main__":
    main()
