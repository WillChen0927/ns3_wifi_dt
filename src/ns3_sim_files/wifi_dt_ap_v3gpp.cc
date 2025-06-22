// ns-3 Library
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/bridge-helper.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/energy-module.h"

// Netconf Library
#include <sysrepo-cpp/Session.hpp>

// VES
#include <curl/curl.h>
#include <cjson/cJSON.h>

// C++ Standard Library
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <regex>
#include <thread>
#include <stdexcept>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <csignal>
#include <unistd.h>
#include <ctime>


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFi-DT-TEST");

Time simTime {"2592000s"};
uint32_t payloadSize = 1472;

NodeContainer apNodes;
NetDeviceContainer apDevices_all;
NetDeviceContainer apDevices_24;
NetDeviceContainer apDevices_5;
MobilityHelper mobility;
YansWifiChannelHelper channel24;
YansWifiChannelHelper channel5;
YansWifiPhyHelper yansPhy24;
YansWifiPhyHelper yansPhy5;
InternetStackHelper stack;
Ipv4AddressHelper address24;
Ipv4AddressHelper address5;
Ssid ssid;

std::map<uint32_t, Ptr<PacketSink>> sinkAppMap;  
std::map<uint32_t, uint64_t> previousRxBytes;
std::map<Ptr<WifiRadioEnergyModel>, double> previouspower;
std::map<Ipv4Address, std::pair<Ptr<NetDevice>, Ptr<Node>>> ipToDeviceNodeMap;
std::vector<Ipv4Address> ipList;
std::vector<Ptr<WifiRadioEnergyModel>> deviceEnergyModels;
std::map<Ptr<WifiRadioEnergyModel>, Ptr<NetDevice>> modelToDeviceMap;
WifiCoTraceHelper coTraceHelper;

static volatile int exit_application = 0;
static void sigint_handler(int signum)
{

    exit_application = 1;
}

class AccessPointManager 
{
    public:
        struct Position {
            double x;
            double y;
            double z;
        };
    
        struct AccessPoint {
            int ap_id;
            std::string ap_name;
            std::string ap_standard;
            Position location;
            std::string ssid;
            int band;           // 1: 2.4GHz, 2: 5GHz, 3: Dual-band
            int channel_band24;  // e.g. channel 1 for 2.4GHz
            int channel_band5;  // e.g. channel 36 for 5GHz
        };
    
        void AddAccessPoint(const AccessPoint& ap) {
            ap_list.push_back(ap);
        }
    
        void PrintAll() const {
            std::cout << "Reicived APs configurations:" << std::endl;
            for (const auto& ap : ap_list) {
                std::cout << "AP ID: " << ap.ap_id << ", Name: " << ap.ap_name
                          << ", SSID: " << ap.ssid << ", Standard: " << ap.ap_standard 
                          << ", Band: " << ap.band
                          << ", Position: (" << ap.location.x << ", "
                          << ap.location.y << ", " << ap.location.z << ")"
                          << ", CH1: " << ap.channel_band24
                          << ", CH2: " << ap.channel_band5
                          << std::endl;
            }
            std::cout << std::string(150, '-') << std::endl;
        }
    
        const std::vector<AccessPoint>& GetList() const {
            return ap_list;
        }
    
    private:
        std::vector<AccessPoint> ap_list;
};

std::string url = "http://192.168.8.121:30417/eventListener/v7";
class VesEventSender 
{
    public:
        static bool Send(const std::string& eventJson, const std::string& url) {
            CURL *curl;
            CURLcode res;
            struct curl_slist *headers = NULL;
    
            curl_global_init(CURL_GLOBAL_ALL);
            curl = curl_easy_init();
    
            if (curl) {
                headers = curl_slist_append(headers, "Content-Type: application/json");
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, eventJson.c_str());
    
                res = curl_easy_perform(curl);
                if (res != CURLE_OK) {
                    std::cerr << "Failed to send VES event: " << curl_easy_strerror(res) << std::endl;
                    curl_easy_cleanup(curl);
                    curl_slist_free_all(headers);
                    curl_global_cleanup();
                    return false;
                }
    
                curl_easy_cleanup(curl);
                curl_slist_free_all(headers);
            }
    
