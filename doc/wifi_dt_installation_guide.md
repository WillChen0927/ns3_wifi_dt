# WiFi Digital Twin Installation Guide

- [WiFi Digital Twin Installation Guide](#wifi-digital-twin-installation-guide)
  - [1. Git Clone](#1-git-clone)
  - [2. Install NS-3](#2-install-ns-3)
      - [Verify NS-3 Installation](#verify-ns-3-installation)
  - [3. Install and Config NETCONF Server](#3-install-and-config-netconf-server)
    - [3.1 Install YANG modules](#31-install-yang-modules)
    - [3.2 Config NETCONF server and YANG models permissions](#32-config-netconf-server-and-yang-models-permissions)
    - [3.3 Clear sysrepo (if needed)](#33-clear-sysrepo-if-needed)
    - [3.4 Verify](#34-verify)
      - [Verify YANG models](#verify-yang-models)
      - [Verify NETCONF Server](#verify-netconf-server)


This document describes how to set up the development environment for this project.  
It includes all necessary dependencies, environment variables, and configurations required to run the project locally or on a server.

## 1. Git Clone
To clone the project source code on github.

```bash
git clone https://github.com/bmw-ece-ntust/ns3_wifi_dt.git
```

## 2. Install NS-3
You can quickly install NS-3 by running below command, or refer to [ns3_installation_guide.md]() for detailed instructions.

```bash
cd ns3_wifi_dt/src/build/scripts/
sudo ./ns3_install.sh
```
You can specify the desired ns-3 version at the end of the script command. If no version is provided, the script will install ns-3.45 by default.
```bash
sudo ./ns3_install.sh 3.44
```
You can visit the [official ns-3 website](https://www.nsnam.org/releases/) to check for the latest version.

#### Verify NS-3 Installation
After building NS-3, you can verify that the installation was successful by running a simple built-in simulation example.  
This confirms that the compiler, Python bindings, and build system are all working correctly.

```bash
cd ~/ns3_wifi_dt/ns-3/ns-3-allinone/ns-3.45/
./ns3 run examples/tutorial/hello-simulator.cc
```

If you see the output:
```txt
Hello Simulator
```

**Your ns-3 installation was successful!**

## 3. Install and Config NETCONF Server

Install the NETCONF-related tools by running the provided installation script. 
```bash
cd ns3_wifi_dt/src/build/scripts/
sudo ./install_netconf.sh
```

### 3.1 Install YANG modules
To install a set of required 3GPP YANG modules, which are essential for the subsequent system setup and configuration. 

```bash
cd ns3_wifi_dt/src/build/scripts/
sudo ./load_yang.sh
```

### 3.2 Config NETCONF server and YANG models permissions

In this step, we import the necessary Netconf configuration files (e.g., access control and server settings) and update the file ownership and permissions for the installed 3GPP YANG modules.

The script sets the owner of each module to the current login user and applies `666` permissions to enable read/write access for subsequent operations.

```bash
sudo ./yang_config.sh
```

### 3.3 Clear sysrepo (if needed)
If you encounter unexpected errors while running the Netconf Server—such as module loading failures or corrupted sysrepo data—you may consider executing the following recovery script. 
This script cleans the sysrepo build environment and reinstalls Netopeer2 to help restore the system to a working state.

```bash
sudo ./sysrepo_clean.sh
```
**Note**: This is not a mandatory step in the installation process and should only be used when server issues occur.
After you run `yang_config.sh` you need to run `load_yang.sh` and `yang_config.sh` again.


### 3.4 Verify

#### Verify YANG models
To verify the YANG modules were installed successfully by executing: 
```bash
sudo sysrepoctl -l
```
This will list all the modules and its permissions currently registered in sysrepo.

#### Verify NETCONF Server
To verify the NETCONF Server were installed successfully by executing:
```bash
sudo ./netopeer_server.sh start
```
This can start the NETCONF server.
And use below command to get the server logs.
```bash
cat /netopeer2-server.log
```
If there is no error in the log, it means the server is started successfully and running normally.