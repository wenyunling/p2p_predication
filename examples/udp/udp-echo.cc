#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/error-model.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RealNetworkSimulation");

// 全局变量
Ptr<FlowMonitor> flowMonitor;
Ptr<Ipv4FlowClassifier> classifier;
std::map<Ipv4Address, uint32_t> ipToNodeMap;
Time simulationStartTime;

struct NodeStats {
    uint64_t txBytes = 0;
    uint64_t rxBytes = 0;
    uint64_t txBytesLast = 0;  // 上一秒的发送字节数
    uint64_t rxBytesLast = 0;  // 上一秒的接收字节数
    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t txPacketsLast = 0;  // 上一秒的发送数据包数
    uint64_t rxPacketsLast = 0;  // 上一秒的接收数据包数
    Time totalDelay = Seconds(0);
};
std::map<uint32_t, NodeStats> nodeStats;

// 文件流对象
std::ofstream outFile("node_performance_stats.csv");

// 修改后的性能记录函数
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

        // 分离发送和接收统计
        nodeStats[srcNode].txBytes += flow.second.txBytes;
        nodeStats[srcNode].txPackets += flow.second.txPackets;
        
        nodeStats[dstNode].rxBytes += flow.second.rxBytes;
        nodeStats[dstNode].rxPackets += flow.second.rxPackets;
        nodeStats[dstNode].totalDelay += flow.second.delaySum;
    }

    Time currentTime = Simulator::Now();
    double elapsed = (currentTime - simulationStartTime).GetSeconds();

    // 写入所有节点的数据
    for (auto& entry : nodeStats) {
        uint32_t nodeId = entry.first;
        NodeStats& stats = entry.second;

        // 计算当前秒内的吞吐量
        double txThroughput = (stats.txBytes - stats.txBytesLast) * 8;  // 发送吞吐量 (bps)
        double rxThroughput = (stats.rxBytes - stats.rxBytesLast) * 8;  // 接收吞吐量 (bps)
        
        // 只有在有数据传输时，才计算延迟
        double avgDelay = (stats.rxPackets > stats.rxPacketsLast) ? 
            stats.totalDelay.GetSeconds() / (stats.rxPackets - stats.rxPacketsLast) : 0;

        // 计算丢包率
        double lossRate = (stats.txPackets > stats.txPacketsLast) ? 
            ((stats.txPackets - stats.rxPackets) * 100.0) / stats.txPackets : 0;

        outFile << elapsed << "," << nodeId << ","
                << txThroughput << "," << rxThroughput << ","
                << avgDelay << "," << lossRate << "\n";
    }

    Simulator::Schedule(Seconds(1.0), &LogNodePerformance);
}


int main(int argc, char* argv[]) {
    outFile << "Time(s),NodeID,TxThroughput(bps),RxThroughput(bps),AvgDelay(s),LossRate(%)\n";

    uint32_t nodeCount = 4;
    double simDuration = 120.0;
    double trafficIntensity = 0.7;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nodes", "Number of nodes", nodeCount);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(nodeCount);

    // 预先初始化所有节点统计
    for (uint32_t i = 0; i < nodeCount; ++i) {
        nodeStats[i] = NodeStats();
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));
    
    Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
    errorModel->SetAttribute("ErrorRate", DoubleValue(0.05));
    errorModel->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
    csma.SetDeviceAttribute("ReceiveErrorModel", PointerValue(errorModel));

    NetDeviceContainer devices = csma.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    for (uint32_t i = 0; i < nodeCount; ++i) {
        ipToNodeMap[interfaces.GetAddress(i)] = i;
    }

    uint16_t port = 9;
    double startTime = 1.0;

    // 安装UDP服务器到所有节点
    UdpEchoServerHelper server(port);
    ApplicationContainer servers = server.Install(nodes);
    servers.Start(Seconds(startTime));
    servers.Stop(Seconds(simDuration));

    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();
    for (uint32_t srcNode = 0; srcNode < nodeCount; ++srcNode) {
        uint32_t destNode;
        do {
            destNode = rand->GetInteger(0, nodeCount-1);
        } while (destNode == srcNode);

        UdpEchoClientHelper client(interfaces.GetAddress(destNode), port);
        client.SetAttribute("MaxPackets", UintegerValue(UINT32_MAX));
        client.SetAttribute("Interval", TimeValue(Seconds(0.1 / trafficIntensity)));
        client.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = client.Install(nodes.Get(srcNode));
        clientApp.Start(Seconds(startTime + rand->GetValue(0, 1.0)));
        clientApp.Stop(Seconds(simDuration - 1));
    }

    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
    classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    simulationStartTime = Simulator::Now();
    Simulator::Schedule(Seconds(startTime + 1), &LogNodePerformance);

    Simulator::Stop(Seconds(simDuration + 2));
    Simulator::Run();

    flowMonitor->SerializeToXmlFile("network-stats.xml", true, true);
    Simulator::Destroy();
    
    outFile.close();

    return 0;
}
