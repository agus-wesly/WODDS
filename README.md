# WOODS - Writer a.k.a Publisher for OpenDDS topic.

WOODS is a tool to automatically generate OpenDDS publisher from given idl files and publish data into it.

WOODS support dynamic QoS setting by specifying each topic configuration
inside `topics.json`

## Requirements

* OpenDDS (https://www.opendds.org) and its dependencies (ACE/TAO, and possibly openssl or xerces3)
* SDL3
* CMake
* A compiler and tool chain capable of C++17

## Tested Platforms

* Ubuntu 24.04 (g++ 11.5.0)
* Windows (TBD)

## Building and Run

```sh
git clone https://github.com/agus-wesly/WODDS
cd WOODS

# Debug build
conan install . --output-folder=build-debug  --build=missing -s build_type=Debug            
cmake --preset conan-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j5
cd build-debug
./woods

# Release build
conan install . --output-folder=build-release  --build=missing -s build_type=Release            
cmake --preset conan-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j5
cd build-release
./woods

```

## Configuration

An OpenDDS configuration file named `rtps.ini` is expected in parent directory of
the WOODS executable for governing the local domain participant, otherwise the application will
crash. 

### Important Notes
For now the configuration for Domain Participant is static (at domain 0). In the future this
will be configurable via config file. You can also change it by manually modifying the source
code. Change the line 45 at `src/main.cpp`

## Usage

Upon startup, the application will read the config file named `topics.json`. To add new topic, add new topic object inside
the array. The minimal working example of topic is defined like so : 

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

name: contains the name of the topic that will be created
idlFileName: contains the name of idl file defined inside idl directory (without .idl extension)
qos: contains QoS configuration for the topic.

For now the QoS supported is still limited. More QoS will be added in the future. Here are the list of supported QoS values.

reliability : 
- reliable
- best_effort

liveliness :
- manual_by_topic
- automatic

durability : 
- volatile
- persistent
- transient

Note that in order for topics to be created, the application need to first got rebuild. That said, when you want to add
new topic, you need to add the corresponding .idl file inside idl/ directory. 

After that, you rebuilding the application. It is also recommended to delete the existing src/generated.hpp file to
invalidates previous generated file.

Finally, rebuild the application, by running this command inside the build dir : 
sh
make -j5 
