#pragma once
#include "Runtime/Application.h"

namespace Nova { class AudioClip; }
using Nova::Application;
using Nova::Array;
using Nova::ApplicationConfiguration;
using Nova::Ref;
using Nova::AudioClip;

class AudioPlayerApplication final : public Application
{
public:
    ApplicationConfiguration GetConfiguration() const override;
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnGUI() override;
    void OnDestroy() override;
private:
    Ref<AudioClip> m_Clip = nullptr;
};
