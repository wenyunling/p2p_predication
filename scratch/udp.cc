#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"
#include "ns3/error-model.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <vector>

using namespace ns3;

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 12345
#define CSV_FILE "node_performance_stats.csv"


NS_LOG_COMPONENT_DEFINE("StructuredP2PSimulation");


// 发送 CSV 文件
void sendCSVFile(const char* filename) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Socket creation failed.\n";
        return;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Connection to Python server failed.\n";
        close(sock);
        return;
    }

    std::ifstream file(filename, std::ios::binary);
    
    if (!file) {
        std::cerr << "Failed to open file: " << filename << "\n";
        close(sock);
        return;
    }

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        send(sock, buffer, file.gcount(), 0);
    }
    send(sock, buffer, file.gcount(), 0);  // 发送剩余部分

    std::cout << "CSV file sent successfully: " << filename << "\n";

    file.close();
    close(sock);
}

class CustomBehavior : public Application {
    public:
        static TypeId GetTypeId() {
            static TypeId tid = TypeId("CustomBehavior")
                .SetParent<Application>()
                .AddAttribute("PacketSize", "Packet size in bytes",
                              UintegerValue(1024),
                              MakeUintegerAccessor(&CustomBehavior::m_packetSize),
                              MakeUintegerChecker<uint32_t>());
            return tid;
        }
        
        virtual void ScheduleNextTx() = 0;
    
        // 新增：接收回调函数
        void HandleRead(Ptr<Socket> socket) {
            Address from;
            Ptr<Packet> packet;
            // 循环读取所有可用数据包
            while ((packet = socket->RecvFrom(from))) {
                NS_LOG_INFO("Node " << GetNode()->GetId() 
                             << " received packet of size " << packet->GetSize());
                // 可以在这里增加自定义逻辑，如更新应用层统计数据
            }
        }
        
    protected:
        uint32_t m_packetSize;
        Ptr<Socket> m_socket;
        Ipv4Address m_targetAddr; // 目标地址成员
    };
    
    class StableBehavior : public CustomBehavior {
    public:
        static TypeId GetTypeId() {
            static TypeId tid = TypeId("StableBehavior")
                .SetParent<CustomBehavior>()
                .AddConstructor<StableBehavior>();
            return tid;
        }
        
        void StartApplication() override {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            m_socket->Bind();
            // 设置接收回调，处理 echo 回复
            m_socket->SetRecvCallback(MakeCallback(&CustomBehavior::HandleRead, this));
            ScheduleNextTx();
        }
        
        void ScheduleNextTx() override {
            Simulator::Schedule(Seconds(1.0 + 0.1 * m_packetSize/1024), &StableBehavior::SendPacket, this);
        }
        
        void SendPacket() {
            Ptr<Packet> packet = Create<Packet>(m_packetSize);
            m_socket->SendTo(packet, 0, InetSocketAddress(m_targetAddr, 9));
            ScheduleNextTx();
        }
        
        void SetTargetAddress(Ipv4Address addr) { // 设置目标地址
            m_targetAddr = addr;
        }
    };
    
    class BurstBehavior : public CustomBehavior {
    public:
        static TypeId GetTypeId() {
            static TypeId tid = TypeId("BurstBehavior")
                .SetParent<CustomBehavior>()
                .AddConstructor<BurstBehavior>()
                .AddAttribute("BurstInterval", "Time between bursts",
                              TimeValue(Seconds(5.0)),
                              MakeTimeAccessor(&BurstBehavior::m_burstInterval),
                              MakeTimeChecker());
            return tid;
        }
        
        void StartApplication() override {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            m_socket->Bind();
            // 设置接收回调，处理 echo 回复
            m_socket->SetRecvCallback(MakeCallback(&CustomBehavior::HandleRead, this));
            ScheduleNextTx();
        }
        
        void ScheduleNextTx() override {
            Simulator::Schedule(m_burstInterval, &BurstBehavior::BurstSend, this);
        }
        
        void BurstSend() {
            for (int i = 0; i < 3; ++i) {
                Simulator::Schedule(Seconds(i * 0.1), &BurstBehavior::SendPacket, this);
            }
            ScheduleNextTx();
        }
        
        void SendPacket() {
            Ptr<Packet> packet = Create<Packet>(m_packetSize);
            m_socket->SendTo(packet, 0, InetSocketAddress(m_targetAddr, 9));
        }
        
        void SetTargetAddress(Ipv4Address addr) {
            m_targetAddr = addr;
        }
    
    private:
        Time m_burstInterval;
    };
    
