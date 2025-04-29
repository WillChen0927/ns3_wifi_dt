/*
This version supports O1, including Netconf and VES, but the yang model used is a custom wifi yang.
*/

// ns-3 includes
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/on-off-helper.h"

// netconf includes
#include <iostream>
#include <signal.h>
#include <unistd.h>
#include <sysrepo-cpp/Session.hpp>
#include <thread>

//VES
#include <curl/curl.h>
#include <cjson/cJSON.h>

#include <ctime>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFi-DT-O1-enable");

Time simulationTime {"2505600s"};

NodeContainer wifiStaNode;
NetDeviceContainer staDevices;
YansWifiPhyHelper phy;
WifiMacHelper mac;
WifiHelper wifi;
InternetStackHelper stack;
Ipv4AddressHelper address;
Ipv4InterfaceContainer apInterface;

//AP Maps
std::map<uint32_t, Ptr<Node>> apNodesMap;                    //ap id -> ap node
std::map<uint32_t, std::string> apNameMap;                   //ap id -> ap name
std::map<std::string, uint64_t> apChennalMap;                //ap name -> ap channel
std::map<std::string, Vector> apPositionMap;                 //ap name -> ap position

//STA Map
std::map<std::string, Ptr<Node>> staNodesMap;                //sta name -> sta node
std::map<std::string, std::string> sta_connected_apNameMap;  //sta name -> connected ap name  


std::map<uint32_t, uint64_t> previousRxBytes;
std::map<uint32_t, Ptr<UdpServer>> serverMap;                //ap id -> ap udp server
std::map<std::string, Ipv4Address> ap_interfaceMap; 
std::map<std::string, std::string> throughputMap;            //ap name -> throughput

static volatile int exit_application = 0;
uint16_t port = 9;
std::string wifi_ssid;
std::string wifi_standard;
std::vector<std::tuple<uint32_t, std::string, uint32_t, ns3::Vector>> aps;
std::vector<std::tuple<std::string, std::string, ns3::Vector>> stas;
std::vector<std::tuple<std::string, std::string, uint32_t, uint32_t, Vector>> staApps;

std::string url = "http://192.168.8.6:30417/eventListener/v7";

