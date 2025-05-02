#!/bin/bash

cd ../config_netconf

OWNER=$(logname)

sudo sysrepocfg --import=nacm_config.xml -v 3 --datastore running --module ietf-netconf-acm
sudo sysrepocfg --import=netconf_server_ipv6.xml -v 3 --datastore running --module ietf-netconf-server

sudo sysrepoctl --change _3gpp-5g-common-yang-types --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-fm --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-managed-element --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-managed-function --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-measurements --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-subscription-control --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-top --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-trace --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-common-yang-types --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-nr-nrm-gnbdufunction --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-nr-nrm-nrcelldu --owner="$OWNER" --permissions=666
sudo sysrepoctl --change _3gpp-nr-nrm-rrmpolicy --owner="$OWNER" --permissions=666


echo "SCRIPT COMPLETED."