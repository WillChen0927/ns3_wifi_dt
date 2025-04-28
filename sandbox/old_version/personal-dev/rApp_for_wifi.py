import xml.etree.ElementTree as ET
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime
from ncclient import manager
import requests
import time
import json
import uuid
import subprocess
from scapy.all import sniff

def update_to_teiv():
    # Load XML file
    tree = ET.parse("ap-config.xml")
    root = tree.getroot()

    # Extract AP data
    namespace = {"ns": "urn:myconfig:wifidtap"}
    ET.register_namespace('', namespace["ns"])

    ap_data = []
    for ap in root.findall("ns:aps", namespace):
        ap_id = ap.find("ns:id", namespace).text
        ap_name = ap.find("ns:ap-name", namespace).text
        ap_channel = int(ap.find("ns:ap-channel", namespace).text)
        position = ap.find("ns:position", namespace)
        ap_position_x = float(position.find("ns:x", namespace).text)
        ap_position_y = float(position.find("ns:y", namespace).text)
        ap_position_z = float(position.find("ns:z", namespace).text)

        ap_data.append({
            "id": ap_id,
            "attributes": {
                "apName": ap_name,
                "apChannel": ap_channel,
                "apPositionX": ap_position_x,
                "apPositionY": ap_position_y,
                "apPositionZ": ap_position_z
            }
        })

    # Construct JSON structure
    entities = [{"o-ran-smo-teiv-wifi:WIFIap": ap_data}]
    content = {
        "entities": entities
    }

    # Generate metadata
    ce_specversion = "1.0"
    ce_id = str(uuid.uuid4())
    ce_source = "dmi-plugin:nm-1"
    ce_type = "ran-logical-topology.create"
    content_type = "application/yang-data+json"
    ce_time = datetime.now().strftime("%Y-%m-%dT%H:%M:%SZ")
    ce_dataschema = "https://ties:8080/schemas/v1/r1-topology"

    # Generate TXT content
    txt_content = (
        f"ce_specversion:::{ce_specversion},"
        f"ce_id:::{ce_id},"
        f"ce_source:::{ce_source},"
        f"ce_type:::{ce_type},"
        f"content-type:::{content_type},"
        f"ce_time:::{ce_time},"
        f"ce_dataschema:::{ce_dataschema},,,"
        f"{json.dumps(content, ensure_ascii=False)}"
    )

    # Save to teiv topology file
    with open("/root/teiv/docker-compose/cloudEventProducer/will/events/topolog_event.txt", "w", encoding="utf-8") as file:
        file.write(txt_content)

    # print("TXT file generated successfully.")


    script_path = "/root/teiv/docker-compose/cloudEventProducer/will/send_event_to_teiv.sh"
    
    try:
        # use sudo
        subprocess.run(
            ["sudo", script_path], 
            check=True  # if error
        )
        print(f"exacute success: {script_path}")
    except FileNotFoundError:
        print(f"file not found: {script_path}")
    except subprocess.CalledProcessError as e:
        print(f"exacute fail, error code: {e.returncode}")
    except Exception as e:
        print(f"error: {e}")

    topology_to_influxdb()

def delete_teiv_items_when_shut_down():
    # Load XML file
    tree = ET.parse("ap-config.xml")
    root = tree.getroot()

    # Extract AP data
    namespace = {"ns": "urn:myconfig:wifidtap"}
    ET.register_namespace('', namespace["ns"])

    ap_data = []
    for ap in root.findall("ns:aps", namespace):
        ap_id = ap.find("ns:id", namespace).text

        ap_data.append({"id": ap_id})

    # Construct JSON structure
    entities = [{"o-ran-smo-teiv-wifi:WIFIap": ap_data}]
    content = {
        "entities": entities
    }

    # Generate metadata
    ce_specversion = "1.0"
    ce_id = str(uuid.uuid4())
    ce_source = "dmi-plugin:nm-1"
    ce_type = "ran-logical-topology.delete"
    content_type = "application/yang-data+json"
    ce_time = datetime.now().strftime("%Y-%m-%dT%H:%M:%SZ")
    ce_dataschema = "https://ties:8080/schemas/v1/r1-topology"

    # Generate TXT content
    txt_content = (
        f"ce_specversion:::{ce_specversion},"
        f"ce_id:::{ce_id},"
        f"ce_source:::{ce_source},"
        f"ce_type:::{ce_type},"
        f"content-type:::{content_type},"
        f"ce_time:::{ce_time},"
        f"ce_dataschema:::{ce_dataschema},,,"
        f"{json.dumps(content, ensure_ascii=False)}"
    )

    # Save to file
    with open("/root/teiv/docker-compose/cloudEventProducer/will/events/topolog_event.txt", "w", encoding="utf-8") as file:
        file.write(txt_content)

    # print("TXT file generated successfully.")


    script_path = "/root/teiv/docker-compose/cloudEventProducer/will/send_event_to_teiv.sh"
    
    try:
        # use sudo
        subprocess.run(
            ["sudo", script_path], 
            check=True  # if error
        )
        print(f"exacute success: {script_path}")
    except FileNotFoundError:
        print(f"file not found: {script_path}")
    except subprocess.CalledProcessError as e:
        print(f"exacute fail, error code: {e.returncode}")
    except Exception as e:
        print(f"error: {e}")

    topology_to_influxdb_delete()

