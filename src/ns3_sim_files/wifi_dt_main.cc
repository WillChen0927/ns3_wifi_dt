// ==================== ns-3 and External Library Includes ====================
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

// VES event export libraries (HTTP + JSON)
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

// NS-3 Logging tag
NS_LOG_COMPONENT_DEFINE("WiFi-DT-TEST");


// ============================== Global Variables =============================
Time simTime {"2592000s"};  // 30 days in seconds
uint32_t payloadSize = 1472;

std::map<uint32_t, Ptr<PacketSink>> sinkAppMap;  
std::map<uint32_t, uint64_t> previousRxBytes;                                      // Map: sinkApp index -> Previous received bytes for throughput calculation
std::map<Ptr<WifiRadioEnergyModel>, double> previouspower;                         // Map: energy model pointer -> previous power for diff calculation
std::map<Ipv4Address, std::pair<Ptr<NetDevice>, Ptr<Node>>> ipToDeviceNodeMap;     // Map: IP -> (NetDevice, Node) for identifying AP devices
std::vector<Ipv4Address> ipList;                                                   // List of IPs of APs
std::vector<Ptr<WifiRadioEnergyModel>> deviceEnergyModels;                         // List of energy models for APs
std::map<Ptr<WifiRadioEnergyModel>, Ptr<NetDevice>> modelToDeviceMap;              // Map: energy model -> NetDevice (for finding node info)

//SMO/VES collector URL
std::string url = "http://192.168.8.121:30417/eventListener/v7";

WifiCoTraceHelper coTraceHelper;                                                   // CoTrace helper for collecting channel occupancy

// For clean termination via Ctrl+C
static volatile int exit_application = 0;
static void sigint_handler(int signum)
{

    exit_application = 1;
}

// ================================= Access Point Manager ===============================
// Manages list of APs and provides utility functions
class AccessPointManager 
{
    public:
        // Represents a 3D spatial position of an AP.
        struct Position {
            double x;       // longitude
            double y;       // latitude
            double z;       // altitude
        };
    
        // Represents a single Access Point's configuration.
        struct AccessPoint {
            int ap_id;                     // Unique identifier for the AP
            std::string ap_name;           // Name of the AP node
            std::string ap_standard;       // Wi-Fi standard (e.g., 802.11ax, 802.11ac)
            Position location;             // 3D coordinates of the AP
            std::string ssid;              // SSID of the AP network
            int band;                      // 1: 2.4GHz, 2: 5GHz, 3: Dual-band
            int channel_band24;            // e.g. channel 1 for 2.4GHz
            int channel_band5;             // e.g. channel 36 for 5GHz
        };
    
        // Adds an AccessPoint struct to the internal list
        void AddAccessPoint(const AccessPoint& ap) {
            ap_list.push_back(ap);
        }
    
        // Prints all AP configurations stored in the manager.
        // Useful for debugging and confirming Netconf data parsing.
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
    
        // Returns the list of all configured Access Points
        const std::vector<AccessPoint>& GetList() const {
            return ap_list;
        }
    
    private:
        // Internal storage for all Access Point configurations
        std::vector<AccessPoint> ap_list;
};

// =============================== VES Event Sender ======================================
// Handles creation and sending of VES events (PNF registration, measurements)
class VesEventSender 
{
    public:
        // Sends JSON string to specified URL via HTTP POST
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
    
        // Create VES PNF registration JSON event
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
    
