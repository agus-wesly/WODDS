#pragma once

#include <string>
#include <dds/DCPS/XTypes/DynamicTypeSupport.h>
#include <thread>
#include <array>

#define MAX_LOG_ITEM 100

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
    char name[512];

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

struct Topic {
    const char* name;
    const char* idl_filename;
    DDS::TopicQos qos;
    std::function<bool(const void*)> write;
    std::function<bool(const char*)> write_string;
    std::function<std::string()> generate_default_json_str;
    DDS::Subscriber_ptr sub;
};

struct Topics {
    std::vector<const char*> name;
    std::vector<const char*> idl_filename;
    std::vector<DDS::TopicQos> qos;
    std::vector<std::function<bool(const void*)>> write;
    std::vector<std::function<bool(const char*)>> write_string;
    std::vector<std::function<std::string()>> generate_default_json_str;
    std::vector<DDS::Subscriber_ptr> sub;
};

struct UIState {
    Topics topics;
    std::vector<std::unique_ptr<Section>> sections;
    int active_section = -1;
    float main_scale = 1.0f;
};
