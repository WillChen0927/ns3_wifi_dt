 #include "ns3/boolean.h"
 #include "ns3/command-line.h"
 #include "ns3/config.h"
 #include "ns3/double.h"
 #include "ns3/internet-stack-helper.h"
 #include "ns3/ipv4-address-helper.h"
 #include "ns3/log.h"
 #include "ns3/mobility-helper.h"
 #include "ns3/rng-seed-manager.h"
 #include "ns3/ssid.h"
 #include "ns3/string.h"
 #include "ns3/udp-client-server-helper.h"
 #include "ns3/udp-server.h"
 #include "ns3/uinteger.h"
 #include "ns3/yans-wifi-channel.h"
 #include "ns3/yans-wifi-helper.h"
 #include "ns3/on-off-helper.h"
 #include "ns3/applications-module.h"
 #include "ns3/bridge-helper.h"
 
 
 using namespace ns3;
 
 NS_LOG_COMPONENT_DEFINE("SimplesHtHiddenStations");

 void ChangeDataRate(Ptr<OnOffApplication> app, std::string newDataRate) {
    app->SetAttribute("DataRate", StringValue(newDataRate));
}
 
 int main(int argc, char* argv[])
 {
     uint32_t payloadSize{1472}; // bytes
     Time simulationTime{"20s"};
     uint32_t nMpdus{1};
     uint32_t maxAmpduSize{0};
     bool enableRts{false};
     double minExpectedThroughput{0};
     double maxExpectedThroughput{0};
 
     RngSeedManager::SetSeed(1);
     RngSeedManager::SetRun(7);
 
     CommandLine cmd(__FILE__);
     cmd.AddValue("nMpdus", "Number of aggregated MPDUs", nMpdus);
     cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
     cmd.AddValue("enableRts", "Enable RTS/CTS", enableRts);
     cmd.AddValue("simulationTime", "Simulation time", simulationTime);
     cmd.AddValue("minExpectedThroughput",
                  "if set, simulation fails if the lowest throughput is below this value",
                  minExpectedThroughput);
     cmd.AddValue("maxExpectedThroughput",
                  "if set, simulation fails if the highest throughput is above this value",
                  maxExpectedThroughput);
     cmd.Parse(argc, argv);
 
     if (!enableRts)
     {
         Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
     }
     else
     {
         Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("0"));
     }
 
     // Set the maximum size for A-MPDU with regards to the payload size
     maxAmpduSize = nMpdus * (payloadSize + 200);
 
    
     Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(5));
 
     NodeContainer wifiStaNodes;
     wifiStaNodes.Create(1);
     NodeContainer wifiApNode;
     wifiApNode.Create(1);
 
     YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
     channel.AddPropagationLoss(
         "ns3::RangePropagationLossModel"); // wireless range limited to 5 meters!
 
     YansWifiPhyHelper phy24;
     phy24.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
     phy24.SetChannel(channel.Create());
     phy24.Set("ChannelSettings", StringValue("{1, 0, BAND_2_4GHZ, 0}"));


     YansWifiPhyHelper phy5;
     phy5.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
     phy5.SetChannel(channel.Create());
     phy5.Set("ChannelSettings", StringValue("{36, 0, BAND_5GHZ, 0}"));
 
     WifiHelper wifi;
     wifi.SetStandard(WIFI_STANDARD_80211ax);
     wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode",
                                  StringValue("HeMcs9"),
                                  "ControlMode",
                                  StringValue("HeMcs0"));
     WifiMacHelper mac;
 
     Ssid ssid = Ssid("simple-mpdu-aggregation");
     mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
 
     NetDeviceContainer staDevices;
     staDevices = wifi.Install(phy24, mac, wifiStaNodes);
 
     mac.SetType("ns3::ApWifiMac",
                 "Ssid",
                 SsidValue(ssid),
                 "EnableBeaconJitter",
                 BooleanValue(false));
 
     NetDeviceContainer apDevice24, apDevice5;
     apDevice24 = wifi.Install(phy24, mac, wifiApNode);
     apDevice5 = wifi.Install(phy5, mac, wifiApNode);

     BridgeHelper bridge;
     NetDeviceContainer bridgeDev;
     bridgeDev = bridge.Install(wifiApNode.Get(0), NetDeviceContainer(apDevice24.Get(0), apDevice5.Get(0))); 
 
     Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize",
                 UintegerValue(maxAmpduSize));
    
 
     // Setting mobility model
     MobilityHelper mobility;
     Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
 
     positionAlloc->Add(Vector(0.0, 0.0, 0.0));
     positionAlloc->Add(Vector(5.0, 0.0, 0.0));
     //positionAlloc->Add(Vector(5.0, 0.0, 0.0));
     mobility.SetPositionAllocator(positionAlloc);
 
     mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
 
     mobility.Install(wifiApNode);
     mobility.Install(wifiStaNodes);
 
     // Internet stack
     InternetStackHelper stack;
     stack.Install(wifiApNode);
     stack.Install(wifiStaNodes);
 
     Ipv4AddressHelper address;
     address.SetBase("192.168.1.0", "255.255.255.0");
     Ipv4InterfaceContainer StaInterface;
     StaInterface = address.Assign(staDevices);
     Ipv4InterfaceContainer ApInterface;
     ApInterface = address.Assign(bridgeDev);

 
     // Setting applications
     //uint16_t port = 9;
     UdpServerHelper server24(9);
     UdpServerHelper server5(10);
     ApplicationContainer serverApp24 = server24.Install(wifiApNode);
     ApplicationContainer serverApp5 = server5.Install(wifiApNode);
     serverApp24.Start(Seconds(0.0));
     serverApp24.Stop(simulationTime + Seconds(1.0));
     serverApp5.Start(Seconds(0.0));
     serverApp5.Stop(simulationTime + Seconds(1.0));


     InetSocketAddress dest(ApInterface.GetAddress(0), 10);
     OnOffHelper clientA("ns3::UdpSocketFactory", dest);
     clientA.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
     clientA.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
     clientA.SetAttribute("DataRate", StringValue("10Mbps"));
     clientA.SetAttribute("PacketSize", UintegerValue(1448));

     ApplicationContainer clientAppA = clientA.Install(wifiStaNodes);
     clientAppA.Start(Seconds(0.1));
     clientAppA.Stop(Seconds(20));

     //Ptr<OnOffApplication> onOffApp = clientAppA.Get(0)->GetObject<OnOffApplication>();
     //Simulator::Schedule(Seconds(11.0), &ChangeDataRate, onOffApp, "1bps");

     /*
     UdpClientHelper client(ApInterface.GetAddress(0), port);
     client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
     client.SetAttribute("Interval", TimeValue(Time("0.0001"))); // packets/s
     client.SetAttribute("PacketSize", UintegerValue(payloadSize));
 
     // Saturated UDP traffic from stations to AP
     ApplicationContainer clientApp1 = client.Install(wifiStaNodes);
     clientApp1.Start(Seconds(1.0));
     clientApp1.Stop(simulationTime + Seconds(1.0));
     streamNumber += client.AssignStreams(wifiStaNodes, streamNumber); */
 
     //phy.EnablePcap("SimpleHtHiddenStations_Ap", apDevice.Get(0));
     //phy.EnablePcap("SimpleHtHiddenStations_Sta1", staDevices.Get(0));
     //phy.EnablePcap("SimpleHtHiddenStations_Sta2", staDevices.Get(1));
 
     //AsciiTraceHelper ascii;
     //phy.EnableAsciiAll(ascii.CreateFileStream("SimpleHtHiddenStations.tr"));
 
     Simulator::Stop(simulationTime + Seconds(1.0));
 
     Simulator::Run();
 
     double totalPacketsThrough24 = DynamicCast<UdpServer>(serverApp24.Get(0))->GetReceived();
     double totalPacketsThrough5 = DynamicCast<UdpServer>(serverApp5.Get(0))->GetReceived();
 
     Simulator::Destroy();
 
     auto throughput24 = totalPacketsThrough24 * payloadSize * 8 / simulationTime.GetMicroSeconds();
     auto throughput5 = totalPacketsThrough5 * payloadSize * 8 / simulationTime.GetMicroSeconds();
     std::cout << "Throughput24: " << throughput24 << " Mbit/s" << '\n';
     std::cout << "Throughput5: " << throughput5 << " Mbit/s" << '\n';
     if (throughput24 < minExpectedThroughput ||
         (maxExpectedThroughput > 0 && throughput24 > maxExpectedThroughput))
     {
         NS_LOG_ERROR("Obtained throughput " << throughput24 << " is not in the expected boundaries!");
         exit(1);
     }
     return 0;
 }
 