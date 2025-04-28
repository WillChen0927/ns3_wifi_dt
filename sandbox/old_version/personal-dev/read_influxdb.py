import time
from datetime import datetime, timedelta
from influxdb_client import InfluxDBClient

url = "http://192.168.8.6:30001"
token = "ZAyM9Iq1hYNR4liKQq4Fi91e4osvhB1G" 
org = "influxdata"
bucket = "ns3-wifi-dt-sta"

# create InfluxDB connection
client = InfluxDBClient(url=url, token=token, org=org)
query_api = client.query_api()

sta_data_list = []

def query_latest_data():
    # get UTC time
    now = datetime.utcnow()
    start_time = (now - timedelta(minutes=1)).strftime("%Y-%m-%dT%H:%M:%SZ")
    stop_time = (now + timedelta(minutes=1)).strftime("%Y-%m-%dT%H:%M:%SZ")


    # query
    query = f'''
        from(bucket: "{bucket}")
        |> range(start: {start_time}, stop: {stop_time})
        |> filter(fn: (r) => r["_measurement"] == "wifi_sta")
        |> filter(fn: (r) => r["_field"] == "throughput")
    '''

    # search
    result = query_api.query(query)

    for table in result:
        for record in table.records:
            sta_id = record.values["STA_ID"]
            ap_name = record.values["AP_name"]
            throughput = record.get_value()
            print(f"STA_ID={sta_id}, AP_name={ap_name}, Throughput={throughput} kb/s")

            sta_data_list.append({
                "STA_ID": sta_id,
                "AP_name": ap_name,
                "Throughput": throughput
            })

    print("\n目前已儲存的資料：")
    for entry in sta_data_list:
        print(entry)

while True:
    print(f"\n[{datetime.utcnow().strftime('%Y-%m-%d %H:%M:%SZ')}] Running query...")
    query_latest_data()
    print("Waiting 1 minutes before next query...\n")
    time.sleep(60)
