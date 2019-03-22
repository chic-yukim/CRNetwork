#include "User.h"

#include <nodePath.h>
#include <materialCollection.h>

#include <render_pipeline/rppanda/showbase/showbase.hpp>
#include <render_pipeline/rpcore/globals.hpp>
#include <render_pipeline/rpcore/util/rpgeomnode.hpp>

#include <crsf/CoexistenceInterface/TDynamicStageMemory.h>
#include <crsf/CoexistenceInterface/TAvatarMemoryObject.h>
#include <crsf/CoexistenceInterface/TPointMemoryObject.h>
#include <crsf/CoexistenceInterface/TSoundMemoryObject.h>
#include <crsf/CREngine/THandInteractionEngineConnector.h>
#include <crsf/CREngine/TDynamicModuleManager.h>
#include <crsf/CRModel/TWorld.h>
#include <crsf/CRModel/TCRHand.h>
#include <crsf/CRModel/TCRHand.h>
#include <crsf/CRModel/TSphere.h>
#include <crsf/CRModel/TCharacter.h>
#include <crsf/CRModel/THandPhysicsInteractor.h>
#include <crsf/RenderingEngine/TGraphicRenderEngine.h>
#include <crsf/RemoteWorldInterface/TNetworkManager.h>
#include <crsf/System/TCRProperty.h>
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
}

UserEntity::~UserEntity()
{
    StopVoice();
}

void UserEntity::SetPointMemoryObject(crsf::TPointMemoryObject* pmo)
{
    point_mo_ = pmo;
}

