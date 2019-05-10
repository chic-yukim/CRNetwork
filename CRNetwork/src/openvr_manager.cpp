/**
 * Coexistence Reality Software Framework (CRSF)
 * Copyright (c) Center of Human-centered Interaction for Coexistence. All rights reserved.
 * See the LICENSE.md file for more details.
 */

#include "openvr_manager.hpp"

#include <spdlog/spdlog.h>

#include <render_pipeline/rpcore/render_pipeline.hpp>
#include <render_pipeline/rpcore/pluginbase/manager.hpp>
#include <render_pipeline/rppanda/task/task_manager.hpp>
#include <render_pipeline/rpcore/globals.hpp>
#include <render_pipeline/rppanda/showbase/showbase.hpp>
#include <render_pipeline/rpcore/loader.hpp>
#include <render_pipeline/rpcore/util/rptextnode.hpp>

#include <rpplugins/openvr/plugin.hpp>
#include <rpplugins/openvr/camera_interface.hpp>

#include <crsf/CRModel/TWorldObject.h>
#include <crsf/CRModel/TWorld.h>
#include <crsf/RenderingEngine/TGraphicRenderEngine.h>

extern spdlog::logger* global_logger;

OpenVRManager::OpenVRManager(rpcore::RenderPipeline& pipeline) : pipeline_(pipeline)
{
    if (!pipeline_.get_plugin_mgr()->is_plugin_enabled("openvr"))
        return;

    openvr_plugin_ = static_cast<rpplugins::OpenVRPlugin*>(pipeline_.get_plugin_mgr()->get_instance("openvr")->downcast());

    caching_devices();

    toggle_ar();

    add_task([this](rppanda::FunctionalTask*) {
        process_controller_event();
        return AsyncTask::DoneStatus::DS_cont;
    }, "process_controller_event");

    accept("OpenVRManager::toggle_ar", [this](const Event*) { toggle_ar(); });
}

OpenVRManager::~OpenVRManager()
{
    remove_all_tasks();
}

void OpenVRManager::toggle_ar()
{
    static bool virtual_mode = true;
    virtual_mode = !virtual_mode;

    if (virtual_mode)
    {
        for (auto&& object : objects_)
        {
            object.second->GetNodePath().show_through(crsf::TWorldObject::GetVirtualObjectMask());
            object.second->GetNodePath().hide(crsf::TWorldObject::GetRealObjectMask());
        }
    }
    else
    {
        for (auto&& object : objects_)
        {
            object.second->GetNodePath().show_through(crsf::TWorldObject::GetRealObjectMask());
            object.second->GetNodePath().hide(crsf::TWorldObject::GetVirtualObjectMask());
        }
    }
}

void OpenVRManager::caching_devices()
{
    hmd_np_ = openvr_plugin_->get_device_node(vr::k_unTrackedDeviceIndex_Hmd);
    for (int k = vr::k_unTrackedDeviceIndex_Hmd + 1; k < vr::k_unMaxTrackedDeviceCount; ++k)
    {
        if (!openvr_plugin_->get_vr_system()->IsTrackedDeviceConnected(k))
            continue;

        auto np = openvr_plugin_->get_device_node(k);
        switch (openvr_plugin_->get_tracked_device_class(k))
        {
        case vr::TrackedDeviceClass_TrackingReference:
            basestation_np_list_.push_back(np);
            break;
        case vr::TrackedDeviceClass_Controller:
            controller_np_list_.push_back(np);
            break;
        case vr::TrackedDeviceClass_GenericTracker:
            tracker_np_list_.push_back(np);
            break;
        default:
            break;
        }
    }
}

void OpenVRManager::process_controller_event()
{
    auto vr_system = openvr_plugin_->get_vr_system();

    // Process SteamVR controller state
    for (vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++)
    {
        if (openvr_plugin_->get_tracked_device_class(unDevice) != vr::TrackedDeviceClass_Controller)
            continue;

        vr::VRControllerState_t state;
        if (!vr_system->GetControllerState(unDevice, &state, sizeof(state)))
            continue;

        const EventParameter ev_param(static_cast<int>(unDevice));

        for (int id = vr::EVRButtonId::k_EButton_System; id < vr::EVRButtonId::k_EButton_Max; ++id)
        {
            const auto button_mask = vr::ButtonMaskFromId(static_cast<vr::EVRButtonId>(id));
            const bool pressed = (button_mask & state.ulButtonPressed) != 0;
            const bool last_pressed = (button_mask & controller_last_states_[unDevice]) != 0;

            if (pressed && !last_pressed)
            {
                const char* name = vr_system->GetButtonIdNameFromEnum(static_cast<vr::EVRButtonId>(id));

                rppanda::Messenger::get_global_instance()->send(
                    std::string("openvr::") + name, ev_param);
            }
            else if (!pressed && last_pressed)
            {
                const char* name = vr_system->GetButtonIdNameFromEnum(static_cast<vr::EVRButtonId>(id));

                rppanda::Messenger::get_global_instance()->send(
                    std::string("openvr::") + name + "-up", ev_param);
            }
        }

        controller_last_states_[unDevice] = state.ulButtonPressed;
    }
}

