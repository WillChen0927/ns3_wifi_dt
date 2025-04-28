# ns-3 Introduction

- [ns-3 Introduction](#ns-3-introduction)
  - [Reference](#reference)
  - [1. Introduction to NS-3](#1-introduction-to-ns-3)
    - [1.1 NS-3 Conceptual Overview](#11-ns-3-conceptual-overview)
    - [1.2 Key Abstract Concepts](#12-key-abstract-concepts)
      - [Node](#node)
      - [Application](#application)
      - [Channel](#channel)
      - [Net Device](#net-device)
      - [Topology Helpers](#topology-helpers)
    - [1.3 Simulation Time and Event System](#13-simulation-time-and-event-system)
    - [1.4 Typical ns-3 Script Structure](#14-typical-ns-3-script-structure)
    - [1.5 Logging Module](#15-logging-module)
      - [Log Levels](#log-levels)
      - [Log Prefixes](#log-prefixes)
      - [How to Use Logging](#how-to-use-logging)
    - [1.6 Tracing and Pcap Capture](#16-tracing-and-pcap-capture)


## Reference
- **NS-3 Official Tutorial**  
  [https://www.nsnam.org/docs/tutorial/html/](https://www.nsnam.org/docs/tutorial/html/)

---

## 1. Introduction to NS-3

### 1.1 NS-3 Conceptual Overview

- **ns-3** is an open-source network simulator for research and education.
- It is developed in **C++** and optionally **Python**, designed as a set of libraries that can be combined with other external software libraries.

![image](https://hackmd.io/_uploads/rkECDmPJA.png)

---

### 1.2 Key Abstract Concepts

#### Node
- In ns-3, a **Node** is an abstract basic computing device.
- Represented in C++ by the class `Node`.
- A Node is like a real-world computer; you add functionalities like applications, protocol stacks, and network devices.
- In the main function of an ns-3 script, we usually create nodes using `NodeContainer`.

#### Application
- In ns-3, an **Application** is the abstraction for a user program that generates some network activity.
- Represented by the C++ class `Application`.
- Developers can specialize the `Application` class to create custom applications.
- In scripts, you typically use helpers (e.g., `UdpEchoServerHelper`) to install applications into nodes via `ApplicationContainer`.

#### Channel
- A **Channel** in ns-3 represents a communication medium that connects Nodes.
- Represented by the C++ class `Channel`.
- Examples: `CsmaChannel`, `PointToPointChannel`, `WifiChannel`.
- Some Channel types can model complex behaviors like switches or wireless propagation with obstacles.
- You can configure and install channels via helpers (e.g., `PointToPointHelper`).

#### Net Device
- A **NetDevice** represents a network interface card that connects a Node to a Channel.
- Represented by the C++ class `NetDevice`.
- Examples: `PointToPointNetDevice`, `WifiNetDevice`.
- A Node can have multiple NetDevices and connect to multiple Channels.
- Typically created and installed via `<specific device helper>.Install(nodes)`, stored in a `NetDeviceContainer`.

#### Topology Helpers
- Topology Helpers simplify repetitive tasks like:
  - Creating nodes and devices
  - Connecting devices to channels
  - Installing internet stacks
  - Assigning IP addresses
- Common helpers: `NodeContainer`, `NetDeviceContainer`, `ApplicationContainer`, `PointToPointHelper`.

![image](https://hackmd.io/_uploads/r184pnqy0.png)

---

### 1.3 Simulation Time and Event System

- **ns-3** is a **discrete-event network simulator**.
- All actions (e.g., packet transmission, application events) are handled as **scheduled events** based on simulation time.
- Important APIs:
  - `Simulator::Schedule(Time, Function)` — Schedule a function to execute at a specific simulation time.
  - `Simulator::Run()` — Start executing scheduled events.
  - `Simulator::Stop()` — Stop the simulation at a specified time.
  - `Simulator::Destroy()` — Free resources and clean up after simulation ends.
- Events are processed one by one, **strictly based on event timestamp order**, not real-world clock time.

---

### 1.4 Typical ns-3 Script Structure

Most ns-3 scripts follow a similar flow:

1. Create nodes
2. Create and configure channels
3. Install NetDevices and connect to channels
4. Install Internet Stack (if using IP protocols)
5. Assign IP addresses
6. Install Applications
7. Set simulation parameters (e.g., logging, tracing)
8. Run the simulation
9. Clean up

---

### 1.5 Logging Module

- Like many large systems, ns-3 supports a powerful **logging facility**.
- You can selectively enable/disable logging for debugging, performance analysis, or troubleshooting.
- Logging output can get messy, so ns-3 provides multi-level and component-based control.

#### Log Levels
ns-3 defines seven log levels, ordered from low verbosity to high:

```cpp
LOG_ERROR    — Log critical error messages (NS_LOG_ERROR);
LOG_WARN     — Log warning messages (NS_LOG_WARN);
LOG_DEBUG    — Log debugging information (NS_LOG_DEBUG);
LOG_INFO     — Log informational progress messages (NS_LOG_INFO);
LOG_FUNCTION — Log every function call (NS_LOG_FUNCTION, NS_LOG_FUNCTION_NOARGS);
LOG_LOGIC    — Log logical decisions inside functions (NS_LOG_LOGIC);
LOG_ALL      — Log all above.
```

- You can enable one or multiple levels.
- Alternatively, you can use `LOG_LEVEL_<type>` macros to enable a level and all levels above it.
- Special unconditional log:
```cpp
NS_LOG_UNCOND — Always display the message, no matter log level settings.
```

#### Log Prefixes
You can prepend useful information to log outputs:
```cpp
LOG_PREFIX_FUNC   — Function name
LOG_PREFIX_TIME   — Simulation time
LOG_PREFIX_NODE   — Node ID
LOG_PREFIX_LEVEL  — Log level
LOG_PREFIX_ALL    — All of the above
```

Prefixes and log levels can be combined for richer outputs.

#### How to Use Logging
- First, define a log component **before the `main()` function**:
```cpp
NS_LOG_COMPONENT_DEFINE("MyComponent");
```
- Then, enable logging using:
```cpp
LogComponentEnable("MyComponent", LOG_LEVEL_INFO);
LogComponentEnableAll(LOG_LEVEL_DEBUG);
```
- You can then use logging macros inside the code:
```cpp
NS_LOG_INFO("Starting simulation...");
NS_LOG_DEBUG("Debugging details...");
```

---

### 1.6 Tracing and Pcap Capture

Besides logging for debugging, ns-3 provides **tracing mechanisms** to record simulation data for analysis:

- **Tracing** differs from **Logging**:
  - **Logging** is mainly for developers.
  - **Tracing** is for analyzing simulation outputs (e.g., network traffic, throughput).
- Types of tracing:
  - **ASCII tracing** — Write human-readable simulation traces into `.tr` files.
  - **Pcap tracing** — Capture network packets into `.pcap` files (Wireshark-readable).
- Example:
```cpp
pointToPoint.EnablePcapAll("output-filename");
```
- Tracing is essential if you want to study:
  - Packet exchanges
  - Application-level performance
  - Link layer statistics