            curl_global_cleanup();
            return true;
        }
    
        static std::string CreatePnfEvent() {
            cJSON *root = cJSON_CreateObject();
            cJSON *event = cJSON_CreateObject();
            cJSON *commonHeader = cJSON_CreateObject();
    
            // Common header
            cJSON_AddStringToObject(commonHeader, "domain", "pnfRegistration");
            cJSON_AddStringToObject(commonHeader, "eventId", "ns3-wifi-dt-pnfRegistration");
            cJSON_AddStringToObject(commonHeader, "eventName", "ns3-wifi-dt-pnfRegistration");
            cJSON_AddStringToObject(commonHeader, "eventType", "ns3-wifi-dt-pnfRegistration");
            cJSON_AddNumberToObject(commonHeader, "sequence", 1);
            cJSON_AddStringToObject(commonHeader, "priority", "Low");
            cJSON_AddStringToObject(commonHeader, "reportingEntityId", "");
            cJSON_AddStringToObject(commonHeader, "reportingEntityName", "ns3-wifi-dt");
            cJSON_AddStringToObject(commonHeader, "sourceId", "5487");
            cJSON_AddStringToObject(commonHeader, "sourceName", "ns3-wifi-dt");
            cJSON_AddNumberToObject(commonHeader, "startEpochMicrosec", 1737314968575);
            cJSON_AddNumberToObject(commonHeader, "lastEpochMicrosec", 1737314968588);
            cJSON_AddStringToObject(commonHeader, "nfNamingCode", "001");
            cJSON_AddStringToObject(commonHeader, "nfVendorName", "ns-3 wifi-dt");
            cJSON_AddStringToObject(commonHeader, "timeZoneOffset", "+00:00");
            cJSON_AddStringToObject(commonHeader, "version", "4.1");
            cJSON_AddStringToObject(commonHeader, "vesEventListenerVersion", "7.2.1");
    
            cJSON_AddItemToObject(event, "commonEventHeader", commonHeader);
    
            // PNF registration fields
            cJSON *pnfFields = cJSON_CreateObject();
            cJSON_AddStringToObject(pnfFields, "pnfRegistrationFieldsVersion", "2.1");
            cJSON_AddStringToObject(pnfFields, "lastServiceDate", "2025-01-01");
            cJSON_AddStringToObject(pnfFields, "macAddress", "00:00:00:00:00:00");
            cJSON_AddStringToObject(pnfFields, "manufactureDate", "2025-01-01");
            cJSON_AddStringToObject(pnfFields, "modelNumber", "ns3-wifi");
            cJSON_AddStringToObject(pnfFields, "oamV4IpAddress", "192.168.8.98");
            cJSON_AddStringToObject(pnfFields, "serialNumber", "ns3-wifi-192.168.8.98");
            cJSON_AddStringToObject(pnfFields, "softwareVersion", "2.3.5");
            cJSON_AddStringToObject(pnfFields, "unitFamily", "ns-3");
            cJSON_AddStringToObject(pnfFields, "unitType", "ns3-wifi");
            cJSON_AddStringToObject(pnfFields, "vendorName", "ns-3");
    
            cJSON *additionalFields = cJSON_CreateObject();
            cJSON_AddStringToObject(additionalFields, "oamPort", "830");
            cJSON_AddStringToObject(additionalFields, "protocol", "SSH");
            cJSON_AddStringToObject(additionalFields, "username", "netconf");
            cJSON_AddStringToObject(additionalFields, "password", "netconf!");
            cJSON_AddStringToObject(additionalFields, "reconnectOnChangedSchema", "false");
            cJSON_AddStringToObject(additionalFields, "sleep-factor", "1.5");
            cJSON_AddStringToObject(additionalFields, "tcpOnly", "false");
            cJSON_AddStringToObject(additionalFields, "connectionTimeout", "20000");
            cJSON_AddStringToObject(additionalFields, "maxConnectionAttempts", "100");
            cJSON_AddStringToObject(additionalFields, "betweenAttemptsTimeout", "2000");
            cJSON_AddStringToObject(additionalFields, "keepaliveDelay", "120");
    
            cJSON_AddItemToObject(pnfFields, "additionalFields", additionalFields);
            cJSON_AddItemToObject(event, "pnfRegistrationFields", pnfFields);
            cJSON_AddItemToObject(root, "event", event);
    
            char *jsonStr = cJSON_PrintUnformatted(root);
            std::string result(jsonStr);
            cJSON_Delete(root);
            free(jsonStr);
    
            return result;
        }
    
        static std::string CreateMeasurementEvent(const std::map<std::string, std::string>& throughputMap) {
            cJSON *root = cJSON_CreateObject();
            cJSON *event = cJSON_CreateObject();
            cJSON *commonHeader = cJSON_CreateObject();
    
            cJSON_AddStringToObject(commonHeader, "version", "4.1");
            cJSON_AddStringToObject(commonHeader, "vesEventListenerVersion", "7.2.1");
            cJSON_AddStringToObject(commonHeader, "domain", "measurement");
            cJSON_AddStringToObject(commonHeader, "eventName", "measurement_wifi-dt_AP_throughput");
            cJSON_AddStringToObject(commonHeader, "eventId", "measurement_event_001");
            cJSON_AddNumberToObject(commonHeader, "sequence", 1);
            cJSON_AddStringToObject(commonHeader, "priority", "Normal");
            cJSON_AddStringToObject(commonHeader, "reportingEntityName", "ns-3 wifi-dt");
            cJSON_AddStringToObject(commonHeader, "sourceName", "ns-3 wifi-dt");
            cJSON_AddNumberToObject(commonHeader, "startEpochMicrosec", Simulator::Now().GetSeconds() * 1000000);
            cJSON_AddNumberToObject(commonHeader, "lastEpochMicrosec", Simulator::Now().GetSeconds() * 1000000);
            cJSON_AddStringToObject(commonHeader, "timeZoneOffset", "UTC-05:30");
    
            cJSON_AddItemToObject(event, "commonEventHeader", commonHeader);
    
            cJSON *measurementFields = cJSON_CreateObject();
            cJSON_AddStringToObject(measurementFields, "measurementFieldsVersion", "4.0");
            cJSON_AddNumberToObject(measurementFields, "measurementInterval", 1000);
    
            cJSON *additionalFields = cJSON_CreateObject();
            cJSON_AddStringToObject(additionalFields, "simulation_time", std::to_string(Simulator::Now().GetSeconds()).c_str());
    
            for (const auto &item : throughputMap) {
                cJSON_AddStringToObject(additionalFields, item.first.c_str(), item.second.c_str());
            }
    
            cJSON_AddItemToObject(measurementFields, "additionalFields", additionalFields);
            cJSON_AddItemToObject(event, "measurementFields", measurementFields);
            cJSON_AddItemToObject(root, "event", event);
    
            char *jsonStr = cJSON_PrintUnformatted(root);
            std::string result(jsonStr);
            cJSON_Delete(root);
            free(jsonStr);
    
            return result;
        }
};