void OpenVRManager::load_objects()
{
    auto world = crsf::TGraphicRenderEngine::GetInstance()->GetWorld();

    auto object_root = crsf::CreateObject<crsf::TWorldObject>("openvr_objects");
    world->AddWorldObject(object_root);
    object_root_ = object_root.get();

    auto object = crsf::CreateObject<crsf::TWorldObject>("HMD");
    object_root_->AddWorldObject(object);
    objects_[0] = object.get();
    for (vr::TrackedDeviceIndex_t unDevice = 1; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++)
    {
        if (!openvr_plugin_->is_tracked_device_connected(unDevice))
            continue;

        if (objects_[unDevice])
            continue;

        auto object = crsf::CreateObject<crsf::TWorldObject>(openvr_plugin_->load_model(unDevice));
        object_root_->AddWorldObject(object);
        objects_[unDevice] = object.get();
    }
}

void OpenVRManager::setup_auto_sync_objects()
{
    add_task([this](rppanda::FunctionalTask* task) {
        for (auto&& object: objects_)
        {
            if (!openvr_plugin_->is_tracked_device_connected(object.first))
                continue;

            if (!objects_[object.first])
                continue;

            objects_[object.first]->SetMatrix(openvr_plugin_->get_device_node(object.first).get_mat());
        }

        return AsyncTask::DS_cont;
    }, "setup_auto_sync_objects", -20);
}

void OpenVRManager::enable_camera_streaming()
{
    if (is_streamed_)
        return;

    if (!openvr_camera_)
    {
        if (openvr_plugin_->has_tracked_camera())
            openvr_camera_ = openvr_plugin_->get_tracked_camera();
    }

    if (openvr_camera_)
    {
        is_streamed_ = openvr_camera_->acquire_video_streaming_service();
        if (!is_streamed_)
            return;

        uint32_t width;
        uint32_t height;
        uint32_t buffer_size;
        if (openvr_camera_->get_frame_size(width, height, buffer_size, frame_type_) != vr::EVRTrackedCameraError::VRTrackedCameraError_None)
        {
            global_logger->error("Failed to call OpenVRCameraInterface::get_frame_size.");
            is_streamed_ = false;
            return;
        }

        last_task_time_ = 0;
        framebuffer_.resize(buffer_size);

        add_task([this](rppanda::FunctionalTask * task) { return fetch_camera_framebuffer(task); }, "upload_texture");
    }
}

void OpenVRManager::disable_camera_streaming()
{
    if (!is_streamed_)
        return;

    remove_task("upload_texture");
    openvr_camera_->release_video_streaming_service();
    is_streamed_ = false;
}

AsyncTask::DoneStatus OpenVRManager::fetch_camera_framebuffer(rppanda::FunctionalTask* task)
{
    auto elapsed_time = task->get_elapsed_time();
    if ((elapsed_time - last_task_time_) < (16.0 / 1000.0))
        return AsyncTask::DS_cont;

    last_task_time_ = elapsed_time;

    vr::CameraVideoStreamFrameHeader_t header;
    openvr_camera_->get_frame_header(header, frame_type_);

    static uint32_t last_frame_sequence = 0;

    // frame hasn't changed yet, nothing to do
    if (header.nFrameSequence == last_frame_sequence)
        return AsyncTask::DS_cont;

    // Frame has changed, do the more expensive frame buffer copy
    auto err = openvr_camera_->get_framebuffer(header, framebuffer_, frame_type_);
    if (err != vr::VRTrackedCameraError_None)
        return AsyncTask::DS_cont;

    rpcore::Globals::base->get_messenger()->send("OpenVRManager::fetch_camera_framebuffer", false);

    last_frame_sequence = header.nFrameSequence;

    return AsyncTask::DS_cont;
}

