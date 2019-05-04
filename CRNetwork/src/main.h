/**
 * Coexistence Reality Software Framework (CRSF)
 * Copyright (c) Center of Human-centered Interaction for Coexistence. All rights reserved.
 * See the LICENSE.md file for more details.
 */

#pragma once

#include <array>

#include <render_pipeline/rppanda/showbase/direct_object.hpp>

#include <crsf/CRAPI/TDynamicModuleInterface.h>

namespace crsf {
class TDynamicStageMemory;
class TGraphicRenderEngine;
class TNetworkManager;
class TCube;
class TWorld;
}

class Event;
class User;
class MainGUI;
class OpenVRManager;

class MainApp: public crsf::TDynamicModuleInterface, public rppanda::DirectObject
{
public:
    MainApp();
    ~MainApp() override;

    void OnLoad() override;
    void OnStart() override;
    void OnExit() override;

private:
    friend class MainGUI;

    void MakeScene();
    void SetupLocalUser();
    void SetupMicrophone();
    void SetupNetworkReceiver();

    void SetupEvent();

    void SetupNetwork();

    User* GetUser(unsigned int system_index);
    void ResetUserPose();
    void ResetControllerMatch();
    void CheckCubeState();

    void AllUserTouchedEvent(const Event* ev);
    void NotTouchedEvent(const Event* ev);

    void VibrateController(int hand_index);
    void UpdateCubeColor(bool contacted);

    crsf::TDynamicStageMemory* dsm_;
    crsf::TGraphicRenderEngine* rendering_engine_;
    crsf::TNetworkManager* network_manager_;
    crsf::TWorld* cr_world_;

    std::unique_ptr<MainGUI> main_gui_;
    std::unique_ptr<OpenVRManager> openvr_manager_;

    crsf::TCube*                        m_pCube;

    bool scene_setup_finished_ = false;
    std::unordered_map<unsigned int, std::unique_ptr<User>> m_users;

    std::array<unsigned int, 2> m_pControllerID;
};
