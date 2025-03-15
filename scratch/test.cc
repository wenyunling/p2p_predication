#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h" 

using namespace ns3;

int main() {
  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper p2p;
  p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
  p2p.SetChannelAttribute("Delay", StringValue("2ms"));
  
  Ptr<RateErrorModel> errorModel = CreateObject<RateErrorModel>();
  errorModel->SetAttribute("ErrorRate", DoubleValue(0.50));
  errorModel->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
  p2p.SetDeviceAttribute("ReceiveErrorModel", PointerValue(errorModel));  
  
  NetDeviceContainer devices = p2p.Install(nodes);

  InternetStackHelper stack;
  stack.Install(nodes);

  Ipv4AddressHelper address;
  address.SetBase("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign(devices);

  UdpServerHelper _server(9);
  ApplicationContainer serverApp = _server.Install(nodes.Get(1));
  serverApp.Start(Seconds(1.0));

  UdpClientHelper _client(interfaces.GetAddress(1), 9);
  _client.SetAttribute("MaxPackets", UintegerValue(1000));
  _client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  _client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer clientApp = _client.Install(nodes.Get(0));
  clientApp.Start(Seconds(2.0));
  
  UdpServerHelper server(9);
  ApplicationContainer _serverApp = server.Install(nodes.Get(0));
  _serverApp.Start(Seconds(1.0));

  UdpClientHelper client(interfaces.GetAddress(0), 9);
  client.SetAttribute("MaxPackets", UintegerValue(1000));
  client.SetAttribute("Interval", TimeValue(Seconds(0.1)));
  client.SetAttribute("PacketSize", UintegerValue(1024));
  ApplicationContainer _clientApp = client.Install(nodes.Get(1));
  _clientApp.Start(Seconds(2.0));



  // monitor
  FlowMonitorHelper fmHelper;
  Ptr<FlowMonitor> flowMonitor = fmHelper.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();

  FlowMonitor::FlowStatsContainer flowStats = flowMonitor->GetFlowStats();
  for (auto it = flowStats.begin(); it != flowStats.end(); ++it) {
    FlowId flowId = it->first;
    FlowMonitor::FlowStats stats = it->second;

    std::cout << "\nFlow ID: " << flowId << std::endl;
    std::cout << "  Tx Packets: " << stats.txPackets << std::endl;
    std::cout << "  Rx Packets: " << stats.rxPackets << std::endl;
    std::cout << "  Lost Packets: " << stats.lostPackets << std::endl;
    std::cout << "  Delay Sum: " << stats.delaySum.GetSeconds() * 1000 << " ms" << std::endl;
    std::cout << "  Throughput: " << (stats.rxBytes * 8.0) / 8.0 / 1e6 << " Mbps" << std::endl; // 仿真时间 8秒（10-2）
  }

  flowMonitor->SerializeToXmlFile("network-stats.xml", true, true);
  Simulator::Destroy();

  return 0;
}