// Function to send the VES event to the VES collector
bool sendVesEvent(const std::string& eventJson, const std::string& url) {
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

std::string createStateChangeVesEvent(bool start) {
    cJSON *root = cJSON_CreateObject();
    cJSON *event = cJSON_CreateObject();
    cJSON *commonHeader = cJSON_CreateObject();

    // Create commonEventHeader
    cJSON_AddStringToObject(commonHeader, "version", "4.1");
    cJSON_AddStringToObject(commonHeader, "vesEventListenerVersion", "7.2.1");
    cJSON_AddStringToObject(commonHeader, "domain", "notification");
    cJSON_AddStringToObject(commonHeader, "eventName", "notification_wifi-dt");
    cJSON_AddStringToObject(commonHeader, "eventId", "notification_event_001");
    cJSON_AddNumberToObject(commonHeader, "sequence", 2);
    cJSON_AddStringToObject(commonHeader, "priority", "High");
    cJSON_AddStringToObject(commonHeader, "reportingEntityName", "ns-3 wifi-dt");
    cJSON_AddStringToObject(commonHeader, "sourceName", "ns-3 wifi-dt");
    cJSON_AddNumberToObject(commonHeader, "startEpochMicrosec", 1737314968575);
    cJSON_AddNumberToObject(commonHeader, "lastEpochMicrosec", 1737314968588);
    cJSON_AddStringToObject(commonHeader, "timeZoneOffset", "-05:30");

    // Add commonEventHeader to the event
    cJSON_AddItemToObject(event, "commonEventHeader", commonHeader);

    // Create stateChangeFields
    cJSON *notificationFields = cJSON_CreateObject();
    cJSON_AddStringToObject(notificationFields, "notificationFieldsVersion", "2.0");
    cJSON_AddStringToObject(notificationFields, "changeType", "ns3");
    cJSON_AddStringToObject(notificationFields, "changeIdentifier", "ns3");

    if (start) {
        cJSON_AddStringToObject(notificationFields, "newState", "inService");
    } else {
        cJSON_AddStringToObject(notificationFields, "newState", "outOfService");
    }
    

    // Add stateChangeFields to the event
    cJSON_AddItemToObject(event, "notificationFields", notificationFields);

    // Add event to the root
    cJSON_AddItemToObject(root, "event", event);

    // Convert to string
    char *jsonStr = cJSON_PrintUnformatted(root);
    std::string eventJson(jsonStr);

    // Clean up
    cJSON_Delete(root);
    free(jsonStr);

    return eventJson;
}

std::string createMeasurementVesEvent(const std::map<std::string, std::string>& throughputMap) {
    cJSON *root = cJSON_CreateObject();
    cJSON *event = cJSON_CreateObject();
    cJSON *commonHeader = cJSON_CreateObject();

    // Create commonEventHeader
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

    // Add commonEventHeader to the event
    cJSON_AddItemToObject(event, "commonEventHeader", commonHeader);

    // Create measurementFields
    cJSON *measurementFields = cJSON_CreateObject();
    cJSON_AddStringToObject(measurementFields, "measurementFieldsVersion", "4.0");
    cJSON_AddNumberToObject(measurementFields, "measurementInterval", 1000);

    // Create additionalFields and add to measurementFields
    cJSON *additionalFields = cJSON_CreateObject();
    cJSON_AddStringToObject(additionalFields, "simulation_time", std::to_string(Simulator::Now().GetSeconds()).c_str());
    for (const auto &throughput_each : throughputMap){
        cJSON_AddStringToObject(additionalFields, throughput_each.first.c_str(), throughput_each.second.c_str());
    }

    cJSON_AddItemToObject(measurementFields, "additionalFields", additionalFields);

    // Add measurementFields to the event
    cJSON_AddItemToObject(event, "measurementFields", measurementFields);

    // Add event to the root
    cJSON_AddItemToObject(root, "event", event);

    // Convert to string
    char *jsonStr = cJSON_PrintUnformatted(root);
    std::string eventJson(jsonStr);

    // Clean up
    cJSON_Delete(root);
    free(jsonStr);

    return eventJson;
}

void CalculateThroughput(const std::map<uint32_t, Ptr<UdpServer>>& serverMap){
    for (const auto &server : serverMap){
        uint32_t apId = server.first;
        Ptr<UdpServer> udpserver = server.second;
        uint64_t totalRx = udpserver -> GetReceived();
        uint64_t prevRx = previousRxBytes[apId];
        double throughput = (totalRx - prevRx) * 1448 * 8.0 / (1.0 * 1000);
        std::string apName = apNameMap[apId];

        std::time_t now = std::time(nullptr); // get current time
        std::tm* localTime = std::localtime(&now); // change to local time
        char buffer[100];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", localTime);

        std::cout << "Time: " << buffer << ", sim time: "<< Simulator::Now().GetSeconds() 
                    << ", AP name: " << apName << ", throughput: " << throughput << " kbps" << std::endl;
    
        previousRxBytes[apId] = totalRx;
        throughputMap[apName] = std::to_string(throughput);
    }

    std::string eventJson_Measurement = createMeasurementVesEvent(throughputMap);
    if (sendVesEvent(eventJson_Measurement, url)) {
        std::cout << "." << std::endl;
    } else {
        std::cerr << "Failed to send VES event." << std::endl;
    }

    std::cout << "----------------------------------------------------------------------------" << std::endl;
    // Call the function again every 1 sec
    Simulator::Schedule(Seconds(1.0), &CalculateThroughput, serverMap);
    throughputMap.clear();
}

void sta_app(){

    for (auto &staApp : staApps)
    {
        std::string sta_do;
        std::string staname;
        uint64_t sendTime;
        uint64_t tp;
        Vector new_position;
        std::tie(sta_do, staname, tp, sendTime, new_position) = staApp;

        if (staNodesMap.find(staname) == staNodesMap.end())
        {
            std::cout << "Time: " << Simulator::Now().GetSeconds() << staname << " not found." << std::endl;
            continue;
            //maybe call add_sta function in future
        }

        if(sta_do == "traffic")
        {
            std::string sta_dataRate = std::to_string(tp) + "kbps";

            InetSocketAddress dest(ap_interfaceMap[sta_connected_apNameMap[staname]], port);
            OnOffHelper clientA("ns3::UdpSocketFactory", dest);
            clientA.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
            clientA.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
            clientA.SetAttribute("DataRate", StringValue(sta_dataRate));
            clientA.SetAttribute("PacketSize", UintegerValue(1448));

            ApplicationContainer clientAppA = clientA.Install(staNodesMap[staname]);
            clientAppA.Start(Seconds(0.1));
            clientAppA.Stop(Seconds(sendTime));

            std::cout << "Time: " << Simulator::Now().GetSeconds() << ", " 
                        << staname << "send straffic " << sendTime << "sec" << std::endl;
        }
        else if(sta_do == "mobility")
        {
            Ptr<MobilityModel> mobilityModel = staNodesMap[staname]->GetObject<MobilityModel>();
            Vector position_check = mobilityModel->GetPosition();

            if(position_check.x == new_position.x && position_check.y == new_position.y && position_check.z == new_position.z)
            {
                //std::cout << "Time: " << Simulator::Now().GetSeconds() << ", " << staname << " not move" << std::endl;
            }
            else
            {
                mobilityModel->SetPosition(new_position);
                position_check = mobilityModel->GetPosition();

                 std::cout << "Time: " << Simulator::Now().GetSeconds() << ", " 
                        << staname << " moved to (" 
                        << position_check.x << ", " 
                        << position_check.y << ", " 
                        << position_check.z << ")" << std::endl; 
            }
        }
    }
    staApps.clear();
    Simulator::Schedule(Seconds(1.0), &sta_app);
}

void add_sta(){
    NodeContainer staNodes;

    for (auto &sta : stas)
    {
        std::string staname;
        std::string connected_apname;
        Vector sta_position;
        std::tie(staname, connected_apname, sta_position) = sta;

        if (staNodesMap.find(staname) != staNodesMap.end())
        {
            //std::cout << "Time: " << Simulator::Now().GetSeconds() << ", " 
                    //<< staname << " already exists." << std::endl;
            
            continue;
        }

        Ptr<Node> staNode = CreateObject<Node>();
        staNodes.Add(staNode);
        staNodesMap[staname] = staNode;
        sta_connected_apNameMap[staname] = connected_apname;

        wifi.SetStandard(WIFI_STANDARD_80211n);
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HtMcs7"),
                                 "ControlMode",
                                 StringValue("HtMcs0"));

        NetDeviceContainer staDevices;
        mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(wifi_ssid));


        std::ostringstream sta_channelValue;
        sta_channelValue << "{" << apChennalMap[sta_connected_apNameMap[staname]] << ", 0, BAND_5GHZ, 0}";
        phy.Set("ChannelSettings", StringValue(sta_channelValue.str()));

        NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

        staDevices.Add(staDevice);

        InternetStackHelper stack;
        stack.Install(staNode);
        Ipv4InterfaceContainer newstaInterface = address.Assign(staDevice);
        
        MobilityHelper sta_mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(sta_position);
        sta_mobility.SetPositionAllocator(positionAlloc);
        sta_mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        sta_mobility.Install(staNode);

        //InetSocketAddress dest(ap_interfaceMap[sta_connected_apNameMap[staname]], port);
        //OnOffHelper clientA("ns3::UdpSocketFactory", dest);
        //clientA.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        //clientA.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        //clientA.SetAttribute("DataRate", StringValue("5Mbps"));
        //clientA.SetAttribute("PacketSize", UintegerValue(1448));

        //ApplicationContainer clientAppA = clientA.Install(staNodesMap[staname]);
        //clientAppA.Start(Seconds(0.1));
        //clientAppA.Stop(Seconds(10.0));

        
        std::cout << "Time: " << Simulator::Now().GetSeconds() << ", " 
                    << staname << " added, " 
                    << "Connected AP: "<< connected_apname 
                    << ", Position: (" << sta_position.x << ", "
                    << sta_position.y << ", " << sta_position.z << ")" << std::endl;
    }
    
    stas.clear();
    Simulator::Schedule(Seconds(1.0), &add_sta);
}

