#include "remote_user.hpp"

#include <crsf/CoexistenceInterface/TPointMemoryObject.h>
#include <crsf/CoexistenceInterface/TSoundMemoryObject.h>
#include <crsf/CRModel/TWorldObject.h>
#include <crsf/RenderingEngine/TAudioRenderEngine.h>

RemoteUser::RemoteUser(unsigned int system_index) : User(system_index)
{
}

RemoteUser::~RemoteUser()
{
    if (point_mo_)
        point_mo_->DetachPointListener(std::to_string(system_index_) + "-update-user-state");
}

void RemoteUser::set_point_memory_object(crsf::TPointMemoryObject* pmo)
{
    User::set_point_memory_object(pmo);
    point_mo_->AttachPointListener(std::to_string(system_index_) + "-update-user-state", [this](crsf::TPointMemoryObject* pmo) {
        set_head_matrix(pmo->GetPointMemory(0).m_Pose.GetMatrix());

        for (size_t k = 0; k < controllers_.size(); ++k)
            set_controller_matrix(k, pmo->GetPointMemory(k + 1).m_Pose.GetMatrix());
    });
}

void RemoteUser::set_voice(crsf::TSoundMemoryObject* voice_mo)
{
    User::set_voice(voice_mo);
    play_voice();
}

void RemoteUser::play_voice()
{
    crsf::TAudioRenderEngine::GetInstance()->AddOutputAudio(voice_mo_);
}

void RemoteUser::stop_voice()
{
    crsf::TAudioRenderEngine::GetInstance()->RemoveOutputAudio(voice_mo_);
}
