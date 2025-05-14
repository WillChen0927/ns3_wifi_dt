# Performance Evaluation in ns-3 using Wifi and Flow Monitoring Helpers

- [Performance Evaluation in ns-3 using Wifi and Flow Monitoring Helpers](#performance-evaluation-in-ns-3-using-wifi-and-flow-monitoring-helpers)
  - [Introduction](#introduction)
  - [1. WifiPhyRxTraceHelper](#1-wifiphyrxtracehelper)
    - [Purpose](#purpose)
    - [Output Metrics](#output-metrics)
    - [Example Usage](#example-usage)
    - [Sample Output](#sample-output)
  - [2. WifiTxStatsHelper](#2-wifitxstatshelper)
    - [Purpose](#purpose-1)
    - [Output Metrics](#output-metrics-1)
    - [Example Usage](#example-usage-1)
    - [Sample Output (pseudo-format)](#sample-output-pseudo-format)
  - [3. WifiCoTraceHelper](#3-wificotracehelper)
    - [Purpose](#purpose-2)
    - [Output Metrics](#output-metrics-2)
    - [Example Usage](#example-usage-2)
    - [Sample Output](#sample-output-1)
  - [4. FlowMonitor](#4-flowmonitor)
    - [Purpose](#purpose-3)
    - [Output Metrics](#output-metrics-3)
    - [Example Usage](#example-usage-3)
    - [Sample Output (in XML)](#sample-output-in-xml)
  - [Example](#example)
    - [Simulation Environment Description](#simulation-environment-description)
    - [Source Code](#source-code)
    - [Output](#output)
  - [Simulation Output Analysis](#simulation-output-analysis)
    - [WifiPhyRxTraceHelper Analysis (PHY Receive Layer)](#wifiphyrxtracehelper-analysis-phy-receive-layer)
    - [WifiCoTraceHelper Analysis (Channel Occupancy)](#wificotracehelper-analysis-channel-occupancy)
    - [WifiTxStatsHelper Analysis (MAC Transmission Layer)](#wifitxstatshelper-analysis-mac-transmission-layer)
    - [FlowMonitor Analysis (IP Flow Statistics)](#flowmonitor-analysis-ip-flow-statistics)
    - [Conclusion](#conclusion)


## Introduction

In ns-3, a suite of trace and monitoring tools is available to facilitate in-depth analysis of wireless network behavior. Among them, `WifiPhyRxTraceHelper`, `WifiTxStatsHelper`, `WifiCoTraceHelper`, and `FlowMonitor` serve as powerful statistical collection and analysis tools.

These modules are designed to **passively monitor simulation events** across different network layers, enabling users to **gather key performance metrics** such as packet success/failure rates, transmission statistics, channel occupancy, latency, and jitter. Because they are non-intrusive and modular, they are ideal for **evaluating network performance**, validating simulation logic, and supporting quantitative comparisons in research and system design.


---

## 1. WifiPhyRxTraceHelper

### Purpose

Collects detailed statistics and records of all received Physical Protocol Data Units (PPDUs) at the PHY layer. Tracks overlap (collisions), success/failure of receptions, and MPDU details.

### Output Metrics

* Total PPDUs received, successful, failed
* Overlapping PPDUs
* MPDU-level outcomes
* Per-device and per-link trace data

### Example Usage

```cpp
WifiPhyRxTraceHelper rxTraceHelper;
rxTraceHelper.Enable(nodeContainer);
rxTraceHelper.Start(Seconds(1));
rxTraceHelper.Stop(Seconds(10));
...
rxTraceHelper.PrintStatistics();
```

### Sample Output

```
Total PPDUs Received: 3
Total Non-Overlapping PPDUs Received: 1
Total Overlapping PPDUs Received: 2
Successful PPDUs: 1
Failed PPDUs: 2
```

You can also access detailed PPDU records:

```cpp
auto records = rxTraceHelper.GetPpduRecords(1);
```

Output includes RSSI, sender/receiver IDs, time, and MPDU count per PPDU.

---

## 2. WifiTxStatsHelper

### Purpose

Collects MAC-layer transmission statistics for MPDUs including:

* Successful, failed, and retransmitted transmissions
* Queuing and dequeueing times
* Drop reasons

### Output Metrics

* `GetSuccesses()`, `GetFailures()`, `GetRetransmissions()`
* `GetSuccessesByNodeDevice()`, etc.
* `GetSuccessRecords()`, `GetFailureRecords()`

### Example Usage

```cpp
WifiTxStatsHelper txStatsHelper(Seconds(1), Seconds(10));
txStatsHelper.Enable(nodeContainer);
...
auto successes = txStatsHelper.GetSuccesses();
auto failures = txStatsHelper.GetFailures();
```

### Sample Output (pseudo-format)

```
Successes: 145
Failures: 12
Retransmissions: 32
```

More advanced:

```cpp
auto successRecords = txStatsHelper.GetSuccessRecords();
```

Each record includes enqueue time, firstTxTime, ack time, MPDU seq, retries, link ID, etc..

---

## 3. WifiCoTraceHelper

### Purpose

Tracks Wi-Fi channel occupancy states at the PHY level over time:

* IDLE
* TX
* RX
* CCA\_BUSY

### Output Metrics

* Total time and percentage spent in each state per device
* Aggregated by node/device and per second

### Example Usage

```cpp
WifiCoTraceHelper coHelper(Seconds(1.0), Seconds(11.0));
coHelper.Enable(nodeContainer);
Simulator::Run();
coHelper.PrintStatistics(std::cout);
```

### Sample Output

```
---- COT for AP:0 ----
IDLE: +5.69s (56.93%)
CCA_BUSY: +1.18s (11.80%)
TX: +301.90ms (3.02%)
RX: +2.83s (28.25%)
```



---

## 4. FlowMonitor

### Purpose

Tracks per-flow network performance across IP layers. Automatically collects data via probes on:

* Tx, Rx, drop, forward events.

### Output Metrics

* Tx/Rx packet/byte counts
* Packet loss
* Delay and jitter (RFC 3393)
* Histograms: delay, jitter, packet size
* Per-flow forwarding count

### Example Usage

```cpp
FlowMonitorHelper flowHelper;
Ptr<FlowMonitor> monitor = flowHelper.InstallAll();
Simulator::Run();
monitor->SerializeToXmlFile("flowmon.xml", true, true);
```

### Sample Output (in XML)

```xml
<Flow flowId="1"
  txBytes="2149400" rxBytes="2149400"
  txPackets="3735" rxPackets="3735"
  delaySum="138731526300.0ns" jitterSum="1849692150.0ns"
  lostPackets="0" timesForwarded="7466"/>
```

---

## Example
### Simulation Environment Description

* **Simulation Type**: Wi-Fi uplink (STA → AP)
* **Wi-Fi Standard**: IEEE 802.11ac (Wi-Fi 5)
* **Nodes**:

  * 1 Access Point (AP)
  * 1 Station (STA)
* **Mobility**:

  * Static positions
  * Distance between AP and STA: 1 meter
* **IP Configuration**:

  * IPv4 stack installed
  * Same subnet address: 10.0.0.0/24
* **Application**:

  * `OnOffApplication` installed on STA
  * Protocol: UDP
  * Data rate: 10 Mbps
  * OnTime: 1s (constant), OffTime: 0s
  * Transmission starts at 1.0s, ends at 10.0s
* **Receiver**:

  * `PacketSink` installed on AP (UDP port 5000)
* **Performance Monitoring Tools**:

  * `WifiPhyRxTraceHelper`: Records received PPDUs and success/failure rates at PHY layer
  * `WifiTxStatsHelper`: Tracks MPDU transmission statistics at MAC layer
  * `WifiCoTraceHelper`: Measures PHY channel state durations (IDLE, TX, RX, CCA\_BUSY)
  * `FlowMonitor`: Collects IP-level metrics such as throughput, delay, jitter, packet loss
* **Simulation Time**: 10 seconds
* **Final Output**:

  * All trace and performance metrics printed to console

### Source Code
```cpp
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

// Trace helpers
#include "ns3/wifi-phy-rx-trace-helper.h"
#include "ns3/wifi-tx-stats-helper.h"
#include "ns3/wifi-co-trace-helper.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  /* 1. Create nodes and assign names -------------------------------------- */
  Ptr<Node> ap  = CreateObject<Node> ();
  Ptr<Node> sta = CreateObject<Node> ();
  Names::Add ("RB_1F_AP01", ap);
  Names::Add ("M11302229",  sta);

  /* 2. 802.11ac devices --------------------------------------------------- */
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();

  /* Use default constructor here to avoid "Default() not found" issue */
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns3-ac");

  // AP
  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));
  NetDeviceContainer apDev = wifi.Install (phy, mac, ap);

  // STA
  mac.SetType ("ns3::StaWifiMac",
               "Ssid",           SsidValue (ssid),
               "ActiveProbing",  BooleanValue (false));
  NetDeviceContainer staDev = wifi.Install (phy, mac, sta);

  /* 3. Mobility ----------------------------------------------------------- */
  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ap);
  mobility.Install (sta);
  ap ->GetObject<MobilityModel> ()->SetPosition (Vector (0, 0, 0));
  sta->GetObject<MobilityModel> ()->SetPosition (Vector (1, 0, 0));

  /* 4. IP stack ----------------------------------------------------------- */
  InternetStackHelper internet;
  internet.InstallAll ();

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.0.0.0", "255.255.255.0");
  ipv4.Assign (apDev);
  Ipv4InterfaceContainer staIf = ipv4.Assign (staDev);

  /* 5. STA→AP uplink 10 Mbps (OnOff) ------------------------------------- */
  const uint16_t port = 5000;
  OnOffHelper onoff ("ns3::UdpSocketFactory",
                     InetSocketAddress (ap->GetObject<Ipv4>()
                                              ->GetAddress (1, 0).GetLocal (),
                                        port));
  onoff.SetConstantRate (DataRate ("10Mbps"));
  onoff.SetAttribute ("OnTime",
                      StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  onoff.SetAttribute ("OffTime",
                      StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer onoffApp = onoff.Install (sta);
  onoffApp.Start (Seconds (1.0));
  onoffApp.Stop  (Seconds (10.0));

  /* Packet sink at AP ----------------------------------------------------- */
  PacketSinkHelper sink ("ns3::UdpSocketFactory",
                         InetSocketAddress (Ipv4Address::GetAny (), port));
  ApplicationContainer sinkApp = sink.Install (ap);
  sinkApp.Start (Seconds (0.0));

  /* 6. Three Wi-Fi trace helpers ------------------------------------------ */
  NodeContainer allNodes (ap, sta);

  WifiPhyRxTraceHelper phyRxHelper;
  phyRxHelper.Enable (ap);
  //phyRxHelper.Enable (sta);
  phyRxHelper.Start(Seconds(1));
  phyRxHelper.Stop(Seconds(11.0));

  WifiTxStatsHelper txStatsHelper;
  txStatsHelper.Enable (allNodes);

  WifiCoTraceHelper coTraceHelper;
  coTraceHelper.Enable (allNodes);

  /* 7. FlowMonitor -------------------------------------------------------- */
  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor>  monitor = flowmonHelper.InstallAll ();

  /* 8. Run simulation ----------------------------------------------------- */
  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();

  /* ==== 1. Wi-Fi Trace helpers ========================================== */
  std::cout << "\n=== WifiPhyRxTraceHelper ===\n";
  phyRxHelper.PrintStatistics(ap);                    
  //phyRxHelper.PrintStatistics(sta);

  std::cout << "\n=== WifiCoTraceHelper (Channel Occupancy) ===\n";
  coTraceHelper.PrintStatistics(std::cout);

  std::cout << "\n=== WifiTxStatsHelper (TX side) ===\n";
  std::cout << "Duration           : " << txStatsHelper.GetDuration().GetSeconds()
            << " s\n";
  std::cout << "Successful MPDUs   : " << txStatsHelper.GetSuccesses()
            << "\nRetransmissions    : " << txStatsHelper.GetRetransmissions()
            << "\nFailed MPDUs       : " << txStatsHelper.GetFailures()
            << std::endl;

  /* ==== 2. FlowMonitor - extended output ================================ */
  std::cout << "\n=== FlowMonitor ===\n";
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier> (flowmonHelper.GetClassifier ());
  for (const auto &f : monitor->GetFlowStats ())
    {
      const auto five = classifier->FindFlow (f.first);
      std::cout << "\nFlow " << f.first << " ("
                << five.sourceAddress << " → " << five.destinationAddress << ")\n";
      double dur = (f.second.timeLastRxPacket - f.second.timeFirstTxPacket)
                     .GetSeconds ();
      double thr = (f.second.rxBytes * 8.0) / (dur * 1e6);      // Mbit/s
      double avgDelay  = f.second.rxPackets ?
                         f.second.delaySum.GetSeconds() / f.second.rxPackets : 0;
      double avgJitter = f.second.rxPackets ?
                         f.second.jitterSum.GetSeconds() / f.second.rxPackets : 0;

      std::cout << "  TxBytes/RxBytes : "
                << f.second.txBytes << " / " << f.second.rxBytes
                << "\n  Throughput     : " << thr        << " Mbit/s"
                << "\n  Lost Packets   : " << f.second.lostPackets
                << "\n  Avg Delay      : " << avgDelay   << " s"
                << "\n  Avg Jitter     : " << avgJitter  << " s"
                << std::endl;
    }

  Simulator::Destroy ();

}

```

### Output
```txt
=== WifiPhyRxTraceHelper ===
Total PPDUs Received: 21963
Total Non-Overlapping PPDUs Received: 21963
Total Overlapping PPDUs Received: 0

Successful PPDUs: 21963
Failed PPDUs: 0

Total MPDUs: 21966
Total Successful MPDUs: 21966
Total Failed MPDUs: 0

=== WifiCoTraceHelper (Channel Occupancy) ===

---- COT for RB_1F_AP01:0 ----
Showing duration by states: 
IDLE:       +8.39s  (83.93%)
CCA_BUSY: +702.94ms  (7.03%)
TX:       +639.59ms  (6.40%)
RX:       +263.95ms  (2.64%)

---- COT for M11302229:0 ----
Showing duration by states: 
IDLE:       +8.39s  (83.94%)
CCA_BUSY: +353.14ms  (3.53%)
TX:         +1.05s  (10.55%)
RX:       +198.18ms  (1.98%)


=== WifiTxStatsHelper (TX side) ===
Duration           : 10 s
Successful MPDUs   : 21967
Retransmissions    : 0
Failed MPDUs       : 0

=== FlowMonitor ===

Flow 1 (10.0.0.2 → 10.0.0.1)
  TxBytes/RxBytes : 11864880 / 11861100
  Throughput     : 10.5439 Mbit/s
  Lost Packets   : 0
  Avg Delay      : 5.47304e-05 s
  Avg Jitter     : 7.14185e-06 s
```

## Simulation Output Analysis

This simulation models an ideal STA → AP uplink scenario using UDP over IEEE 802.11ac. The application sends data at 10 Mbps for 10 seconds. The output shows:

* All packets were successfully transmitted with **zero retransmissions or failures**.
* The actual throughput is **10.54 Mbps**, slightly higher than the configured 10 Mbps, which is expected due to protocol headers.
* Most of the channel time was **IDLE**, indicating no congestion or interference.
* PHY and MAC layer statistics align perfectly, indicating a stable and clean simulation environment.

---

### WifiPhyRxTraceHelper Analysis (PHY Receive Layer)

| Metric                            | Value             | Interpretation                                  |
| --------------------------------- | ----------------- | ----------------------------------------------- |
| Total PPDUs Received              | 21963             | All PPDUs were received without overlap         |
| Successful / Failed PPDUs         | 21963 / 0         | All PPDUs successfully decoded, no interference |
| Total MPDUs / Successful / Failed | 21966 / 21966 / 0 | All MPDUs delivered successfully                |

**Notes**:

* No overlapping PPDUs implies **no collisions or interference** occurred.
* The number of MPDUs slightly exceeds PPDUs, likely due to A-MPDU aggregation (multiple MPDUs per PPDU).

---

### WifiCoTraceHelper Analysis (Channel Occupancy)

| Node              | IDLE   | CCA\_BUSY | TX     | RX    | Interpretation                             |
| ----------------- | ------ | --------- | ------ | ----- | ------------------------------------------ |
| AP (RB\_1F\_AP01) | 83.93% | 7.03%     | 6.40%  | 2.64% | Mostly idle; AP primarily receives traffic |
| STA (M11302229)   | 83.94% | 3.53%     | 10.55% | 1.98% | STA drives most of the transmission        |

**Notes**:

* Both nodes are idle \~84% of the time → **channel underutilized**, no congestion.
* The STA shows \~10.5% TX time, consistent with continuous traffic generation.

---

### WifiTxStatsHelper Analysis (MAC Transmission Layer)

| Metric           | Value | Interpretation              |
| ---------------- | ----- | --------------------------- |
| Duration         | 10 s  | Simulation run time         |
| Successful MPDUs | 21967 | All transmissions succeeded |
| Retransmissions  | 0     | No retransmissions needed   |
| Failed MPDUs     | 0     | No transmission failures    |

**Notes**:

* **Perfect MAC layer performance**, thanks to ideal conditions (short range, no interference).
* Matches PHY-layer stats, confirming internal consistency.

---

### FlowMonitor Analysis (IP Flow Statistics)

| Metric            | Value                   | Interpretation                                   |
| ----------------- | ----------------------- | ------------------------------------------------ |
| Flow              | 10.0.0.2 → 10.0.0.1     | STA → AP UDP flow                                |
| TxBytes / RxBytes | 11,864,880 / 11,861,100 | Almost all data was received (negligible loss)   |
| Throughput        | **10.54 Mbps**          | Slightly exceeds 10 Mbps due to header inclusion |
| Lost Packets      | 0                       | No packet loss                                   |
| Avg Delay         | 54.7 μs                 | Very low → no queuing or retransmission delays   |
| Avg Jitter        | 7.14 μs                 | Extremely low → excellent for real-time traffic  |

**Notes**:

* Throughput > 10 Mbps is expected because **FlowMonitor counts UDP + IP headers**.
* Using payload-only, the throughput would be close to the configured 10 Mbps.

---

### Conclusion

This simulation demonstrates a **highly ideal and stable Wi-Fi uplink** scenario with:

* **Zero collisions, retransmissions, or losses**
* **Accurate and consistent throughput**
* **Extremely low delay and jitter**
