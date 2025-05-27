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

Time simulationTime {"2592000s"};
uint32_t payloadSize = 1472;

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
            std::cout << "============================================================" << std::endl;
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
            NodeContainer apNodes;
            NetDeviceContainer apDevices;
            
            const auto& ns3_ap_list = apMgr->GetList();
            for (const auto& ns3_ap : ns3_ap_list)
            {
                Ptr<Node> apNode = CreateObject<Node>();
                apNodes.Add(apNode);
                Names::Add (ns3_ap.ap_name, apNode);

                //Mobility model
                Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
                positionAlloc->Add(Vector(ns3_ap.position.x, ns3_ap.position.y, ns3_ap.position.z));

                MobilityHelper ap_mobility;
                ap_mobility.SetPositionAllocator(positionAlloc);
                ap_mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
                ap_mobility.Install(apNode);


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
                } else {
                    std::cerr << "Unsupported WiFi standard: " << ns3_ap.ap_standard << std::endl;
                    continue;
                }

                wifi.SetRemoteStationManager("ns3::MinstrelWifiManager");
                Ssid ssid = Ssid(ns3_ap.ssid);

                YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
                channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel");
                channel.AddPropagationLoss("ns3::NakagamiPropagationLossModel");
                YansWifiPhyHelper yansPhy24G;
                phy.SetChannel(channel.Create());

                WifiMacHelper mac;



                //std::cout << "AP ID: " << ns3_ap.ap_id << std::endl;
            }
            
            // Simulator::Schedule(Seconds(1), &ns3sim::count);
            // Simulator::Schedule(Seconds(1), &ns3sim::printAP, apMgr);
            // Simulator::Stop(Seconds(11));
            // Simulator::Run();
            // Simulator::Destroy();
            // std::cout << "Simulation finished." << std::endl;
        }

        static void count()
        {
            static uint32_t count = 0;
            std::cout << "count: " << count++ << std::endl;
            Simulator::Schedule(Seconds(1), &ns3sim::count);
        }

        static void printAP(const AccessPointManager* apMgr)
        {
            const auto& list = apMgr->GetList();
            for (const auto& ap : list)
            {
                std::cout << "AP ID: " << ap.ap_id << std::endl;
            }
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