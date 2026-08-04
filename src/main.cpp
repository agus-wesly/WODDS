#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/WaitSet.h>
#include <dds/DCPS/StaticIncludes.h>
#include <dds/DCPS/JsonValueWriter.h>
#include <dds/DCPS/BuiltInTopicUtils.h>
#include <dds/DCPS/XTypes/DynamicTypeSupport.h>
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include <optional>
#include <thread>

#include "main.hpp"
#include "generated.hpp"
#include "../ui/main_loop.h"

std::unordered_map<std::string, DDS::ReliabilityQosPolicyKind> RELIABILITY_QOS_MAP = {
    {"reliable", DDS::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS},
    {"best_effort", DDS::ReliabilityQosPolicyKind::BEST_EFFORT_RELIABILITY_QOS},
};

std::unordered_map<std::string, DDS::LivelinessQosPolicyKind> LIVELINESS_QOS_MAP = {
    {"manual_by_topic", DDS::LivelinessQosPolicyKind::MANUAL_BY_TOPIC_LIVELINESS_QOS},
    {"automatic", DDS::LivelinessQosPolicyKind::AUTOMATIC_LIVELINESS_QOS},
};

std::unordered_map<std::string, DDS::DurabilityQosPolicyKind> DURABILITY_QOS_MAP = {
    {"volatile", DDS::DurabilityQosPolicyKind::VOLATILE_DURABILITY_QOS},
    {"persistent", DDS::DurabilityQosPolicyKind::PERSISTENT_DURABILITY_QOS},
    {"transient", DDS::DurabilityQosPolicyKind::TRANSIENT_DURABILITY_QOS},
};

