# This script will add new netconf user
#!/bin/bash

adduser --system netconf && \
   echo "netconf:netconf!" | chpasswd

mkdir -p /home/netconf/.ssh && \
   ssh-keygen -A && \
   ssh-keygen -t dsa -P '' -f /home/netconf/.ssh/id_dsa && \
   cat /home/netconf/.ssh/id_dsa.pub > /home/netconf/.ssh/authorized_keys

