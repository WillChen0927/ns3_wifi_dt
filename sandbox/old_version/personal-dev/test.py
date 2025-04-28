import xml.etree.ElementTree as ET
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime, timedelta
from ncclient import manager
import requests
import json
import uuid
import subprocess
from scapy.all import sniff
import urllib3
import threading
import time

# Constants
XML_FILE = "ap-config.xml"
TEIV_EVENT_PATH = "/root/teiv/docker-compose/cloudEventProducer/will/events/topolog_event.txt"
TEIV_SCRIPT_PATH = "/root/teiv/docker-compose/cloudEventProducer/will/send_event_to_teiv.sh"
INFLUX_URL = "http://192.168.8.6:30001"
INFLUX_TOKEN = "ZAyM9Iq1hYNR4liKQq4Fi91e4osvhB1G"
INFLUX_ORG = "influxdata"
INFLUX_BUCKET_AP = "ns3-wifi-dt-test"
INFLUX_BUCKET_STA = "ns3-wifi-dt-sta"
NETCONF_HOST = "192.168.8.98"
NETCONF_PORT = "830"
NETCONF_USER = "netconf"
NETCONF_PASS = "netconf!"

sta_data_list = []

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)  # Disable HTTPS warnings

client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
write_api = client.write_api(write_options=SYNCHRONOUS)
query_api = client.query_api()

# Load XML
def load_ap_config():
    tree = ET.parse(XML_FILE)
    root = tree.getroot()
    namespace = {"ns": "urn:myconfig:wifidtap"}
    ET.register_namespace('', namespace["ns"])
    return root, namespace

# Extract AP data
def extract_ap_data(root, namespace, delete=False):
    ap_data = []
    for ap in root.findall("ns:aps", namespace):
        ap_id = ap.find("ns:id", namespace).text
        if delete:
            ap_data.append({"id": ap_id})
        else:
            ap_data.append({
                "id": ap_id,
                "attributes": {
                    "apName": ap.find("ns:ap-name", namespace).text,
                    "apChannel": int(ap.find("ns:ap-channel", namespace).text),
                    "apPositionX": float(ap.find("ns:position/ns:x", namespace).text),
                    "apPositionY": float(ap.find("ns:position/ns:y", namespace).text),
                    "apPositionZ": float(ap.find("ns:position/ns:z", namespace).text)
                }
            })
    return ap_data

# Generate and save TEIV event
def save_teiv_event(ap_data, event_type):
    content = {"entities": [{"o-ran-smo-teiv-wifi:WIFIap": ap_data}]}
    metadata = {
        "ce_specversion": "1.0",
        "ce_id": str(uuid.uuid4()),
        "ce_source": "dmi-plugin:nm-1",
        "ce_type": f"ran-logical-topology.{event_type}",
        "content-type": "application/yang-data+json",
        "ce_time": datetime.utcnow().strftime("%Y-%m-%dT%H:%M:%SZ"),
        "ce_dataschema": "https://ties:8080/schemas/v1/r1-topology"
    }
    txt_content = ",".join(f"{k}:::{v}" for k, v in metadata.items()) + ",,," + json.dumps(content, ensure_ascii=False)
    with open(TEIV_EVENT_PATH, "w", encoding="utf-8") as file:
        file.write(txt_content)
    subprocess.run(["sudo", TEIV_SCRIPT_PATH], check=True)

# Send config to NETCONF
def send_config():
    root, namespace = load_ap_config()
    date = ET.Element("date")
    date.text = datetime.now().strftime("%Y-%m-%d-%H:%M:%S")
    root.insert(0, date)
    config_data = f'<config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">{ET.tostring(root, encoding="unicode")}</config>'
    try:
        with manager.connect(host=NETCONF_HOST, port=NETCONF_PORT, username=NETCONF_USER, password=NETCONF_PASS, hostkey_verify=False) as smo:
            smo.edit_config(target="running", config=config_data)
    except Exception as e:
        print(f"Error updating config: {e}")

# Write topology data to AP InfluxDB
def write_to_ap_influxdb(ap_data, delete=False):
    for ap in ap_data:
        point = Point("wifi_ap").tag("teiv-ap", ap["id"]).field("ap_id", "N/A" if delete else ap["id"])
        if not delete:
            for key, value in ap["attributes"].items():
                point = point.field(key, value)
        write_api.write(bucket=INFLUX_BUCKET_AP, org=INFLUX_ORG, record=point)

# Query STA data from STA InfluxDB
def query_data_from_sta_influxdb():
    while True:
        now = datetime.utcnow()
        start_time = (now - timedelta(minutes=1)).strftime("%Y-%m-%dT%H:%M:%SZ")
        stop_time = (now + timedelta(minutes=1)).strftime("%Y-%m-%dT%H:%M:%SZ")
        query = f'''
            from(bucket: "{INFLUX_BUCKET_STA}")
            |> range(start: {start_time}, stop: {stop_time})
            |> filter(fn: (r) => r["_measurement"] == "wifi_sta")
            |> filter(fn: (r) => r["_field"] == "throughput")
        '''
        result = query_api.query(query)
        for table in result:
            for record in table.records:
                sta_id = record.values["STA_ID"]
                ap_name = record.values["AP_name"]
                throughput = record.get_value()
                print(f"STA_ID={sta_id}, AP_name={ap_name}, Throughput={throughput} kb/s")

        print("Waiting 1 minute before next query...\n")
        time.sleep(60)

# Packet handler
def packet_handler(packet):
    if packet.haslayer("Raw"):
        raw_text = packet["Raw"].load.decode("utf-8", errors="ignore")
        if '"domain":"notification"' in raw_text:
            if '"newState":"inService"' in raw_text:
                # initall
                root, namespace = load_ap_config()
                ap_data = extract_ap_data(root, namespace)
                save_teiv_event(ap_data, "create")
                send_config()
                write_to_ap_influxdb(ap_data)
                print("WiFi DT started!")
                # STA part
                sta_query_thread = threading.Thread(target=query_data_from_sta_influxdb, daemon=True)
                sta_query_thread.start()
            elif '"newState":"outOfService"' in raw_text:
                root, namespace = load_ap_config()
                ap_data = extract_ap_data(root, namespace, delete=True)
                save_teiv_event(ap_data, "delete")
                write_to_ap_influxdb(ap_data, delete=True)
                print("WiFi DT shut down!")

# Main execution
if __name__ == "__main__":
    try:
        print("WiFi DT rApp running...")
        print("Listening simulator event...")
        sniff(filter="tcp port 30417", prn=packet_handler)
    except KeyboardInterrupt:
        print("\nCtrl+C detected, shutting down...")
        client.close()  # 關閉 InfluxDB 連線
        print("InfluxDB connection closed. Exiting...")