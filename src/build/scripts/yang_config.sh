#!/bin/bash

cd ../netconf_config_xml

sudo sysrepocfg --import=nacm-config.xml -v 3 --datastore running --module ietf-netconf-acm
sudo sysrepocfg --import=netconf_server_ipv6.xml -v 3 --datastore running --module ietf-netconf-server

sudo sysrepoctl --change _3gpp-5g-common-yang-types --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-fm --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-managed-element --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-managed-function --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-measurements --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-subscription-control --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-top --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-trace --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-common-yang-types --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-nr-nrm-gnbdufunction --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-nr-nrm-nrcelldu --owner=ns3 --permissions=666
sudo sysrepoctl --change _3gpp-nr-nrm-rrmpolicy --owner=ns3 --permissions=666


echo "SCRIPT COMPLETED."