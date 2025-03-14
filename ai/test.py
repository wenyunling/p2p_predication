import socket
import pandas as pd
import io

def receive_csv_from_socket(host='localhost', port=12345, filename='received_data.csv'):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((host, port))
    server_socket.listen(1)
    print(f"Waiting for connection on {host}:{port}...")

    conn, addr = server_socket.accept()
    print(f"Connected by {addr}")

    data = b""
    while True:
        packet = conn.recv(4096)
        if not packet:
            break
        data += packet
    conn.close()
    
    # 解析 CSV 数据
    csv_data = io.StringIO(data.decode())
    df = pd.read_csv(csv_data)
    
    # 将数据保存到文件
    df.to_csv(filename, index=False)
    print(f"Data saved to {filename}")

if __name__ == "__main__":
    receive_csv_from_socket()
