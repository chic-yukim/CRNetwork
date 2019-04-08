/**
* Coexistence Reality Software Framework (CRSF)
* Copyright (c) Center of Human-centered Interaction for Coexistence. All rights reserved.
* See the LICENSE.md file for more details.
*/

#include "main.h"

#include <spdlog/logger.h>
#include <fmt/format.h>

#include <regex>
#include <unordered_set>

#include <render_pipeline/rppanda/showbase/messenger.hpp>
#include <render_pipeline/rppanda/showbase/loader.hpp>
#include <render_pipeline/rppanda/showbase/showbase.hpp>
#include <render_pipeline/rpcore/pluginbase/day_manager.hpp>
#include <render_pipeline/rpcore/pluginbase/manager.hpp>
#include <render_pipeline/rpcore/util/rpmaterial.hpp>
#include <render_pipeline/rpcore/render_pipeline.hpp>
#include <render_pipeline/rpcore/globals.hpp>
#include <render_pipeline/rpcore/util/primitives.hpp>

#include <crsf/CoexistenceInterface/TDynamicStageMemory.h>
#include <crsf/CoexistenceInterface/TPointMemoryObject.h>
#include <crsf/CoexistenceInterface/TSoundMemoryObject.h>
#include <crsf/CREngine/TDynamicModuleManager.h>
#include <crsf/CRModel/TGraphicModel.h>
#include <crsf/CRModel/TWorld.h>
#include <crsf/CRModel/TCube.h>
#include <crsf/RemoteWorldInterface/TNetworkManager.h>
#include <crsf/RenderingEngine/TGraphicRenderEngine.h>
#include <crsf/RenderingEngine/TAudioRenderEngine.h>

#include <rpplugins/openvr/plugin.hpp>

#include "main_gui/main_gui.hpp"
#include "openvr_manager.hpp"
#include "user.h"

CRSEEDLIB_MODULE_CREATOR(HandsTogether);

spdlog::logger* global_logger;

HandsTogether::HandsTogether() : crsf::TDynamicModuleInterface(CRMODULE_ID_STRING)
{
    global_logger = m_logger.get();

    dsm_ = crsf::TDynamicStageMemory::GetInstance();
    rendering_engine_ = crsf::TGraphicRenderEngine::GetInstance();
    network_manager_ = crsf::TNetworkManager::GetInstance();

    m_pControllerID = { 0, 0 };
}

HandsTogether::~HandsTogether() = default;

void HandsTogether::OnLoad()
{
    cr_world_ = rendering_engine_->GetWorld();
}

void HandsTogether::OnStart()
{
    rendering_engine_->SetWindowTitle(CRMODULE_ID_STRING);

    openvr_manager_ = std::make_unique<OpenVRManager>(*rendering_engine_->GetRenderPipeline());
    if (openvr_manager_->is_available())
    {
        openvr_manager_->load_objects();
        openvr_manager_->setup_auto_sync_objects();
    }
    else
    {
        openvr_manager_.reset();
    }

    // setup (mouse) controller
    rendering_engine_->EnableControl();
    if (!openvr_manager_)
        rendering_engine_->SetControllerInitialPosHpr(LVecBase3(0, -4.0f, 2.5f), LVecBase3(0, -24, 0));
    rendering_engine_->ResetControllerInitial();

    SetupNetwork();

    do_method_later(1.0f, [this](rppanda::FunctionalTask*) {
        if (!network_manager_->IsConnected())
        {
            m_logger->debug("Waiting for establishing network connection ...");
            return AsyncTask::DS_again;
        }

        MakeScene();
        SetupLocalUser();
        SetupMicrophone();

        scene_setup_finished_ = true;
        return AsyncTask::DS_done;
    }, "ExampleInterfaceNetworkd::MakeScene");

    SetupEvent();

    main_gui_ = std::make_unique<MainGUI>(*this);
}

void HandsTogether::OnExit()
{
    remove_all_tasks();

    m_users.clear();
    crsf::TAudioRenderEngine::GetInstance()->Deactivate();
    network_manager_->Exit();
}

void HandsTogether::SetupNetwork()
{
    SetupNetworkReceiver();
    network_manager_->Init();
}

UserEntity* HandsTogether::GetUser(unsigned int system_index)
{
    if (m_users.find(system_index) != m_users.end())
        return m_users[system_index].get();

    if (system_index == dsm_->GetSystemIndex())
        return m_users.emplace(system_index, std::make_unique<LocalUserEntity>(system_index)).first->second.get();
    else
        return m_users.emplace(system_index, std::make_unique<RemoteUserEntity>(system_index)).first->second.get();
}

