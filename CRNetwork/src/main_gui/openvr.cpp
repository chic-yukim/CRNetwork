#include "main_gui/main_gui.hpp"

#include <imgui.h>

#include <fmt/format.h>

#include <render_pipeline/rpcore/render_pipeline.hpp>

#include "openvr_manager.hpp"
#include "main.h"

void MainGUI::ui_openvr()
{
    if (!app_.openvr_manager_)
        return;

    if (!ImGui::CollapsingHeader("OpenVR"))
        return;

    if (ImGui::Button("Load Objects"))
        app_.openvr_manager_->load_objects();

    if (ImGui::Button("Sync Objects"))
        app_.openvr_manager_->setup_auto_sync_objects();
}
