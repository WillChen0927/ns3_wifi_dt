# NS-3 WiFi Digital Twin

## Purpose
This document provides a comprehensive overview of the NS-3 WiFi Digital Twin system, its architecture, and how they interact. The WiFi Digital Twin is a simulation environment built on NS-3 that implements O-RAN compliant interfaces for configuration management and monitoring, allowing it to be integrated with Service Management Orchestration (SMO) systems.

## System Architecture
![image](./doc/images/wifi_dt_architecture.png)

## Message Sequence Chart
Notes on the current MCS module progress:
So far, only partial functionality has been implemented.

### Inintial Setting
![image](./doc/images/wifi_dt_initial_setting_msc.png)

### Synchronization STA and Traffic
![image](./doc/images/wifi_dt_synchronization_sta_and_traffic_msc.png)

### Synchronization AP
![image](./doc/images/wifi_dt_synchronization_ap_msc.png)


## How to use it?


### Installation Guide
For quick install all related tools or library.
```shell
sudo ./install_all.sh
```

For installation details, please refer to the [wifi dt installation guide](./doc/wifi_dt_installation_guide.md).

### User Guide
TBD: Because the program do not finish yet, I will update user guide after all done.

## Folder Structure
```plaintext
ns3_wifi_dt/
├── doc/                        # Project documentation
│   ├── notes                   # Personal notes for records or references
├── sandbox/                    # Experimental or temporary code for testing
├── src/
│   ├── build                   # Files for building environment
│   ├── config_dt               # Configuration file example for ns-3 DT
│   ├── ns3_sim_files           # wifi DT ns-3 code
│   └── rapp                    # rapp for configuration wifi DT
└── README.md
```