#pragma once
#include "Runtime/Application.h"

namespace Nova { class AudioClip; }
using Nova::Application;
using Nova::Array;
using Nova::ApplicationConfiguration;
using Nova::Ref;
using Nova::AudioClip;
using Nova::CmdLineArgs;

class AudioPlayerApplication final : public Application
{
public:
    explicit AudioPlayerApplication(CmdLineArgs&& cmdLineArgs) : Application(std::move(cmdLineArgs)){}
    ApplicationConfiguration GetConfiguration() const override;
    void OnInit() override;
    void OnUpdate(float deltaTime) override;
    void OnGUI() override;
    void OnDestroy() override;
private:
    Ref<AudioClip> m_Clip = nullptr;
};
