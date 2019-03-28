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
#include <crsf/CoexistenceInterface/TAvatarMemoryObject.h>
#include <crsf/CREngine/TPhysicsManager.h>
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

    crsf::TPhysicsManager* physics_manager = crsf::TPhysicsManager::GetInstance();
    physics_manager->Init(crsf::EPHYX_ENGINE_BULLET);
    physics_manager->SetGravity(LVecBase3(0.0f, 0.0f, -0.98f));
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
        if (network_manager_->GetStatus() != 4)
        {
            m_logger->debug("Waiting for establishing network connection ...");
            return AsyncTask::DS_again;
        }

        MakeScene();
        MakeHand();
        SetupMicrophone();

        scene_setup_finished_ = true;
        return AsyncTask::DS_done;
    }, "ExampleInterfaceNetworkd::MakeScene");

    SetupEvent();

    main_gui_ = std::make_unique<MainGUI>(*this);
}

void HandsTogether::OnExit()
{
    m_users.clear();
    crsf::TAudioRenderEngine::GetInstance()->Deactivate();
    crsf::TPhysicsManager::GetInstance()->Exit();
    network_manager_->Exit();
}

void HandsTogether::SetupNetwork()
{
    SetupNetworkReceiver();
    network_manager_->Init();
}

void HandsTogether::StartPhysicsEngine()
{
	crsf::TPhysicsManager::GetInstance()->Start();
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
    auto cube_position = m_pCube->GetPosition();

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

void HandsTogether::ToggleControllerVisibility()
{
    if (!openvr_manager_)
        return;

    for (auto id : { m_pControllerID[0], m_pControllerID[1] })
    {
        if (id == 0)
            continue;

        if (auto object = openvr_manager_->get_object(id))
        {
            if (object->IsHidden())
                object->Show();
            else
                object->Hide();
        }
    }
}

void HandsTogether::MakeScene()
{
    // Load table model file
    crsf::TWorldObject* table = cr_world_->LoadPandaModel("resources/models/Ikea_Lisabo_table/Ikea_Lisabo_table.bam");
    table->SetScale(LVecBase3(1.05f));
    table->SetPosition(LVecBase3(0.0f), cr_world_);

    {
        // Set table graphics
        crsf::TCube::Parameters params;
        params.m_strName = "test_table";
        params.m_vec3Origin = LVecBase3(0.0f, 0.0f, 0.74f);
        params.m_vec3HalfExtent = LVecBase3(0.7f, 0.39f, 0.037f);
        auto m_pTable = crsf::CreateObject<crsf::TCube>(params);
        auto m_pTableGraphicModel = m_pTable->CreateGraphicModel();
        m_pTableGraphicModel->Hide();
        cr_world_->AddWorldObject(m_pTable);

        // Set table physics
        crsf::TPhysicsModel::Parameters phyx_params;
        phyx_params.m_fMass = 0.0f;
        auto pTablePhysicsModel = m_pTable->CreatePhysicsModel(phyx_params);
        crsf::TPhysicsManager::GetInstance()->AddModel(m_pTable);
    }

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

    // Set cube physics
    crsf::TPhysicsModel::Parameters phyx_params;
    phyx_params.m_fMass = 1000.0f;
    auto pCubePhysicsModel = pCube->CreatePhysicsModel(phyx_params);
    crsf::TPhysicsManager::GetInstance()->AddModel(pCube);

    crsf::TPhysicsManager::GetInstance()->AddTask([this]() {
        m_pCube->SetPosition(0.0f, 0.0f, 0.82f);
        m_pCube->SetRotation(LQuaternionf::ident_quat());
        return false;
    }, "StaticCube");

    pCubePhysicsModel->AttachUpdateListener(boost::bind(&HandsTogether::UpdateObjectEvent, this, _1), "HandsTogether::UpdateObjectEvent::pCubePhysicsModel");

    auto user = GetUser(dsm_->GetSystemIndex());     // make local user
    ResetUserPose();
}

void HandsTogether::MakeHand()
{
    LVecBase3 m_vec3ZeroToSensor(0);
    if (crsf::TDynamicModuleManager::GetInstance()->IsModuleEnabled("leap_motion"))
    {
        float m_vec3ZeroToSensor_x = m_property.get("handprop.m_vec3ZeroToSensor_x", 0.0f);
        float m_vec3ZeroToSensor_y = m_property.get("handprop.m_vec3ZeroToSensor_y", -0.4f);
        float m_vec3ZeroToSensor_z = m_property.get("handprop.m_vec3ZeroToSensor_z", 1.2f);
        m_vec3ZeroToSensor = LVecBase3f(m_vec3ZeroToSensor_x, m_vec3ZeroToSensor_y, m_vec3ZeroToSensor_z);;
    }

    // remove device dependency for local hands
    crsf::TAvatarMemoryObject* hand_amo;
    if (dsm_->HasMemoryObject<crsf::TAvatarMemoryObject>("Hands"))
    {
        hand_amo = dsm_->GetAvatarMemoryObjectByName("Hands");
    }
    else
    {
        crsf::TCRProperty props;
        props.m_strName = "Hands";
        props.m_propAvatar.m_nJointNumber = 44;
        hand_amo = dsm_->CreateAvatarMemoryObject(props);
    }

    auto user = GetUser(dsm_->GetSystemIndex());

    user->LoadHandModel(hand_amo, m_vec3ZeroToSensor);

    if (openvr_manager_)
    {
        ResetControllerMatch();

        user->add_task([this, user](rppanda::FunctionalTask*) {
            auto world = rendering_engine_->GetWorld();

            user->SetHeadMatrix(openvr_manager_->get_object(0)->GetMatrix(world));

            auto amo = dsm_->GetAvatarMemoryObjectByName("Hands");

            if (m_pControllerID[0] != 0)
            {
                auto pose = amo->GetAvatarMemory(21);
                pose.SetMatrix(openvr_manager_->get_object(m_pControllerID[0])->GetMatrix(world));
                amo->SetAvatarMemory(21, pose);
            }

            if (m_pControllerID[1] != 0)
            {
                auto pose = amo->GetAvatarMemory(43);
                pose.SetMatrix(openvr_manager_->get_object(m_pControllerID[1])->GetMatrix(world));
                amo->SetAvatarMemory(43, pose);
            }

            amo->UpdateAvatarMemoryObject();
            return AsyncTask::DS_cont;
        }, "update_hand_pose", -5);

        ToggleControllerVisibility();
    }
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
        if (std::regex_match(crmemory->GetProperty().m_strName, match, std::regex("^.*-UserPoint$")))
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
                }
                else
                {
                    auto head = rpcore::create_cube("head");
                    head.set_scale(0.1f);
                    user->SetAvatarModel(head);
                }

                return AsyncTask::DS_done;
            }, "load_avatar_" + std::to_string(pmo->GetSystemIndex()));
        }
        return false;
    });

    dsm_->AttachCreateEventListener<crsf::TAvatarMemoryObject>([this](crsf::TCRMemory* crmemory) {
        if (dsm_->IsLocalMemory(crmemory))
            return false;

        std::smatch match;
        if (std::regex_match(crmemory->GetProperty().m_strName, match, std::regex("^.*-Hands$")))
        {
            auto amo = dynamic_cast<crsf::TAvatarMemoryObject*>(crmemory);
            add_task([this, amo](rppanda::FunctionalTask*) {
                if (!scene_setup_finished_)
                    return AsyncTask::DS_cont;

                auto user = GetUser(amo->GetSystemIndex());
                user->LoadHandModel(amo);

                return AsyncTask::DS_done;
            }, "load_hand_" + std::to_string(amo->GetSystemIndex()));
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

bool HandsTogether::UpdateObjectEvent(const std::shared_ptr<crsf::TCRModel>& MyModel)
{
    std::unordered_set<unsigned int> touched_users;

    const auto local_system_index = dsm_->GetSystemIndex();
    auto contact_info = MyModel->GetPhysicsModel()->GetContactInfo();

    bool is_hand_contacted[2] = { false, false };
    for (const auto& model : contact_info->GetContactedModel())
    {
        if (model->GetModelGroup() != crsf::EMODEL_GROUP_HANDPHYSICSINTERACTOR)
            continue;

        std::smatch match;
        if (std::regex_match(model->GetName(), match, std::regex(".*Hands_PhysicsInteractor_3DModel_Fixed0_([0-9]+)")))
        {
            unsigned int system_index = std::stoi(model->GetTag("system_index"));
            if (m_users.find(system_index) == m_users.end())
            {
                m_logger->error("Invalid system index ({}). Please set correct tag.", system_index);
                break;
            }

            touched_users.insert(system_index);
            if (system_index == local_system_index)
            {
                if (std::stoi(match[1]) <= 9043)
                    is_hand_contacted[0] = true;
                else
                    is_hand_contacted[1] = true;
            }
        }
    }

    m_logger->trace("Touched Users: {}, current users: {}", touched_users.size(), m_users.size());
    if (touched_users.size() == m_users.size())
    {
        int contact_id = (is_hand_contacted[0] ? 1 : 0) * 1 + (is_hand_contacted[1] ? 1 : 0) * 2;
        rpcore::Globals::base->get_messenger()->send("AllUserTouched", EventParameter(contact_id), true);
    }
    else
    {
        rpcore::Globals::base->get_messenger()->send("NotTouched", true);
    }

    return false;
}

void HandsTogether::SetupEvent()
{
    accept("openvr::k_EButton_Grip", [this](const Event* ev) { ResetUserPose(); });
    accept("AllUserTouched", [this](const Event* ev) { AllUserTouchedEvent(ev); });
    accept("NotTouched", [this](const Event* ev) { NotTouchedEvent(ev); });
    accept("0", [this](const Event*) { StartPhysicsEngine(); });
}
