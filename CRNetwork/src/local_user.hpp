#pragma once

#include "user.hpp"

class LocalUser : public User
{
public:
    LocalUser(unsigned int system_index);
    ~LocalUser() override;

    void set_voice(crsf::TSoundMemoryObject* voice_mo) override;

    void set_front_view(crsf::TImageMemoryObject* front_view_mo) override;
    void set_front_view(const std::shared_ptr<crsf::TTexture>& front_view_tex) override;
};
