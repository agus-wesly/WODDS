// #include "globals.hpp"
#include "ui.h"
#include "imgui.h"
#include <climits>
#include <misc/cpp/imgui_stdlib.h>
#include <chrono>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "nfd.h"

extern "C" void ui_draw(UIState *state) 
{
    UI::draw(*state);
}

static uint16_t global_section_id = 0;
static float g_main_scale = 1.0f;

ImVec2 Vec2(int x, int y) {
    return ImVec2(x*g_main_scale, y*g_main_scale);
}

void log_entry_set_time(std::string &out)
{
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);

    std::tm local_tm = *std::localtime(&tt);

    out.clear();
    out.reserve(8);

    auto append_two_digits = [&out](int value) {
        if (value < 10) out.push_back('0');
        out += std::to_string(value);
    };

    append_two_digits(local_tm.tm_hour);
    out.push_back(':');
    append_two_digits(local_tm.tm_min);
    out.push_back(':');
    append_two_digits(local_tm.tm_sec);
}

void logs_add(Logs &l, std::string_view topic_name, std::string_view reason) {
    LogEntry &e = l.items[l.index];
    log_entry_set_time(e.time);

    // [topic_name] reason
    // =====================================
    e.message.clear();
    e.message.push_back('[');
    e.message.append(topic_name);
    e.message.push_back(']');
    e.message.push_back(' ');
    e.message.append(reason);
    ++l.index;

    if (l.index == MAX_LOG_ITEM) l.index = l.index % MAX_LOG_ITEM;
    if (l.n < MAX_LOG_ITEM) ++l.n;
}

bool format_json_string(std::string &str) 
{
    rapidjson::Document doc;
    if (doc.Parse(str.c_str()).HasParseError()) return false;
    rapidjson::StringBuffer buff;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buff);
    writer.SetIndent(' ', 4);
    doc.Accept(writer);
    str = buff.GetString();
    return true;
}

void stop_worker(Worker &worker)
{
    worker.running = false;
    if (worker.job.joinable())
        worker.job.join();
}

void start_publish(
    Topic &topic,
    Worker &worker,
    Logs &logs,
    QosSettings qos,
    std::string json_data,
    float freqs)
{
    if (worker.running) return;

    const int delay_time_ms = 1000.0f / freqs;

    if (!topic.write_string(json_data.c_str())) {
        logs_add(logs, topic.name, "publish failed. Invalid JSON input data");
        return;
    }

    worker.running = true;
    worker.job = std::thread([&worker, topic, delay_time_ms, json_data, &logs]() mutable {
        while (worker.running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_time_ms));
            if (topic.write_string(json_data.c_str())) {
                logs_add(logs, topic.name, "publish success");
            }
        }
    });
}

void render_sidebar(UIState &ui_state)
{
    if (ImGui::Begin("Sidebar"))
    {
        ImGui::BeginChild("Collections", Vec2(0, 0), true);

        ImGui::Text("Collections");

        float buttonSize = 28;
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - buttonSize*g_main_scale);

        // Add new section
        if (ImGui::Button("+", Vec2(buttonSize, buttonSize)))
        {
            auto newSec = std::make_unique<Section>();
            newSec->name = "Section " + std::to_string(global_section_id++);
            ui_state.sections.push_back(std::move(newSec));
            ui_state.active_section = static_cast<int>(ui_state.sections.size()) - 1;
        }

        ImGui::Separator();

        // List sections
        int to_delete = -1;

        for (int i = 0; i < static_cast<int>(ui_state.sections.size()); i++)
        {
            Section* current_section = ui_state.sections[i].get();
            bool isActive = (i == ui_state.active_section);

            if (ImGui::Selectable(ui_state.sections[i]->name.c_str(), isActive)) ui_state.active_section = i;

            // Right-click context menu to delete
            if (ImGui::BeginPopupContextItem(ui_state.sections[i]->name.c_str()))
            {
                if (ImGui::MenuItem("Delete")) to_delete = i;

                ImGui::EndPopup();
            }
        }

        // Delete after iteration to avoid invalidating indices mid-loop
        if (to_delete != -1)
        {
            auto item_to_delete = ui_state.sections.begin() + to_delete;
            stop_worker(item_to_delete->get()->worker);
            ui_state.sections.erase(item_to_delete);

            if (ui_state.sections.empty()) ui_state.active_section = -1;
            else
                ui_state.active_section = std::clamp(ui_state.active_section, 0,
                        static_cast<int>(ui_state.sections.size()) - 1);
        }

        ImGui::EndChild();
    }
    ImGui::End();
}

