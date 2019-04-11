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
        point_mo_->DetachPointListener(std::to_string(m_systemIndex) + "-update-user-state");
}

void RemoteUser::SetPointMemoryObject(crsf::TPointMemoryObject* pmo)
{
    User::SetPointMemoryObject(pmo);
    point_mo_->AttachPointListener(std::to_string(m_systemIndex) + "-update-user-state", [this](crsf::TPointMemoryObject* pmo) {
        SetHeadMatrix(pmo->GetPointMemory(0).m_Pose.GetMatrix());

        for (size_t k = 0; k < controllers_.size(); ++k)
            SetControllerMatrix(k, pmo->GetPointMemory(k + 1).m_Pose.GetMatrix());
    });
}

void RemoteUser::SetVoice(crsf::TSoundMemoryObject* voice_mo)
{
    User::SetVoice(voice_mo);
    PlayVoice();
}

void RemoteUser::PlayVoice()
{
    crsf::TAudioRenderEngine::GetInstance()->AddOutputAudio(voice_mo_);
}

void RemoteUser::StopVoice()
{
    crsf::TAudioRenderEngine::GetInstance()->RemoveOutputAudio(voice_mo_);
}