void sim_start(){

    // Create AP nodes
    NodeContainer apNodes;
    NetDeviceContainer apDevices;

    Ssid ssid = Ssid(wifi_ssid);

    if (wifi_standard == "80211n")
    {
        wifi.SetStandard(WIFI_STANDARD_80211n);
    }
    else
    {
        wifi.SetStandard(WIFI_STANDARD_80211n);
    }

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("999999"));
    Config::SetDefault("ns3::RangePropagationLossModel::MaxRange", DoubleValue(5));
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.AddPropagationLoss("ns3::RangePropagationLossModel");
    phy.SetChannel(channel.Create());

    for (auto &ap : aps) 
    {
        uint32_t id;
        std::string apname;
        uint32_t apchannel;
        Vector position;
        std::tie(id, apname, apchannel, position) = ap;

        Ptr<Node> apNode = CreateObject<Node>();
        apNodes.Add(apNode);
        apNodesMap[id] = apNode;
        apNameMap[id] = apname;
        apChennalMap[apname] = apchannel;
        apPositionMap[apname] = position;

        InternetStackHelper stack;
        stack.Install(apNodes);

        // Set up mobility
        MobilityHelper ap_mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
        positionAlloc->Add(position);
        ap_mobility.SetPositionAllocator(positionAlloc);
        ap_mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        ap_mobility.Install(apNode);
    }

    for (uint32_t i = 0; i < apNodes.GetN(); ++i)
    {
        mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

        std::ostringstream channelValue;
        channelValue << "{" << apChennalMap[apNameMap[i+1]] << ", 0, BAND_5GHZ, 0}";
        phy.Set("ChannelSettings", StringValue(channelValue.str()));

        NetDeviceContainer apDevice = wifi.Install(phy, mac, apNodes.Get(i));

        apDevices.Add(apDevice);
    }

    address.SetBase("10.2.1.0", "255.255.255.0");
    apInterface = address.Assign(apDevices);

    
    for(uint32_t i = 1; i <= apInterface.GetN(); ++i)
    {
        ap_interfaceMap[apNameMap[i]] = apInterface.GetAddress(i-1);
    }

    // UDP flow
    UdpServerHelper server(9);
    ApplicationContainer serverApps = server.Install(apNodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simulationTime + Seconds(1.0));

    for(uint32_t i = 0; i < serverApps.GetN(); ++i){
        serverMap[i+1] = DynamicCast<UdpServer>(serverApps.Get(i));
    }

    phy.EnablePcap("SimpleHtHiddenStations_Ap", apDevices.Get(0));
    Simulator::Schedule(Seconds(1.0), &CalculateThroughput, serverMap);
    Simulator::Schedule(Seconds(1.0), &add_sta);
    Simulator::Schedule(Seconds(0.5), &sta_app);


    //Simulator::Stop(Seconds(2505600.0));
    Simulator::Stop(simulationTime + Seconds(1.0));

    //Time::SetResolution(Time::MS);

    NS_LOG_INFO("Run Simulation.");

    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");
}