void UserEntity::SetHeadMatrix(const LMatrix4f& mat)
{
    head_->SetMatrix(mat);
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

void UserEntity::LoadHandModel(crsf::TAvatarMemoryObject* hand_memory_object, const LVecBase3f& m_vec3ZeroToSensor)
{
    auto rendering_engine = crsf::TGraphicRenderEngine::GetInstance();
    auto world = rendering_engine->GetWorld();

    // Set hand property
    crsf::TCRProperty crHandProperty;
    crHandProperty.m_propAvatar.SetJointNumber(44);
    crHandProperty.m_propHand.m_strName = hand_memory_object->GetProperty().m_strName;
    crHandProperty.m_propHand.m_vec3ZeroToSensor = m_vec3ZeroToSensor;
    crHandProperty.m_propHand.SetRenderMode(false, false, true);

    // Load hand model
    m_pHandObject = world->LoadModel("resources/models/hands/hand.egg");
    root_->AddWorldObject(m_pHandObject);

    m_pHandObject->SetScale(0.011f, world);
    m_pHandObject->DisableTestBounding();
    auto hand_character = m_pHandObject->GetChild("leap_hand")->GetPtrOf<crsf::TCharacter>();
    hand_character->MakeAllControlJoint();
    crHandProperty.m_propHand.m_p3DModel = hand_character;
    crHandProperty.m_propHand.m_p3DModel_LeftWrist = hand_character->FindByName("Bip01FBXASC032RFBXASC032Hand001");
    crHandProperty.m_propHand.m_p3DModel_RightWrist = hand_character->FindByName("Bip01FBXASC032RFBXASC032Hand");
    //m_pHandObject->PrintAll();

    // Create hand instance
    m_pHand = std::make_unique<crsf::TCRHand>(crHandProperty);

    // Set DSM for hand
    m_pHandInteractionEngineConnector = std::make_unique<crsf::THandInteractionEngineConnector>();
    m_pHandInteractionEngineConnector->Init(m_pHand.get());
    m_pHandInteractionEngineConnector->ConnectHand(std::bind(&UserEntity::RenderHandModel, this, std::placeholders::_1), hand_memory_object);

    // Add hand physics interactor
    m_pHand->ConstructPhysicsInteractor_FixedVertex_Sphere("resources/models/hands/PhysicsInteractorIndex_full_new.txt", 0.0025, false, false, "both");

    for (const auto& interactor : m_pHand->GetPhysicsInteractorList())
        interactor.second->SetTag("system_index", std::to_string(m_systemIndex));
}

void UserEntity::SetVoice(crsf::TSoundMemoryObject* voice_mo)
{
    voice_mo_ = voice_mo;
}

void UserEntity::RenderHandModel(crsf::TAvatarMemoryObject* pAvatarMemoryObject)
{
    static const int wrist_index[2] = { 21, 43 };

    auto world = crsf::TGraphicRenderEngine::GetInstance()->GetWorld();

    const auto& am = pAvatarMemoryObject->GetAvatarMemory();

    m_pHand->Get3DModel_LeftWrist()->SetPosition(am[wrist_index[0]].GetPosition(), world);
    LQuaternionf vr_lquat;
    vr_lquat.set_hpr(am[wrist_index[0]].GetQuaternion().get_hpr());
    LQuaternionf lquat;
    lquat.set_hpr(LVecBase3f(90, 0, 50));
    m_pHand->Get3DModel_LeftWrist()->SetRotation(lquat * vr_lquat);

    m_pHand->Get3DModel_RightWrist()->SetPosition(am[wrist_index[1]].GetPosition(), world);
    LQuaternionf rquat;
    rquat.set_hpr(LVecBase3f(-90, 0, -230));
    m_pHand->Get3DModel_RightWrist()->SetRotation(rquat * am[wrist_index[1]].GetQuaternion());
}

// ************************************************************************************************

LocalUserEntity::LocalUserEntity(unsigned int system_index) : UserEntity(system_index)
{
    auto dsm = crsf::TDynamicStageMemory::GetInstance();

    crsf::TCRProperty props;
    props.m_strName = "UserPoint";
    props.m_propPoint.m_nPointNumber = 1;
    point_mo_ = dsm->CreatePointMemoryObject(props);
}

LocalUserEntity::~LocalUserEntity() = default;

void LocalUserEntity::SetHeadMatrix(const LMatrix4f& mat)
{
    UserEntity::SetHeadMatrix(mat);

    crsf::TPoint p;
    p.m_nIndex = 0;
    p.m_Pose.SetMatrix(mat);
    point_mo_->SetPointMemory(0, p);
}

void LocalUserEntity::LoadHandModel(crsf::TAvatarMemoryObject* hand_memory_object, const LVecBase3f& m_vec3ZeroToSensor)
{
    UserEntity::LoadHandModel(hand_memory_object, m_vec3ZeroToSensor);

    // hand to remote
    hand_memory_object->AttachAvatarListener([this](crsf::TAvatarMemoryObject* amo) {
        static double last = 0;

        auto now = rpcore::Globals::clock->get_real_time();
        if (now - last < 1.0 / 30.0)
            return;
        last = now;

        auto nm = crsf::TNetworkManager::GetInstance();
        if (nm->GetStatus() != 4)
            return;
        nm->Send(amo, 3);
        nm->Send(point_mo_, 3);
    });
}

void LocalUserEntity::SetVoice(crsf::TSoundMemoryObject* voice_mo)
{
    UserEntity::SetVoice(voice_mo);

    if (!voice_mo_)
        return;

    voice_mo_->AttachSoundListener([this](crsf::TSoundMemoryObject* smo) {
        auto nm = crsf::TNetworkManager::GetInstance();
        if (nm->GetStatus() == 4)
            nm->Send(smo, 2);
    });
}

// ************************************************************************************************

RemoteUserEntity::RemoteUserEntity(unsigned int system_index) : UserEntity(system_index)
{
}

RemoteUserEntity::~RemoteUserEntity() = default;

void RemoteUserEntity::SetPointMemoryObject(crsf::TPointMemoryObject* pmo)
{
    UserEntity::SetPointMemoryObject(pmo);
    point_mo_->AttachPointListener([this](crsf::TPointMemoryObject* pmo) {
        SetHeadMatrix(pmo->GetPointMemory(0).m_Pose.GetMatrix());
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
