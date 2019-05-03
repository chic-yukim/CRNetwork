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

    unsigned int get_system_index() const;

    crsf::TPointMemoryObject* get_point_memory_object() const;
    virtual void set_point_memory_object(crsf::TPointMemoryObject* pmo);

    virtual crsf::TWorldObject* get_head() const;
    virtual void set_avatar_model(const NodePath& np);

    virtual crsf::TWorldObject* get_controller(int side) const;
    virtual void set_controller_matrix(int side, const LMatrix4f& mat);
    virtual void set_controller_model(int side, const NodePath& np);

    virtual void set_voice(crsf::TSoundMemoryObject* voice_mo);
    virtual void play_voice() {}
    virtual void stop_voice() {}

protected:
    User(unsigned int system_index);

    const unsigned int system_index_;

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

// ************************************************************************************************

inline unsigned int User::get_system_index() const
{
    return system_index_;
}

inline crsf::TPointMemoryObject* User::get_point_memory_object() const
{
    return point_mo_;
}

inline crsf::TWorldObject* User::get_head() const
{
    return head_;
}

inline crsf::TWorldObject* User::get_controller(int side) const
{
    return controllers_[side].root;
}
