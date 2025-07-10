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

#include "httplib.h"  // header-only REST Server
#include <thread>
#include <atomic>
#include "json.hpp"



using namespace ns3;
using namespace httplib;
using json = nlohmann::json;

NS_LOG_COMPONENT_DEFINE("WiFi-DT-TEST");

Time simTime {"2592000s"};
uint32_t payloadSize = 1472;
uint16_t port_ul = 777;
uint16_t port_dl = 778;

NodeContainer apNodes;
NodeContainer staNodes;
NetDeviceContainer apDevices_all;
NetDeviceContainer apDevices_24;
NetDeviceContainer apDevices_5;
YansWifiChannelHelper channel24;
YansWifiChannelHelper channel5;
YansWifiPhyHelper yansPhy24;
YansWifiPhyHelper yansPhy5;
Ipv4AddressHelper address24;
Ipv4AddressHelper address5;
MobilityHelper mobility;
InternetStackHelper stack;
Ssid ssid;
WifiCoTraceHelper coTraceHelper;
ApplicationContainer sinkApps_ap;

std::map<uint32_t, Ptr<PacketSink>> sinkAppMap_ap;
std::map<std::string, Ptr<PacketSink>> sinkAppMap_sta;
std::map<uint32_t, uint64_t> previousRxBytes_ap;

std::map<uint32_t, uint64_t> g_txBytesPerDevice;
std::map<uint32_t, uint64_t> g_prevTxBytesPerDevice;

std::map<std::string, uint64_t> previousRxBytes_sta;
std::map<Ptr<WifiRadioEnergyModel>, double> previouspower;
std::map<Ipv4Address, std::pair<Ptr<NetDevice>, Ptr<Node>>> ipToDeviceNodeMap;
std::vector<Ipv4Address> ipList;
std::vector<Ptr<WifiRadioEnergyModel>> deviceEnergyModels;
std::map<Ptr<WifiRadioEnergyModel>, Ptr<NetDevice>> modelToDeviceMap;

json g_staConfig;
std::mutex g_mutex;
std::atomic<bool> g_triggerSend(false);


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
            std::cout << "[NETCONF] Reicived APs configurations:" << std::endl;
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


class RestfulServer
{
    public:
        static void StartRestfulServer()
        {
            Server svr;

            svr.Post("/upload", [](const Request& req, Response& res) {
            try {
                auto j = json::parse(req.body);

                std::lock_guard<std::mutex> lock(g_mutex);
                g_staConfig.clear();

                if (!j.contains("Minute")) {
                    std::cerr << "[REST] No 'Minute' field found\n";
                    res.set_content("{\"status\": \"missing Minute\"}", "application/json");
                    return;
                }

                const auto& minuteData = j["Minute"];

                if (!minuteData.contains("STAConfig")) {
                    std::cerr << "[REST] Invalid 'Minute' format\n";
                    res.set_content("{\"status\": \"invalid format\"}", "application/json");
                    return;
                }

                g_staConfig = minuteData;

                std::cout << std::string(150, '-') << std::endl;
                std::cout << "[REST] Received REST upload.\n";
                std::cout << "  DateTime: " << g_staConfig["DateTime"] << "\n";

                int i = 0;
                for (const auto& sta : g_staConfig["STAConfig"]) {
                    std::cout << "    [" << i++ << "] "
                            << sta["STAName"] << ", "
                            << sta["ssid"] << ", "
                            << sta["connectedofAP"] << ", "
                            << sta["radio band"] << ", "
                            << sta["bytes"] << " bytes, "
                            << sta["dataDirection"] << "\n";
                }
                std::cout << std::string(150, '-') << std::endl;

                g_triggerSend = true;
                res.set_content("{\"status\": \"single-minute ok\"}", "application/json");

            } catch (const std::exception& e) {
                std::cerr << "JSON parsing error: " << e.what() << std::endl;
                res.set_content("{\"status\": \"invalid json\"}", "application/json");
            }
        });

            std::cout << "RESTful server listening on port 5000" << std::endl;
            svr.listen("0.0.0.0", 5000);
        }
};