static void sigint_handler(int signum)
{
    std::string eventJson_StateChange = createStateChangeVesEvent(false);
    if (sendVesEvent(eventJson_StateChange, url)) {
        std::cout << "send StateChange VES event." << std::endl;
    } else {
        std::cerr << "Failed to send VES event." << std::endl;
    }

    exit_application = 1;
}

void printApps()
{
    size_t num_items = staApps.size();

    std::cout << "There are " << num_items << " Apps in scenario: " << std::endl;
    for (const auto& staApp : staApps)
    {
        if (std::get<0>(staApp) == "mobility")
        {
            std::cout << "STA Name: " << std::get<1>(staApp)
                  << ", new Position: (" << std::get<4>(staApp).x
                  << ", " << std::get<4>(staApp).y
                  << ", " << std::get<4>(staApp).z << ")"
                  << std::endl;
        }
        else if (std::get<0>(staApp) == "traffic")
        {
            std::cout << "STA Name: " << std::get<1>(staApp)
                  << ", TP: " << std::get<2>(staApp)
                  << ", Send time: " << std::get<3>(staApp)
                  << std::endl;
        }
    }
}

void printAllAps()
{
    size_t num_items = aps.size();
    std::cout << "SSID: " << wifi_ssid << std::endl;
    std::cout << "wifi standard: " << wifi_standard << std::endl;

    std::cout << "There are " << num_items << " ap in scenario: " << std::endl;
    for (const auto& ap : aps)
    {
        std::cout << "AP ID: " << std::get<0>(ap)
                  << ", Name: " << std::get<1>(ap)
                  << ", Channel: " << std::get<2>(ap)
                  << ", Position: (" << std::get<3>(ap).x
                  << ", " << std::get<3>(ap).y
                  << ", " << std::get<3>(ap).z << ")"
                  << std::endl;
    }
}

