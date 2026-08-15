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
#include <SDL3/SDL.h>

#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include "main.hpp"
#include "generated.hpp"
#include "globals.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include "ui.h"


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
    g_main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    g_main_scale += 0.25;
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window *window = SDL_CreateWindow("WOODS", (int)(1280 * g_main_scale), (int)(800 * g_main_scale), window_flags);
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
                                                          // io.ConfigViewportsNoAutoMerge = true;
                                                          // io.ConfigViewportsNoTaskBarIcon = true;

                                                          // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup scaling
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(g_main_scale);   // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
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
    style.FontSizeBase = 16.0f * g_main_scale;
    // io.Fonts->AddFontDefaultBitmap();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    // IM_ASSERT(font != nullptr);

    // OUR STATE
    // =================================================================
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
                if (ctrl && (event.key.key == SDLK_EQUALS || event.key.key == SDLK_KP_PLUS)) g_main_scale += 0.1f;
                if (ctrl && (event.key.key == SDLK_MINUS || event.key.key == SDLK_KP_MINUS)) g_main_scale -= 0.1f;
                g_main_scale = SDL_clamp(g_main_scale, 0.5f, 3.0f);

                style.FontSizeBase = 16.0f * g_main_scale;
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

        // Custom Drawer
        UI::render_sidebar(ui_state);

        UI::render_publisher(ui_state);
        UI::render_publisher_info(ui_state);

        // UI::RenderSubscriber(ui_state);

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
    // [If using SDL_MAIN_USE_CALLBACKS: all code below would likely be your SDL_AppQuit() function]
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

int main(int argc, ACE_TCHAR* argv[]) {
    UIState ui_state{};

    // TODO(wesly): move into opendds_init function
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

    init_ui(ui_state);

    participant->delete_contained_entities();
    dpf->delete_participant(participant);
    TheServiceParticipant->shutdown();

    return 0;
}
