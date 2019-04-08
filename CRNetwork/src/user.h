#pragma once

#include <memory>
#include <array>

#include <luse.h>

#include <render_pipeline/rppanda/showbase/direct_object.hpp>

namespace crsf {
class TWorldObject;
class TPointMemoryObject;
class TSoundMemoryObject;
}

class NodePath;

class UserEntity : public rppanda::DirectObject
{
public:
    virtual ~UserEntity();

    unsigned int GetSystemIndex() const;

    crsf::TPointMemoryObject* GetPointMemoryObject() const;
    virtual void SetPointMemoryObject(crsf::TPointMemoryObject* pmo);

    virtual crsf::TWorldObject* GetHead() const;
    virtual void SetHeadMatrix(const LMatrix4f& mat);
    virtual void SetAvatarModel(const NodePath& np);

    virtual crsf::TWorldObject* GetController(int side) const;
    virtual void SetControllerMatrix(int side, const LMatrix4f& mat);
    virtual void SetControllerModel(int side, const NodePath& np);

    virtual void SetVoice(crsf::TSoundMemoryObject* voice_mo);
    virtual void PlayVoice() {}
    virtual void StopVoice() {}

protected:
    UserEntity(unsigned int system_index);

    const unsigned int m_systemIndex;

    crsf::TWorldObject* root_;
    crsf::TWorldObject* head_;
    crsf::TWorldObject* avatar_ = nullptr;

    struct ControllerData
    {
        crsf::TWorldObject* root;
        crsf::TWorldObject* model = nullptr;
    };
    std::array<ControllerData, 2> controllers_;

    crsf::TPointMemoryObject* point_mo_ = nullptr;
    crsf::TSoundMemoryObject* voice_mo_ = nullptr;
};

inline unsigned int UserEntity::GetSystemIndex() const
{
    return m_systemIndex;
}

inline crsf::TPointMemoryObject* UserEntity::GetPointMemoryObject() const
{
    return point_mo_;
}

inline crsf::TWorldObject* UserEntity::GetHead() const
{
    return head_;
}

inline crsf::TWorldObject* UserEntity::GetController(int side) const
{
    return controllers_[side].root;
}

// ************************************************************************************************

class LocalUserEntity : public UserEntity
{
public:
    LocalUserEntity(unsigned int system_index);
    ~LocalUserEntity() override;

    void SetVoice(crsf::TSoundMemoryObject* voice_mo) override;
};

// ************************************************************************************************

class RemoteUserEntity : public UserEntity
{
public:
    RemoteUserEntity(unsigned int system_index);
    ~RemoteUserEntity() override;

    void SetPointMemoryObject(crsf::TPointMemoryObject* pmo) override;

    void SetVoice(crsf::TSoundMemoryObject* voice_mo) override;
    void PlayVoice() override;
    void StopVoice() override;
};
