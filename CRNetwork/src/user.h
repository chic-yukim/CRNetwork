#pragma once

#include <memory>
#include <array>

#include <luse.h>

#include <render_pipeline/rppanda/showbase/direct_object.hpp>

namespace crsf {
class TWorldObject;
class TCRHand;
class THandInteractionEngineConnector;
class TAvatarMemoryObject;
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

    virtual void SetHeadMatrix(const LMatrix4f& mat);

    crsf::TCRHand* GetHand() const;

    virtual void SetAvatarModel(const NodePath& np);
    virtual void LoadHandModel(crsf::TAvatarMemoryObject* hand_memory_object, const LVecBase3f& m_vec3ZeroToSensor=LVecBase3f(0));

    virtual void SetVoice(crsf::TSoundMemoryObject* voice_mo);
    virtual void PlayVoice() {}
    virtual void StopVoice() {}

    // Hand parameter
    std::unique_ptr<crsf::TCRHand> m_pHand;
    crsf::TWorldObject* m_pHandObject;
    std::unique_ptr<crsf::THandInteractionEngineConnector> m_pHandInteractionEngineConnector;

protected:
    UserEntity(unsigned int system_index);

    void RenderHandModel(crsf::TAvatarMemoryObject *pAvatarMemoryObject);

    const unsigned int m_systemIndex;

    crsf::TWorldObject* root_;
    crsf::TWorldObject* head_;
    crsf::TWorldObject* avatar_ = nullptr;

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

inline crsf::TCRHand* UserEntity::GetHand() const
{
    return m_pHand.get();
}

// ************************************************************************************************

class LocalUserEntity : public UserEntity
{
public:
    LocalUserEntity(unsigned int system_index);
    ~LocalUserEntity() override;

    void SetHeadMatrix(const LMatrix4f& mat) override;

    void LoadHandModel(crsf::TAvatarMemoryObject* hand_memory_object, const LVecBase3f& m_vec3ZeroToSensor = LVecBase3f(0)) override;
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
