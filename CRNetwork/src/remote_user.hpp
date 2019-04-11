#pragma once

#include "user.hpp"

class RemoteUser : public User
{
public:
    RemoteUser(unsigned int system_index);
    ~RemoteUser() override;

    void SetPointMemoryObject(crsf::TPointMemoryObject* pmo) override;

    void SetVoice(crsf::TSoundMemoryObject* voice_mo) override;
    void PlayVoice() override;
    void StopVoice() override;
};
