import numpy as np
import pandas as pd
from sklearn.preprocessing import MinMaxScaler
from keras.models import Sequential
from keras.layers import LSTM, Dense
from sklearn.model_selection import train_test_split
import matplotlib.pyplot as plt

# 加载数据
try:
    data = pd.read_csv("test.csv")
    print("文件加载成功！")
    print("列名：", data.columns)
except FileNotFoundError:
    print("错误：文件 'test.csv' 未找到，请检查文件路径。")
    exit()

# 检查列名
required_columns = ['Time(s)', 'NodeID', 'TxThroughput(bps)', 'RxThroughput(bps)', 'AvgDelay(s)', 'LossRate(%)']
if not all(column in data.columns for column in required_columns):
    print("错误：文件中缺少以下列：", required_columns)
    print("实际列名：", data.columns)
    exit()

# 按节点分组
nodes = data['NodeID'].unique()
print("节点列表：", nodes)

# 定义创建数据窗口的函数
def create_dataset(data, past_steps=20, future_steps=4):
    X, y = [], []
    for i in range(len(data) - past_steps - future_steps + 1):
        # 过去 past_steps 组数据作为输入
        X.append(data[i:i + past_steps, :])
        # 未来 future_steps 组数据作为输出
        y.append(data[i + past_steps:i + past_steps + future_steps, :])
    return np.array(X), np.array(y)

# 定义训练和预测函数
def train_and_predict(node_data, node_id, past_steps=20, future_steps=4):
    # 归一化数据
    scaler = MinMaxScaler(feature_range=(0, 1))
    scaled_data = scaler.fit_transform(node_data[['TxThroughput(bps)', 'RxThroughput(bps)', 'AvgDelay(s)', 'LossRate(%)']])

    # 创建数据窗口
    X, y = create_dataset(scaled_data, past_steps, future_steps)

    # 分割数据集
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, shuffle=False)

    # 构建LSTM模型
    model = Sequential()
    model.add(LSTM(units=50, return_sequences=True, input_shape=(X_train.shape[1], X_train.shape[2])))
    model.add(LSTM(units=50, return_sequences=False))
    model.add(Dense(units=4 * future_steps))  # 输出维度为 4 * future_steps
    model.compile(optimizer='adam', loss='mean_squared_error')

    # 训练模型
    model.fit(X_train, y_train.reshape(y_train.shape[0], -1), epochs=50, batch_size=32, verbose=0)

    # 预测
    predictions = model.predict(X_test)

    # 将预测结果 reshape 为 (n_samples, future_steps, 4)
    predictions = predictions.reshape(predictions.shape[0], future_steps, -1)

    # 逆归一化
    predictions = np.array([scaler.inverse_transform(pred) for pred in predictions])

    # 获取对应的时间节点
    time_stamps = node_data['Time(s)'].values[past_steps:past_steps + len(predictions)]

    # 打印预测结果
    print(f"\n节点 {node_id} 的预测结果：")
    for i in range(len(predictions)):
        print(f"时间节点: {time_stamps[i]}s")
        for j in range(future_steps):
            print(f"  未来第 {j + 1} 组数据: {predictions[i][j]}")

    # 绘制预测和实际的TxThroughput(bps)对比图
    if node_id == 1:  # 只绘制节点1的图
        # 获取实际的TxThroughput(bps)
        actual_tx = node_data['TxThroughput(bps)'].values[past_steps:past_steps + len(predictions) * future_steps]

        # 获取预测的TxThroughput(bps)
        predicted_tx = predictions[:, :, 0].flatten()  # 提取所有预测的TxThroughput(bps)

        # 生成对应的时间戳
        prediction_time_stamps = np.repeat(time_stamps, future_steps) + np.tile(np.arange(future_steps), len(time_stamps))

        # 绘制曲线
        plt.figure(figsize=(12, 6))
        plt.plot(prediction_time_stamps, actual_tx, label='Actual TxThroughput(bps)', marker='o')
        plt.plot(prediction_time_stamps, predicted_tx, label='Predicted TxThroughput(bps)', marker='x')
        plt.xlabel('Time(s)')
        plt.ylabel('TxThroughput(bps)')
        plt.title(f'Node {node_id} - Actual vs Predicted TxThroughput(bps) (22-25s)')
        plt.legend()
        plt.grid(True)
        plt.show()

# 对每个节点分别训练和预测
for node in nodes:
    node_data = data[data['NodeID'] == node]
    train_and_predict(node_data, node)