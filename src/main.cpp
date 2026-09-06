#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>
#include <dds/DCPS/WaitSet.h>
#include <dds/DCPS/StaticIncludes.h>
#include <dds/DCPS/JsonValueWriter.h>
#include <dds/DCPS/BuiltInTopicUtils.h>
#include <dds/DCPS/XTypes/DynamicTypeSupport.h>
#include <optional>
#include <thread>
#include <SDL3/SDL.h>
#include <dlfcn.h>
#include <filesystem>
#include <stdio.h>
#include "ryml_std.hpp"
#include "ryml.hpp"

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "main.hpp"
#include "generated.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#define CR_HOST
#include "third_party/cr.h"

const char *SAVED_STATE_PATH = "../state.json";
static Toast toast;

void show_toast(std::string_view message, float duration = 2.0f) {
    std::strncpy(toast.message, message.data(), sizeof(toast.message) - 1);
    toast.duration = duration;
    toast.timer = duration;
    toast.is_visible = true;
}

void render_toast(float deltatime) {
    if (!toast.is_visible) return;

    toast.timer -= deltatime;
    if (toast.timer <= 0) {
        toast.is_visible = false;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 windowPos(io.DisplaySize.x * 0.5f, io.DisplaySize.y - 60.0f);

    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 12));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.15f, 0.15f, 0.17f, 0.95f)); // dark gray, not pure black
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.35f, 0.65f, 1.0f, 0.6f));     // subtle blue border
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));

    ImGui::Begin("##Toast", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoInputs);
    ImGui::TextUnformatted(toast.message);
    ImGui::End();

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(4);
}

void load_ui_state_from_json(UIState& ui_state) {
    std::ifstream file(SAVED_STATE_PATH);
    if (!file.is_open()) return;

    std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    rapidjson::Document doc;
    if (doc.Parse(json.c_str()).HasParseError()) return;

    // Parse Topics
    if (!doc.HasMember("topics") || !doc["topics"].IsArray()) return;
    ui_state.sections.clear();
    for (const auto& root_obj : doc["topics"].GetArray()) {
        if (!root_obj.IsObject()) continue;

        Section s{};

        // Basic fields
        s.id = ui_state.latest_id++;

        if (root_obj.HasMember("name") && root_obj["name"].IsString()) {
            std::strncpy(s.name, root_obj["name"].GetString(), sizeof(s.name) - 1);
            s.name[sizeof(s.name) - 1] = '\0';
        }
        if (root_obj.HasMember("selected_topic") && root_obj["selected_topic"].IsInt()) {
            s.selected_topic = root_obj["selected_topic"].GetInt();
        }
        if (root_obj.HasMember("selected_qos") && root_obj["selected_qos"].IsInt()) {
            s.selected_qos = root_obj["selected_qos"].GetInt();
        }
        if (root_obj.HasMember("file_path") && root_obj["file_path"].IsString()) {
            std::strncpy(s.filePath, root_obj["file_path"].GetString(), sizeof(s.filePath) - 1);
            s.filePath[sizeof(s.filePath) - 1] = '\0';
        }
        if (root_obj.HasMember("topic_filter") && root_obj["topic_filter"].IsString()) {
            std::strncpy(s.topic_filter, root_obj["topic_filter"].GetString(), sizeof(s.topic_filter) - 1);
            s.topic_filter[sizeof(s.topic_filter) - 1] = '\0';
        }
        if (root_obj.HasMember("json_buffer") && root_obj["json_buffer"].IsString()) {
            s.json_buffer = root_obj["json_buffer"].GetString();
        }
        if (root_obj.HasMember("freqs") && root_obj["freqs"].IsNumber()) {
            s.freqs = root_obj["freqs"].GetFloat();
        }

        // QoS
        if (root_obj.HasMember("qos") && root_obj["qos"].IsObject()) {
            const auto& qos = root_obj["qos"];
            if (qos.HasMember("reliability") && qos["reliability"].IsInt()) {
                s.qos.reliability = static_cast<Reliability>(qos["reliability"].GetInt());
            }
            if (qos.HasMember("durability") && qos["durability"].IsInt()) {
                s.qos.durability = static_cast<Durability>(qos["durability"].GetInt());
            }
        }

        // Logs
        if (root_obj.HasMember("logs") && root_obj["logs"].IsObject()) {
            const auto& logs = root_obj["logs"];

            if (logs.HasMember("items") && logs["items"].IsArray()) {
                const auto arr = logs["items"].GetArray();
                for (size_t i = 0; i < arr.Size(); ++i) {
                    const auto &item = arr[i];
                    LogEntry log_entry{};

                    if (item.HasMember("time") && item["time"].IsString()) {
                        std::strncpy(log_entry.time, item["time"].GetString(), sizeof(log_entry.time) - 1);
                        log_entry.time[sizeof(log_entry.time) - 1] = '\0';
                    }
                    if (item.HasMember("message") && item["message"].IsString()) {
                        std::strncpy(log_entry.message, item["message"].GetString(), sizeof(log_entry.message) - 1);
                        log_entry.message[sizeof(log_entry.message) - 1] = '\0';
                    }
                    s.logs.items[i] = log_entry;
                }
            }

            if (logs.HasMember("index") && logs["index"].IsInt()) {
                s.logs.index = logs["index"].GetInt();
            }

            if (logs.HasMember("n") && logs["n"].IsInt()) {
                s.logs.n = logs["n"].GetInt();
            }
        }
        ui_state.workers.try_emplace(s.id);
        ui_state.sections.push_back(std::move(s));
    }
    ui_state.active_section = static_cast<int>(ui_state.sections.size()) - 1;

    if (doc.HasMember("main_scale") && doc["main_scale"].IsFloat()) {
        ui_state.main_scale = doc["main_scale"].GetFloat();
    }
    if (doc.HasMember("active_section") && doc["active_section"].IsInt()) {
        ui_state.active_section = doc["active_section"].GetInt();
    }
}

