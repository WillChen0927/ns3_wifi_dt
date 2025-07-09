from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS
from datetime import datetime, timedelta

url = "http://192.168.8.121:30001"
token = "6DH4UsdiCDVUbfaxkNNIsYVw7FX1bTCw" 
org = "Will"
bucket = "fake-ap"


client = InfluxDBClient(url=url, token=token, org=org)
write_api = client.write_api(write_options=SYNCHRONOUS)


def write_in_ap(ap_name, radio_band, channel, idle_time, channel_busy_time, tx_time, rx_time, rx_throughput, tx_throughput, input_time):
    
    input_time_dt = datetime.strptime(input_time, "%Y-%m-%dT%H:%M:%SZ")
    stored_time_dt = input_time_dt - timedelta(hours=8)
    stored_time_str = stored_time_dt.strftime("%Y-%m-%dT%H:%M:%SZ")

    point = (
        Point("fake_ap_info")
        .tag("AP_Name", ap_name)
        .tag("RadioBand", radio_band)
        .tag("Channel", channel)
        .field("Idel time", idle_time)
        .field("Channel busy time", channel_busy_time)
        .field("TX time", tx_time)
        .field("RX time", rx_time)
        .field("Rx_Throughput", rx_throughput)
        .field("Tx_Throughput", tx_throughput)
        .time(stored_time_str)
    )
    
    write_api.write(bucket=bucket, org=org, record=point)


def write_in_sta(sta_name, ssid, radio_band, ap_name, rx_bytes, tx_bytes, input_time):
    
    input_time_dt = datetime.strptime(input_time, "%Y-%m-%dT%H:%M:%SZ")
    stored_time_dt = input_time_dt - timedelta(hours=8)
    stored_time_str = stored_time_dt.strftime("%Y-%m-%dT%H:%M:%SZ")

    point = (
        Point("fake_ap_info")
        .tag("STA_Name", sta_name)
        .tag("SSID", ssid)
        .tag("RadioBand", radio_band)
        .tag("AP_Name", ap_name)
        .field("RX_Bytes", rx_bytes)
        .field("Tx_Bytes", tx_bytes)
        .time(stored_time_str)
    )
    
    write_api.write(bucket=bucket, org=org, record=point)


write_in_sta("BMW_1F_AP01", "5GHz", "36", "95%", "2%", "1%", "2%", "100 kb/s", "200 kb/s", "2025-02-27T16:41:00Z")
write_in_sta("BMW_1F_AP01", "2.4GHz", "1", "90%", "5%", "3%", "2%", "150 kb/s", "250 kb/s", "2025-02-27T16:42:00Z")
write_in_sta("BMW_1F_AP02", "5GHz", "40", "92%", "3%", "2%", "3%", "120 kb/s", "220 kb/s", "2025-02-27T16:43:00Z")

write_in_ap("STA1", "bmwlab-peap", "5GHz", "BMW_1F_AP01", "500 kb/s", "600 kb/s", "2025-02-27T16:41:00Z")
write_in_ap("STA2", "bmwlab-peap", "2.4GHz", "BMW_1F_AP01", "550 kb/s", "650 kb/s", "2025-02-27T16:42:00Z")
write_in_ap("STA3", "bmwlab-peap", "5GHz", "BMW_1F_AP02", "520 kb/s", "620 kb/s", "2025-02-27T16:43:00Z")


print("DONE")
client.close()
