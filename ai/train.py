import torch
import torch.nn as nn
import torch.optim as optim
import numpy as np
import pandas as pd
import socket
import io
import matplotlib.pyplot as plt
from torch.utils.data import Dataset, DataLoader

# 动态时序数据编码器
class DynamicEncoder(nn.Module):
    def __init__(self, input_dim=4, hidden_dim=32, num_layers=1):
        super(DynamicEncoder, self).__init__()
        self.lstm = nn.LSTM(input_dim, hidden_dim, num_layers, batch_first=True)
    
    def forward(self, x):
        output, (h_n, _) = self.lstm(x)
        return h_n[-1]  # 取最后一层的隐藏状态

# 负载预测模型
class LoadPredictionModel(nn.Module):
    def __init__(self):
        super(LoadPredictionModel, self).__init__()
        self.dynamic_encoder = DynamicEncoder(input_dim=4, hidden_dim=32)
        self.fc = nn.Linear(32, 30)  # 预测未来30s负载
    
    def forward(self, dynamic_input):
        dynamic_feat = self.dynamic_encoder(dynamic_input)  # (batch, 32)
        load_pred = torch.sigmoid(self.fc(dynamic_feat))  # (batch, 30), 归一化到[0,1]
        return load_pred

# 自定义数据集类
class TimeSeriesDataset(Dataset):
    def __init__(self, data, seq_length=80, pred_length=30):
        self.data = data
        self.seq_length = seq_length
        self.pred_length = pred_length
    
    def __len__(self):
        return len(self.data) - self.seq_length - self.pred_length + 1
    
    def __getitem__(self, idx):
        # 提取输入序列和目标序列
        dynamic_input = self.data[idx:idx+self.seq_length, :4]  # 前4列为特征
        load_target = self.data[idx+self.seq_length:idx+self.seq_length+self.pred_length, 4]  # 第5列为负载
        return {
            'dynamic': torch.tensor(dynamic_input, dtype=torch.float32),
            'load_target': torch.tensor(load_target, dtype=torch.float32)
        }

# 负载能力归一化加权计算
def compute_load(tx, rx, delay, loss):
    return 0.5 * (tx + rx) / max(tx.max(), 1) + 0.3 * (1 - delay) + 0.2 * (1 - loss / 100)

# 通过Socket接收CSV数据
def receive_csv_from_socket(host='localhost', port=12345):
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((host, port))
    server_socket.listen(1)
    print("Waiting for connection...")
    conn, addr = server_socket.accept()
    print(f"Connected by {addr}")
    
    data = b""
    while True:
        packet = conn.recv(4096)
        if not packet:
            break
        data += packet
    conn.close()
    
    csv_data = io.StringIO(data.decode())
    df = pd.read_csv(csv_data)
    return df

# 数据预处理
def preprocess_data(df, node_id=6):
    node_data = df[df['NodeID'] == node_id]
    node_data = node_data.sort_values(by='Time(s)')
    # 计算负载
    node_data['Load'] = compute_load(
        node_data['TxThroughput(bps)'], node_data['RxThroughput(bps)'],
        node_data['AvgDelay(s)'], node_data['LossRate(%)']
    )
    data = node_data[['TxThroughput(bps)', 'RxThroughput(bps)', 'AvgDelay(s)', 'LossRate(%)', 'Load']].values
    return data

# 训练函数
def train_model(model, dataloader, num_epochs=10, learning_rate=1e-3):
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model.to(device)
    optimizer = optim.Adam(model.parameters(), lr=learning_rate)
    criterion = nn.MSELoss()
    
    model.train()
    for epoch in range(num_epochs):
        epoch_loss = 0.0
        for batch in dataloader:
            dynamic_input = batch['dynamic'].to(device)
            load_target = batch['load_target'].to(device)
            
            optimizer.zero_grad()
            load_pred = model(dynamic_input)
            loss = criterion(load_pred, load_target)
            loss.backward()
            optimizer.step()
            
            epoch_loss += loss.item()
        print(f"Epoch {epoch+1}/{num_epochs}, Loss: {epoch_loss/len(dataloader):.4f}")

# 绘制折线图
def plot_results(actual_load, predicted_load):
    plt.figure(figsize=(10, 6))
    time_axis = np.arange(1, 31) 
    plt.plot(time_axis, actual_load, label='Actual Load', color='blue')
    plt.plot(time_axis, predicted_load, label='Predicted Load', color='red', linestyle='--')
    plt.xlabel('Time (s)')
    plt.ylabel('Load')
    plt.title('Actual vs Predicted Load (86-116 seconds)')
    plt.legend()
    plt.grid(True)
    plt.show()

# 主函数
def main():
    df = receive_csv_from_socket()
    
    data = preprocess_data(df, node_id=6)
    
    dataset = TimeSeriesDataset(data, seq_length=80, pred_length=30)
    dataloader = DataLoader(dataset, batch_size=32, shuffle=True)
    
    model = LoadPredictionModel()
    
    train_model(model, dataloader, num_epochs=10, learning_rate=1e-3)
    
    # 预测未来30秒负载
    model.eval()
    with torch.no_grad():
        test_input = torch.tensor(data[5:85, :4], dtype=torch.float32).unsqueeze(0)  
        predicted_load = model(test_input).squeeze().numpy() 
        actual_load = data[86:116, 4] 

    plot_results(actual_load, predicted_load)

if __name__ == "__main__":
    main()