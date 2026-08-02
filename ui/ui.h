#pragma once
#include "imfilebrowser.h"
#include "../src/main.hpp"

namespace UI
{
    void render_sidebar(UIState &ui_state);

    void render_publisher(UIState &ui_state, ImGui::FileBrowser *file_dialog);
    void render_publisher_info(UIState &ui_state);

    void render_subscriber(UIState &ui_state, ImGui::FileBrowser *file_dialog);
};
