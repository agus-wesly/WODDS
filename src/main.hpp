#pragma once

#include <string>
#include <dds/DCPS/XTypes/DynamicTypeSupport.h>
#include <thread>
#include <array>

#define MAX_LOG_ITEM 100

struct Topic {
    const char* name;
    const char* idl_filename;
    DDS::TopicQos qos;
    std::function<bool(const void*)> write;
    std::function<bool(const char*)> write_string;
    // NOTE(wesly): We can optimize by just retrieving reference / pointer to string, and modify it directly
    // instead of doing allocation
    std::function<std::string()> generate_default_json_str;
    // std::function<DDS::DataReader_ptr(std::function<void(const char *)>)> begin_read;
    DDS::Subscriber_ptr sub;
};

enum Reliability {
    BEST_EFFORT = 0,
    RELIABLE
};
static const char *reliability_opts[] = {"BEST_EFFORT", "RELIABLE"};

enum Durability {
    VOLATILE = 0,
    TRANSIENT_LOCAL,
    TRANSIENT,
    PERSISTENT
};
static const char *durability_opts[] = {"VOLATILE", "TRANSIENT_LOCAL", "TRANSIENT", "PERSISTENT"};

struct QosSettings
{
    Reliability reliability = RELIABLE;

    // Durability
    Durability durability = VOLATILE;

    // TODO(wesly): add more QoS here.
};

struct Worker
{
    std::thread job;
    std::atomic<bool> running = false;

    Worker() = default;
    Worker(const Worker &) = delete;
    Worker &operator=(const Worker &) = delete;
};

struct LogEntry
{
    char time[1024];
    char message[1024];
};

struct Logs {
    std::array<LogEntry, MAX_LOG_ITEM> items{};
    int index = 0;
    size_t n = 0;
};

struct Section {
    // NOTE(wesly): Because this is read only, we can just store something like char[1024] instead
    std::string name;

    // Publisher state
    int selected_topic = 0;
    int selected_qos = 0;
    char filePath[256] = "";
    char topic_filter[256] = "";
    std::string json_buffer = "{\n\t\n}";
    float freqs = 1.0f;

    QosSettings qos;
    Logs logs{};
    Worker worker{};
};

struct UIState {
    std::vector<std::unique_ptr<Topic>> topics;
    std::vector<std::unique_ptr<Section>> sections;
    int active_section = -1;
    float main_scale = 1.0f;
};
