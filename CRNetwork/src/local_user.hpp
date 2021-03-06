#pragma once

#include "user.hpp"

class LocalUser : public User
{
public:
    LocalUser(unsigned int system_index);
    ~LocalUser() override;

    void set_voice(crsf::TSoundMemoryObject* voice_mo) override;
};
