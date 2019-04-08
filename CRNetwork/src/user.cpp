#include "user.h"

#include <crsf/CoexistenceInterface/TDynamicStageMemory.h>
#include <crsf/CoexistenceInterface/TPointMemoryObject.h>
#include <crsf/CoexistenceInterface/TSoundMemoryObject.h>
#include <crsf/CREngine/TDynamicModuleManager.h>
#include <crsf/CRModel/TWorld.h>
#include <crsf/RenderingEngine/TGraphicRenderEngine.h>
#include <crsf/RenderingEngine/TAudioRenderEngine.h>

UserEntity::UserEntity(unsigned int system_index) : m_systemIndex(system_index)
{
    auto rendering_engine = crsf::TGraphicRenderEngine::GetInstance();
    auto world = rendering_engine->GetWorld();

    auto user = crsf::CreateObject<crsf::TWorldObject>("User" + std::to_string(system_index));
    root_ = user.get();
    world->AddWorldObject(user);

    auto head = crsf::CreateObject<crsf::TWorldObject>("head");
    head_ = head.get();
    root_->AddWorldObject(head_);

    for (size_t k = 0; k < controllers_.size(); ++k)
    {
        auto controller = crsf::CreateObject<crsf::TWorldObject>("controller-" + std::to_string(k));
        controllers_[k].root = controller.get();
        root_->AddWorldObject(controllers_[k].root);
    }
}

UserEntity::~UserEntity()
{
    StopVoice();

    root_->DetachWorldObject();
}

void UserEntity::SetPointMemoryObject(crsf::TPointMemoryObject* pmo)
{
    point_mo_ = pmo;
}

void UserEntity::SetHeadMatrix(const LMatrix4f& mat)
{
    head_->SetMatrix(mat);
}

void UserEntity::SetControllerMatrix(int side, const LMatrix4f& mat)
{
    controllers_[side].root->SetMatrix(mat);
}

void UserEntity::SetAvatarModel(const NodePath& np)
{
    if (!np)
        return;

    if (avatar_)
        avatar_->DetachWorldObject();

    auto avatar = crsf::CreateObject(np);
    avatar_ = avatar.get();
    head_->AddWorldObject(avatar);
}

void UserEntity::SetControllerModel(int side, const NodePath& np)
{
    if (!np)
        return;

    if (controllers_[side].model)
        controllers_[side].model->DetachWorldObject();

    auto model = crsf::CreateObject(np);
    controllers_[side].model = model.get();
    controllers_[side].root->AddWorldObject(controllers_[side].model);
}

void UserEntity::SetVoice(crsf::TSoundMemoryObject* voice_mo)
{
    voice_mo_ = voice_mo;
}

// ************************************************************************************************

LocalUserEntity::LocalUserEntity(unsigned int system_index) : UserEntity(system_index)
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

LocalUserEntity::~LocalUserEntity()
{
    auto dsm = crsf::TDynamicStageMemory::GetInstance();

    if (voice_mo_)
        dsm->DisableNetworking(voice_mo_);
}

void LocalUserEntity::SetVoice(crsf::TSoundMemoryObject* voice_mo)
{
    UserEntity::SetVoice(voice_mo);

    if (!voice_mo_)
        return;

    crsf::TDynamicStageMemory::GetInstance()->EnableNetworking(voice_mo_, 2);
}

// ************************************************************************************************

RemoteUserEntity::RemoteUserEntity(unsigned int system_index) : UserEntity(system_index)
{
}

RemoteUserEntity::~RemoteUserEntity()
{
    if (point_mo_)
        point_mo_->DetachPointListener(std::to_string(m_systemIndex) + "-update-user-state");
}

void RemoteUserEntity::SetPointMemoryObject(crsf::TPointMemoryObject* pmo)
{
    UserEntity::SetPointMemoryObject(pmo);
    point_mo_->AttachPointListener(std::to_string(m_systemIndex) + "-update-user-state", [this](crsf::TPointMemoryObject* pmo) {
        SetHeadMatrix(pmo->GetPointMemory(0).m_Pose.GetMatrix());

        for (size_t k = 0; k < controllers_.size(); ++k)
            SetControllerMatrix(k, pmo->GetPointMemory(k + 1).m_Pose.GetMatrix());
    });
}

void RemoteUserEntity::SetVoice(crsf::TSoundMemoryObject* voice_mo)
{
    UserEntity::SetVoice(voice_mo);
    PlayVoice();
}

void RemoteUserEntity::PlayVoice()
{
    crsf::TAudioRenderEngine::GetInstance()->AddOutputAudio(voice_mo_);
}

void RemoteUserEntity::StopVoice()
{
    crsf::TAudioRenderEngine::GetInstance()->RemoveOutputAudio(voice_mo_);
}
