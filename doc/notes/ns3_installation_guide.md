# ns-3 Installation Guide (for ns-3.44)

- [ns-3 Installation Guide (for ns-3.44)](#ns-3-installation-guide-for-ns-344)
  - [0. Requirement](#0-requirement)
  - [1. Environment Preparation](#1-environment-preparation)
  - [2. Installation and Setup Process](#2-installation-and-setup-process)
    - [2.1 Add Toolchain Repository](#21-add-toolchain-repository)
    - [2.2 Install Development Tools](#22-install-development-tools)
    - [2.3 Create Workspace and Download ns-3](#23-create-workspace-and-download-ns-3)
  - [3. Build ns-3](#3-build-ns-3)
    - [3.1 Configure Build Options](#31-configure-build-options)
    - [3.2 Build the Project](#32-build-the-project)
    - [3.3 Verify the Installation](#33-verify-the-installation)
  - [4. Appendix: Installation and Build Scripts](#4-appendix-installation-and-build-scripts)
    - [4.1 `ns3_install.sh`](#41-ns3_installsh)
    - [4.2 `build_ns3.sh`](#42-build_ns3sh)
- [Conclusion](#conclusion)


## 0. Requirement
![image](../doc/images/requirement_ns3.png)

## 1. Environment Preparation

This guide is for installing and building **ns-3.44** on **Ubuntu 20.04**.

We will:
- Install required build tools (GCC 10, CMake, Ninja, etc.)
- Set up Python environment
- Download the ns-3.44 source code
- Build and verify the ns-3.44 environment

---

## 2. Installation and Setup Process

### 2.1 Add Toolchain Repository

Add the GCC 10 toolchain PPA repository:

```bash
sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
sudo apt-get update
```

> **Explanation:** Ubuntu 20.04 provides gcc-9 by default; ns-3.44 requires gcc-10 for newer C++ standards.

---

### 2.2 Install Development Tools

Install gcc-10, g++-10, CMake, Ninja, and Python3 related packages:

```bash
sudo apt-get install -y gcc-10 g++-10
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100

sudo apt-get install -y git vim cmake ninja-build
sudo apt-get install -y python3 python3-dev python3-pip python3-pygraphviz python3-gi python3-gi-cairo ipython3
sudo apt-get install -y gir1.2-gtk-3.0 gir1.2-goocanvas-2.0
sudo apt-get install -y pkg-config qt5-qmake qt5-default qtchooser qtbase5-dev-tools qtbase5-dev libgtk-3-dev
```

Install the Python `cppyy` module for binding support:

```bash
python3 -m pip install --user cppyy==3.1.2
```

---

### 2.3 Create Workspace and Download ns-3

Set up your workspace:

```bash
mkdir -p ns-3
cd ns-3
git clone https://gitlab.com/nsnam/ns-3-allinone.git
cd ns-3-allinone
python3 download.py -n ns-3.44
```

> **Explanation:**
> - `ns-3-allinone` manages source downloads and dependencies for ns-3.
> - `download.py -n ns-3.44` downloads only the specific ns-3.44 version.

---

## 3. Build ns-3

### 3.1 Configure Build Options

Move to the ns-3.44 source folder and configure the build options:

```bash
cd ../../ns-3/ns-3-allinone/ns-3.44
./ns3 configure --enable-examples --enable-tests --enable-python-bindings
```

> **Explanation:**
> - `--enable-examples`: Enable example programs.
> - `--enable-tests`: Enable unit tests.
> - `--enable-python-bindings`: Enable Python bindings for ns-3 modules.

This command will create a `cmake-cache/` folder under `ns-3.44/`.

---

### 3.2 Build the Project

After configuring, start the build process:

```bash
./ns3 build
```

> **Explanation:** This will compile the entire ns-3.44 project, including examples, tests, and Python bindings, based on the previously configured settings.

---

### 3.3 Verify the Installation

To verify if everything was installed correctly, run the basic example:

```bash
cd cmake-cache
./ns3 run hello-simulator
```

If you see the output:

```
Hello Simulator
```

✅ Your ns-3.44 installation was successful!

---

## 4. Appendix: Installation and Build Scripts

### 4.1 `ns3_install.sh`

```bash
#!/bin/bash

set -e
echo "Starting ns-3.44 Environment Setup..."

CURRENT_PATH=$(pwd)
ROOT_DIR="$CURRENT_PATH/../.."
cd "$ROOT_DIR"

echo "Adding toolchain PPA if not already added..."
if ! grep -q "ubuntu-toolchain-r/test" /etc/apt/sources.list /etc/apt/sources.list.d/*; then
    sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
else
    echo "Toolchain PPA already exists, skipping..."
fi

sudo apt-get update

echo "Installing GCC-10 and G++-10..."
sudo apt-get install -y gcc-10 g++-10
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-10 100
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-10 100

echo "Installing developer tools..."
sudo apt-get install -y git vim cmake ninja-build
sudo apt-get install -y python3 python3-dev python3-pip python3-pygraphviz python3-gi python3-gi-cairo ipython3
sudo apt-get install -y gir1.2-gtk-3.0 gir1.2-goocanvas-2.0
sudo apt-get install -y pkg-config qt5-qmake qt5-default qtchooser qtbase5-dev-tools qtbase5-dev libgtk-3-dev
python3 -m pip install --user cppyy==3.1.2

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

echo ""
echo "Verifying installed versions:"
echo -n "gcc version: "; gcc --version | head -n1
echo -n "g++ version: "; g++ --version | head -n1
echo -n "python3 version: "; python3 --version
echo -n "cmake version: "; cmake --version
echo ""

echo "All tools installed and ready for ns-3.44 build!"
```

---

### 4.2 `build_ns3.sh`

```bash
#!/bin/bash

cd ../../ns-3/ns-3-allinone/ns-3.44
./ns3 configure --enable-examples --enable-tests --enable-python-bindings
./ns3 build
```

---

# Conclusion

Following this installation guide, you will have a fully functional **ns-3.44** environment, ready for simulation, development, and research.

