# WOODS

Writer (publisher) for OpenDDS Topics

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows-brightgreen.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)

WOODS is a tool that automatically generates OpenDDS publishers from IDL files. 

Right now OpenDDS itself dont really have official tools for mock publishing. That's why I create WOODS.  

It provides a user-friendly UI to dynamically configure QoS settings and publish data to OpenDDS topics in real-time.

## Features

- 🚀 **Automatic Publisher Generation** – Generate OpenDDS publishers from IDL files automatically
- ⚙️ **Dynamic QoS Configuration** – Configure Quality of Service settings per topic via `topics.json`
- 🎨 **Interactive UI** – Intuitive graphical interface for managing multiple topics
- 📊 **Real-time Publishing** – Publish messages with configurable frequency

## 📋 Prerequisites

- **OpenDDS** (https://www.opendds.org) with dependencies:
  - ACE/TAO
  - OpenSSL or Xerces3
- **SDL3** – Graphics library for UI
- **CMake** ≥ 3.x – Build system
- **C++17 Compatible Compiler** – GCC 11.5.0+ or MSVC 2019+
- **Conan** – Package manager

## 🖥️ Tested Platforms

| OS | Compiler | Status |
|---|---|---|
| Ubuntu 24.04 | GCC 11.5.0 | ✅ Tested |
| Windows | MSVC | 🚧 In Progress |

## 🚀 Quick Start

### Build from Source

```bash
# Clone the repository
git clone https://github.com/agus-wesly/WOODS
cd WOODS

conan install . --output-folder=build --build=missing -s build_type=Release
cmake --preset conan-release -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/woods

```

## ⚙️ Configuration

### Domain Participant Configuration

WOODS requires an OpenDDS configuration file named **`rtps.ini`** in the parent directory of the executable. This file governs the local domain participant settings.

```ini
# Example rtps.ini
[common]
DCPSDefaultDiscovery=DEFAULT_RTPS
DCPSGlobalTransportConfig=$file
DCPSLogLevel=notice
DCPSLogFile=opendds_debug.log

[transport/the_rtps_transport]
transport_type=rtps_udp
```

**Note:** Currently, the domain participant is hardcoded to domain 0. To change this, modify line 45 in `src/main.cpp`:

```cpp
// src/main.cpp (line 45)
DomainParticipantFactory::get_instance()->create_participant(
    0,  // <-- Change domain ID here
    ...
);
```

This will be made configurable via configuration file in a future release.

### Topics Configuration

WOODS reads a `topics.json` file at startup to configure topics. Each topic can be independently configured with custom QoS settings.

#### Configuration File Format

```json
[
  {
    "name": "Foo",
    "idlFileName": "Foo",
    "qos": {
      "reliability": {
        "kind": "best_effort"
      },
      "liveliness": {
        "kind": "manual_by_topic"
      },
      "durability": {
        "kind": "persistent"
      }
    }
  }
]
```

#### Configuration Parameters

| Parameter | Type | Description |
|---|---|---|
| `name` | string | Topic name (must match IDL type name) |
| `idlFileName` | string | IDL file name without `.idl` extension; file must exist in `idl/` directory |
| `qos` | object | Quality of Service settings for the topic |

#### Supported QoS Settings

| QoS Policy | Supported Values | Default |
|---|---|---|
| **reliability** | `reliable`, `best_effort` | `best_effort` |
| **liveliness** | `manual_by_topic`, `automatic` | `automatic` |
| **durability** | `volatile`, `persistent`, `transient` | `volatile` |

## 📖 Usage Guide

### Adding a New Topic

1. **Create an IDL File**  
   Add your IDL type definition to the `idl/` directory:
   
   ```idl
   // idl/MyMessage.idl
   module messages {
     struct MyMessage {
       unsigned long id;
       string data;
     };
   };
   ```

2. **Update `topics.json`**  
   Add a new topic entry with desired QoS settings

3. **Rebuild the Application**  
   ```bash
   # Clean previous generated files (optional but recommended)
   rm -f src/generated.hpp
   
   # Rebuild
   cd build
   cmake --build . -j$(nproc)
   ```

4. **Run WOODS**  
   ```bash
   ./woods
   ```

## 🔄 Workflow Summary

```
1. Define IDL types in idl/
        ↓
2. Configure topics in topics.json
        ↓
3. Rebuild application (cmake --build ...)
        ↓
4. Run WOODS executable
        ↓
5. Use UI to publish messages
```

## 📝 Future Improvements

- [ ] Support for additional QoS policies
- [ ] Configurable domain participant via config file
- [ ] Export .json data

## 🤝 Contributing

(TBD)

## 📄 License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.

## 📧 Support

For issues, questions, or suggestions:
- 📝 Open an [Issue](https://github.com/agus-wesly/WOODS/issues)
- 💬 Start a [Discussion](https://github.com/agus-wesly/WOODS/discussions)

---
