#include "AudioPlayerApplication.h"
#include "FreeFlyCameraComponent.h"
#include "Audio/AudioClip.h"
#include "Audio/AudioSystem.h"
#include "Components/Camera.h"
#include "Components/Transform.h"
#include "Components/Audio/AudioListener.h"
#include "Runtime/DesktopWindow.h"
#include "Runtime/EntryPoint.h"
#include "Runtime/Path.h"
#include "Runtime/Scene.h"
#include "Spectrum.h"
#include "Components/Rendering/AmbientLight.h"
#include "Components/Rendering/DirectionalLight.h"
#include "Containers/StringConversion.h"
#include "Rendering/Shader.h"
#include <imgui.h>

using namespace Nova;

NOVA_DEFINE_APPLICATION_CLASS(AudioPlayerApplication);


ApplicationConfiguration AudioPlayerApplication::GetConfiguration() const
{
    ApplicationConfiguration configuration;
    configuration.applicationName = "Audio FTT Test App";
    configuration.windowFlags = WindowCreateFlagBits::Default | WindowCreateFlagBits::CreateAtCenter | WindowCreateFlagBits::Resizable;
    configuration.windowWidth = 1600;
    configuration.windowHeight = 900;
    configuration.vsync = false;
    return configuration;
}

static EntityHandle audio = nullptr;

void AudioPlayerApplication::OnInit()
{
    Application::OnInit();

    AssetDatabase& assetDatabase = GetAssetDatabase();

    MaterialCreateInfo materialCreateInfo;
    Ref<Material> material = GetDevice()->CreateMaterial(MaterialCreateInfo().WithShader(assetDatabase.Get<Shader>("BlinnPhongShader")));
    material->SetTexture("diffuseTex", assetDatabase.Get<Texture>("CheckerTexPlaceholder"));
    assetDatabase.AddAsset(material, "CubeMaterial");

    Ref<StaticMesh> cubeMesh = assetDatabase.CreateAsset<StaticMesh>("CubeMesh");
    cubeMesh->LoadFromFile(R"(C:\Users\Tiwann\Desktop\Cube.glb)", false);
    cubeMesh->SetMaterial(0, material);

    Scene* scene = new Scene(this, "Main Scene");
    scene->OnInit();

    audio = scene->CreateEntity("Audio");
    audio->AddComponent<AudioListener>();
    AudioSource* audioSource = audio->AddComponent<AudioSource>();

    EntityHandle spectrumEntity = scene->CreateEntity("Spectrum");
    Spectrum* spectrumComponent = spectrumEntity->AddComponent<Spectrum>();
    spectrumComponent->SetAudioSource(audioSource);
    spectrumComponent->SetStaticMesh(cubeMesh);
    spectrumComponent->InitializeSpectrum();

    EntityHandle cameraEntity = scene->CreateEntity("Camera");
    Camera* camera = cameraEntity->AddComponent<Camera>();
    camera->SetFieldOfView(45.0f);
    FreeFlyCameraComponent* cameraMovement = cameraEntity->AddComponent<FreeFlyCameraComponent>();
    cameraMovement->SetCamera(camera);

    EntityHandle light = scene->CreateEntity("Light");
    light->AddComponent<DirectionalLight>();
    AmbientLight* ambient = light->AddComponent<AmbientLight>();
    ambient->SetIntensity(0.2f);

    SceneManager* sceneManager = GetSceneManager();
    sceneManager->LoadScene(scene);

    m_Clip = new AudioClip;
}

void AudioPlayerApplication::OnUpdate(float deltaTime)
{
    if (Ref<DesktopWindow> window = GetWindow().As<DesktopWindow>())
    {
        if (window->GetKeyDown(KeyCode::Space))
        {
            AudioSource* audioSource = audio->GetComponent<AudioSource>();
            audioSource->Play();
        }
    }
}

void AudioPlayerApplication::OnGUI()
{
    Application::OnGUI();
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Open audio file"))
            {
                const String filepath = Path::OpenFileDialog("Open an audio file...", Path::GetDesktopDirectory(), DialogFilters::AudioFilters, *GetWindow());

                AudioSource* audioSource = audio->GetComponent<AudioSource>();
                audioSource->Stop();

                m_Clip->Destroy();
                if (!m_Clip->LoadFromFile(filepath, AudioPlaybackFlagBits::Music | AudioPlaybackFlagBits::ComputeFFT))
                {
                    std::wcerr << "Failed to load file: " << StringConvertToWide(filepath) << "\n";
                }

                auto fft = m_Clip->GetFFTAudioNode();
                fft->SetFFTWindow(FFTWindow::BlackmanHarris);
                audioSource->SetAudioClip(m_Clip);
            }
            if (ImGui::MenuItem("Exit")) Exit();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void AudioPlayerApplication::OnDestroy()
{
    Application::OnDestroy();
}
