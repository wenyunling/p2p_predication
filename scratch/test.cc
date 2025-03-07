#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mpi-interface.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // 启用 MPI
    MpiInterface::Enable(&argc, &argv);

    // 获取当前进程的排名和总进程数
    uint32_t systemId = MpiInterface::GetSystemId();
    uint32_t systemCount = MpiInterface::GetSize();

    // 打印 MPI 信息
    std::cout << "Running on system " << systemId << " of " << systemCount << std::endl;

    // 创建节点
    NodeContainer nodes;
    nodes.Create(2);

    // 创建点对点链路
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

    // 安装网络设备
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // 安装网络协议栈
    InternetStackHelper stack;
    stack.Install(nodes);

    // 分配 IP 地址
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // 安装应用程序
    if (systemId == 0) {
        // 在系统 0 上安装 UDP 回显服务器
        UdpEchoServerHelper echoServer(9);
        ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
        serverApp.Start(Seconds(1.0));
        serverApp.Stop(Seconds(10.0));
    } else {
        // 在其他系统上安装 UDP 回显客户端
        UdpEchoClientHelper echoClient(interfaces.GetAddress(0), 9);
        echoClient.SetAttribute("MaxPackets", UintegerValue(1));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(1));
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(Seconds(10.0));
    }

    // 运行仿真
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    // 关闭 MPI
    MpiInterface::Disable();

    return 0;
}