def send_config():
    # Load XML file
    tree = ET.parse("ap-config.xml")
    root = tree.getroot()

    # Define namespace
    namespace = {"ns": "urn:myconfig:wifidtap"}
    ET.register_namespace('', namespace["ns"])

    # Get current time
    current_time = datetime.now().strftime("%Y-%m-%d-%H:%M:%S")

    # add <date> element
    date = ET.Element("date")
    date.text = current_time
    root.insert(0, date)  # Insert

    # change to string
    result = ET.tostring(root, encoding="unicode")
    wrapped_result = f'<config xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">{result}</config>'

    # connect to netconf
    try:
        with manager.connect(
            host='192.168.8.98',
            port="830",
            username="netconf",
            password="netconf!",
            hostkey_verify=False
        ) as smo:
            smo.edit_config(target="running", config=wrapped_result)
            print(f"Config updated successfully at {current_time}")
    except Exception as e:
        print(f"Error updating config: {e}")

def topology_to_influxdb():
    # InfluxDB settings
    url = "http://192.168.8.6:30001"
    token = "ZAyM9Iq1hYNR4liKQq4Fi91e4osvhB1G"
    org = "influxdata"
    bucket = "ns3-wifi-dt-test"

    # Initialize InfluxDB client
    client = InfluxDBClient(url=url, token=token, org=org)
    write_api = client.write_api(write_options=SYNCHRONOUS)

    # Ignore HTTPS warnings
    import urllib3
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

    # Path to the XML file
    xml_file = "ap-config.xml"

    # Parse the XML file
    tree = ET.parse(xml_file)
    root = tree.getroot()

    # Define XML namespace
    namespace = {"ns": "urn:myconfig:wifidtap"}  # XML namespace
    ET.register_namespace('', namespace["ns"])  # Register namespace

    # Iterate through <aps> nodes
    for aps in root.findall("ns:aps", namespace):
        ap_id = aps.find("ns:id", namespace).text  # Get <id>
        ap_name = aps.find("ns:ap-name", namespace).text  # Get <ap-name>
        
        
        # Generate URL based on the id
        api_url = f"http://10.233.64.232:8080/topology-inventory/v1alpha11/domains/WIFI/entity-types/WIFIap/entities/{ap_id}"
        
        # Define headers
        headers = {
            "Accept": "application/problem+json"
        }
        
        # Send GET request
        response = requests.get(api_url, headers=headers, verify=False)
        response.raise_for_status()
        data = response.json()
        
        # Extract required data
        wifi_ap = data.get("o-ran-smo-teiv-wifi:WIFIap", [])[0]
        attributes = wifi_ap.get("attributes", {})
        
        ap_name = attributes.get("apName", "N/A")
        #ap_channel = attributes.get("apChannel", "N/A")
        ap_channel = attributes.get("apChannel", "N/A")
        ap_position_x = attributes.get("apPositionX", "N/A")
        ap_position_y = attributes.get("apPositionY", "N/A")
        ap_position_z = attributes.get("apPositionZ", "N/A")
        
        # Create a data point to write to InfluxDB
        
        point = (
            Point("wifi_ap")
            .tag("teiv-ap", str(ap_name))
            .field("ap_id", str(ap_id))
            .field("apName", str(ap_name))
            .field("apChannel", str(ap_channel))
            .field("apPositionX", str(ap_position_x))
            .field("apPositionY", str(ap_position_y))
            .field("apPositionZ", str(ap_position_z))
        )
              
        # Write to InfluxDB
        write_api.write(bucket=bucket, org=org, record=point)
        print(f"Data for AP ID {ap_id} written to InfluxDB.")

    # Close the InfluxDB client
    client.close()

def topology_to_influxdb_delete():
    # InfluxDB settings
    url = "http://192.168.8.6:30001"
    token = "ZAyM9Iq1hYNR4liKQq4Fi91e4osvhB1G"
    org = "influxdata"
    bucket = "ns3-wifi-dt-test"

    # Initialize InfluxDB client
    client = InfluxDBClient(url=url, token=token, org=org)
    write_api = client.write_api(write_options=SYNCHRONOUS)

    # Ignore HTTPS warnings
    import urllib3
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

    # Path to the XML file
    xml_file = "ap-config.xml"

    # Parse the XML file
    tree = ET.parse(xml_file)
    root = tree.getroot()

    # Define XML namespace
    namespace = {"ns": "urn:myconfig:wifidtap"}  # XML namespace
    ET.register_namespace('', namespace["ns"])  # Register namespace

    # Iterate through <aps> nodes
    for aps in root.findall("ns:aps", namespace):
        ap_id = aps.find("ns:id", namespace).text  # Get <id>
        ap_name = aps.find("ns:ap-name", namespace).text  # Get <ap-name>
        
        # Create a data point to write to InfluxDB
        
        point = (
            Point("wifi_ap")
            .tag("teiv-ap", str(ap_name))
            .field("ap_id", "N/A")
            .field("apName", "N/A")
            .field("apChannel", "N/A")
            .field("apPositionX", "N/A")
            .field("apPositionY", "N/A")
            .field("apPositionZ", "N/A")
        )
              
        # Write to InfluxDB
        write_api.write(bucket=bucket, org=org, record=point)
        print(f"Data for AP ID {ap_id} written to InfluxDB.")

    # Close the InfluxDB client
    client.close()

def packet_handler(packet):
    if packet.haslayer("Raw"):
        raw_data = bytes(packet["Raw"].load)
        raw_text = raw_data.decode("utf-8", errors="ignore")
        #print(raw_text)

        if '"domain":"notification"' in raw_text:
            if '"newState":"inService"' in raw_text:
                print("Wifi DT started!!")
            elif '"newState":"outOfService"' in raw_text:
                delete_teiv_items_when_shut_down()
                print("Wifi DT shut down!!")



update_to_teiv()
send_config()
print("Listening for packets...")
sniff(filter="tcp port 30417", prn=packet_handler)