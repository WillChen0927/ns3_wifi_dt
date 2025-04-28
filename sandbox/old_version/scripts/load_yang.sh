#!/bin/bash

# Stop script on errors
set -e

# folder path
YANGS_DIR="../yang_files"
XML_DIR="../xml_files"

# Import YANG models
sudo sysrepoctl -i $YANGS_DIR/wifi-dt-ap.yang
sudo sysrepoctl -i $YANGS_DIR/wifi-dt-sta.yang

# Change owner and permissions
sudo sysrepoctl --change wifi-dt-ap --owner=ns3 --permissions=666
sudo sysrepoctl --change wifi-dt-sta --owner=ns3 --permissions=666

# Import NACM configuration
#sudo sysrepocfg --import=$XML_DIR/nacm-config.xml -v 3 --datastore running --module ietf-netconf-acm

# Import NETCONF server configuration
#sudo sysrepocfg --import=$XML_DIR/netconf_server_ipv6.xml -v 3 --datastore running --module ietf-netconf-server

echo "SCRIPT COMPLETED."
