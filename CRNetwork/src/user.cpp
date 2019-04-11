#include "user.hpp"

#include <crsf/CRModel/TWorld.h>
#include <crsf/RenderingEngine/TGraphicRenderEngine.h>

User::User(unsigned int system_index) : system_index_(system_index)
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

User::~User()
{
    stop_voice();

    root_->DetachWorldObject();
}

void User::set_point_memory_object(crsf::TPointMemoryObject* pmo)
{
    point_mo_ = pmo;
}

void User::set_head_matrix(const LMatrix4f& mat)
{
    head_->SetMatrix(mat);
}

void User::set_controller_matrix(int side, const LMatrix4f& mat)
{
    controllers_[side].root->SetMatrix(mat);
}

void User::set_avatar_model(const NodePath& np)
{
    if (!np)
        return;

    if (avatar_)
        avatar_->DetachWorldObject();

    auto avatar = crsf::CreateObject(np);
    avatar_ = avatar.get();
    head_->AddWorldObject(avatar);
}

void User::set_controller_model(int side, const NodePath& np)
{
    if (!np)
        return;

    if (controllers_[side].model)
        controllers_[side].model->DetachWorldObject();

    auto model = crsf::CreateObject(np);
    controllers_[side].model = model.get();
    controllers_[side].root->AddWorldObject(controllers_[side].model);
}

void User::set_voice(crsf::TSoundMemoryObject* voice_mo)
{
    voice_mo_ = voice_mo;
}
