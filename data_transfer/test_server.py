import socket
import struct
import time

MAGIC = 0xAA
TYPE_START = 0x01
TYPE_DATA = 0x02
TYPE_END = 0x03

def start_mock_server(host='127.0.0.1', port=55555):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        s.bind((host, port))
        s.listen()

        print(f"Server listening on {host}:{port}...")

        conn, addr = s.accept()
        with conn:
            print(f"Client connected from {addr}")

            print("Sending START packet...")
            timestamp = int(time.time())
            target_name = b"B0329+54"

            # '!' = Network Byte Order
            # 'Q' = uint64_t (8 bytes)
            # '256s' = char array of 256 bytes
            start_payload = struct.pack('!Q256s', timestamp, target_name)

            # 'B' = uint8_t, 'B' = uint8_t, 'H' = uint16_t
            start_header = struct.pack('!BBI', MAGIC, TYPE_START, len(start_payload))

            conn.sendall(start_header + start_payload)
            time.sleep(1)

            for i in range(1, 4):
                print(f"Sending DATA packet {i}...")

                data_payload = f"Here is some dummy data block {i}.".encode('utf-8')

                data_header = struct.pack('!BBI', MAGIC, TYPE_DATA, len(data_payload))

                conn.sendall(data_header + data_payload)
                time.sleep(1)

            print("Sending END packet...")
            end_header = struct.pack('!BBI', MAGIC, TYPE_END, 0)
            conn.sendall(end_header)















            print("Sending START packet...")
            timestamp = int(time.time())
            target_name = b"Sun"

            # '!' = Network Byte Order
            # 'Q' = uint64_t (8 bytes)
            # '256s' = char array of 256 bytes
            start_payload = struct.pack('!Q256s', timestamp, target_name)

            # 'B' = uint8_t, 'B' = uint8_t, 'H' = uint16_t
            start_header = struct.pack('!BBI', MAGIC, TYPE_START, len(start_payload))

            conn.sendall(start_header + start_payload)
            time.sleep(1)

            for i in range(1, 4):
                print(f"Sending DATA packet {i}...")

                data_payload = f"Here is some dummy data block {i}.".encode('utf-8')

                data_header = struct.pack('!BBI', MAGIC, TYPE_DATA, len(data_payload))

                conn.sendall(data_header + data_payload)
                time.sleep(1)

            print("Sending END packet...")
            end_header = struct.pack('!BBI', MAGIC, TYPE_END, 0)
            conn.sendall(end_header)

            print("Finished sending. Closing connection.")

if __name__ == "__main__":
    try:
        start_mock_server()
    except KeyboardInterrupt:
        print("\nServer stopped manually.")