void render_publisher(UIState &ui_state)
{
    static float frequency = 0.0f;
    ImGui::SetNextWindowSize(Vec2(600, 900), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Publisher"))
    {
        if (ui_state.sections.empty())
        {
            ImGui::TextDisabled("No section selected.");
            ImGui::End();
            return;
        }

        assert(ui_state.active_section != -1);
        Section &section = *ui_state.sections[ui_state.active_section];
        Topic &topic = *ui_state.topics[section.selected_topic];

        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextUnformatted("WOODS (Writer for OpenDDS)");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::Spacing();

        const float labelWidth = 170.0f;
        const float inputWidth = 585.0f;

        //----------------------------------
        // Selected Topic
        //----------------------------------
        {
            ImGui::Text("Topic QoS");
            ImGui::SameLine(labelWidth*g_main_scale);
            ImGui::SetNextItemWidth(inputWidth);

            const char* previewText = (section.selected_topic >= 0 &&
                    section.selected_topic < (int)ui_state.topics.size())
                ? ui_state.topics[section.selected_topic]->name.c_str()
                : "##none";

            if (ImGui::BeginCombo("##topic", previewText))
            {
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    section.topic_filter[0] = '\0';
                }

                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::InputText("##topic_filter", section.topic_filter, IM_ARRAYSIZE(section.topic_filter));

                ImGuiTextFilter filter(section.topic_filter);
                filter.Build();

                for (size_t i = 0; i < ui_state.topics.size(); ++i)
                {
                    auto &t = *ui_state.topics[i];
                    if (!filter.PassFilter(t.name.c_str()))
                        continue;

                    bool isSelected = (section.selected_topic == i);
                    if (ImGui::Selectable(t.name.c_str(), isSelected))
                        section.selected_topic = i;

                    if (isSelected)
                        ImGui::SetItemDefaultFocus();
                }

                ImGui::EndCombo();
            }

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
        }
        //----------------------------------
        // Selected QoS (Disabled for now)
        //----------------------------------
        if (false) {
            ImGui::SetWindowFontScale(1.25f);
            ImGui::Text("Selected Topic QoS");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Spacing();

            const float policyLabelWidth = labelWidth;
            const float policyInputWidth = inputWidth - policyLabelWidth;

            // --- Reliability ---(
            ImGui::Text("Reliability");
            ImGui::SameLine(policyLabelWidth*g_main_scale);
            ImGui::SetNextItemWidth(policyInputWidth);
            {
                ImGui::Combo("##reliability", (int *)&section.qos.reliability, reliability_opts, IM_ARRAYSIZE(reliability_opts));
            }

            ImGui::Spacing();

            // --- Durability ---
            ImGui::Text("Durability");
            ImGui::SameLine(policyLabelWidth*g_main_scale);
            ImGui::SetNextItemWidth(policyInputWidth);
            {
                ImGui::Combo("##durability", (int *)&section.qos.durability, durability_opts, IM_ARRAYSIZE(durability_opts));
            }

            ImGui::Spacing();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
        }

        //----------------------------------
        // Upload Data
        //----------------------------------
        {
            ImGui::Text("Upload Data");
            ImGui::SameLine(labelWidth*g_main_scale);

            if (ImGui::Button("Browse", Vec2(130, 28))) {
                nfdchar_t *out_path = NULL;
                const nfdchar_t *filter_list = "json"; 
                nfdresult_t result = NFD_OpenDialog( filter_list, NULL, &out_path);

                if (result == NFD_OKAY) 
                {
                    std::snprintf(section.filePath, sizeof(section.filePath), "%s", out_path);
                    std::ifstream file(out_path);
                    if (file.is_open())
                    {
                        std::ostringstream ss;
                        ss << file.rdbuf();
                        section.json_buffer = ss.str();
                    }
                    free(out_path);
                }
                else {
                    printf("Error: %s\n", NFD_GetError() );
                }
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(inputWidth - 140);
            ImGui::InputText("##filepath", section.filePath, sizeof(section.filePath));

            ImGui::Spacing();
            ImGui::Spacing();

            //----------------------------------
            // JSON Editor
            //----------------------------------
            {
                ImGui::Text("JSON Data");
                const int fullWidth = ImGui::GetContentRegionAvail().x;
                const int width = 140; 
                ImGui::SameLine(fullWidth - 2*width*g_main_scale);

                if (ImGui::Button("Format", Vec2(width, 28)))
                {
                    if (!format_json_string(section.json_buffer)) 
                    {
                        logs_add(section.logs, topic.name ,"format failed. Invalid JSON input data");
                    }
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(width - 140);

                if (ImGui::Button("Generate", Vec2(width, 28)))
                {
                    section.json_buffer = topic.generate_default_json_str();
                    format_json_string(section.json_buffer);
                }
                ImGui::Spacing();

                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));

                ImGui::InputTextMultiline(
                        "##jsoneditor",
                        &section.json_buffer,
                        Vec2(-1, 256),
                        ImGuiInputTextFlags_AllowTabInput);

                ImGui::PopStyleColor(3);

            }
            ImGui::Spacing();
            ImGui::Spacing();
        }

        //----------------------------------
        // Publish Buttons
        //----------------------------------
        {
            int fullWidth = ImGui::GetContentRegionAvail().x;
            int setButtonWidth = 140;
            ImGui::SameLine(fullWidth - setButtonWidth*g_main_scale);

            auto &worker = section.worker;
            if (!worker.running) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.75f, 0.50f, 1.0f));
                if (ImGui::Button("Start Publish", Vec2(setButtonWidth, 40)))
                {
                    start_publish(
                            topic, worker, 
                            section.logs, section.qos, std::string(section.json_buffer),
                            section.freqs);
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.05f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.10f, 0.10f, 1.0f));
                if (ImGui::Button("Stop Publish", Vec2(setButtonWidth, 40)))
                {
                    stop_worker(worker);
                    logs_add(section.logs, ui_state.topics[section.selected_topic]->name, "publisher stopped.");
                }
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        //----------------------------------
        // Frequency
        //----------------------------------
        {
            ImGui::Text("Frequency");
            ImGui::SameLine(labelWidth*g_main_scale);

            ImGui::SetNextItemWidth(inputWidth);
            ImGui::InputFloat("##frequency", &section.freqs, 0.0f, 0.0f, "%.1f");
            section.freqs = std::clamp(section.freqs, 0.0f, 100.0f);

            ImGui::SameLine();
            ImGui::Text("Hz");

            ImGui::Dummy(Vec2(0, 0));
            ImGui::SameLine(labelWidth*g_main_scale);

            ImGui::Text("0.0 ... 100.0 Hz");
            ImGui::Spacing();
        }
    }

    ImGui::End();
}

