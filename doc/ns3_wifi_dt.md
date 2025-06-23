# WiFi Digital Twin

## Installation Guide
This section provides step-by-step instructions to set up the development and runtime environment for the project. 
Please make sure all dependencies are installed before proceeding with execution.

### Git Clone
To get the project of source code.
```
git clone https://github.com/bmw-ece-ntust/ns3_wifi_dt.git
```

### Install NS-3
You can quickly install NS-3 by running the provided [installation script](#script-ns3_installsh), or refer to [ns3_installation_guide.md](#ns3_installation_guidemd) for detailed instructions.
```
cd ns3_wifi_dt/src/build/scripts/
sudo ./ns3_install.sh
```
After run the `ns3_install.sh`, run `build_ns3.sh` to build NS-3.
```
./build_ns3.sh
```
After installation, run [`cp_to_ns3_scratch.sh`]() to copy the WiFi Digital Twin simulation program into the ns-3 directory. This allows you to execute the program directly within the ns-3 environment.
(Programs placed in the scratch/ directory are automatically compiled when executed.)
```
./cp_to_ns3_scratch.sh
```

### Install NETCONF
You can install the NETCONF-related tools by running the provided [installation script](#script-install_netconfsh). 
For detailed instructions, please refer to [HERE](#script-install_netconfsh).
```
cd ns3_wifi_dt/src/build/scripts/
sudo ./install_netconf.sh
```

### NETCONF Setting
Some preliminary setup is required to enable the NETCONF server to receive configuration data.

#### 

## Execution Steps
This section describes how to run the simulation program, including the order of scripts, required input formats, and tips for validating the output.

## NS-3 Related Notes

### [ns3_introduction.md](./ns3_introduction.md)

### [ns3_installation_guide.md](./ns3_installation_guide.md)

### [ns3_wifi_use_documentation.md](./ns3_wifi_use_documentation.md)

### [ns-3 Doxygen API documentation](https://www.nsnam.org/docs/release/3.44/doxygen/index.html)

### [ns-3 Wi-Fi Module official documentation (Release 3.44)](https://www.nsnam.org/docs/release/3.44/models/html/wifi.html)

## Main Program Insights (Developer Notes)



### [AP can't recieve traffic](https://hackmd.io/y0cCoDv6R6Cg6Fqxdr8Omg?view#AP-cant-recieve-traffic)


## Appendix: Reference Scripts

This section lists the key scripts required during the execution process. 
Each script is briefly described along with its purpose and usage context. 
These references can help new developers or maintainers understand what needs to be run and when.

### Script: `add_netconf_user.sh`

**Purpose:**
This script creates a dedicated system user named `netconf`, sets a default password, and prepares its SSH environment for secure access. 
It generates an SSH key pair and sets up `authorized_keys` for key-based login. 
This is essential for enabling remote NETCONF access over SSH.

**When to Use:**
Run this script **before starting the NETCONF server** for the first time, or whenever you need to (re)initialize the `netconf` user on a clean system.

**Prerequisites:**

* Must be run with root privileges (via `sudo`)
* SSH server (`sshd`) must be installed.
* This script assumes `/home/netconf/` exists or will be created automatically by `adduser`.

**What it does:**

* Adds a system user `netconf` with no login shell.
* Sets the password to `netconf!`.
* Initializes the `.ssh` directory.
* Generates a DSA key pair.
* Populates `authorized_keys` for key-based SSH authentication.

### Script: `build_ns3.sh`

**Purpose:**
This script configures and builds the ns-3 simulator from source. It enables key features such as example programs, testing framework, and Python bindings, which are often required for development, debugging, or running demonstration scripts.

**When to Use:**
Run this script **after running the script `ns3_install.sh`** and ensuring that all dependencies are installed. This step should be completed **before running any simulation** using ns-3.

**Prerequisites:**

* You have already run the script `ns3_install.sh`.
* Cannot be executed with root privileges (`sudo` or executed as root).


**What it does:**

1. Navigates to the ns-3.44 source directory
2. Configures ns-3 with examples, test suites, and Python bindings enabled
3. Builds the project using `./ns3 build`

### Script: `cp_to_ns3_scratch.sh`

**Purpose:**
This script copies custom simulation source files into the `scratch` directory of the ns-3 environment. It ensures that user-defined simulation programs are properly placed so they can be compiled and run directly using the ns-3 build system.

**When to Use:**
Run this script **after building ns-3 by running the script `ns3_install.sh` and `build_ns3.sh`** and **before executing your simulation**, especially if your simulation code is stored outside the ns-3 source tree.

**Prerequisites:**

* You have already run the script `ns3_install.sh` and `build_ns3.sh`.
* Simulation source files are located in `../../ns3_sim_files/`

**What it does:**

1. Checks if the `scratch` folder exists in the specified ns-3 directory
2. If not found, prints an error and exits
3. If found, copies all files from `../../ns3_sim_files/` to the `scratch/` directory
4. Prints confirmation messages

### Script: `install_netconf.sh`

**Purpose:**
This script installs all required dependencies and builds the necessary libraries for setting up a NETCONF server environment, including `libssh`, `cJSON`, `libcurl`, `libyang`, `sysrepo`, `libnetconf2`, and `Netopeer2`. It also installs system packages and configures shared libraries.

**When to Use:**
Run this script **before building or running any NETCONF-related modules** in your project. It should be executed on a clean environment or any new system that will act as a NETCONF server.

**Prerequisites:**

* Must be run with root privileges (via `sudo`)
* Internet connection is required to clone the GitHub repositories
* OS must support `apt-get` (e.g., Ubuntu or Debian-based distributions)

**What it does:**

1. Installs system-level prerequisite packages such as `cmake`, `libssl-dev`, `protobuf-c-compiler`, etc.
2. Clones and builds the following libraries from source:

   * `libssh` (for SSH transport)
   * `cJSON` (for lightweight JSON handling)
   * `libcurl` (for HTTP communication)
   * `libyang` (YANG parsing)
   * `sysrepo` (data store and configuration engine)
   * `libnetconf2` (NETCONF protocol implementation)
   * `Netopeer2` (the NETCONF server)
3. Applies a patch to increase data change timeout in `sysrepo`
4. Calls `ldconfig` to update the system’s dynamic linker


### Script: `load_yang.sh`

**Purpose:**
This script installs a predefined list of 3GPP YANG modules into the `sysrepo` datastore. These YANG models are necessary for the NETCONF server to understand and manage 5G-related network configurations.

**When to Use:**
Run this script **after sysrepo and netopeer2 are installed by running script `install_netconf.sh`**, and **before starting the NETCONF server**. It should also be re-run whenever YANG models are updated or added to the project.

**Prerequisites:**

* Must be run with root privileges (via `sudo`)
* You have already install the script `install_netconf.sh`.

**What it does:**

1. Initializes path variables based on the script’s current directory
2. Defines an array of YANG file names corresponding to 3GPP modules
3. Iterates through the list and installs each module using `sysrepoctl -i`
4. Installs modules such as:

   * `_3gpp-common-managed-element.yang`
   * `_3gpp-nr-nrm-gnbdufunction.yang`
   * `_3gpp-nr-nrm-nrcelldu.yang`
   * ... and others related to measurements, subscriptions, and 5G NR


### Script: `netopeer_server.sh`

**Purpose:**
This script is a simple control interface to start or stop the `netopeer2-server`, which is the NETCONF server implementation used in this project. It allows developers to quickly bring the server online or terminate it as needed during testing or simulation.

**When to Use:**
Use this script to **manually start or stop the NETCONF server** before running simulations that rely on NETCONF-based configuration (e.g., setting up APs via YANG models).

**Prerequisites:**

* `netopeer2-server` must be installed and in the system’s executable path
* Must be run with root privileges (via `sudo`)

**What it does:**

1. Checks if the user has passed a command-line argument (`start` or `stop`)
2. If `start` is provided:

   * Launches the `netopeer2-server` in daemon mode with verbosity level 2
   * Redirects logs to `/netopeer2-server.log`
3. If `stop` is provided:

   * Terminates all running `netopeer2-server` processes using `pidof` and `kill -9`

#### **Sample Usage:**

```bash
./netopeer.sh start     # Start NETCONF server
./netopeer.sh stop      # Stop NETCONF server
```

#### **Check Log**:

```bash
cat /netopeer2-server.log
```


### Script: `ns3_install.sh`

**Purpose:**
This script automates the installation of development tools and dependencies required for building and running `ns-3.44`. It also sets up the workspace by downloading the `ns-3-allinone` environment and retrieving the specific ns-3 version source code.

**When to Use:**
Run this script **when setting up a new machine or virtual environment** for ns-3 development, especially if you are targeting version 3.44. It is designed to work with Ubuntu-based systems.

**Prerequisites:**

* Must be run with root privileges (via `sudo`)

**What it does:**

1. Adds the Ubuntu Toolchain PPA if not already present (for GCC-10)
2. Installs `gcc-10`, `g++-10`, and sets them as default using `update-alternatives`
3. Installs common developer tools (`git`, `cmake`, `ninja`, `pip`, GTK, Qt, etc.)
4. Installs Python packages including `cppyy==3.1.2` for Python-C++ integration
5. Creates the `ns-3` working directory if not present
6. Clones the `ns-3-allinone` repository (skips if already cloned)
7. Downloads the `ns-3.44` source using the official `download.py` script
8. Prints version information of all critical tools (gcc, g++, python3, cmake)


### Script: `sysrepo_clean.sh`

**Purpose:**
This script performs a partial reinstallation of the Netconf environment. It **cleans up the sysrepo build artifacts** and **reinstalls the Netopeer2 server**, which is the NETCONF server implementation used for handling YANG-based configurations.

**When to Use:**
Use this script if:

* You encounter build issues with `sysrepo` and want to clean and reinstall related components
* You want to ensure that `netopeer2-server` is freshly reinstalled after changes in YANG models or library dependencies
* You have previously compiled the environment and want to rerun only critical portions

**Prerequisites:**

* The Netconf toolchain must have already been cloned and built once
* Directory structure is assumed to follow the pattern `netconf/sysrepo/build/` and `netconf/Netopeer2/build/` relative to the script location
* Must be run with root privileges (via `sudo`)

**What it does:**

1. Navigates to the sysrepo build directory
2. Executes `make sr_clean` to clean up sysrepo-related artifacts
3. Navigates to the Netopeer2 build directory
4. Reinstalls `netopeer2-server` by running `sudo make install`
5. Returns to the root directory
6. Prints `"SCRIPT COMPLETED."` to indicate successful execution

**Note:** If you make changes to the YANG models, consider rerunning `sysrepoctl -i` commands as well to reinstall affected modules.


### Script: `yang_config.sh`

**Purpose:**
This script imports essential NACM and Netconf server configuration files into the sysrepo running datastore, and sets appropriate ownership and permissions for all 3GPP-related YANG modules to ensure they are accessible by the current user.

**When to Use:**
Use this script **after installing the sysrepo and YANG modules**, to:

* Configure access control for NETCONF (NACM)
* Enable IPv6 settings for the NETCONF server
* Grant read/write permissions to YANG modules for testing or development under your user account

**Prerequisites:**

* Sysrepo and its dependencies must be installed
* All required YANG modules should already be loaded into sysrepo
* Must be run with root privileges (via `sudo`)

**What it does:**

1. Navigates to the configuration directory: `../config_netconf`
2. Imports two configuration XML files into sysrepo using `sysrepocfg`
   * `nacm_config.xml`: NACM (NETCONF Access Control Model) settings
   * `netconf_server_ipv6.xml`: IPv6 configuration for the NETCONF server
3. Uses `sysrepoctl` to change file ownership and permissions (set to 666) for all 3GPP YANG modules
4. Echoes `SCRIPT COMPLETED.` when finished


**Note:**
Setting permission to `666` allows full read and write access for all users, which is suitable for development but **not recommended in production** environments.
