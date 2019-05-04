#include "main_gui/main_gui.hpp"

#include <imgui.h>

#include <fmt/format.h>

#include <render_pipeline/rppanda/showbase/showbase.hpp>
#include <render_pipeline/rpcore/pluginbase/manager.hpp>
#include <render_pipeline/rpcore/util/primitives.hpp>
#include <render_pipeline/rpcore/render_pipeline.hpp>

#include <crsf/CRModel/TActorObject.h>

#include "main.h"
#include "openvr_manager.hpp"

MainGUI::MainGUI(MainApp& app) : app_(app)
{
    rppanda::Messenger::get_global_instance()->send(
        "imgui-setup-context",
        EventParameter(new rppanda::FunctionalTask([this](rppanda::FunctionalTask* task) {
            ImGui::SetCurrentContext(std::static_pointer_cast<ImGuiContext>(task->get_user_data()).get());
            accept("imgui-new-frame", [this](const Event*) { on_imgui_new_frame(); });
            return AsyncTask::DS_done;
        }, "MainApp::setup-imgui"))
    );
}

MainGUI::~MainGUI() = default;

void MainGUI::on_imgui_new_frame()
{
    ImGui::Begin(CRMODULE_ID_STRING, &is_open_);

    if (ImGui::Button("Reset User Pose"))
        app_.ResetUserPose();

    if (ImGui::Button("Reset Controller Match"))
        app_.ResetControllerMatch();

    if (ImGui::Button("Vibrate Controller"))
    {
        app_.VibrateController(0);
    }

    ui_openvr();

    ImGui::End();
}
