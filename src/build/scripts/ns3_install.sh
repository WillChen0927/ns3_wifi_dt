#!/bin/bash

set -e  # if error then stop
echo "Starting ns-3.44 Environment Setup..."

# Save current path
CURRENT_PATH=$(pwd)
ROOT_DIR="$CURRENT_PATH/../.."
cd "$ROOT_DIR"

echo "Adding toolchain PPA if not already added..."
if ! grep -q "ubuntu-toolchain-r/test" /etc/apt/sources.list /etc/apt/sources.list.d/*; then
    sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
else
    echo "Toolchain PPA already exists, skipping..."
fi

echo "Updating package lists..."
sudo apt-get update

echo "Installing GCC-10 and G++-10..."
sudo apt-get install -y gcc-10 g++-10

echo "Setting up gcc and g++ alternatives..."
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100

echo "Installing developer tools..."
sudo apt-get install -y git vim cmake ninja-build
sudo apt-get install -y python3 python3-dev python3-pygraphviz python3-gi python3-gi-cairo ipython3
sudo apt-get install -y gir1.2-gtk-3.0 gir1.2-goocanvas-2.0
sudo apt-get install -y pkg-config qt5-qmake qt5-default qtchooser qtbase5-dev-tools qtbase5-dev libgtk-3-dev

echo "Creating ns-3 workspace..."
mkdir -p ns-3
cd ns-3

if [ ! -d "ns-3-allinone" ]; then
    echo "Cloning ns-3-allinone repository..."
    git clone https://gitlab.com/nsnam/ns-3-allinone.git
else
    echo "ns-3-allinone already cloned, skipping..."
fi

cd ns-3-allinone

echo "Downloading ns-3.44 source code..."
python3 download.py -n ns-3.44

echo "Environment setup completed!"

# Version checks
echo ""
echo "Verifying installed versions:"
echo -n "gcc version: "; gcc --version | head -n1
echo -n "g++ version: "; g++ --version | head -n1
echo -n "python3 version: "; python3 --version
echo -n "cmake version: "; cmake --version
echo ""

echo "All tools installed and ready for ns-3.44 build!"
