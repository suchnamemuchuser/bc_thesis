import socket
import struct
import os
import numpy as np
import matplotlib.pyplot as plt

# --- Configuration ---
HOST = '127.0.0.1'
PORT = 65200
IMAGE_OUT_PATH = '/var/www/html/rt2/telescope_spectrogram.png'
TEMP_IMAGE_PATH = '/tmp/telescope_temp.png'

# Protocol Constants
PROTOCOL_MAGIC = 0xAA
PACKET_TYPE_START = 0x01
PACKET_TYPE_DATA = 0x02
PACKET_TYPE_END = 0x03
PACKET_TYPE_INACTIVE = 0x04

# Data Structure Constants
SAMPLES_PER_MS = 1024
NUM_STREAMS = 4
TOTAL_SAMPLES = SAMPLES_PER_MS * NUM_STREAMS # 4096
# 8 bytes for timestamp + (4096 * 8 bytes for data) = 32776 bytes per row
ROW_SIZE_BYTES = 8 + (TOTAL_SAMPLES * 8) 

UPDATE_EVERY_N_ROWS = 10 # Overwrite the image every 10 seconds of data

def recv_exact(sock, num_bytes):
    """Helper to ensure we receive exactly the requested number of bytes."""
    data = bytearray()
    while len(data) < num_bytes:
        packet = sock.recv(num_bytes - len(data))
        if not packet:
            return None # Socket closed
        data.extend(packet)
    return bytes(data)

def save_waterfall_image(data_2d_array, target_id):
    """Renders the 2D numpy array as a spectrogram and saves it atomically."""
    if len(data_2d_array) == 0:
        return

    plt.figure(figsize=(12, 6))

    # Render the heatmap
    # aspect='auto' stretches the image to fill the figure
    plt.imshow(data_2d_array, aspect='auto', cmap='viridis', origin='lower')

    plt.colorbar(label='Signal Intensity')
    plt.title(f'Radio Telescope Waterfall - Target ID: {target_id}')
    plt.xlabel('Frequency Bins')
    plt.ylabel('Time (Seconds)')
    plt.tight_layout()

    # Save and atomically swap
    plt.savefig(TEMP_IMAGE_PATH)
    plt.close()
    os.rename(TEMP_IMAGE_PATH, IMAGE_OUT_PATH)

def main():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    print(f"Connecting to C server at {HOST}:{PORT}...")
    try:
        sock.connect((HOST, PORT))
    except ConnectionRefusedError:
        print("Connection refused. Is the C server running?")
        return

    print("Connected! Waiting for packets...")

    current_target_id = None
    waterfall_data = [] 
    byte_accumulator = bytearray()
    rows_since_last_update = 0

    try:
        while True:
            # 1. Read Header (6 bytes)
            header_bytes = recv_exact(sock, 6)
            if not header_bytes:
                print("Server closed connection.")
                break

            # Unpack: > means Big-Endian (because of htonl)
            # B = unsigned char (1 byte), B = unsigned char (1 byte), I = unsigned int (4 bytes)
            magic, pkt_type, value = struct.unpack('>BBI', header_bytes)

            if magic != PROTOCOL_MAGIC:
                print(f"Desync! Bad magic byte: {hex(magic)}. Closing connection.")
                break

            # 2. Handle Packet Types
            if pkt_type == PACKET_TYPE_INACTIVE:
                continue

            elif pkt_type == PACKET_TYPE_START:
                current_target_id = value
                waterfall_data = [] 
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

                # Process all full rows we've accumulated
                while len(byte_accumulator) >= ROW_SIZE_BYTES:
                    # Extract one row's worth of bytes
                    row_bytes = byte_accumulator[:ROW_SIZE_BYTES]
                    del byte_accumulator[:ROW_SIZE_BYTES] # Remove processed bytes

                    # 1 timestamp + 4096 samples = 4097 Qs
                    unpack_format = f'<{TOTAL_SAMPLES + 1}Q'
                    row_data = struct.unpack(unpack_format, row_bytes)

                    timestamp_ms = row_data[0]
                    samples = row_data[1:] # Slice off the timestamp for the image

                    waterfall_data.append(samples)
                    rows_since_last_update += 1

                    if rows_since_last_update >= UPDATE_EVERY_N_ROWS:
                        print(f"Updating web image... (Rows: {len(waterfall_data)})")
                        save_waterfall_image(np.array(waterfall_data), current_target_id)
                        rows_since_last_update = 0

            elif pkt_type == PACKET_TYPE_END:
                print("Recording ended. Saving final image.")
                save_waterfall_image(np.array(waterfall_data), current_target_id)

                # Reset state
                current_target_id = None
                waterfall_data = [] 
                byte_accumulator = bytearray()

    except KeyboardInterrupt:
        print("\nShutting down client...")
    finally:
        sock.close()

if __name__ == "__main__":
    main()
