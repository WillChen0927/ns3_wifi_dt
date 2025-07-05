#!/bin/bash

set -e  # Exit immediately if a command exits with a non-zero status

echo "==================================="
echo "  Starting ns-3 Environment Setup"
echo "==================================="

# Parse version argument; default to ns-3.45 if none is provided
NS3_VERSION=${1:-"3.45"}

# Validate version format (e.g., 3.44)
if [[ ! "$NS3_VERSION" =~ ^[0-9]+\.[0-9]+$ ]]; then
    echo "Invalid version format: '$NS3_VERSION'"
    echo "Usage: sudo ./ns3_install_and_copy.sh [ns-3 version]"
    echo "  Example: sudo ./ns3_install_and_copy.sh 3.45"
    exit 1
fi

echo "Target ns-3 version: ns-${NS3_VERSION}"

# Get the absolute path of the script and define project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOME_DIR="$(cd "$SCRIPT_DIR/../../../" && pwd)"
cd "$HOME_DIR/ns3_wifi_dt"

# Add toolchain PPA if it's not already added
echo "Adding toolchain PPA if not already added..."
if ! grep -q "ubuntu-toolchain-r/test" /etc/apt/sources.list /etc/apt/sources.list.d/* 2>/dev/null; then
    sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
else
    echo "Toolchain PPA already exists, skipping..."
fi

# Update package list
echo "Updating package lists..."
sudo apt-get update

# Install GCC 10 and G++ 10
echo "Installing GCC-10 and G++-10..."
sudo apt-get install -y gcc-10 g++-10

# Set GCC-10 and G++-10 as default alternatives
echo "Setting up gcc and g++ alternatives..."
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100

# Install development tools and required Python packages
echo "Installing developer tools and Python packages..."
sudo apt-get install -y git vim cmake ninja-build
sudo apt-get install -y python3 python3-dev python3-pygraphviz python3-gi python3-gi-cairo ipython3 python3-pip
sudo apt-get install -y gir1.2-gtk-3.0 gir1.2-goocanvas-2.0
sudo apt-get install -y pkg-config qt5-qmake qt5-default qtchooser qtbase5-dev-tools qtbase5-dev libgtk-3-dev
python3 -m pip install --user cppyy==3.1.2

# Create ns-3 workspace directory
echo "Creating ns-3 workspace..."
mkdir -p ns-3
cd ns-3

echo ""
echo "================================="
echo "  Installing ns-3"
echo "================================="

# Clone ns-3-allinone if not already present
if [ ! -d "ns-3-allinone" ]; then
    echo "Cloning ns-3-allinone repository..."
    git clone https://gitlab.com/nsnam/ns-3-allinone.git
else
    echo "ns-3-allinone already cloned, skipping..."
fi

cd ns-3-allinone

# Download the specified version of ns-3
echo "Downloading ns-${NS3_VERSION} source code..."
python3 download.py -n ns-${NS3_VERSION}

echo ""
echo "================================="
echo "  Building ns-3"
echo "================================="

# Configure and build ns-3
cd "ns-${NS3_VERSION}"
./ns3 configure --enable-examples --enable-tests --enable-python-bindings
./ns3 build

echo ""
echo "ns-${NS3_VERSION} build completed!"

# ----------------------------
# Post-installation checks
# ----------------------------

echo ""
echo "================================="
echo "  Verifying Installed Versions"
echo "================================="

echo -n "gcc version     : "; gcc --version | head -n1
echo -n "g++ version     : "; g++ --version | head -n1
echo -n "python3 version : "; python3 --version
echo -n "cmake version   : "; cmake --version | head -n1

echo ""
echo "All tools installed and ns-${NS3_VERSION} is ready!"

# ----------------------------
# Copy Simulation Files
# ----------------------------

echo ""
echo "==================================="
echo "  Copying Simulation Files to ns-3"
echo "==================================="

# Define full path to ns-3 build directory
NS3_DIR="$HOME_DIR/ns3_wifi_dt/ns-3/ns-3-allinone/ns-${NS3_VERSION}"

# Ensure scratch directory exists
if [ ! -d "$NS3_DIR/scratch" ]; then
    echo "Error: ns-3 scratch directory not found at $NS3_DIR/scratch"
    exit 1
fi

# Copy simulation files into ns-3 scratch directory
cp "$HOME_DIR/ns3_wifi_dt/src/ns3_sim_files/"* "$NS3_DIR/scratch/"

echo "Done. Now you can run the WiFi DT main program from ns-3!"