void save_ui_state_to_json(const UIState &ui_state) {
    rapidjson::Document doc;
    doc.SetObject();
    auto &allocator = doc.GetAllocator();

    rapidjson::Value root_arr(rapidjson::kObjectType);
    root_arr.SetArray();

    for (const auto &s: ui_state.sections) {
        rapidjson::Value root_sections(rapidjson::kObjectType);
        root_sections.AddMember(
            "name",
            rapidjson::Value(s.name, allocator),
            allocator
        );
        root_sections.AddMember("selected_topic", s.selected_topic, allocator);
        root_sections.AddMember("selected_qos", s.selected_qos, allocator);
        root_sections.AddMember("file_path", rapidjson::Value(s.filePath, allocator), allocator);
        root_sections.AddMember("topic_filter", rapidjson::Value(s.topic_filter, allocator), allocator);
        root_sections.AddMember("json_buffer", rapidjson::Value(s.json_buffer.c_str(), allocator), allocator);
        root_sections.AddMember("freqs", s.freqs, allocator);

        rapidjson::Value qos_obj(rapidjson::kObjectType);
        qos_obj.AddMember("reliability", s.qos.reliability, allocator);
        qos_obj.AddMember("durability", s.qos.durability, allocator);
        root_sections.AddMember("qos", qos_obj, allocator);

        rapidjson::Value log_obj(rapidjson::kObjectType);
        {
            rapidjson::Value log_items_arr(rapidjson::kArrayType);
            for (const auto &l: s.logs.items) {
                rapidjson::Value log_item_obj(rapidjson::kObjectType);
                log_item_obj.AddMember("time", rapidjson::Value(l.time, allocator), allocator);
                log_item_obj.AddMember("message", rapidjson::Value(l.message, allocator), allocator);
                log_items_arr.PushBack(log_item_obj, allocator);
            }
            log_obj.AddMember("items", log_items_arr, allocator);
            log_obj.AddMember("index", s.logs.index, allocator);
            log_obj.AddMember("n", s.logs.n, allocator);
        }
        root_sections.AddMember("logs", log_obj, allocator);
        root_arr.PushBack(root_sections, allocator);
    }
    doc.AddMember("topics", root_arr, allocator);
    doc.AddMember("main_scale", ui_state.main_scale, allocator);
    doc.AddMember("active_section", ui_state.active_section, allocator);

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    std::ofstream file(SAVED_STATE_PATH);
    file << buffer.GetString();
}

