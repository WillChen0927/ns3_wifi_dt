# Integrate ns-3 with the configuration rApp (Jerry's TEIV rApp)

This note explains how to integrate the configuration rApp (TEIV rApp) developed by Jerry with the ns-3 WiFi Digital Twin (DT) to achieve automatic initialization and configuration of the WiFi DT.
The goal of this integration is to convert parameters and values from the TEIV database into XML-based configuration files via the rApp, and send them to the ns-3 DT using NETCONF, enabling automated AP configuration within the ns-3 simulation environment.

---

- [Integrate ns-3 with the configuration rApp (Jerry's TEIV rApp)](#integrate-ns-3-with-the-configuration-rapp-jerrys-teiv-rapp)
  - [System Architecture](#system-architecture)
  - [Steps:](#steps)
    - [1. Install NETCONF Server](#1-install-netconf-server)
    - [2. Store AP configuratio in TEIV](#2-store-ap-configuratio-in-teiv)
    - [3. Send PNF Registration VES Event](#3-send-pnf-registration-ves-event)
      - [hppt client](#hppt-client)
      - [PNF Registration VES event JSON format](#pnf-registration-ves-event-json-format)
    - [4. Run ns-3 DT](#4-run-ns-3-dt)
    - [5. Receive configuration file through NETCONF](#5-receive-configuration-file-through-netconf)


---

## System Architecture
![wifi-DT-architecture](./images/wifi_dt_architecture.png)

## Steps:

### 1. [Install NETCONF Server]()
In this note, steps 1 to 4 guide us through the installation and configuration of the NETCONF server. Including: 
- **installing NETCONF, loading the YANG model, configuring the server, and starting the NETCONF server**.

---

### 2. Store AP configuratio in TEIV
* **Use PostgreSQL management tools**
To store the AP configuration in the TEIV system, allowing the rApp to convert it into an XML configuration file, we can use PostgreSQL management tools to manually save the data.

![image](./images/teiv_parameters.png)

---

### 3. Send PNF Registration VES Event
Write an HTTP client to send a PNF Registration VES event to the SMO's VES collector.
Through the PNF Registration VES event, the rApp can obtain the following:
- sourceName
    - The rApp uses the source name to extract relevant data associated with the corresponding network element.
- NETCONF username, password, and port
    - This allows the rApp to send configuration files via NETCONF. 

#### hppt client
```cpp=
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
```


#### PNF Registration VES event JSON format
```json=
{
  "event": {
    "commonEventHeader": {
      "domain": "pnfRegistration",
      "eventId": "ns3-wifi-dt-pnfRegistration",
      "eventName": "ns3-wifi-dt-pnfRegistration",
      "eventType": "ns3-wifi-dt-pnfRegistration",
      "sequence": 1,
      "priority": "Low",
      "reportingEntityId": "",
      "reportingEntityName": "ns3-wifi-dt",
      "sourceId": "5487",
      "sourceName": "ns3-wifi-dt",
      "startEpochMicrosec": 1737314968575,
      "lastEpochMicrosec": 1737314968588,
      "nfNamingCode": "001",
      "nfVendorName": "ns-3 wifi-dt",
      "timeZoneOffset": "+00:00",
      "version": "4.1",
      "vesEventListenerVersion": "7.2.1"
    },
    "pnfRegistrationFields": {
      "pnfRegistrationFieldsVersion": "2.1",
      "lastServiceDate": "2025-01-01",
      "macAddress": "00:00:00:00:00:00",
      "manufactureDate": "2025-01-01",
      "modelNumber": "ns3-wifi",
      "oamV4IpAddress": "<Server IP>",
      "serialNumber": "ns3-wifi-<Server IP>",
      "softwareVersion": "2.3.5",
      "unitFamily": "ns-3",
      "unitType": "ns3-wifi",
      "vendorName": "ns-3",
      "additionalFields": {
        "oamPort": "<port>",
        "protocol": "SSH",
        "username": "<username>",
        "password": "<password>",
        "reconnectOnChangedSchema": "false",
        "sleep-factor": "1.5",
        "tcpOnly": "false",
        "connectionTimeout": "20000",
        "maxConnectionAttempts": "100",
        "betweenAttemptsTimeout": "2000",
        "keepaliveDelay": "120"
      }
    }
  }
}
```

---

### 4. Run ns-3 DT
When executing the ns-3 DT program, it will open a NETCONF session, subscribe to the YANG model, and send a PNF Registration VES event to the SMO.

![image](./images/send_pnf_event.png)

---

### 5. Receive configuration file through NETCONF
The rApp will use the `sourceName` from the VES event to retrieve the corresponding configuration from TEIV, convert it into XML format, and send it to the ns-3 server.

Upon receiving the configuration, the ns-3 server will parse it and deploy the APs according to the specified settings.

![image](./images/ns3_received.png)