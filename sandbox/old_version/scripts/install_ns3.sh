#!/bin/bash

CURRENT_PATH=`pwd`
HOME="$CURRENT_PATH/../.."
cd $HOME

# Update the package list
sudo apt-get update

# Install required packages
sudo apt-get install -y git vim g++ python3-pip cmake ninja-build
sudo apt-get install -y gir1.2-goocanvas-2.0 python3-gi python3-gi-cairo python3-pygraphviz gir1.2-gtk-3.0 ipython3

# Install Python packages
python3 -m pip install --user cppyy

# Create the ns-3 directory and download the source code
mkdir ns-3
cd ns-3
git clone https://gitlab.com/nsnam/ns-3-allinone.git
cd ns-3-allinone
python3 download.py -n ns-3.42

# Compile and build ns-3
./build.py
cd ns-3.42
./ns3 configure --enable-examples --enable-tests --enable-python-bindings
./ns3 build

# Install gnuplot
sudo apt-get install -y gnuplot

# Display a completion message
echo "ns-3 installation and build complete!"