void HandsTogether::ResetUserPose()
{
    LVecBase3f head_position;
    LVecBase3f head_hpr;
    head_position.set(
        m_property.get("user.user" + std::to_string(dsm_->GetSystemIndex()) + ".position.x", 1.5f),
        m_property.get("user.user" + std::to_string(dsm_->GetSystemIndex()) + ".position.y", 0.0f),
        m_property.get("user.user" + std::to_string(dsm_->GetSystemIndex()) + ".position.z", 1.5f));

    head_hpr = rpcore::Globals::base->get_cam().get_hpr();
    head_hpr.set_x(m_property.get("user.user" + std::to_string(dsm_->GetSystemIndex()) + ".hpr.x", 90.0f));

    LMatrix4f mat;
    compose_matrix(mat, LVecBase3f(1), LVecBase3f(0), head_hpr, head_position * rendering_engine_->GetRenderingScaleFactor());

    rpcore::Globals::base->get_camera().set_mat(invert(rpcore::Globals::base->get_cam().get_mat()) * mat);

    if (openvr_manager_)
    {
        compose_matrix(mat, LVecBase3f(1), LVecBase3f(0), head_hpr, head_position);
        openvr_manager_->get_object_root()->SetMatrix(invert(openvr_manager_->get_plugin()->get_device_node(0).get_mat()) * mat);
    }
}

void HandsTogether::ResetControllerMatch()
{
    if (!openvr_manager_)
        return;

    int hand_index = 0;
    for (int k = vr::k_unTrackedDeviceIndex_Hmd + 1; k < vr::k_unMaxTrackedDeviceCount; ++k)
    {
        if (!openvr_manager_->get_plugin()->is_tracked_device_connected(k))
            continue;

        switch (openvr_manager_->get_plugin()->get_tracked_device_class(k))
        {
        case vr::TrackedDeviceClass_Controller:
        {
            m_pControllerID[hand_index] = k;
            ++hand_index;
            break;
        }
        default:
            break;
        }

        if (hand_index == 2)
            break;
    }

    m_logger->info("Found {} controllers", hand_index);
}

void HandsTogether::MakeScene()
{
    // Set cube graphics
    crsf::TCube::Parameters params;
    params.m_strName = "test_Cube";
    params.m_vec3Origin = LVecBase3(0.0f, 0.0f, 0.87f);
    params.m_vec3HalfExtent = LVecBase3(0.05f);
    auto pCube = crsf::CreateObject<crsf::TCube>(params);
    m_pCube = pCube.get();
    auto m_pCubeGraphicModel = pCube->CreateGraphicModel();

    rpcore::RPMaterial mat(m_pCubeGraphicModel->GetMaterial());
    mat.set_roughness(1.0f);
    mat.set_base_color(LColorf(1.0, 0.2, 0.2, 1));

    cr_world_->AddWorldObject(pCube);

    add_task([this](rppanda::FunctionalTask*) {
        CheckCubeState();
        return AsyncTask::DS_cont;
    }, "check_cube_state");
}

void HandsTogether::SetupLocalUser()
{
    auto user = GetUser(dsm_->GetSystemIndex());
    ResetUserPose();

    if (!openvr_manager_)
        return;

    ResetControllerMatch();

    user->add_task([this, user](rppanda::FunctionalTask*) {
        auto world = rendering_engine_->GetWorld();

        user->SetHeadMatrix(openvr_manager_->get_object(0)->GetMatrix(world));

        if (m_pControllerID[0] != 0)
        {
            if (auto object = openvr_manager_->get_object(m_pControllerID[0]))
            {
                user->SetControllerMatrix(0, object->GetMatrix(world));
            }
        }

        if (m_pControllerID[1] != 0)
        {
            if (auto object = openvr_manager_->get_object(m_pControllerID[1]))
            {
                user->SetControllerMatrix(1, object->GetMatrix(world));
            }
        }

        return AsyncTask::DS_cont;
    }, "update_openvr_to_user", -5);
}

void HandsTogether::SetupMicrophone()
{
    auto m_pAudioRenderingEngine = crsf::TAudioRenderEngine::GetInstance();
    m_pAudioRenderingEngine->Setup();
    m_pAudioRenderingEngine->Render();

    GetUser(dsm_->GetSystemIndex())->SetVoice(crsf::TAudioRenderEngine::GetInstance()->GetCaptureMemoryObject());
}