void init_ui(UIState &ui_state) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return;
    }

    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window *window = SDL_CreateWindow("WOODS", (int)(1280 * ui_state.main_scale), (int)(800 * ui_state.main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
                                                          
    ImGui::StyleColorsDark();

    // Setup scaling
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(ui_state.main_scale);   // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
                                         // style.FontScaleDpi = main_scale;   // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
    io.ConfigDpiScaleFonts = true;     // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true; // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.
    style.WindowRounding = 8.0f;

    io.IniFilename = "../config/imgui.ini";

    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(12, 10);
    style.WindowPadding = ImVec2(14, 14);

    style.FrameBorderSize = 1.0f;
    style.WindowBorderSize = 0.0f;

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    style.FontSizeBase = 16.0f * ui_state.main_scale;
    // io.Fonts->AddFontDefaultBitmap();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    // IM_ASSERT(font != nullptr);

    // Hot reloading
    cr_plugin plugin{};
    plugin.userdata = &ui_state;
    if (!cr_plugin_open(plugin, "./libui.so")) {
        std::cerr << "Failed to opend libui.so" << std::endl;
        exit(69);
    }

    bool done = false;
    while (!done)
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        // [If using SDL_MAIN_USE_CALLBACKS: call ImGui_ImplSDL3_ProcessEvent() from your SDL_AppEvent() function]
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                bool ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;

                // Handle Zoom In / Out
                if (ctrl && (event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS)) ui_state.main_scale += 0.1f;
                if (ctrl && (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS)) ui_state.main_scale -= 0.1f;
                ui_state.main_scale = SDL_clamp(ui_state.main_scale, 0.5f, 3.0f);
                style.FontSizeBase = 16.0f * ui_state.main_scale;

                // Handle Save Layout (CTRL+S)
                if (ctrl && (event.key.key == SDLK_S)) {
                    save_ui_state_to_json(ui_state);
                    show_toast("Layout saved !");
                } 
            }
        }
        // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppIterate() function]
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

        // Call the UI::Draw method
        render_toast(io.DeltaTime);
        cr_plugin_update(plugin);

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call SDL_GL_MakeCurrent(window, gl_context) directly)
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window *backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    cr_plugin_close(plugin);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

DDS::ReliabilityQosPolicyKind eval_reliability_qos(std::string_view str_val)
{
    if (str_val == "reliable")    return DDS::ReliabilityQosPolicyKind::RELIABLE_RELIABILITY_QOS;
    if (str_val == "best_effort") return DDS::ReliabilityQosPolicyKind::BEST_EFFORT_RELIABILITY_QOS;
    std::cerr << "Unhandled reliability qos" << std::endl;
    exit(69);
}

DDS::LivelinessQosPolicyKind eval_liveliness_qos(std::string_view str_val) 
{
    if (str_val == "manual_by_topic") return DDS::LivelinessQosPolicyKind::MANUAL_BY_TOPIC_LIVELINESS_QOS;
    if (str_val == "automatic")       return DDS::LivelinessQosPolicyKind::AUTOMATIC_LIVELINESS_QOS;
    std::cerr << "Unhandled liveliness qos" << std::endl;
    exit(69);
}

DDS::DurabilityQosPolicyKind eval_durability_qos(std::string_view str_val) 
{
    if (str_val == "volatile")   return DDS::DurabilityQosPolicyKind::VOLATILE_DURABILITY_QOS;
    if (str_val == "persistent") return DDS::DurabilityQosPolicyKind::PERSISTENT_DURABILITY_QOS;
    if (str_val == "transient")  return DDS::DurabilityQosPolicyKind::TRANSIENT_DURABILITY_QOS;
    std::cerr << "Unhandled durability qos" << std::endl;
    exit(69);
}

void add_topic(UIState &ui_state, const Topic &t) {
    ui_state.topics.name.push_back(t.name);
    ui_state.topics.idl_filename.push_back(t.idl_filename);
    ui_state.topics.qos.push_back(t.qos);
    ui_state.topics.write.push_back(t.write);
    ui_state.topics.write_string.push_back(t.write_string);
    ui_state.topics.generate_default_json_str.push_back(t.generate_default_json_str);
    ui_state.topics.sub.push_back(t.sub);
}

