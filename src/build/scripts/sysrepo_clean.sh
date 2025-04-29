#!/bin/bash

# Navigate to the sysrepo build directory and clean up
cd ../../../..
cd netconf/sysrepo/build/
sudo make sr_clean

# Return to the parent directory
cd ../..

# Navigate to the Netopeer2 build directory and install
cd Netopeer2/build/
sudo make install

# Return to the root directory
cd ../../..

echo "SCRIPT COMPLETED."