void HandsTogether::SetupNetworkReceiver()
{
    dsm_->AttachCreateEventListener<crsf::TPointMemoryObject>([this](crsf::TCRMemory* crmemory) {
        if (dsm_->IsLocalMemory(crmemory))
            return false;

        std::smatch match;
        if (std::regex_match(crmemory->GetProperty().m_strName, match, std::regex("^.*-UserState$")))
        {
            auto pmo = dynamic_cast<crsf::TPointMemoryObject*>(crmemory);
            add_task([this, pmo](rppanda::FunctionalTask*) {
                if (!scene_setup_finished_)
                    return AsyncTask::DS_cont;

                auto user = GetUser(pmo->GetSystemIndex());
                user->SetPointMemoryObject(pmo);

                if (openvr_manager_)
                {
                    user->SetAvatarModel(openvr_manager_->get_plugin()->load_model(0));

                    auto controller_model = openvr_manager_->get_plugin()->load_model("vr_controller_vive_1_5");
                    for (size_t k = 0; k < 2; ++k)
                    {
                        user->SetControllerModel(k, controller_model.copy_to(rpcore::Globals::render));
                    }
                }
                else
                {
                    auto head = rpcore::create_cube("head");
                    head.set_scale(0.1f);
                    user->SetAvatarModel(head);

                    auto controller = rpcore::create_cube("controller");
                    controller.set_scale(0.05f);
                    for (size_t k = 0; k < 2; ++k)
                    {
                        user->SetControllerModel(k, controller.copy_to(rpcore::Globals::render));
                    }
                }

                return AsyncTask::DS_done;
            }, "load_avatar_" + std::to_string(pmo->GetSystemIndex()));
        }
        return false;
    });

    dsm_->AttachCreateEventListener<crsf::TSoundMemoryObject>([this](crsf::TCRMemory* crmemory) {
        if (dsm_->IsLocalMemory(crmemory))
            return false;

        std::smatch match;
        if (std::regex_match(crmemory->GetProperty().m_strName, match, std::regex("^.*-AudioCapture$")))
        {
            auto smo = dynamic_cast<crsf::TSoundMemoryObject*>(crmemory);
            add_task([this, smo](rppanda::FunctionalTask*) {
                if (!scene_setup_finished_)
                    return AsyncTask::DS_cont;

                auto user = GetUser(smo->GetSystemIndex());
                user->SetVoice(smo);

                return AsyncTask::DS_done;
            }, "load_voice_" + std::to_string(smo->GetSystemIndex()));
        }

        return false;
    });
}

void HandsTogether::AllUserTouchedEvent(const Event* ev)
{
    int contact_id = ev->get_parameter(0).get_int_value();

    UpdateCubeColor(true);

    if (contact_id == 1 || contact_id == 3)
        VibrateController(0);

    if (contact_id == 2 || contact_id == 3)
        VibrateController(1);
}

void HandsTogether::NotTouchedEvent(const Event* ev)
{
    UpdateCubeColor(false);
}

void HandsTogether::VibrateController(int hand_index)
{
    if (!openvr_manager_)
        return;

    if (m_pControllerID[hand_index] == 0)
        return;

    openvr_manager_->get_plugin()->get_vr_system()->TriggerHapticPulse(m_pControllerID[hand_index], 0, 40000);
}

void HandsTogether::UpdateCubeColor(bool contacted)
{
    auto pCubeGraphicModel = m_pCube->GetGraphicModel();
    rpcore::RPMaterial mat(pCubeGraphicModel->GetMaterial());
    mat.set_base_color(contacted ? LColor(0.2, 0.2, 1.0, 1) : LColorf(1.0, 0.2, 0.2, 1));
}

void HandsTogether::CheckCubeState()
{
    static bool prev_all_touched = false;

    const auto my_system_index = dsm_->GetSystemIndex();
    bool my_touched[2] = { false, false };
    bool all_touched = true;
    const auto& cube_pos = m_pCube->GetPosition();
    for (const auto& user : m_users)
    {
        bool touched = false;
        for (size_t k = 0; k < 2; ++k)
        {
            auto controller = user.second->GetController(k);
            if (controller)
            {
                if ((controller->GetPosition() - cube_pos).length() < 0.12)
                {
                    touched = true;
                    if (user.second->GetSystemIndex() == my_system_index)
                        my_touched[k] = true;
                }
            }
        }

        all_touched = all_touched && touched;
    }

    if (all_touched)
    {
        int contact_id = (my_touched[0] ? 1 : 0) + ((my_touched[1] ? 1 : 0) << 1);
        rpcore::Globals::base->get_messenger()->send("AllUserTouched", EventParameter(contact_id), true);
    }
    else
    {
        if (prev_all_touched)
            rpcore::Globals::base->get_messenger()->send("NotTouched", true);
    }

    prev_all_touched = all_touched;
}

void HandsTogether::SetupEvent()
{
    accept("openvr::k_EButton_Grip", [this](const Event* ev) { ResetUserPose(); });
    accept("AllUserTouched", [this](const Event* ev) { AllUserTouchedEvent(ev); });
    accept("NotTouched", [this](const Event* ev) { NotTouchedEvent(ev); });
}
