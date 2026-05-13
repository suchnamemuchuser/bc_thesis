import socket
import struct
import os
import time
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
from collections import deque

# --- Configuration ---
HOST = '127.0.0.1'
PORT = 65200
IMAGE_OUT_PATH = '/var/www/html/rt2/spectrogram.png'
TEMP_IMAGE_PATH = '/var/www/html/rt2/spectrogram_temp.png'

# --- Display & Layout Controls ---
SAMPLES_PER_SECOND = 4096  # HEIGHT: How many frequency samples in a 1-second row
DISPLAY_SECONDS = 60       # WIDTH: Max history to show on the X-axis before scrolling
UPDATE_FREQ_SECONDS = 10    # FREQ: How many new seconds to receive before redrawing the image
ONE_SEC_SIZE = 32776

# --- Units & Ranges ---
FREQ_MIN = 1e9             # E.g., 1 GHz
FREQ_MAX = 2e9             # E.g., 2 GHz
INTENSITY_UNIT = "1"       # Placeholder unit for the colorbar

# Protocol Constants
PROTOCOL_MAGIC = 0xAA
PACKET_TYPE_START = 0x01
PACKET_TYPE_DATA = 0x02
PACKET_TYPE_END = 0x03
PACKET_TYPE_INACTIVE = 0x04

# Data Structure Constants
# 8 bytes for timestamp + (Samples * 8 bytes for data)
ROW_SIZE_BYTES = 8 + (SAMPLES_PER_SECOND * 8) 

def recv_exact(sock, num_bytes):
    """Helper to ensure we receive exactly the requested number of bytes."""
    data = bytearray()
    while len(data) < num_bytes:
        packet = sock.recv(num_bytes - len(data))
        if not packet:
            return None # Socket closed
        data.extend(packet)
    return bytes(data)

def save_waterfall_image(data_deque, target_id):
    """Renders the 2D numpy array as a spectrogram and saves it atomically."""
    if len(data_deque) == 0:
        return

    plt.figure(figsize=(12, 6))

    # Convert the deque of rows to a numpy array, then transpose (.T)
    # This turns shape (Time, Frequency) into (Frequency, Time)
    plot_data = np.array(data_deque).T

    # LogNorm crashes if it encounters absolute zero or negative numbers.
    # This clips the floor to a tiny decimal to guarantee it renders properly.
    plot_data = np.clip(plot_data, 1e-5, None)

    # Render the heatmap using LogNorm for logarithmic scaling.
    # The 'extent' parameter maps our data indices to the actual physical ranges.
    plt.imshow(
        plot_data, 
        aspect='auto', 
        cmap='viridis', 
        origin='lower', 
        norm=LogNorm(),
        extent=[0, DISPLAY_SECONDS, FREQ_MIN, FREQ_MAX]
    )

    plt.colorbar(label=f'Signal Intensity [{INTENSITY_UNIT}] (Log Scale)')
    
    # Update titles and labels
    plt.xlabel('Time (Seconds)')
    plt.ylabel('Frequency (GHz)')
    plt.tight_layout()

    # Save and atomically swap
    plt.savefig(TEMP_IMAGE_PATH)
    plt.close()
    os.rename(TEMP_IMAGE_PATH, IMAGE_OUT_PATH)

def prefill_waterfall(waterfall_data):
    """Fills the deque with baseline noise so the image is 60 seconds wide from the start."""
    waterfall_data.clear()
    base_row = [1e-5] * SAMPLES_PER_SECOND
    for _ in range(DISPLAY_SECONDS):
        waterfall_data.append(base_row)

def main():
    # Outer loop strictly to handle disconnections and reconnect attempts
    try:
        while True:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            print(f"Connecting to C server at {HOST}:{PORT}...")
            
            try:
                sock.connect((HOST, PORT))
                print("Connected! Waiting for packets...")
            except ConnectionRefusedError:
                print("Connection refused. Retrying in 3 seconds...")
                time.sleep(3)
                continue
            except Exception as e:
                print(f"Connection error: {e}. Retrying in 3 seconds...")
                time.sleep(3)
                continue

            # Connection established, set up state
            current_target_id = None
            waterfall_data = deque(maxlen=DISPLAY_SECONDS) 
            
            # Immediately pre-fill so the graph is always 'DISPLAY_SECONDS' wide
            prefill_waterfall(waterfall_data)
            
            byte_accumulator = bytearray()
            rows_since_last_update = 0

            # Inner loop for reading and processing packets
            try:
                while True:
                    # 1. Read Header (6 bytes)
                    header_bytes = recv_exact(sock, 6)
                    if not header_bytes:
                        print("Server closed connection. Attempting to reconnect...")
                        break # Break inner loop, trigger reconnect

                    # Unpack: > means Big-Endian
                    magic, pkt_type, value = struct.unpack('>BBi', header_bytes)

                    if magic != PROTOCOL_MAGIC:
                        print(f"Desync! Bad magic byte: {hex(magic)}. Closing connection to resync.")
                        break

                    # 2. Handle Packet Types
                    if pkt_type == PACKET_TYPE_INACTIVE:
                        print("Inactive")
                        continue

                    elif pkt_type == PACKET_TYPE_START:
                        current_target_id = value
                        prefill_waterfall(waterfall_data) # Pre-fill on new recordings
                        byte_accumulator = bytearray()
                        rows_since_last_update = 0
                        print(f"Started new recording. Target ID: {current_target_id}")

                    elif pkt_type == PACKET_TYPE_DATA:
                        payload_len = value
                        payload_bytes = recv_exact(sock, payload_len)
                        if not payload_bytes:
                            break

                        # Add incoming chunk to our accumulator
                        byte_accumulator.extend(payload_bytes)

                        if len(byte_accumulator) < ONE_SEC_SIZE * UPDATE_FREQ_SECONDS:
                            continue

                        # Process all full rows we've accumulated
                        while len(byte_accumulator) >= ROW_SIZE_BYTES:
                            row_bytes = byte_accumulator[:ROW_SIZE_BYTES]
                            del byte_accumulator[:ROW_SIZE_BYTES] 

                            unpack_format = f'<{SAMPLES_PER_SECOND + 1}Q'
                            row_data = struct.unpack(unpack_format, row_bytes)

                            timestamp_ms = row_data[0]
                            print(f"Received second: {timestamp_ms}")
                            samples = row_data[1:] 

                            waterfall_data.append(samples)
                            rows_since_last_update += 1

                            if rows_since_last_update >= UPDATE_FREQ_SECONDS:
                                print(f"Updating web image...")
                                save_waterfall_image(waterfall_data, current_target_id)
                                rows_since_last_update = 0

                    elif pkt_type == PACKET_TYPE_END:
                        print("Recording ended. Saving final image.")
                        if waterfall_data:
                            save_waterfall_image(waterfall_data, current_target_id)

                        # Reset state
                        current_target_id = None
                        prefill_waterfall(waterfall_data)
                        byte_accumulator = bytearray()

            except Exception as e:
                print(f"Socket or processing error: {e}. Attempting to reconnect...")
            finally:
                sock.close()
                time.sleep(1) # Brief pause before the next loop tries to reconnect

    except KeyboardInterrupt:
        print("\nShutting down client...")

if __name__ == "__main__":
    main()