// ================== 全局变量和监控部分 ==================
Ptr<FlowMonitor> flowMonitor;
Ptr<Ipv4FlowClassifier> classifier;
std::map<Ipv4Address, uint32_t> ipToNodeMap;
Time simulationStartTime;

struct NodeStats {
    uint64_t txBytes = 0;
    uint64_t rxBytes = 0;
    uint64_t txBytesLast = 0;
    uint64_t rxBytesLast = 0;
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t txPacketsLast = 0;
    uint64_t rxPacketsLast = 0;
    Time totalDelay = Seconds(0);
};
std::map<uint32_t, NodeStats> nodeStats;

std::ofstream outFile("node_performance_stats.csv");

void LogNodePerformance() {
    FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats();
    
    // 清空当前统计数据
    for (auto& entry : nodeStats) {
        entry.second.txBytesLast = entry.second.txBytes;
        entry.second.rxBytesLast = entry.second.rxBytes;
        entry.second.txPacketsLast = entry.second.txPackets;
        entry.second.rxPacketsLast = entry.second.rxPackets;
        
        entry.second.txBytes = 0;
        entry.second.rxBytes = 0;
        entry.second.txPackets = 0;
        entry.second.rxPackets = 0;
        entry.second.totalDelay = Seconds(0);
    }

    for (auto& flow : stats) {
        Ipv4FlowClassifier::FiveTuple fiveTuple = classifier->FindFlow(flow.first);
        
        auto srcIt = ipToNodeMap.find(fiveTuple.sourceAddress);
        auto dstIt = ipToNodeMap.find(fiveTuple.destinationAddress);
        if (srcIt == ipToNodeMap.end() || dstIt == ipToNodeMap.end()) continue;

        uint32_t srcNode = srcIt->second;
        uint32_t dstNode = dstIt->second;

        nodeStats[srcNode].txBytes += flow.second.txBytes;
        nodeStats[srcNode].txPackets += flow.second.txPackets;
        
        nodeStats[dstNode].rxBytes += flow.second.rxBytes;
        nodeStats[dstNode].rxPackets += flow.second.rxPackets;
        nodeStats[dstNode].totalDelay += flow.second.delaySum;
    }

    Time currentTime = Simulator::Now();
    double elapsed = (currentTime - simulationStartTime).GetSeconds();

    for (auto& entry : nodeStats) {
        uint32_t nodeId = entry.first;
        NodeStats& stats = entry.second;

        double txThroughput = (stats.txBytes - stats.txBytesLast) * 8;
        double rxThroughput = (stats.rxBytes - stats.rxBytesLast) * 8;
        
        double avgDelay = (stats.rxPackets > stats.rxPacketsLast) ? 
            stats.totalDelay.GetSeconds() / (stats.rxPackets - stats.rxPacketsLast) : 0;

        double lossRate = (stats.txPackets > stats.txPacketsLast) ? 
            ((stats.txPackets - stats.rxPackets) * 100.0) / stats.txPackets : 0;

        outFile << elapsed << "," << nodeId << ","
                << txThroughput << "," << rxThroughput << ","
                << avgDelay << "," << lossRate << "\n";
    }

    Simulator::Schedule(Seconds(1.0), &LogNodePerformance);
}

