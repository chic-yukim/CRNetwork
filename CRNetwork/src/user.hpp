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

class User : public rppanda::DirectObject
{
public:
    virtual ~User();

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
    User(unsigned int system_index);

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

inline unsigned int User::GetSystemIndex() const
{
    return m_systemIndex;
}

inline crsf::TPointMemoryObject* User::GetPointMemoryObject() const
{
    return point_mo_;
}

inline crsf::TWorldObject* User::GetHead() const
{
    return head_;
}

inline crsf::TWorldObject* User::GetController(int side) const
{
    return controllers_[side].root;
}
