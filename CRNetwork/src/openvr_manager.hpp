/**
 * Coexistence Reality Software Framework (CRSF)
 * Copyright (c) Center of Human-centered Interaction for Coexistence. All rights reserved.
 * See the LICENSE.md file for more details.
 */

#pragma once

#include <unordered_map>

#include <nodePath.h>
#include <render_pipeline/rppanda/showbase/direct_object.hpp>

#include <openvr.h>

namespace rpcore {
class RenderPipeline;
}

namespace rpplugins {
class OpenVRPlugin;
class OpenVRCameraInterface;
}

namespace crsf {
class TWorldObject;
}

class OpenVRManager : public rppanda::DirectObject
{
public:
    OpenVRManager(rpcore::RenderPipeline& pipeline);
    virtual ~OpenVRManager();

    bool is_available() const;

    rpplugins::OpenVRPlugin* get_plugin() const;

    void toggle_ar();

    NodePath get_hmd_nodepath() const;
    const std::vector<NodePath>& get_basestation_nodepaths() const;
    const std::vector<NodePath>& get_controller_nodepaths() const;
    const std::vector<NodePath>& get_tracker_nodepaths() const;

    void load_objects();
    void setup_auto_sync_objects();

    crsf::TWorldObject* get_object_root() const;
    crsf::TWorldObject* get_object(vr::TrackedDeviceIndex_t device_index) const;

    void enable_camera_streaming();
    void disable_camera_streaming();
    const std::vector<uint8_t>& get_camera_framebuffer() const;

private:
    void caching_devices();
    void process_controller_event();

    AsyncTask::DoneStatus fetch_camera_framebuffer(rppanda::FunctionalTask* task);

    rpcore::RenderPipeline& pipeline_;
    rpplugins::OpenVRPlugin* openvr_plugin_ = nullptr;

    uint64_t controller_last_states_[vr::k_unMaxTrackedDeviceCount];

    NodePath hmd_np_;
    std::vector<NodePath> basestation_np_list_;
    std::vector<NodePath> controller_np_list_;
    std::vector<NodePath> tracker_np_list_;

    crsf::TWorldObject* object_root_;
    std::unordered_map<vr::TrackedDeviceIndex_t, crsf::TWorldObject*> objects_;

    rpplugins::OpenVRCameraInterface* openvr_camera_ = nullptr;
    bool is_streamed_ = false;
    std::vector<uint8_t> framebuffer_;
    vr::EVRTrackedCameraFrameType frame_type_ = vr::VRTrackedCameraFrameType_Undistorted;
    double last_task_time_ = 0.0f;
};

inline bool OpenVRManager::is_available() const
{
    return openvr_plugin_ != nullptr;
}

inline rpplugins::OpenVRPlugin* OpenVRManager::get_plugin() const
{
    return openvr_plugin_;
}

inline NodePath OpenVRManager::get_hmd_nodepath() const
{
    return hmd_np_;
}

inline const std::vector<NodePath>& OpenVRManager::get_basestation_nodepaths() const
{
    return basestation_np_list_;
}

inline const std::vector<NodePath>& OpenVRManager::get_controller_nodepaths() const
{
    return controller_np_list_;
}

inline const std::vector<NodePath>& OpenVRManager::get_tracker_nodepaths() const
{
    return tracker_np_list_;
}

inline crsf::TWorldObject* OpenVRManager::get_object_root() const
{
    return object_root_;
}

inline crsf::TWorldObject* OpenVRManager::get_object(vr::TrackedDeviceIndex_t device_index) const
{
    auto found = objects_.find(device_index);
    if (found == objects_.end())
        return nullptr;
    else
        return objects_.at(device_index);
}

inline const std::vector<uint8_t>& OpenVRManager::get_camera_framebuffer() const
{
    return framebuffer_;
}