void render_publisher_info(UIState &ui_state)
{
    if (ImGui::Begin("Publisher Info"))
    {
        if (ui_state.sections.empty())
        {
            ImGui::TextDisabled("No section selected.");
            ImGui::End();
            return;
        }

        const Section &section = *ui_state.sections[ui_state.active_section];

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("##LogViewer", Vec2(0, 0), true,
                ImGuiWindowFlags_HorizontalScrollbar);

        int current_log = section.logs.index-1;
        for (size_t n = 0; n < section.logs.n; ++n) 
        {
            if (current_log < 0) current_log = MAX_LOG_ITEM - 1;
            const LogEntry &log = section.logs.items[current_log];
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.67f, 0.72f, 1.0f));
            ImGui::TextUnformatted(log.time.c_str());
            ImGui::PopStyleColor();

            ImGui::SameLine(110*g_main_scale);
            ImGui::TextUnformatted(log.message.c_str());

            current_log--;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();
}

void UI::draw(UIState &ui_state) 
{
    render_sidebar(ui_state);
    render_publisher(ui_state);
    render_publisher_info(ui_state);
}

struct Item
{
    const char* name;
    const char* type;
    const char* value;
};

void render_subscriber(UIState &ui_state)
{
    static float frequency = 0.0f;

    static Item items[] = {
        { "Health",   "int",    "100" },
        { "Speed",    "float",  "5.25" },
        { "IsAlive",  "bool",   "true" },
        { "Username", "string", "Player1" },
    };

    ImGui::SetNextWindowSize(Vec2(600, 900), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Subscriber"))
    {
        if (ui_state.sections.empty())
        {
            ImGui::TextDisabled("No section selected.");
            ImGui::End();
            return;
        }

        Section &section = *ui_state.sections[ui_state.active_section];

        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextUnformatted("OpenDDS C++ Subscriber");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Spacing();
        ImGui::Spacing();

        const float labelWidth = 170.0f;
        const float inputWidth = 585.0f;

        //----------------------------------
        // Selected Topic
        //----------------------------------
        {
            ImGui::Text("Topic QoS");
            ImGui::SameLine(labelWidth*g_main_scale);
            ImGui::SetNextItemWidth(inputWidth);

            const char *topicCStrs[ui_state.topics.size()];
            for (int i = 0; i < ui_state.topics.size(); ++i)
            {
                auto &t = *ui_state.topics[i];
                topicCStrs[i] = t.name.c_str();
            }
            ImGui::Combo("##topic", &section.selected_topic, topicCStrs, static_cast<int>(ui_state.topics.size()));

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
        }
        //----------------------------------
        // TODO: Selected QoS (Disabled for now)
        //----------------------------------
        if (false) {
            ImGui::SetWindowFontScale(1.25f);
            ImGui::Text("Selected Topic QoS");
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Spacing();

            const float policyLabelWidth = labelWidth;
            const float policyInputWidth = inputWidth - policyLabelWidth;

            // --- Reliability ---(
            ImGui::Text("Reliability");
            ImGui::SameLine(policyLabelWidth*g_main_scale);
            ImGui::SetNextItemWidth(policyInputWidth);
            {
                ImGui::Combo("##reliability", (int *)&section.qos.reliability, reliability_opts, IM_ARRAYSIZE(reliability_opts));
            }

            ImGui::Spacing();

            // --- Durability ---
            ImGui::Text("Durability");
            ImGui::SameLine(policyLabelWidth*g_main_scale);
            ImGui::SetNextItemWidth(policyInputWidth);
            {
                ImGui::Combo("##durability", (int *)&section.qos.durability, durability_opts, IM_ARRAYSIZE(durability_opts));
            }

            ImGui::Spacing();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::Separator();
        }

        ImGui::Spacing();
        ImGui::Spacing();

        //----------------------------------
        // Subscribe Buttons
        //----------------------------------
        {
            int fullWidth = ImGui::GetContentRegionAvail().x;
            const int setButtonWidth = 170;
            ImGui::SameLine((fullWidth - setButtonWidth)*g_main_scale);

            auto &worker = section.worker;
            if (!worker.running) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.65f, 0.45f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.40f, 0.75f, 0.50f, 1.0f));
                if (ImGui::Button("Start Subscribe", Vec2(setButtonWidth, 40)))
                {
                    //TODO
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.05f, 0.05f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.10f, 0.10f, 1.0f));
                if (ImGui::Button("Stop Subscribe", Vec2(setButtonWidth, 40)))
                {
                    //TODO
                }
            }
            ImGui::PopStyleColor(2);

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }

        //----------------------------------
        // History
        //----------------------------------
        {
            ImGui::BeginChild("##History", Vec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

            for (int i = 0; i < 10 ; ++i) 
            {
                ImGui::TextUnformatted("10:01:02");
            }

            ImGui::EndChild();
        }

        ImGui::SameLine(labelWidth*g_main_scale);

        //----------------------------------
        // Tables
        //----------------------------------
        {
            ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg;

            if (ImGui::BeginTable("MyTable", 3, flags))
            {
                // Setup headers
                ImGui::TableSetupColumn("Name");
                ImGui::TableSetupColumn("Type");
                ImGui::TableSetupColumn("Value");
                ImGui::TableHeadersRow();

                // Fill rows
                for (int i = 0; i < IM_ARRAYSIZE(items); i++)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(items[i].name);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(items[i].type);

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(items[i].value);
                }

                ImGui::EndTable();
            }    
        }
    }

    ImGui::End();
}
