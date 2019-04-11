#pragma once

#include "user.hpp"

class RemoteUser : public User
{
public:
    RemoteUser(unsigned int system_index);
    ~RemoteUser() override;

    void set_point_memory_object(crsf::TPointMemoryObject* pmo) override;

    void set_voice(crsf::TSoundMemoryObject* voice_mo) override;
    void play_voice() override;
    void stop_voice() override;
};
