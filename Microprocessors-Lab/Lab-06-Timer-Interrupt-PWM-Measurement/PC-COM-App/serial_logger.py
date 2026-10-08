import serial

PORT = "COM5"
BAUD_RATE = 115200
OUTPUT_FILE = "time_log.txt"

with serial.Serial(
    port=PORT,
    baudrate=BAUD_RATE,
    bytesize=8,
    parity="N",
    stopbits=1,
    timeout=1
) as ser, open(OUTPUT_FILE, "a", encoding="utf-8") as file:

    print("Serial logger started...")
    print(f"Reading from {PORT} at {BAUD_RATE}")
    print(f"Saving to {OUTPUT_FILE}")
    print("Press Ctrl+C to stop.\n")

    try:
        while True:
            line = ser.readline().decode("utf-8", errors="ignore").strip()

            if not line:
                continue

            print(line)
            file.write(line + "\n")
            file.flush()

    except KeyboardInterrupt:
        print("\nLogger stopped.")