void printAllStas()
{
    size_t num_items = stas.size();

    std::cout << "There are " << num_items << " sta in scenario: " << std::endl;
    for (const auto& sta : stas)
    {
        std::cout << "STA Name: " << std::get<0>(sta)
                  << ", Conected AP name: " << std::get<1>(sta)
                  << ", Position: (" << std::get<2>(sta).x
                  << ", " << std::get<2>(sta).y
                  << ", " << std::get<2>(sta).z << ")"
                  << std::endl;
    }
}

class APConfigCb : public sysrepo::Callback
{
public:
    int module_change(sysrepo::S_Session session, const char *module_name, const char *xpath, sr_event_t event, uint32_t request_id, void *private_data)
    {
        if (event == SR_EV_DONE)
        {
            try
            {
                aps.clear();

                std::cout << "WiFi AP deploying" << std::endl;

                std::string date;
                uint32_t ap_id;
                std::string ap_name;
                uint32_t ap_channel;
                ns3::Vector ap_position;

                auto wifi_date_item = session->get_item("/wifi-dt-ap:ap-config/date");
                date = wifi_date_item->data()->get_string();
                std::cout << "Date: " << date << std::endl;

                auto wifi_ssid_item = session->get_item("/wifi-dt-ap:ap-config/ssid");
                wifi_ssid = wifi_ssid_item->data()->get_string();

                auto wifi_standard_item = session->get_item("/wifi-dt-ap:ap-config/wifi-standard");
                wifi_standard = wifi_standard_item->data()->get_string();

                auto aps_items = session->get_items("/wifi-dt-ap:ap-config/aps");
                size_t num_of_aps = aps_items->val_cnt();
                //std::cout << "num_of_aps: " << num_of_aps << std::endl;

                for (uint32_t target_id = 1; target_id <= num_of_aps; target_id++)
                {
                    std::string base_xpath = "/wifi-dt-ap:ap-config/aps[id='" + std::to_string(target_id) + "']";

                    ap_id = target_id;

                    auto ap_name_item = session->get_item((base_xpath + "/ap-name").c_str());
                    ap_name = ap_name_item->data()->get_string();
                    //std::string node_xpath = ap_name_item->xpath();
                    //std::cout << "node_xpath: " << node_xpath << std::endl;

                    auto ap_channel_item = session->get_item((base_xpath + "/ap-channel").c_str());
                    ap_channel = ap_channel_item->data()->get_uint32();
                    
                    auto x_item = session->get_item((base_xpath + "/position/x").c_str());
                    auto y_item = session->get_item((base_xpath + "/position/y").c_str());
                    auto z_item = session->get_item((base_xpath + "/position/z").c_str());
                    ap_position.x = x_item->data()->get_decimal64();
                    ap_position.y = y_item->data()->get_decimal64();
                    ap_position.z = z_item->data()->get_decimal64();
                    aps.push_back(std::make_tuple(ap_id, ap_name, ap_channel, ap_position));
                }
                printAllAps();
                //sim_start();
                std::thread sim_thread([]() {sim_start();});
                sim_thread.detach();
            }
            catch (const std::exception &e)
            {
                std::cerr << "ap module change callback error: " << e.what() << std::endl;
                return SR_ERR_CALLBACK_FAILED;
            }
        }
        return SR_ERR_OK;
    }
};