// ================== 获取邻居节点 ==================
std::vector<Ipv4Address> GetNeighbors(Ptr<Node> node) {
    std::vector<Ipv4Address> neighbors;
    // 遍历该节点上的所有 NetDevice
    for (uint32_t i = 0; i < node->GetNDevices(); ++i) {
        Ptr<NetDevice> device = node->GetDevice(i);
        // 仅考虑点对点设备（若拓扑中有其他类型，如 CSMA，可扩展）
        Ptr<PointToPointNetDevice> ptpDevice = DynamicCast<PointToPointNetDevice>(device);
        if (ptpDevice) {
            Ptr<Channel> channel = ptpDevice->GetChannel();
            // 遍历同一信道上的所有设备
            for (uint32_t j = 0; j < channel->GetNDevices(); ++j) {
                Ptr<NetDevice> otherDevice = channel->GetDevice(j);
                // 排除自身设备
                if (otherDevice != device) {
                    Ptr<Node> otherNode = otherDevice->GetNode();
                    Ptr<Ipv4> ipv4 = otherNode->GetObject<Ipv4>();
                    // 遍历该节点的所有接口地址，排除回环地址
                    for (uint32_t k = 0; k < ipv4->GetNInterfaces(); ++k) {
                        for (uint32_t l = 0; l < ipv4->GetNAddresses(k); ++l) {
                            Ipv4InterfaceAddress addr = ipv4->GetAddress(k, l);
                            if (addr.GetLocal() != Ipv4Address("127.0.0.1")) {
                                neighbors.push_back(addr.GetLocal());
                            }
                        }
                    }
                }
            }
        }
    }
    return neighbors;
}