int main(int argc, char* argv[]) {
    if (argc <= 1)
    {
        // Args is not provided, use default value
        // In the future we might want to extend this 
        // But for now lets keep it simple
        char* arg1 = const_cast<ACE_TCHAR*>("woods");
        char* arg2 = const_cast<ACE_TCHAR*>("-DCPSConfigFile");
        char* arg3 = const_cast<ACE_TCHAR*>("../rtps.ini");
        constexpr size_t ARG_COUNT = 3;
        argc = ARG_COUNT;
        // Let it leak.. Let it leak...
        auto *args = new std::array<ACE_TCHAR*, ARG_COUNT> {arg1, arg2, arg3};
        argv = args->data();
    }

    DDS::DomainParticipantFactory_var dpf = TheParticipantFactoryWithArgs(argc, argv);
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
    const std::string yaml_path = "../topics.yaml";
    std::ifstream ifs(yaml_path, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open file " + yaml_path);
    }

    std::string yaml_content(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>()
    );

    ryml::Tree tree = ryml::parse_in_place(ryml::to_substr(yaml_content));

    UIState ui_state{};
    for (ryml::NodeRef item: tree.rootref()) {
        if (!item.is_map()) throw std::runtime_error("Must be an object");

        Topic topic_entry{};
        participant->get_default_topic_qos(topic_entry.qos);

        if (item.has_child("name")) {
            const auto val = item["name"].val();
            char *ptr = new char[val.len + 1]; std::memcpy(ptr, val.str, val.len); ptr[val.len] = '\0';
            topic_entry.name = ptr;
        }
        if (item.has_child("idlFileName")) {
            const auto val = item["idlFileName"].val();
            char *ptr = new char[val.len + 1]; std::memcpy(ptr, val.str, val.len); ptr[val.len] = '\0';
            topic_entry.idl_filename = ptr;
        }
        if (item.has_child("qos") && item["qos"].is_map()) {
            auto qos_obj = item["qos"];

            if (qos_obj.has_child("reliability") && qos_obj["reliability"].is_map()) {
                const auto val = qos_obj["reliability"]["kind"].val();
                auto v = eval_reliability_qos(std::string_view(val.str, val.len));
                topic_entry.qos.reliability.kind = v;

                if (qos_obj["reliability"].has_child("max_blocking_time_sec")) {
                    const auto val = qos_obj["reliability"]["max_blocking_time_sec"].val();
                    topic_entry.qos.reliability.max_blocking_time.sec = std::stod(std::string(val.str,val.len));
                }
                if (qos_obj["reliability"].has_child("max_blocking_time_nanosec")) {
                    const auto val = qos_obj["reliability"]["max_blocking_time_nanosec"].val();
                    topic_entry.qos.reliability.max_blocking_time.nanosec = std::stod(std::string(val.str,val.len));
                }
            }
            if (qos_obj.has_child("liveliness") && qos_obj["liveliness"].is_map()) {
                const auto val = qos_obj["liveliness"]["kind"].val();
                const auto v = eval_liveliness_qos(std::string_view(val.str, val.len));
                topic_entry.qos.liveliness.kind = v;

                if (qos_obj["liveliness"].has_child("lease_duration_sec")) {
                    const auto val = qos_obj["liveliness"]["lease_duration_sec"].val();
                    topic_entry.qos.liveliness.lease_duration.sec = std::stod(std::string(val.str, val.len));
                }
                if (qos_obj["liveliness"].has_child("lease_duration_nanosec")) {
                    const auto val = qos_obj["liveliness"]["lease_duration_nanosec"].val();
                    topic_entry.qos.liveliness.lease_duration.nanosec = std::stod(std::string(val.str, val.len));
                }
            }
            if (qos_obj.has_child("durability") && qos_obj["durability"].is_map()) {
                const auto val = qos_obj["durability"]["kind"].val();
                auto v = eval_durability_qos(std::string_view(val.str, val.len));
                topic_entry.qos.durability.kind = v;
            }
        }

        // NOTE(wesly): This can be simplified by storing nm directly
        auto nm = topic_entry.idl_filename + std::string("_Message");
        auto tsf = typeSupportFactory.find(nm);
        if (tsf == typeSupportFactory.end()) {
            std::cerr << "ERROR: Cannot find topic name " << nm << ". Make sure idlFileName is exist inside /idl directory!" << std::endl;
            throw std::runtime_error("Invalid topic");
        }

        auto type_name = tsf->second.createTypeSupport();
        if (DDS::RETCODE_OK != type_name->register_type(participant, "")) throw std::runtime_error("register_type failed." );

        auto topic = participant->create_topic(topic_entry.name, type_name->get_type_name(), topic_entry.qos, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
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
        topic_entry.write = std::move(tsf->second.bindWriter(writer));
        topic_entry.write_string = std::move(tsf->second.bindStringWriter(writer));

        topic_entry.generate_default_json_str = std::move(tsf->second.bindGenerator(writer));

        auto sub = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT, nullptr, OpenDDS::DCPS::DEFAULT_STATUS_MASK);
        DDS::DataReaderQos reader_qos;
        sub->get_default_datareader_qos(reader_qos);
        reader_qos.reliability = configured_topic_qos.reliability;
        reader_qos.durability = configured_topic_qos.durability;
        reader_qos.liveliness = configured_topic_qos.liveliness;
        sub->get_default_datareader_qos(reader_qos);
        // topic_entry->begin_read = std::move(tsf->second.bindReader(sub, topic, reader_qos));
        topic_entry.sub = sub;

        std::cout << "Successfully added topic : " << topic_entry.name << std::endl;
        add_topic(ui_state, topic_entry);
    }
    std::cout << "Sucessfully initiate all topics. App is running..." << std::endl;

    load_ui_state_from_json(ui_state);
    init_ui(ui_state);

    participant->delete_contained_entities();
    dpf->delete_participant(participant);
    TheServiceParticipant->shutdown();

    return 0;
}
