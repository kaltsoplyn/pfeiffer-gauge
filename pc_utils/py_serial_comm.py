
import serial
import json
import threading
import time
from collections import deque

# --- Configuration ---
ESP_SERIAL_PORT = "COM6"  # <<<<<<< IMPORTANT: Use correct ESP32-C6 COM port
BAUD_RATE = 115200
MAX_LOG_SIZE = 1000 # Maximum number of data points to keep in memory

# --- Shared Data ---
# Using a deque for efficient appends and optionally for capping size
sensor_data_log = deque(maxlen=MAX_LOG_SIZE)
data_lock = threading.Lock()
stop_event = threading.Event()

def serial_reader_thread_func(port, baudrate, data_list, lock, stop_event_flag):
    """
    Thread function to read serial data, parse JSON, and append to a shared list.
    """
    ser = None
    while not stop_event_flag.is_set():
        try:
            if ser is None or not ser.is_open:
                print(f"Attempting to connect to {port} at {baudrate} baud...")
                # For ESP32-C6 native USB, DTR/RTS handling might be important for resets or bootloader
                # For general communication after boot, it might not be strictly needed.
                ser = serial.Serial(port=None, baudrate=baudrate, timeout=1) # Open later
                ser.port = port
                # ser.dtr = False # Data Terminal Ready - uncomment if connection issues
                # ser.rts = False # Request To Send - uncomment if connection issues
                ser.open()
                print(f"Connected to {ser.name}")
                time.sleep(0.1) # Small delay after opening

            if ser.in_waiting > 0:
                line_bytes = ser.readline()
                if line_bytes:
                    try:
                        line_str = line_bytes.decode('utf-8').strip()
                        if line_str: # Ensure it's not an empty line after strip
                            data_point = json.loads(line_str)
                            with lock:
                                data_list.append(data_point)
                            # print(f"Logged: {data_point}") # Uncomment for verbose logging
                    except json.JSONDecodeError as e:
                        print(f"JSON Decode Error: {e} - Received: '{line_str}'")
                    except UnicodeDecodeError as e:
                        print(f"Unicode Decode Error: {e} - Received bytes: {line_bytes}")
                    except Exception as e:
                        print(f"An unexpected error occurred while processing data: {e}")
            else:
                # No data, sleep briefly to avoid busy-waiting if timeout is short or None
                time.sleep(0.01)

        except serial.SerialException as e:
            print(f"Serial Error: {e}. Reconnecting in 5 seconds...")
            if ser and ser.is_open:
                ser.close()
            ser = None # Reset ser object to trigger re-open attempt
            time.sleep(5)
        except Exception as e:
            print(f"An unexpected error occurred in reader thread: {e}")
            # Potentially add a delay here too before retrying
            time.sleep(1)


    if ser and ser.is_open:
        ser.close()
    print("Serial reader thread stopped.")

def main():
    print("Starting ESP32 Data Logger...")
    print(f"Logging data to a list with max size: {MAX_LOG_SIZE}")

    # Start the serial reader thread
    reader_thread = threading.Thread(
        target=serial_reader_thread_func,
        args=(ESP_SERIAL_PORT, BAUD_RATE, sensor_data_log, data_lock, stop_event),
        daemon=True # Daemon threads exit when the main program exits
    )
    reader_thread.start()

    print("Reader thread started. Press Ctrl+C to stop.")

    try:
        while True:
            time.sleep(5) # Main thread can do other work or just periodically check data
            with data_lock:
                current_log_size = len(sensor_data_log)
                if current_log_size > 0:
                    latest_entry = sensor_data_log[-1] # Get the most recent entry
                    print(f"\n--- Log Status ({time.strftime('%H:%M:%S')}) ---")
                    print(f"Logged data points: {current_log_size}")
                    print(f"Latest entry: {latest_entry}")
                else:
                    print(f"\n--- Log Status ({time.strftime('%H:%M:%S')}) ---")
                    print("No data logged yet.")

    except KeyboardInterrupt:
        print("\nStopping application...")
    finally:
        stop_event.set()
        if reader_thread.is_alive():
            print("Waiting for reader thread to finish...")
            reader_thread.join(timeout=5) # Wait for the thread to close
        print("Application stopped.")

if __name__ == "__main__":
    main()