int main(int argc, char* argv[]) {
    outFile << "Time(s),NodeID,TxThroughput(bps),RxThroughput(bps),AvgDelay(s),LossRate(%)\n";

    double simDuration = 120.0;
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // ================== 节点分组 ==================
    NodeContainer groupCore, groupMid, groupEdge;
    groupCore.Create(3);
    groupMid.Create(3);
    groupEdge.Create(3);

    NodeContainer allNodes;
    allNodes.Add(groupCore);
    allNodes.Add(groupMid);
    allNodes.Add(groupEdge);

    // ================== 网络拓扑 ==================
    InternetStackHelper stack;
    stack.Install(allNodes);

    PointToPointHelper p2pCore, p2pMid, p2pEdge;
    p2pCore.SetDeviceAttribute("DataRate", StringValue("2Gbps"));
    p2pCore.SetChannelAttribute("Delay", StringValue("1ms"));
    p2pMid.SetDeviceAttribute("DataRate", StringValue("800Mbps"));
    p2pMid.SetChannelAttribute("Delay", StringValue("3ms"));
    p2pEdge.SetDeviceAttribute("DataRate", StringValue("200Mbps"));
    p2pEdge.SetChannelAttribute("Delay", StringValue("10ms"));

    // 错误模型配置
    Ptr<RateErrorModel> errorCore = CreateObject<RateErrorModel>();
    errorCore->SetAttribute("ErrorRate", DoubleValue(0.001)); // 0.1%
    
    Ptr<RateErrorModel> errorMid = CreateObject<RateErrorModel>();
    errorMid->SetAttribute("ErrorRate", DoubleValue(0.01));   // 1%
    
    Ptr<RateErrorModel> errorEdge = CreateObject<RateErrorModel>();
    errorEdge->SetAttribute("ErrorRate", DoubleValue(0.05));  // 5%

    Ipv4AddressHelper address("10.1.0.0", "255.255.255.0");
    NetDeviceContainer allDevices;

    // ----------- 核心层全连接 -----------
    for (uint32_t i = 0; i < groupCore.GetN(); ++i) {
        for (uint32_t j = i + 1; j < groupCore.GetN(); ++j) {
            NodeContainer pair(groupCore.Get(i), groupCore.Get(j));
            NetDeviceContainer dev = p2pCore.Install(pair);
            
            dev.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(errorCore));
            dev.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorCore));            
            address.Assign(dev);
            address.NewNetwork();
            allDevices.Add(dev);
        }
    }

    // ----------- 核心到中间层连接 -----------
    for (uint32_t i = 0; i < groupCore.GetN(); ++i) {
        NodeContainer pair(groupCore.Get(i), groupMid.Get(i));
        NetDeviceContainer dev = p2pMid.Install(pair);
        
        dev.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(errorCore));
        dev.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorCore));        
        address.Assign(dev);
        address.NewNetwork();
        allDevices.Add(dev);
    }

    // ----------- 核心到边缘层连接 -----------
    for (uint32_t i = 0; i < groupCore.GetN(); ++i) {
        NodeContainer pair(groupCore.Get(i), groupEdge.Get(i));
        NetDeviceContainer dev = p2pMid.Install(pair);
        
        dev.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(errorCore));
        dev.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errorCore));        
        address.Assign(dev);
        address.NewNetwork();
        allDevices.Add(dev);
    }


    // ================== IP地址映射 ==================
    Ipv4InterfaceContainer interfaces = address.Assign(allDevices);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    for (uint32_t i = 0; i < allNodes.GetN(); ++i) {
        Ptr<Node> node = allNodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        for (uint32_t j = 0; j < ipv4->GetNInterfaces(); ++j) {
            for (uint32_t k = 0; k < ipv4->GetNAddresses(j); ++k) {
                Ipv4InterfaceAddress addr = ipv4->GetAddress(j, k);
                if (addr.GetLocal() != Ipv4Address("127.0.0.1")) {
                    ipToNodeMap[addr.GetLocal()] = i;
                }
            }
        }
    }
    
    // ================== 应用层配置 ==================
    UdpEchoServerHelper server(9);
    ApplicationContainer servers = server.Install(allNodes);
    servers.Start(Seconds(1.0));
    servers.Stop(Seconds(simDuration));

    // 随机分配行为模式
    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    for (uint32_t i = 0; i < allNodes.GetN(); ++i) {
        Ptr<Node> node = allNodes.Get(i);

        // 获取邻居节点
        std::vector<Ipv4Address> neighbors = GetNeighbors(node);
        if (neighbors.empty()) {
            NS_LOG_WARN("Node " << i << " has no neighbors.");
            continue;
        }
        uint32_t targetIndex = rand->GetInteger(0, neighbors.size() - 1);
        Ipv4Address targetAddr = neighbors[targetIndex];

        // 随机分配行为模式
        if (rand->GetValue(0, 1) < 0.5) { // 50%概率稳定型
            Ptr<StableBehavior> app = CreateObject<StableBehavior>();
            app->SetAttribute("PacketSize", UintegerValue(1024 + 512 * (i % 3)));
            app->SetTargetAddress(targetAddr);
            node->AddApplication(app);
            app->SetStartTime(Seconds(1.0 + 0.5 * i));
            app->SetStopTime(Seconds(simDuration - 1));
        } else { // 50%概率突发型
            Ptr<BurstBehavior> app = CreateObject<BurstBehavior>();
            app->SetAttribute("PacketSize", UintegerValue(2048));
            app->SetAttribute("BurstInterval", TimeValue(Seconds(3.0 + i % 2)));
            app->SetTargetAddress(targetAddr);
            node->AddApplication(app);
            app->SetStartTime(Seconds(1.0 + 0.5 * i));
            app->SetStopTime(Seconds(simDuration - 1));
        }
    }    
    
    
    // ================== 监控配置 ==================
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
    classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    simulationStartTime = Simulator::Now();
    Simulator::Schedule(Seconds(1.0), &LogNodePerformance);

    Simulator::Stop(Seconds(simDuration + 2));
    Simulator::Run();

    flowMonitor->SerializeToXmlFile("network-stats.xml", true, true);
    Simulator::Destroy();
    
    outFile.close();
    sendCSVFile(CSV_FILE);
    
    return 0;
}