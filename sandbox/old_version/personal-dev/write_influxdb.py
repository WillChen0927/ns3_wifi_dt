from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime, timedelta

url = "http://192.168.8.6:30001"
token = "ZAyM9Iq1hYNR4liKQq4Fi91e4osvhB1G" 
org = "influxdata"
bucket = "ns3-wifi-dt-sta"


client = InfluxDBClient(url=url, token=token, org=org)
write_api = client.write_api(write_options=SYNCHRONOUS)


def write_in(sta_id, ap_name, throughput, input_time):
    
    input_time_dt = datetime.strptime(input_time, "%Y-%m-%dT%H:%M:%SZ")
    stored_time_dt = input_time_dt - timedelta(hours=8)
    stored_time_str = stored_time_dt.strftime("%Y-%m-%dT%H:%M:%SZ")

    point = (
        Point("wifi_sta")
        .tag("STA_ID", sta_id)
        .tag("AP_name", ap_name)
        .field("throughput", throughput)
        .time(stored_time_str)
    )
    
    write_api.write(bucket=bucket, org=org, record=point)
    
    #print(f"Input Time: {input_time} → Stored Time: {stored_time_str}")
    print(f"Data written: STA_ID={sta_id}, AP_name={ap_name}, Throughput={throughput}")


write_in("m11302229", "RB_1F_AP01", "100 kb/s", "2025-02-27T16:41:00Z")
write_in("m11302230", "RB_1F_AP02", "200 kb/s", "2025-02-27T16:41:00Z")

print("DONE")
client.close()