int main(int argc, ACE_TCHAR* argv[]) {
    UIState ui_state{};

    char* arg1 = const_cast<ACE_TCHAR*>("opendds-try");
    char* arg2 = const_cast<ACE_TCHAR*>("-DCPSConfigFile");
    char* arg3 = const_cast<ACE_TCHAR*>("../rtps.ini");

    int args_count = 3;
    std::array<ACE_TCHAR*, 3> args = {arg1, arg2, arg3};

    DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(args_count, args.data());
    auto participant = dpf->create_participant(
            0,
            PARTICIPANT_QOS_DEFAULT,
            nullptr,
            OpenDDS::DCPS::DEFAULT_STATUS_MASK);

    if (!participant) {
        std::cerr << "create_participant failed." << std::endl;
        return 1;
    }

    auto publisher = participant->create_publisher(PUBLISHER_QOS_DEFAULT, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
    if (!publisher) {
        throw std::runtime_error("create_publisher failed." );
    }

    // Setup Topics from config
    std::string path = "../topics.json";
    std::ifstream ifs(path);
    if (!ifs.is_open()) throw std::runtime_error("Could not open file " + path);
    std::stringstream buff;
    buff << ifs.rdbuf();
    auto json_str = buff.str();

    rapidjson::Document doc;
    rapidjson::ParseResult ok = doc.Parse(json_str.c_str());

    if (!ok) throw std::runtime_error(std::string("JSON parse error") + GetParseError_En(ok.Code()));
    if (!doc.IsArray()) throw std::runtime_error("Root JSON must be an array");

    ui_state.topics.reserve(doc.Size());

    for (auto& item: doc.GetArray()) {
        if (!item.IsObject()) throw std::runtime_error("Must be an object");

        auto topic_entry = std::make_unique<Topic>();
        participant->get_default_topic_qos(topic_entry->qos);

        if (item.HasMember("name") && item["name"].IsString()) {
            topic_entry->name = item["name"].GetString();
        }
        if (item.HasMember("idlFileName") && item["idlFileName"].IsString()) {
            topic_entry->idl_filename = item["idlFileName"].GetString();
        }
        if (item.HasMember("qos") && item["qos"].IsObject()) {
            auto qos_obj = item["qos"].GetObject();

            if (qos_obj.HasMember("reliability") && qos_obj["reliability"].IsObject()) {
                auto v = RELIABILITY_QOS_MAP.find(qos_obj["reliability"]["kind"].GetString());
                // TODO(wesly): When error, provide available correct values for each QoS.
                assert (v != RELIABILITY_QOS_MAP.end() && "Invalid reliability kind value.");
                topic_entry->qos.reliability.kind = v->second;

                if (qos_obj["reliability"].HasMember("max_blocking_time_sec") && qos_obj["reliability"]["max_blocking_time_sec"].IsNumber()) {
                    topic_entry->qos.reliability.max_blocking_time.sec = qos_obj["reliability"]["max_blocking_time_sec"].GetUint64();
                }
                if (qos_obj["reliability"].HasMember("max_blocking_time_nanosec") && qos_obj["reliability"]["max_blocking_time_nanosec"].IsNumber()) {
                    topic_entry->qos.reliability.max_blocking_time.nanosec = qos_obj["reliability"]["max_blocking_time_nanosec"].GetFloat();
                }
            }
            if (qos_obj.HasMember("liveliness") && qos_obj["liveliness"].IsObject()) {
                auto v = LIVELINESS_QOS_MAP.find(qos_obj["liveliness"]["kind"].GetString());
                assert(v != LIVELINESS_QOS_MAP.end() && "Invalid liveliness kind value.");
                topic_entry->qos.liveliness.kind = v->second;

                if (qos_obj["liveliness"].HasMember("lease_duration_sec") && qos_obj["liveliness"]["lease_duration_sec"].IsNumber()) {
                    topic_entry->qos.liveliness.lease_duration.sec = qos_obj["liveliness"]["lease_duration_sec"].GetUint64();
                }
                if (qos_obj["liveliness"].HasMember("lease_duration_nanosec") && qos_obj["liveliness"]["lease_duration_nanosec"].IsNumber()) {
                    topic_entry->qos.liveliness.lease_duration.nanosec = qos_obj["liveliness"]["lease_duration_nanosec"].GetUint64();
                }
            }
            if (qos_obj.HasMember("durability") && qos_obj["durability"].IsObject()) {
                auto v = DURABILITY_QOS_MAP.find(qos_obj["durability"]["kind"].GetString());
                assert(v != DURABILITY_QOS_MAP.end() && "Invalid durability kind value");
                topic_entry->qos.durability.kind = v->second;
            }
        }

        // NOTE(wesly): This can be simplified by storing nm directly
        auto nm = topic_entry->idl_filename + std::string("_Message");
        auto tsf = typeSupportFactory.find(nm);
        if (tsf == typeSupportFactory.end()) {
            std::cerr << "ERROR: Cannot find topic name " << nm << ". Make sure idlFileName is exist inside /idl directory!" << std::endl;
            throw std::runtime_error("Invalid topic");
        }

        auto type_name = tsf->second.createTypeSupport();
        if (DDS::RETCODE_OK != type_name->register_type(participant, "")) throw std::runtime_error("register_type failed." );

        auto topic = participant->create_topic(topic_entry->name.c_str(), type_name->get_type_name(), topic_entry->qos, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
        if (!topic) throw std::runtime_error("create_topic failed.");

        DDS::TopicQos configured_topic_qos;
        topic->get_qos(configured_topic_qos);

        DDS::DataWriterQos writer_qos;
        publisher->get_default_datawriter_qos(writer_qos);
        writer_qos.reliability = configured_topic_qos.reliability;
        writer_qos.durability = configured_topic_qos.durability;
        writer_qos.liveliness = configured_topic_qos.liveliness;

        auto writer = publisher->create_datawriter(topic, writer_qos, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
        if (!writer) throw std::runtime_error("create datawriter failed.");
        topic_entry->write = std::move(tsf->second.bindWriter(writer));
        topic_entry->write_string = std::move(tsf->second.bindStringWriter(writer));

        topic_entry->generate_default_json_str = std::move(tsf->second.bindGenerator(writer));

        auto sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
        DDS::DataReaderQos reader_qos;
        sub->get_default_datareader_qos(reader_qos);
        reader_qos.reliability = configured_topic_qos.reliability;
        reader_qos.durability = configured_topic_qos.durability;
        reader_qos.liveliness = configured_topic_qos.liveliness;
        sub->get_default_datareader_qos(reader_qos);
        // topic_entry->begin_read = std::move(tsf->second.bindReader(sub, topic, reader_qos));
        topic_entry->sub = sub;

        std::cout << "Successfully added topic : " << topic_entry->name << std::endl;
        ui_state.topics.push_back(std::move(topic_entry));
    }
    std::cout << "Sucessfully initiate all topics. App is running..." << std::endl;

    UIMainLoop::run(ui_state);

    participant->delete_contained_entities();
    dpf->delete_participant(participant);
    TheServiceParticipant->shutdown();

    return 0;
}