class ns3sim
{
    public:
        static void StartSimulation(const AccessPointManager* apMgr)
        {   
            // Create the channel and PHY
            channel24 = YansWifiChannelHelper::Default();
            channel24.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
            channel24.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
            channel24.AddPropagationLoss("ns3::RangePropagationLossModel");
            Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(5));

            yansPhy24.SetChannel(channel24.Create());
            yansPhy24.Set("Antennas", UintegerValue(4));
            yansPhy24.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
            yansPhy24.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

            channel5 = YansWifiChannelHelper::Default();
            channel5.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
            channel5.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
            channel5.AddPropagationLoss("ns3::RangePropagationLossModel");
            Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(5));

            yansPhy5.SetChannel(channel5.Create());
            yansPhy5.Set("Antennas", UintegerValue(4));
            yansPhy5.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
            yansPhy5.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

            //get AP configurations from AccessPointManager
            const auto& ns3_ap_list = apMgr->GetList();

            //create AP nodes
            for (const auto& ns3_ap : ns3_ap_list)
            {
                Ptr<Node> apNode = CreateObject<Node>();
                apNodes.Add(apNode);
                Names::Add (ns3_ap.ap_name, apNode);

                //Mobility model
                Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
                positionAlloc->Add(Vector(ns3_ap.location.x, ns3_ap.location.y, ns3_ap.location.z));

                // MobilityHelper mobility;
                mobility.SetPositionAllocator(positionAlloc);
                mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                mobility.Install(apNode);

                //Wifi standard
                WifiHelper wifi;
                if (ns3_ap.ap_standard == "802.11ax") {
                    wifi.SetStandard(WIFI_STANDARD_80211ax);
                } else if (ns3_ap.ap_standard == "802.11n") {
                    wifi.SetStandard(WIFI_STANDARD_80211n);
                } else if (ns3_ap.ap_standard == "802.11ac") {
                    wifi.SetStandard(WIFI_STANDARD_80211ac);
                }

                // Set remote station manager
                wifi.SetRemoteStationManager("ns3::IdealWifiManager");
                ssid = Ssid(ns3_ap.ssid);

                //Install netDevice
                if (ns3_ap.band == 1) {
                    if (ns3_ap.ap_standard == "802.11ax") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(20.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(20.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11n") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(18.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(18.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11ac") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(19.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(19.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    }

                    std::ostringstream channelValue;
                    channelValue << "{" << ns3_ap.channel_band24 << ", 20, BAND_2_4GHZ, 0}";
                    yansPhy24.Set("ChannelSettings", StringValue(channelValue.str()));

                    WifiMacHelper mac;
                    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
                    NetDeviceContainer apDevice = wifi.Install(yansPhy24, mac, apNode);
                    apDevices_24.Add(apDevice);

                } else if (ns3_ap.band == 2) {
                    if (ns3_ap.ap_standard == "802.11ax") {
                        yansPhy5.Set("TxPowerStart", DoubleValue(30.0));
                        yansPhy5.Set("TxPowerEnd", DoubleValue(30.0));
                        yansPhy5.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11n") {
                        yansPhy5.Set("TxPowerStart", DoubleValue(18.0));
                        yansPhy5.Set("TxPowerEnd", DoubleValue(18.0));
                        yansPhy5.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11ac") {
                        yansPhy5.Set("TxPowerStart", DoubleValue(20.0));
                        yansPhy5.Set("TxPowerEnd", DoubleValue(20.0));
                        yansPhy5.Set("TxPowerLevels", UintegerValue(1));
                    }

                    std::string channelStr = Get5GChannelConfig(ns3_ap.channel_band5);
                    yansPhy5.Set("ChannelSettings", StringValue(channelStr));

                    WifiMacHelper mac;
                    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
                    NetDeviceContainer apDevice = wifi.Install(yansPhy5, mac, apNode);
                    apDevices_5.Add(apDevice);

                } else if (ns3_ap.band == 3) {
                    if (ns3_ap.ap_standard == "802.11ax") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(20.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(20.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11n") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(18.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(18.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11ac") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(19.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(19.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    }

                    std::ostringstream channelValue;
                    channelValue << "{" << ns3_ap.channel_band24 << ", 20, BAND_2_4GHZ, 0}";
                    yansPhy24.Set("ChannelSettings", StringValue(channelValue.str()));

                    WifiMacHelper mac1;
                    mac1.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
                    NetDeviceContainer apDevice_24G = wifi.Install(yansPhy24, mac1, apNode);
                    apDevices_24.Add(apDevice_24G);

                    if (ns3_ap.ap_standard == "802.11ax") {
                        yansPhy5.Set("TxPowerStart", DoubleValue(30.0));
                        yansPhy5.Set("TxPowerEnd", DoubleValue(30.0));
                        yansPhy5.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11n") {
                        yansPhy5.Set("TxPowerStart", DoubleValue(18.0));
                        yansPhy5.Set("TxPowerEnd", DoubleValue(18.0));
                        yansPhy5.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11ac") {
                        yansPhy5.Set("TxPowerStart", DoubleValue(20.0));
                        yansPhy5.Set("TxPowerEnd", DoubleValue(20.0));
                        yansPhy5.Set("TxPowerLevels", UintegerValue(1));
                    }

                    std::string channelStr = Get5GChannelConfig(ns3_ap.channel_band5);
                    yansPhy5.Set("ChannelSettings", StringValue(channelStr));

                    WifiMacHelper mac2;
                    mac2.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
                    NetDeviceContainer apDevice_5G = wifi.Install(yansPhy5, mac2, apNode);
                    apDevices_5.Add(apDevice_5G);
                }
            }

            //Internet stack
            stack.Install(apNodes);

            //Assign IP addresses
            address24.SetBase("10.1.0.0", "255.255.252.0");
            address5.SetBase("10.1.4.0", "255.255.252.0");
            Ipv4InterfaceContainer apInterface24 = address24.Assign(apDevices_24);
            Ipv4InterfaceContainer apInterface5 = address5.Assign(apDevices_5);

            apDevices_all.Add(apDevices_24);
            apDevices_all.Add(apDevices_5);

            //std::cout << "First AP IP: " << apInterface.GetAddress(0) << std::endl;

            //energy model
            BasicEnergySourceHelper basicSourceHelper;
            basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1e9));
            WifiRadioEnergyModelHelper radioEnergyHelper;
            radioEnergyHelper.Set("IdleCurrentA", DoubleValue(0.273));
            radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.313));
            radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.033));
            //radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.380));
            radioEnergyHelper.SetTxCurrentModel("ns3::LinearWifiTxCurrentModel",
                                        "Voltage",DoubleValue(3.0),
                                        "IdleCurrent",DoubleValue(0.273),
                                        "Eta",DoubleValue(0.1));
                                        
            for (uint32_t i = 0; i < apNodes.GetN(); ++i)
            {
                Ptr<Node> node = apNodes.Get(i);
                energy::EnergySourceContainer sources = basicSourceHelper.Install(node);
                Ptr<energy::EnergySource> source = sources.Get(0);

                for (uint32_t j = 0; j < node->GetNDevices()-1; ++j)
                {
                    Ptr<NetDevice> dev = node->GetDevice(j);
                    Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
                    if (wifiDev)
                    {
                        energy::DeviceEnergyModelContainer deviceModel = radioEnergyHelper.Install(wifiDev, source);
                        Ptr<WifiRadioEnergyModel> model = DynamicCast<WifiRadioEnergyModel>(deviceModel.Get(0));
                        deviceEnergyModels.push_back(model);
                        modelToDeviceMap[model] = dev;
                    }
                    else
                    {
                        std::cout << "Skipped: device is not WifiNetDevice or is null" << std::endl;
                    }
                }
            }


            //Application
            // uint16_t port = 9;
            // PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            // ApplicationContainer sinkApp = sink.Install(apNodes);
            // sinkApp.Start(Seconds(0.1));
            // sinkApp.Stop(Seconds(simTime));

            // uint16_t port = 777;
            // PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
            // ApplicationContainer sinkApp = sink.Install(apNodes);
            // sinkApp.Start(Seconds(0.1));
            // sinkApp.Stop(simTime);

            ApplicationContainer sinkApps;
            uint16_t port = 777;
            for (uint32_t i = 0; i < apNodes.GetN(); ++i)
            {
                Ptr<Node> node = apNodes.Get(i);
                Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
                for (uint32_t j = 0; j < node->GetNDevices(); ++j)
                {
                    Ptr<NetDevice> dev = node->GetDevice(j);
                    if (DynamicCast<LoopbackNetDevice>(dev)) 
                    {
                        continue;
                    }
                    int32_t interfaceIndex = ipv4->GetInterfaceForDevice(dev);
                    Ipv4InterfaceAddress iaddr = ipv4->GetAddress(interfaceIndex, 0);
                    Ipv4Address ipAddr = iaddr.GetLocal();
                    // std::cout << "AP Node: " << Names::FindName(node) 
                    //           << ", Device: " << dev->GetInstanceTypeId().GetName() 
                    //           << ", IP Address: " << iaddr << std::endl;

                    ipToDeviceNodeMap[ipAddr] = std::make_pair(dev, node);
                    ipList.push_back(ipAddr);

                    PacketSinkHelper sink("ns3::TcpSocketFactory",InetSocketAddress(ipAddr, port));
                    ApplicationContainer app = sink.Install(node);
                    app.Start(Seconds(0.1));
                    app.Stop(simTime);

                    sinkApps.Add(app);
                }
            }

            for(uint32_t i = 0; i < sinkApps.GetN(); ++i)
            {
                sinkAppMap[i] = DynamicCast<PacketSink>(sinkApps.Get(i));
            }

            coTraceHelper.Enable (apDevices_all);
            coTraceHelper.Start(Seconds(0.1));
            coTraceHelper.Stop(simTime);

            Simulator::Schedule(Seconds(1), &ns3sim::show_deployed_ap_info);
            Simulator::Schedule(Seconds(60), &ns3sim::show_throughput);
            Simulator::Schedule(Seconds(60), &ns3sim::show_CO);
            Simulator::Schedule(Seconds(60), &ns3sim::show_energy);
            Simulator::Schedule(Seconds(5), &ns3sim::test);


            Simulator::Stop(simTime);
            Simulator::Run();
            Simulator::Destroy();
        }

    private:
        static std::string Get5GChannelConfig(int inputChannel)
        {
            std::map<int, std::vector<int>> channelGroups = {
                {42,  {36, 40, 44, 48}},
                {58,  {52, 56, 60, 64}},
                {106, {100, 104, 108, 112}},
                {122, {116, 120, 124, 128}},
                {138, {132, 136, 140, 144}},
                {155, {149, 153, 157, 161}}
            };

            for (const auto& [group, channels] : channelGroups)
            {
                for (size_t i = 0; i < channels.size(); ++i)
                {
                    if (channels[i] == inputChannel)
                    {
                        std::ostringstream channelValue;
                        channelValue << "{" << group << ", 80, BAND_5GHZ, " << i << "}";
                        return channelValue.str();
                    }
                }
            }

            return "Invalid channel";
        }

        static void show_deployed_ap_info()
        {
            std::string name;
            Vector position;
            uint8_t ChannelWidth;
            uint8_t ChannelNumber = 0;
            uint16_t Frequency;
            WifiStandard Standard = WIFI_STANDARD_80211a;
            Ssid dev_ssid;

            std::cout << "Deployed APs infomations:" << std::endl;
            for (uint32_t i = 0; i < apNodes.GetN(); ++i)
            {
                Ptr<Node> node = apNodes.Get(i);
                name = Names::FindName(node);
                position = node->GetObject<MobilityModel>()->GetPosition();
                std::cout << "Node Name: " << name 
                          << ", Position: (" << position.x << ", " 
                          << position.y << ", " << position.z << ")" << std::endl;

                for (uint32_t j = 0; j < node->GetNDevices()-1; ++j)
                {
                    Ptr<NetDevice> dev = node->GetDevice(j);
                    std::cout << "   Device[" << j << "]: " << dev->GetInstanceTypeId().GetName();

                    Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
                    Ptr<WifiPhy> phy = wifiDev->GetPhy();
                    ChannelWidth = phy->GetChannelWidth();
                    Frequency = phy->GetFrequency();
                    uint8_t Primary20Index = phy->GetPrimary20Index();
                    if (Frequency >= 5000)
                    {
                        ChannelNumber = phy->GetChannelNumber() - 6 + Primary20Index*4;
                    }
                    else
                    {
                        ChannelNumber = phy->GetChannelNumber();
                    }
                    Standard = phy->GetStandard();

                    Ptr<WifiMac> dev_mac = wifiDev->GetMac();
                    dev_ssid = dev_mac->GetSsid();
                    
                    std::cout << ", " << dev_ssid << ", Standard: " << Standard << ", Channel Width: " 
                          << static_cast<int>(ChannelWidth) << " MHz, BAND: " 
                          << ((Frequency >= 5000) ? "5" : "2.4") << " GHz, Channel Number: " 
                          << static_cast<int>(ChannelNumber) << std::endl;
                }
            }
        }

        static void show_throughput()
        {
            std::cout << std::string(150, '=') << std::endl;
            for (const auto &sinkApp : sinkAppMap)
            {
                uint64_t totalRx = sinkApp.second -> GetTotalRx();
                uint64_t currentRx = totalRx - previousRxBytes[sinkApp.first];
                previousRxBytes[sinkApp.first] = totalRx;
                double throughput = (currentRx * 8.0) / (60 * 1000); // Convert to Kbps
                Ipv4Address ip = ipList[sinkApp.first];
                auto it = ipToDeviceNodeMap.find(ip);
                Ptr<NetDevice> dev = it->second.first;
                Ptr<Node> node = it->second.second;
                std::string nodeName = Names::FindName(node);
                std::cout << "Node Name: " << nodeName 
                          << ", Band: " << ((dev->GetObject<WifiNetDevice>()->GetPhy()->GetFrequency() >= 5000) ? "5 GHz" : "2.4 GHz")
                          << ", Throughput: " << throughput 
                          << " Kbps" << std::endl;
                // std::cout << "Node Name: " << nodeName 
                //           << ", Received Bytes: " << totalRx 
                //           << ", Current Received Bytes: " << currentRx 
                //           << ", Previous Received Bytes: " << previousRxBytes[sinkApp.first] 
                //           << ", Throughput: " << throughput 
                //           << " Kbps" << std::endl;
            }
            Simulator::Schedule(Seconds(60), &ns3sim::show_throughput);
        }

        static void show_CO()
        {
            //123
            coTraceHelper.PrintStatistics (std::cout);
            coTraceHelper.Reset();
            Simulator::Schedule(Seconds(60), &ns3sim::show_CO);
        }

        static void show_energy()
        {
            std::cout << "Energy consumption of devices:" << std::endl;
            for (auto model : deviceEnergyModels)
            {
                double TotalenergyConsumed = model->GetTotalEnergyConsumption();
                double currentenergyConsumed = TotalenergyConsumed - previouspower[model];
                previouspower[model] = TotalenergyConsumed;
                Ptr<NetDevice> dev = modelToDeviceMap.find(model)->second;
                Ptr<Node> node = dev->GetNode();
                std::cout << "    Node Name: " << Names::FindName(node) 
                          << ", Band: " << ((dev->GetObject<WifiNetDevice>()->GetPhy()->GetFrequency() >= 5000) ? "5 GHz" : "2.4 GHz") 
                          << ", Energy Consumed: " << currentenergyConsumed << " J" << std::endl;
            }
            Simulator::Schedule(Seconds(60), &ns3sim::show_energy);
        }

        static void test()
        {
            std::cout << "Test function called." << std::endl;
            // Add your test logic here
            NodeContainer staNode;
            staNode.Create(1);

            WifiHelper wifi;
            wifi.SetStandard(WIFI_STANDARD_80211ax);
            wifi.SetRemoteStationManager("ns3::IdealWifiManager");

            WifiMacHelper mac;
            mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(true));

            Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
            positionAlloc->Add(Vector(0.0, 0.0, 0.0));
            mobility.SetPositionAllocator(positionAlloc);
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(staNode);

            stack.Install(staNode);

            yansPhy24.Set("ChannelSettings", StringValue("{1, 20, BAND_2_4GHZ, 0}"));
            //yansPhy5.Set("ChannelSettings", StringValue("{42, 80, BAND_5GHZ, 0}"));

            NetDeviceContainer staDevice;
            staDevice = wifi.Install(yansPhy24, mac, staNode);

            Ipv4InterfaceContainer staInterface = address24.Assign(staDevice);

            Ipv4Address apAddress("10.1.0.1");
            uint16_t port = 777;
            Address sinkAddress(InetSocketAddress(apAddress, port));
            BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
            source.SetAttribute("MaxBytes", UintegerValue(10000000)); // 10 MB
            source.SetAttribute("SendSize", UintegerValue(payloadSize));
            ApplicationContainer sourceApps = source.Install(staNode);
            sourceApps.Start(Seconds(0.1));
            sourceApps.Stop(Seconds(50));
        }
};

