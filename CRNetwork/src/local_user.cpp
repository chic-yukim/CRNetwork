#include "local_user.hpp"

#include <crsf/CoexistenceInterface/TDynamicStageMemory.h>
#include <crsf/CoexistenceInterface/TPointMemoryObject.h>
#include <crsf/CoexistenceInterface/TSoundMemoryObject.h>
#include <crsf/CRModel/TWorldObject.h>

LocalUser::LocalUser(unsigned int system_index) : User(system_index)
{
    auto dsm = crsf::TDynamicStageMemory::GetInstance();

    crsf::TCRProperty props;
    props.m_strName = "UserState";
    props.m_propPoint.m_nPointNumber = 3;
    point_mo_ = dsm->CreatePointMemoryObject(props);
    dsm->EnableNetworking(point_mo_, 3);

    add_task([this](rppanda::FunctionalTask * task) {
        {
            crsf::TPoint p;
            p.m_nIndex = 0;
            p.m_Pose.SetMatrix(head_->GetMatrix());
            point_mo_->SetPointMemory(0, p);
        }

        for (size_t k = 0; k < controllers_.size(); ++k)
        {
            crsf::TPoint p;
            p.m_nIndex = k + 1;
            p.m_Pose.SetMatrix(controllers_[k].root->GetMatrix());
            point_mo_->SetPointMemory(k + 1, p);
        }

        point_mo_->UpdatePointMemoryObject();

        return AsyncTask::DS_cont;

    }, "send-user-state");
}

LocalUser::~LocalUser()
{
    auto dsm = crsf::TDynamicStageMemory::GetInstance();

    if (voice_mo_)
        dsm->DisableNetworking(voice_mo_);
}

void LocalUser::SetVoice(crsf::TSoundMemoryObject* voice_mo)
{
    User::SetVoice(voice_mo);

    if (!voice_mo_)
        return;

    crsf::TDynamicStageMemory::GetInstance()->EnableNetworking(voice_mo_, 2);
}