class ns3sim
{
    public:
        static void StartSimulation(const AccessPointManager* apMgr)
        {   
            std::thread RestServerThread(&RestfulServer::StartRestfulServer);
            RestServerThread.detach();

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

                    PacketSinkHelper sink("ns3::TcpSocketFactory",InetSocketAddress(ipAddr, port_ul));
                    ApplicationContainer sink_ap = sink.Install(node);
                    sink_ap.Start(Seconds(0.1));
                    sink_ap.Stop(simTime);

                    sinkApps_ap.Add(sink_ap);
                }
            }

            for(uint32_t i = 0; i < sinkApps_ap.GetN(); ++i)
            {
                sinkAppMap_ap[i] = DynamicCast<PacketSink>(sinkApps_ap.Get(i));
            }

            uint32_t devCounter = 0;
            for (uint32_t i = 0; i < apNodes.GetN(); ++i)
            {
                Ptr<Node> node = apNodes.Get(i);
                for (uint32_t j = 0; j < node->GetNDevices(); ++j)
                {
                    Ptr<NetDevice> dev = node->GetDevice(j);
                    if (!DynamicCast<WifiNetDevice>(dev)) continue;

                    std::ostringstream path;
                    path << "/NodeList/" << node->GetId()
                        << "/DeviceList/" << j
                        << "/$ns3::WifiNetDevice/Mac/MacTx";

                    Config::ConnectWithoutContext(path.str(), MakeBoundCallback(&MacTxTrace, devCounter));
                    devCounter++;
                }
            }

            coTraceHelper.Enable (apDevices_all);
            coTraceHelper.Start(Seconds(0.1));
            coTraceHelper.Stop(simTime);

            Simulator::Schedule(Seconds(1), &ns3sim::show_deployed_ap_info);
            Simulator::Schedule(Seconds(60), &ns3sim::show_ap_output_info);
            Simulator::Schedule(Seconds(60), &ns3sim::show_sta_output_info);
            Simulator::Schedule(Seconds(60), &ns3sim::show_CO);

            //Simulator::Schedule(Seconds(5), &ns3sim::test);
            //Simulator::Schedule(Seconds(60), &ns3sim::test_change_ap);
            Simulator::Schedule(Seconds(60), &ns3sim::sta_config);


            Simulator::Stop(simTime);
            Simulator::Run();
            Simulator::Destroy();
        }

    private:

        static void MacTxTrace(uint32_t devId, Ptr<const Packet> packet)
        {
            g_txBytesPerDevice[devId] += packet->GetSize();
        }

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

            std::cout << "[NS-3] Deployed APs infomations:" << std::endl;
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

        static void show_CO()
        {
            coTraceHelper.PrintStatistics (std::cout);
            coTraceHelper.Reset();
            std::cout << std::string(150, '=') << std::endl;
            Simulator::Schedule(Seconds(60), &ns3sim::show_CO);
        }

        static void show_ap_output_info()
        {
            double simTime = ns3::Simulator::Now().GetSeconds();
            std::time_t realTime = std::time(nullptr);
            std::cout << "==>[SIM " << simTime << "s] Wall time: " << std::ctime(&realTime);

            std::map<std::pair<std::string, std::string>, double> rx_throughputMap;
            std::map<std::pair<std::string, std::string>, double> tx_throughputMap;

            for (const auto &sinkApp : sinkAppMap_ap)
            {
                uint64_t totalRx = sinkApp.second->GetTotalRx();
                uint64_t currentRx = totalRx - previousRxBytes_ap[sinkApp.first];
                previousRxBytes_ap[sinkApp.first] = totalRx;
                double rx_throughput = (currentRx * 8.0) / (60 * 1000); // Kbps

                Ipv4Address ip = ipList[sinkApp.first];
                auto it = ipToDeviceNodeMap.find(ip);
                Ptr<NetDevice> dev = it->second.first;
                Ptr<Node> node = it->second.second;
                std::string nodeName = Names::FindName(node);
                std::string band = (dev->GetObject<WifiNetDevice>()->GetPhy()->GetFrequency() >= 5000) ? "  5 GHz" : "2.4 GHz";

                rx_throughputMap[{nodeName, band}] = rx_throughput;
            }

            for (const auto& [devId, totalBytes] : g_txBytesPerDevice)
            {
                uint64_t deltaBytes = totalBytes - g_prevTxBytesPerDevice[devId];
                g_prevTxBytesPerDevice[devId] = totalBytes;
                double tx_throughput = (deltaBytes * 8.0) / 60.0 / 1000.0;

                Ipv4Address ip = ipList[devId];
                auto it = ipToDeviceNodeMap.find(ip);
                Ptr<NetDevice> dev = it->second.first;
                Ptr<Node> node = it->second.second;
                std::string nodeName = Names::FindName(node);
                std::string band = (dev->GetObject<WifiNetDevice>()->GetPhy()->GetFrequency() >= 5000) ? "  5 GHz" : "2.4 GHz";

                tx_throughputMap[{nodeName, band}] = tx_throughput;
            }
            
            for (auto model : deviceEnergyModels)
            {
                double totalEnergy = model->GetTotalEnergyConsumption();
                double currentEnergy = totalEnergy - previouspower[model];
                previouspower[model] = totalEnergy;

                Ptr<NetDevice> dev = modelToDeviceMap[model];
                Ptr<Node> node = dev->GetNode();
                std::string nodeName = Names::FindName(node);
                std::string band = (dev->GetObject<WifiNetDevice>()->GetPhy()->GetFrequency() >= 5000) ? "  5 GHz" : "2.4 GHz";

                double rx_throughput = rx_throughputMap[{nodeName, band}];
                double tx_throughput = tx_throughputMap[{nodeName, band}];
                std::cout << "Node Name: " << nodeName
                        << ", Band: " << band
                        << ", TX Throughput: " << tx_throughput << " Kbps"
                        << ", RX Throughput: " << rx_throughput << " Kbps"
                        << ", Energy Consumed: " << currentEnergy << " J"
                        << std::endl;
            }

            Simulator::Schedule(Seconds(60), &ns3sim::show_ap_output_info);
        }

        static void show_sta_output_info()
        {
            std::cout << "show sta output info: " << std::endl;
            for (const auto &sinkApp : sinkAppMap_sta)
            {
                uint64_t totalRx = sinkApp.second->GetTotalRx();
                uint64_t currentRx = totalRx - previousRxBytes_sta[sinkApp.first];
                previousRxBytes_sta[sinkApp.first] = totalRx;
                double throughput = (currentRx * 8.0) / (60 * 1000); // Kbps

                std::cout << "STA Node: " << sinkApp.first 
                          << ", RX Throughput: " << throughput << " Kbps" 
                          << std::endl;
            }
            Simulator::Schedule(Seconds(60), &ns3sim::show_sta_output_info);
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
            

            NetDeviceContainer staDevice24;
            staDevice24 = wifi.Install(yansPhy24, mac, staNode);
            Ipv4InterfaceContainer staInterface24 = address24.Assign(staDevice24);

            yansPhy5.Set("ChannelSettings", StringValue("{42, 80, BAND_5GHZ, 0}"));
            NetDeviceContainer staDevice5;
            staDevice5 = wifi.Install(yansPhy5, mac, staNode);
            Ipv4InterfaceContainer staInterface5 = address5.Assign(staDevice5);

            Ipv4Address apAddress("10.1.4.1");
            Address sinkAddress(InetSocketAddress(apAddress, port_ul));
            BulkSendHelper source("ns3::TcpSocketFactory", sinkAddress);
            source.SetAttribute("MaxBytes", UintegerValue(10000000)); // 10 MB
            source.SetAttribute("SendSize", UintegerValue(payloadSize));
            ApplicationContainer sourceApps = source.Install(staNode);
            sourceApps.Start(Seconds(0.1));
            sourceApps.Stop(Seconds(60));
        }

        static void test_change_ap_config()
        {
            std::cout << "Test change AP connection." << std::endl;
            
        }

        static void sta_config()
        {
            if (g_triggerSend) {
                std::lock_guard<std::mutex> lock(g_mutex);
                
                if (!g_staConfig["STAConfig"].empty()){
                    for (const auto& sta : g_staConfig["STAConfig"]) 
                    {
                        ApplicationContainer sink_sta;
                        if (Names::Find<Node>(sta["STAName"]) == nullptr)
                        {
                            std::cout << sta["STAName"] << " NOT exist, adding STA....."<< std::endl;
                            Ptr<Node> staNode = CreateObject<Node>();
                            staNodes.Add(staNode);
                            Names::Add (sta["STAName"], staNode);

                            WifiHelper wifi;
                            wifi.SetStandard(WIFI_STANDARD_80211ax);
                            wifi.SetRemoteStationManager("ns3::IdealWifiManager");

                            WifiMacHelper mac;
                            std::string ssidStr = sta["ssid"];
                            mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssidStr), "ActiveProbing", BooleanValue(true));

                            stack.Install(staNode);

                            yansPhy24.Set("ChannelSettings", StringValue("{1, 20, BAND_2_4GHZ, 0}"));
                            NetDeviceContainer staDevice24;
                            staDevice24 = wifi.Install(yansPhy24, mac, staNode);
                            Ipv4InterfaceContainer staInterface24 = address24.Assign(staDevice24);

                            yansPhy5.Set("ChannelSettings", StringValue("{42, 80, BAND_5GHZ, 0}"));
                            NetDeviceContainer staDevice5;
                            staDevice5 = wifi.Install(yansPhy5, mac, staNode);
                            Ipv4InterfaceContainer staInterface5 = address5.Assign(staDevice5);
                        }else
                        {
                            std::cout << sta["STAName"] << " exist, skip adding STA." << std::endl;
                        }

                        
                        Ptr<Node> staNode_P = Names::Find<Node>(sta["STAName"]);
                        Ptr<Node> apNode_staconneced = Names::Find<Node>(sta["connectedofAP"]);
                        
                        Ptr<Ipv4> ap_ipv4 = apNode_staconneced->GetObject<Ipv4>();
                        Ipv4Address ap_netdevice_ipAddr;

                        Ptr<Ipv4> sta_ipv4 = staNode_P->GetObject<Ipv4>();
                        Ipv4Address sta_netdevice_ipAddr;
                        
                        Ptr<MobilityModel> ap_mobility = apNode_staconneced->GetObject<MobilityModel>();
                        Vector ap_position = ap_mobility->GetPosition();
                        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
                        positionAlloc->Add(ap_position);
                        mobility.SetPositionAllocator(positionAlloc);
                        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                        mobility.Install(staNode_P);

                        if (sinkAppMap_sta.find(sta["STAName"]) == sinkAppMap_sta.end()) {
                            PacketSinkHelper sinksta("ns3::TcpSocketFactory",InetSocketAddress(Ipv4Address::GetAny(), port_dl));
                            sink_sta = sinksta.Install(staNode_P);
                            sink_sta.Start(Seconds(0.0));
                            sink_sta.Stop(Seconds(60.0));
                            sinkAppMap_sta[sta["STAName"]] = DynamicCast<PacketSink>(sink_sta.Get(0));
                        } else {
                            sink_sta.Start(Seconds(0.0));
                            sink_sta.Stop(Seconds(60.0));
                        }
                        
                        if (sta["radio band"] == "2.4GHz") {
                            Ptr<NetDevice> ap_dev = apNode_staconneced->GetDevice(0);
                            int32_t ap_interfaceIndex = ap_ipv4->GetInterfaceForDevice(ap_dev);
                            ap_netdevice_ipAddr = ap_ipv4->GetAddress(ap_interfaceIndex, 0).GetLocal();
                            Ptr<WifiNetDevice> ap_wifiDev = DynamicCast<WifiNetDevice>(ap_dev);
                            Ptr<WifiPhy> ap_wifiphy = ap_wifiDev->GetPhy();
                            uint8_t ap_ChannelNumber = ap_wifiphy->GetChannelNumber();
                            Ptr<NetDevice> sta_dev = staNode_P->GetDevice(1);
                            Ptr<WifiNetDevice> staDevice24 = DynamicCast<WifiNetDevice>(sta_dev);
                            Ptr<WifiPhy> sta_wifiphy24 = staDevice24->GetPhy();
                            sta_wifiphy24->SetOperatingChannel(WifiPhy::ChannelTuple{ap_ChannelNumber, 20, WIFI_PHY_BAND_2_4GHZ, 0});
                            int32_t sta_interfaceIndex = sta_ipv4->GetInterfaceForDevice(sta_dev);
                            sta_netdevice_ipAddr = sta_ipv4->GetAddress(sta_interfaceIndex, 0).GetLocal();
                        }else{
                            uint16_t ap_device_index;
                            if (apNode_staconneced->GetNDevices() == 3){
                                ap_device_index = 1;
                            }
                            else{
                                ap_device_index = 0;
                            }
                            
                            Ptr<NetDevice> ap_dev = apNode_staconneced->GetDevice(ap_device_index);
                            int32_t ap_interfaceIndex = ap_ipv4->GetInterfaceForDevice(ap_dev);
                            ap_netdevice_ipAddr = ap_ipv4->GetAddress(ap_interfaceIndex, 0).GetLocal();
                            Ptr<WifiNetDevice> ap_wifiDev = DynamicCast<WifiNetDevice>(ap_dev);
                            Ptr<WifiPhy> ap_wifiphy = ap_wifiDev->GetPhy();
                            uint8_t ap_ChannelNumber = ap_wifiphy->GetChannelNumber();
                            uint8_t ap_primary20Index = ap_wifiphy->GetPrimary20Index();
                            //uint8_t ap_ChannelNumber_E = ap_ChannelNumber - 6 + ap_primary20Index*4;
                            Ptr<NetDevice> sta_dev = staNode_P->GetDevice(2);
                            Ptr<WifiNetDevice> staDevice5 = DynamicCast<WifiNetDevice>(sta_dev);
                            Ptr<WifiPhy> sta_wifiphy5 = staDevice5->GetPhy();
                            sta_wifiphy5->SetOperatingChannel(WifiPhy::ChannelTuple{ap_ChannelNumber, 80, WIFI_PHY_BAND_5GHZ, ap_primary20Index});
                            int32_t sta_interfaceIndex = sta_ipv4->GetInterfaceForDevice(sta_dev);
                            sta_netdevice_ipAddr = sta_ipv4->GetAddress(sta_interfaceIndex, 0).GetLocal();
                        }
                        
                        if (sta["dataDirection"] == "UL") {
                            Address ul_sinkAddress(InetSocketAddress(ap_netdevice_ipAddr, port_ul));
                            BulkSendHelper ul_source("ns3::TcpSocketFactory", ul_sinkAddress);
                            ul_source.SetAttribute("MaxBytes", UintegerValue(sta["bytes"]));
                            ul_source.SetAttribute("SendSize", UintegerValue(payloadSize));
                            ApplicationContainer ul_sourceApps = ul_source.Install(staNode_P);
                            ul_sourceApps.Start(Seconds(0.1));
                            ul_sourceApps.Stop(Seconds(60));
                        } else if (sta["dataDirection"] == "DL") {
                            Address dl_sinkAddress(InetSocketAddress(sta_netdevice_ipAddr, port_dl));
                            BulkSendHelper dl_source("ns3::TcpSocketFactory", dl_sinkAddress);
                            dl_source.SetAttribute("MaxBytes", UintegerValue(sta["bytes"]));
                            dl_source.SetAttribute("SendSize", UintegerValue(payloadSize));
                            ApplicationContainer dl_sourceApps = dl_source.Install(apNode_staconneced);
                            dl_sourceApps.Start(Seconds(0.1));
                            dl_sourceApps.Stop(Seconds(60));
                        } else {
                            std::cerr << "Invalid data direction: " << sta["dataDirection"] << std::endl;
                            continue;
                        }
                    }
                }else
                {
                    std::cout << "No STA update in this Mins." << std::endl;
                }

                g_triggerSend = false;
                std::cout << "g_triggerSend: " << g_triggerSend << std::endl;
                std::cout << std::string(150, '=') << std::endl;
            }
            Simulator::Schedule(Seconds(60), &ns3sim::sta_config);
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