#define XPATH_MAX_LEN 256
class APConfigCb : public sysrepo::Callback
{
    public:
        int module_change(sysrepo::S_Session session, const char *module_name, const char *xpath, sr_event_t event, uint32_t request_id, void *private_data)
        {
            char change_path[XPATH_MAX_LEN];

            if (event == SR_EV_CHANGE)
            {
                try
                {
                    std::string ssid;
                    int apId, band = 0;
                    std::string band_G;
                    std::string ap_standard;

                    snprintf(change_path, XPATH_MAX_LEN, "/%s:*//.", module_name);
                    auto it = session->get_changes_iter(change_path);

                    std::map<int, AccessPointManager::AccessPoint> tempAPMap;

                    while (auto change = session->get_change_next(it)) {
                        if(nullptr != change->new_val())
                        {
                            std::string val = change->new_val()->val_to_string();
                            std::string parent, leaf;

                            getLeafInfo(change->new_val()->to_string(), parent, leaf);
                            //std::cout << "parent: " << parent<< std::endl;
                            //std::cout << "leaf: " << leaf<< std::endl;
                            //std::cout << "val: " << val<< std::endl;
                            //std::cout << "-------------------------------------------------------------------------"<< std::endl;


                            if (parent.find("GNBDUFunction") != std::string::npos && leaf == "id"){
                                std::regex regex_pattern(R"(.*,([^,]+),AP-(\d+),([^,]+),band=(\d+))");
                                std::smatch match;

                                if (std::regex_match(val, match, regex_pattern)) {
                                    ssid = match[1];
                                    apId = std::stoi(match[2]);
                                    ap_standard = match[3];
                                    band = std::stoi(match[4]);
                                    tempAPMap[apId].ap_id = apId;
                                    tempAPMap[apId].ssid = ssid;
                                    tempAPMap[apId].ap_standard = ap_standard;
                                    tempAPMap[apId].band = band;
                                }
                            }
                            else if (parent.find("NRCellDU") && leaf == "id"){
                                std::regex regex_pattern(R"(.*,([^,]+))");
                                std::smatch match;

                                if (std::regex_match(val, match, regex_pattern)) {
                                    band_G = match[1];
                                }
                            }
                            else if (parent == "attributes"){
                                if (leaf == "gNBDUName"){
                                    tempAPMap[apId].ap_name = val;
                                }
                                else if (leaf == "arfcnDL"){
                                    if (band == 1){
                                        tempAPMap[apId].channel_band24 = std::stoi(val);
                                        tempAPMap[apId].channel_band5 = 0;
                                    }
                                    else if (band == 2){
                                        tempAPMap[apId].channel_band24 = 0;
                                        tempAPMap[apId].channel_band5 = std::stoi(val);
                                    }
                                    else if (band == 3){
                                        if (band_G == "2.4G"){
                                            tempAPMap[apId].channel_band24 = std::stoi(val);
                                        }
                                        else if (band_G == "5G"){
                                            tempAPMap[apId].channel_band5 = std::stoi(val);
                                        }
                                    }
                                }
                            }
                            else if (leaf == "siteLatitude"){
                                tempAPMap[apId].location.x = std::stod(val);
                            }
                            else if(leaf == "siteLongitude"){
                                tempAPMap[apId].location.y = std::stod(val);
                            }
                            else if (leaf == "siteAltitude"){
                                tempAPMap[apId].location.z = std::stod(val);
                            }
                        }
                    }

                    for (const auto& [key, ap] : tempAPMap) 
                    {
                        accessPointManager.AddAccessPoint(ap);
                    }
                    accessPointManager.PrintAll();

                    std::thread simThread(&ns3sim::StartSimulation, &accessPointManager);
                    simThread.detach();
                }
                catch (const std::exception &e)
                {
                    std::cerr << "ap module change callback error: " << e.what() << std::endl;
                    return SR_ERR_CALLBACK_FAILED;
                }
            }
            return SR_ERR_OK;
        }