class STAConfigCb : public sysrepo::Callback
{
public:
    int module_change(sysrepo::S_Session session, const char *module_name, const char *xpath, sr_event_t event, uint32_t request_id, void *private_data)
    {
        if (event == SR_EV_DONE)
        {
            try
            {
                //std::cout << "add-sta-list:" << std::endl;
                auto add_sta_items = session->get_items("/wifi-dt-sta:sta-config/add-sta-list//*");

                std::string add_staname;
                std::string add_apname;
                ns3::Vector add_position;

                if (add_sta_items)
                {
                    for (size_t i = 0; i < add_sta_items->val_cnt(); ++i)
                    {
                        auto val = add_sta_items->val(i);
                        switch (i%6)
                        {
                        case 0:
                            add_staname = val->data()->get_string();
                            break;
                        case 1:
                            add_apname = val->data()->get_string();
                            break;
                        case 2:
                            continue;;
                        case 3:
                            add_position.x = val->data()->get_decimal64();
                            break;
                        case 4:
                            add_position.y = val->data()->get_decimal64();
                            break;
                        case 5:
                            add_position.z = val->data()->get_decimal64();
                            stas.push_back(std::make_tuple(add_staname, add_apname, add_position));
                            break;
                        default:
                            std::cout << "Unsupported data type." << std::endl;
                            break;
                        }
                    }
                    //printAllStas();
                }

                //std::cout << "mobility-sta-list:" << std::endl;
                auto mobility_items = session->get_items("/wifi-dt-sta:sta-config/mobility-sta-list//*");

                std::string mobility_staname;
                ns3::Vector mobility_position;

                if (mobility_items)
                {
                    for (size_t i = 0; i < mobility_items->val_cnt(); ++i)
                    {
                        auto val = mobility_items->val(i);
                        switch (i%5)
                        {
                        case 0:
                            mobility_staname = val->data()->get_string();
                            break;
                        case 1:
                            continue;;
                        case 2:
                            mobility_position.x = val->data()->get_decimal64();
                            break;
                        case 3:
                            mobility_position.y = val->data()->get_decimal64();
                            break;
                        case 4:
                            mobility_position.z = val->data()->get_decimal64();
                            staApps.push_back(std::make_tuple("mobility", mobility_staname, 0, 0, mobility_position));
                            break;
                        default:
                            std::cout << "Unsupported data type." << std::endl;
                            break;
                        }
                    }
                }

                //std::cout << "traffic-sta-list:" << std::endl;
                auto traffic_items = session->get_items("/wifi-dt-sta:sta-config/traffic-sta-list//*");

                std::string traffic_staname;
                uint32_t traffic_sendTime;
                uint32_t traffic_tp;

                if (traffic_items)
                {
                    for (size_t i = 0; i < traffic_items->val_cnt(); ++i)
                    {
                        auto val = traffic_items->val(i);
                        switch (i%3)
                        {
                        case 0:
                            traffic_staname = val->data()->get_string();
                            break;
                        case 1:
                            traffic_tp = val->data()->get_uint32();
                            break;
                        case 2:
                            traffic_sendTime = val->data()->get_uint32();
                            staApps.push_back(std::make_tuple("traffic", traffic_staname, traffic_tp, traffic_sendTime, ns3::Vector(0, 0, 0)));
                            break;
                        default:
                            std::cout << "Unsupported data type." << std::endl;
                            break;
                        }
                    }
                }
                //printApps();
            }
            catch (const std::exception &e)
            {
                std::cerr << "sta module change callback error: " << e.what() << std::endl;
                return SR_ERR_CALLBACK_FAILED;
            }
        }
        return SR_ERR_OK;
    }
};

int main(int argc, char *argv[]) 
{
    LogComponentEnable("WiFi-DT-O1-enable", LOG_LEVEL_INFO);
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
    subscribe->module_change_subscribe("wifi-dt-ap", wifi_ap_config_cb);
    

    sysrepo::S_Callback wifi_sta_config_cb(new STAConfigCb());
    subscribe->module_change_subscribe("wifi-dt-sta", wifi_sta_config_cb);

    std::string eventJson_StateChange = createStateChangeVesEvent(true);
    if (sendVesEvent(eventJson_StateChange, url)) {
        std::cout << "send StateChange VES event." << std::endl;
    } else {
        std::cerr << "Failed to send VES event." << std::endl;
    }
    
    std::cout << "Waiting netconf client send configuration file...." << std::endl;


    while (!exit_application)
        {
            sleep(1);
        }

    return 0;
}