        // Create VES Measurement JSON event (Will modify to file base PM, so this event will be change to notification SMO in the future)
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
        // Entry point to run simulation given an AP list from AccessPointManager
        static void StartSimulation(const AccessPointManager* apMgr)
        {
            NodeContainer apNodes;                  // Container for AP nodes
            NetDeviceContainer apDevices;           // Container for AP network devices
            
            const auto& ns3_ap_list = apMgr->GetList();
            for (const auto& ns3_ap : ns3_ap_list)
            {
                // Create AP node and register name
                Ptr<Node> apNode = CreateObject<Node>();
                apNodes.Add(apNode);
                Names::Add (ns3_ap.ap_name, apNode);

                //Install mobility model to set static position for AP
                Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
                positionAlloc->Add(Vector(ns3_ap.location.x, ns3_ap.location.y, ns3_ap.location.z));

                MobilityHelper ap_mobility;
                ap_mobility.SetPositionAllocator(positionAlloc);
                ap_mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                ap_mobility.Install(apNode);

                // Configure WiFi standard (802.11 variants)
                WifiHelper wifi;
                if (ns3_ap.ap_standard == "802.11ax") {
                    wifi.SetStandard(WIFI_STANDARD_80211ax);
                } else if (ns3_ap.ap_standard == "802.11n") {
                    wifi.SetStandard(WIFI_STANDARD_80211n);
                } else if (ns3_ap.ap_standard == "802.11ac") {
                    wifi.SetStandard(WIFI_STANDARD_80211ac);
                } else if (ns3_ap.ap_standard == "802.11g") {
                    wifi.SetStandard(WIFI_STANDARD_80211g);
                } else if (ns3_ap.ap_standard == "802.11b") {
                    wifi.SetStandard(WIFI_STANDARD_80211b);
                }

                // Create PHY and propagation channel
                wifi.SetRemoteStationManager("ns3::IdealWifiManager");
                Ssid ssid = Ssid(ns3_ap.ssid);

                // Create the channel and PHY
                YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
                channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
                channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
                channel.AddPropagationLoss("ns3::RangePropagationLossModel");
                Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(5));

                // Depending on band, configure PHY and MAC
                // 1 = 2.4GHz only
                // 2 = 5GHz only
                // 3 = Dual-band (both 2.4 and 5)
                if (ns3_ap.band == 1) {
                    YansWifiPhyHelper yansPhy24;
                    yansPhy24.SetChannel(channel.Create());

                    yansPhy24.Set("Antennas", UintegerValue(4));
                    yansPhy24.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
                    yansPhy24.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

                    if (ns3_ap.ap_standard == "802.11ax") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(24.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(24.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11n") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(18.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(18.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    } else if (ns3_ap.ap_standard == "802.11ac") {
                        yansPhy24.Set("TxPowerStart", DoubleValue(20.0));
                        yansPhy24.Set("TxPowerEnd", DoubleValue(20.0));
                        yansPhy24.Set("TxPowerLevels", UintegerValue(1));
                    }

                    std::ostringstream channelValue;
                    channelValue << "{" << ns3_ap.channel_band24 << ", 20, BAND_2_4GHZ, 0}";
                    yansPhy24.Set("ChannelSettings", StringValue(channelValue.str()));

                    WifiMacHelper mac;
                    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
                    NetDeviceContainer apDevice = wifi.Install(yansPhy24, mac, apNode);
                    apDevices.Add(apDevice);

                } else if (ns3_ap.band == 2) {
                    YansWifiPhyHelper yansPhy5;
                    yansPhy5.SetChannel(channel.Create());

                    yansPhy5.Set("Antennas", UintegerValue(4));
                    yansPhy5.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
                    yansPhy5.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

                    if (ns3_ap.ap_standard == "802.11ax") {
                        yansPhy5.Set("TxPowerStart", DoubleValue(24.0));
                        yansPhy5.Set("TxPowerEnd", DoubleValue(24.0));
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
                    apDevices.Add(apDevice);

                } else if (ns3_ap.band == 3) {
                    YansWifiPhyHelper yansPhy24;
                    yansPhy24.SetChannel(channel.Create());

                    yansPhy24.Set("Antennas", UintegerValue(4));
                    yansPhy24.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
                    yansPhy24.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

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
                    NetDeviceContainer apDevice_24G = wifi.Install(yansPhy24, mac, apNode);
                    apDevices.Add(apDevice_24G);

                    YansWifiPhyHelper yansPhy5;
                    yansPhy5.SetChannel(channel.Create());

                    yansPhy5.Set("Antennas", UintegerValue(4));
                    yansPhy5.Set("MaxSupportedTxSpatialStreams", UintegerValue(4));
                    yansPhy5.Set("MaxSupportedRxSpatialStreams", UintegerValue(4));

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

                    NetDeviceContainer apDevice_5G = wifi.Install(yansPhy5, mac, apNode);
                    apDevices.Add(apDevice_5G);
                }
            }

            // Install Internet protocol stack to all APs
            InternetStackHelper stack;
            stack.Install(apNodes);

            // Assign IP addresses to all AP devices
            Ipv4AddressHelper address;
            address.SetBase("10.1.0.0", "255.255.240.0");
            Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

            // Install Energy models
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
                           
            // For each AP node, bind energy model to each NetDevice
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

            // Create and start TCP packet sinks for each AP interface
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

                    ipToDeviceNodeMap[ipAddr] = std::make_pair(dev, node);
                    ipList.push_back(ipAddr);

                    PacketSinkHelper sink("ns3::TcpSocketFactory",InetSocketAddress(ipAddr, port));
                    ApplicationContainer app = sink.Install(node);
                    app.Start(Seconds(0.1));
                    app.Stop(simTime);

                    sinkApps.Add(app);
                }
            }