    private:
        void getLeafInfo(std::string xpath,std::string &parent, std::string &leaf )
        {
            std::stringstream ssLeaf(xpath);
            std::stringstream ssParent(xpath);
            char ch='/';
            getline(ssLeaf, leaf, ch);
            while(getline(ssLeaf, leaf, ch))
            {
                getline(ssParent, parent, ch);
            }
            std::stringstream ss(leaf);
            ch=' ';
            getline(ss, leaf, ch);
        }

        AccessPointManager accessPointManager;
};

int main(int argc, char *argv[]) 
{
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    // Connect to sysrepo
    sysrepo::S_Connection conn(new sysrepo::Connection());

    // Start a session
    sysrepo::S_Session session(new sysrepo::Session(conn));

    // Subscribe to changes
    sysrepo::S_Subscribe subscribe(new sysrepo::Subscribe(session));

    // Create and set up the callback
    sysrepo::S_Callback wifi_ap_config_cb(new APConfigCb());
    subscribe->module_change_subscribe("_3gpp-common-managed-element", wifi_ap_config_cb);

    
    // std::string pnfEvent = VesEventSender::CreatePnfEvent();
    // if (VesEventSender::Send(pnfEvent, url)) {
    //     std::cout << "[pnfRegistration]" << std::endl;
    // } else {
    //     std::cerr << "Failed to send VES event." << std::endl;
    // }

    while (!exit_application)
        {
            sleep(1);
        }

    return 0;
}