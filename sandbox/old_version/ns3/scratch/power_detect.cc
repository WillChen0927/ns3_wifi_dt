#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/propagation-delay-model.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NearestApExample");

uint32_t GetApIndex(NodeContainer aps, Ptr<Node> ap) {
    for (uint32_t i = 0; i < aps.GetN(); ++i) {
        if (aps.Get(i) == ap) {
            return i;
        }
    }
    return 0;
}

Ptr<Node> SelectNearestAp(Ptr<Node> sta, NodeContainer aps, Ptr<LogDistancePropagationLossModel> lossModel, double txPowerDbm, uint32_t& selectedApIndex) {
    double bestRssi = -1e9;
    Ptr<Node> bestAp = nullptr;

    Ptr<MobilityModel> staMob = sta->GetObject<MobilityModel>();

    for (uint32_t i = 0; i < aps.GetN(); i++) {
        Ptr<MobilityModel> apMob = aps.Get(i)->GetObject<MobilityModel>();
        double rxPower = lossModel->CalcRxPower(txPowerDbm, apMob, staMob);
        NS_LOG_UNCOND("AP[" << i << "] RSSI: " << rxPower << " dBm");
        if (rxPower > bestRssi) {
            bestRssi = rxPower;
            bestAp = aps.Get(i);
            selectedApIndex = i;
        }
    }

    NS_LOG_UNCOND("Selected AP with RSSI = " << bestRssi << " dBm");
    return bestAp;
}

void ReportApThroughput(std::vector<Ptr<PacketSink>> sinks, Time duration, uint32_t totalAps) {
    for (uint32_t i = 0; i < totalAps; ++i) {
        if (sinks[i]) {
            double throughput = (sinks[i]->GetTotalRx() * 8.0) / duration.GetSeconds() / 1e6;
            NS_LOG_UNCOND("AP[" << i << "] throughput: " << throughput << " Mbps");
        } else {
            NS_LOG_UNCOND("AP[" << i << "] throughput: 0 Mbps");
        }
    }
}

int main(int argc, char *argv[]) {
    LogComponentEnable("NearestApExample", LOG_LEVEL_INFO);

    NodeContainer staNodes;
    staNodes.Create(3);

    NodeContainer apNodes;
    apNodes.Create(3);

    MobilityHelper mobility;

    // AP positions
    Ptr<ListPositionAllocator> apPositions = CreateObject<ListPositionAllocator>();
    apPositions->Add(Vector(0, 0, 0));
    apPositions->Add(Vector(10, 0, 0));
    apPositions->Add(Vector(20, 0, 0));
    mobility.SetPositionAllocator(apPositions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    // STA random positions in 20x5 area
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=20.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=5.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    // Output STA positions
    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        Ptr<MobilityModel> staMob = staNodes.Get(i)->GetObject<MobilityModel>();
        Vector pos = staMob->GetPosition();
        NS_LOG_UNCOND("STA[" << i << "] position: x=" << pos.x << ", y=" << pos.y);
    }

    // Build channel manually
    Ptr<YansWifiChannel> wifiChannel = CreateObject<YansWifiChannel>();
    Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    wifiChannel->SetPropagationLossModel(lossModel);
    wifiChannel->SetPropagationDelayModel(delayModel);

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    phy.Set("TxPowerStart", DoubleValue(16.0));
    phy.Set("TxPowerEnd", DoubleValue(16.0));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-ap");

    // STA devices
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    // AP devices (all APs are active)
    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer allApDevices = wifi.Install(phy, mac, apNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign(allApDevices);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    uint16_t basePort = 5000;
    std::vector<Ptr<PacketSink>> apSinks(apNodes.GetN(), nullptr);

    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        uint32_t selectedApIndex = 0;
        Ptr<Node> selectedAp = SelectNearestAp(staNodes.Get(i), apNodes, lossModel, 16.0, selectedApIndex);

        OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(selectedApIndex), basePort + i));
        onoff.SetConstantRate(DataRate("1Mbps"));
        onoff.SetAttribute("PacketSize", UintegerValue(512));
        ApplicationContainer app = onoff.Install(staNodes.Get(i));
        app.Start(Seconds(1.0));
        app.Stop(Seconds(10.0));

        if (apSinks[selectedApIndex] == nullptr) {
            PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), basePort + i));
            ApplicationContainer sinkApp = sink.Install(apNodes.Get(selectedApIndex));
            sinkApp.Start(Seconds(0.0));
            sinkApp.Stop(Seconds(10.0));
            apSinks[selectedApIndex] = DynamicCast<PacketSink>(sinkApp.Get(0));
        }
    }

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();

    ReportApThroughput(apSinks, Seconds(9.0), apNodes.GetN());

    Simulator::Destroy();

    return 0;
}