            // Store all sinks in global map for throughput monitoring
            for(uint32_t i = 0; i < sinkApps.GetN(); ++i)
            {
                sinkAppMap[i] = DynamicCast<PacketSink>(sinkApps.Get(i));
            }

            // Enable channel occupancy tracing
            coTraceHelper.Enable (apDevices);
            coTraceHelper.Start(Seconds(0.1));
            coTraceHelper.Stop(simTime);

            // Schedule periodic reports
            Simulator::Schedule(Seconds(1), &ns3sim::show_deployed_ap_info, apNodes);
            Simulator::Schedule(Seconds(60), &ns3sim::show_throughput, sinkAppMap, ipToDeviceNodeMap, ipList);
            Simulator::Schedule(Seconds(60), &ns3sim::show_CO);
            Simulator::Schedule(Seconds(60), &ns3sim::show_energy, deviceEnergyModels, modelToDeviceMap);

            // Run simulation
            Simulator::Stop(simTime);
            Simulator::Run();
            Simulator::Destroy();

            // Simulator::Schedule(Seconds(1), &ns3sim::count);
            // Simulator::Schedule(Seconds(1), &ns3sim::printAP, apMgr);
            // Simulator::Stop(Seconds(11));
            // Simulator::Run();
            // Simulator::Destroy();
            // std::cout << "Simulation finished." << std::endl;
        }

    private:
        // Utility to get channel config string for 5GHz
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

        // Prints deployed AP info after placement and setup
        static void show_deployed_ap_info(const NodeContainer &nodes)
        {
            std::string name;
            Vector position;
            uint8_t ChannelWidth;
            uint8_t ChannelNumber = 0;
            uint16_t Frequency;
            WifiStandard Standard = WIFI_STANDARD_80211a;
            Ssid dev_ssid;

            std::cout << "Deployed APs infomations:" << std::endl;
            for (uint32_t i = 0; i < nodes.GetN(); ++i)
            {
                Ptr<Node> node = nodes.Get(i);
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

        // Calculates and prints throughput of each AP
        static void show_throughput(const std::map<uint32_t, Ptr<PacketSink>>& sinkAppMap, 
                                    const std::map<Ipv4Address, std::pair<Ptr<NetDevice>, Ptr<Node>>>& ipToDeviceNodeMap,
                                    const std::vector<Ipv4Address>& ipList)
        {
            std::cout << std::string(150, '=') << std::endl;
            for (const auto &sinkApp : sinkAppMap)
            {
                uint64_t totalRx = sinkApp.second -> GetTotalRx();
                uint64_t currentRx = totalRx - previousRxBytes[sinkApp.first];
                previousRxBytes[sinkApp.first] = totalRx;
                double throughput = (currentRx * 8.0) / 60 * 1000.0; // Convert to Kbps
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
            Simulator::Schedule(Seconds(60), &ns3sim::show_throughput, sinkAppMap, ipToDeviceNodeMap, ipList);
        }

        // Prints channel occupancy statistics using CoTrace
        static void show_CO()
        {
            //call coTraceHelper api to print statistics
            coTraceHelper.PrintStatistics (std::cout);
            coTraceHelper.Reset();
            Simulator::Schedule(Seconds(60), &ns3sim::show_CO);
        }

        // Calculates and prints per-device energy usage
        static void show_energy(const std::vector<Ptr<WifiRadioEnergyModel>>& deviceEnergyModels, const std::map<Ptr<WifiRadioEnergyModel>, Ptr<NetDevice>>& modelToDeviceMap)
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
            Simulator::Schedule(Seconds(60), &ns3sim::show_energy, deviceEnergyModels, modelToDeviceMap);
        }
};

// =============================== Sysrepo Callback ========================================
// Callback handler for Netconf config change.
// This class implements a sysrepo callback to handle changes in NETCONF/YANG configurations
// related to Access Point (AP) deployment. When a configuration is applied (via SR_EV_CHANGE),
// the callback extracts relevant AP parameters from the change event and uses them to launch
// an ns-3 simulation via the ns3sim class.
#define XPATH_MAX_LEN 256
class APConfigCb : public sysrepo::Callback
{
    public:
        // Sysrepo callback triggered when config changes for target YANG module
        int module_change(sysrepo::S_Session session, const char *module_name, const char *xpath, sr_event_t event, uint32_t request_id, void *private_data)
        {
            char change_path[XPATH_MAX_LEN];

            // Only handle config-change events
            if (event == SR_EV_CHANGE)
            {
                try
                {
                    std::string ssid;
                    int apId, band = 0;
                    std::string band_G;
                    std::string ap_standard;

                    // Create a change iterator for all nodes under the target module
                    snprintf(change_path, XPATH_MAX_LEN, "/%s:*//.", module_name);
                    auto it = session->get_changes_iter(change_path);

                    // Temporary container to group parsed AP info by apId
                    std::map<int, AccessPointManager::AccessPoint> tempAPMap;

                    // Iterate through all config changes
                    while (auto change = session->get_change_next(it)) {
                        if(nullptr != change->new_val())
                        {
                            // Get new value and corresponding xpath leaf
                            std::string val = change->new_val()->val_to_string();
                            std::string parent, leaf;

                            getLeafInfo(change->new_val()->to_string(), parent, leaf);
                            //std::cout << "parent: " << parent<< std::endl;
                            //std::cout << "leaf: " << leaf<< std::endl;
                            //std::cout << "val: " << val<< std::endl;
                            //std::cout << "-------------------------------------------------------------------------"<< std::endl;

                            // ========== Parse AP-level info from GNBDUFunction ========== //
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
                            // ========== Parse band name info from NRCellDU id ========== //
                            else if (parent.find("NRCellDU") && leaf == "id"){
                                std::regex regex_pattern(R"(.*,([^,]+))");
                                std::smatch match;

                                if (std::regex_match(val, match, regex_pattern)) {
                                    band_G = match[1];
                                }
                            }
                            // ========== Parse general attributes like name, arfcnDL ========== //
                            else if (parent == "attributes"){
                                if (leaf == "gNBDUName"){
                                    tempAPMap[apId].ap_name = val;
                                }
                                // Assign channel depending on band type
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
                            // ========== Parse physical location info ========== //
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

                    // Add each parsed AP config to the AccessPointManager
                    for (const auto& [key, ap] : tempAPMap) 
                    {
                        accessPointManager.AddAccessPoint(ap);
                    }

                    // Print all parsed APs for confirmation
                    accessPointManager.PrintAll();

                    // Launch the simulation in a separate thread (non-blocking)
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
        // Parses the full leaf path into parent + leaf name
        void getLeafInfo(std::string xpath,std::string &parent, std::string &leaf )
        {
            std::stringstream ssLeaf(xpath);
            std::stringstream ssParent(xpath);
            char ch='/';

            // Advance leaf stream to the last component
            getline(ssLeaf, leaf, ch);
            while(getline(ssLeaf, leaf, ch))
            {
                getline(ssParent, parent, ch);
            }

            // Strip possible trailing condition or space
            std::stringstream ss(leaf);
            ch=' ';
            getline(ss, leaf, ch);
        }

        // AP configuration holder that will be passed to the simulator
        AccessPointManager accessPointManager;
};

// =============================== Main Function ============================================
int main(int argc, char *argv[]) 
{
    // Set simulator to real-time
    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    // Graceful termination on SIGINT/SIGTERM
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    // Connect to sysrepo and open a session
    sysrepo::S_Connection conn(new sysrepo::Connection());
    sysrepo::S_Session session(new sysrepo::Session(conn));

    // Subscribe to changes in the YANG module for AP config
    sysrepo::S_Subscribe subscribe(new sysrepo::Subscribe(session));
    sysrepo::S_Callback wifi_ap_config_cb(new APConfigCb());
    subscribe->module_change_subscribe("_3gpp-common-managed-element", wifi_ap_config_cb);

    
    // std::string pnfEvent = VesEventSender::CreatePnfEvent();
    // if (VesEventSender::Send(pnfEvent, url)) {
    //     std::cout << "[pnfRegistration]" << std::endl;
    // } else {
    //     std::cerr << "Failed to send VES event." << std::endl;
    // }

    // Loop until signal received
    while (!exit_application)
        {
            sleep(1);
        }

    return 0;
}