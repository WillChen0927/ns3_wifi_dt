#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"

#include "httplib.h"  // header-only REST Server
#include <thread>
#include <atomic>
#include "json.hpp"

using namespace ns3;
using namespace httplib;
using json = nlohmann::json;

NS_LOG_COMPONENT_DEFINE("RestServerExample");

std::vector<json> g_staConfigs;
std::mutex g_mutex;
std::atomic<bool> g_triggerSend(false);

// RESTful Server
void StartRestServer() {
    Server svr;

    svr.Post("/upload", [](const Request& req, Response& res) {
        try {
            auto j = json::parse(req.body);

            if (j.contains("numberOfSTA") && j.contains("STAConfig") && j["STAConfig"].is_array()) {
                size_t numSta = j["numberOfSTA"];
                auto staConfigs = j["STAConfig"];

                if (staConfigs.size() == numSta) {
                    for (const auto& sta : staConfigs) {
                        std::string name = sta["STAName"];
                        std::string ap = sta["connectedofAP"];
                        int throughput = sta["throughput"];
                        int duration = sta["Duration"];
                        std::lock_guard<std::mutex> lock(g_mutex);
                        g_staConfigs.clear();
                        for (const auto& sta : staConfigs) {
                            g_staConfigs.push_back(sta);
                        }
                        g_triggerSend = true;

                        std::cout << "[REST] STA " << name << " connect to " << ap
                                  << ", throughput=" << throughput
                                  << " kbps, duration=" << duration << "s\n";
                    }
                    res.set_content("{\"status\": \"multi-sta ok\"}", "application/json");
                } else {
                    res.set_content("{\"status\": \"numberOfSTA mismatch STAConfig length\"}", "application/json");
                }
            } else {
                res.set_content("{\"status\": \"invalid json format\"}", "application/json");
            }

        } catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            res.set_content("{\"status\": \"invalid json\"}", "application/json");
        }
    });

    std::cout << "RESTful server listening on port 5000" << std::endl;
    svr.listen("0.0.0.0", 5000);
}


void PollApiStatus() {
    if (g_triggerSend) {
        std::lock_guard<std::mutex> lock(g_mutex);
        std::cout << "sim time: " << Simulator::Now().GetSeconds()
                  << " >>> Received " << g_staConfigs.size()
                  << " STA(s) configuration. Already add to simulation." << std::endl;

        for (size_t i = 0; i < g_staConfigs.size(); ++i) {
            const auto& sta = g_staConfigs[i];
            std::cout << "  [" << i << "] STAName: " << sta["STAName"]
                      << ", connectedofAP: " << sta["connectedofAP"]
                      << ", throughput: " << sta["throughput"]
                      << ", Duration: " << sta["Duration"] << std::endl;
        }

        g_triggerSend = false;
    } else {
        std::cout << "sim time: " << Simulator::Now().GetSeconds()
                  << " >>> " << std::endl;
    }

    Simulator::Schedule(Seconds(1.0), &PollApiStatus);
}

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    GlobalValue::Bind("SimulatorImplementationType", StringValue("ns3::RealtimeSimulatorImpl"));

    // Start REST server thread
    std::thread serverThread(StartRestServer);

    Simulator::Schedule(Seconds(1.0), &PollApiStatus);
    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    Simulator::Destroy();

    serverThread.detach();
    return 0;
}
