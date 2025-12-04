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

#include "EclipseVisualizer.h"

using namespace Nova;

NOVA_DEFINE_APPLICATION_CLASS(AudioPlayerApplication);


ApplicationConfiguration AudioPlayerApplication::GetConfiguration() const
{
    ApplicationConfiguration configuration;
    configuration.applicationName = "Audio Player [Nova Engine]";
    configuration.windowFlags = WindowCreateFlagBits::Default | WindowCreateFlagBits::CreateAtCenter | WindowCreateFlagBits::Resizable;
    configuration.windowWidth = 1600;
    configuration.windowHeight = 900;
    configuration.vsync = true;
    return configuration;
}

static EntityHandle audio = nullptr;

void AudioPlayerApplication::OnInit()
{
    Application::OnInit();
    AssetDatabase& assetDatabase = GetAssetDatabase();
    m_Clip = assetDatabase.CreateAsset<AudioClip>("LoadedSound");

    const CmdLineArgs& args = GetProgramArguments();
    if (args.Count() == 2)
    {
        const StringView filepath = args.GetArgument(1);
        if (filepath.IsEmpty())
            Exit();

        if (!m_Clip->LoadFromFile(filepath, AudioPlaybackFlagBits::Music | AudioPlaybackFlagBits::ComputeFFT))
            Exit();
    }

    Ref<DesktopWindow> window = GetWindow();
    window->OnDropEvent.Bind([this](const Array<StringView>& filepaths)
    {
        if (filepaths.IsEmpty()) return;
        const StringView& filepath = filepaths[0];
        if (!m_Clip->LoadFromFile(filepath, AudioPlaybackFlagBits::Music | AudioPlaybackFlagBits::ComputeFFT))
            Exit();

        AudioSource* audioSource = audio->GetComponent<AudioSource>();
        audioSource->Stop();
        audioSource->SetAudioClip(m_Clip);
        audioSource->Play();
    });

    Ref<Device> device = GetDevice();
    Ref<Shader> blinnPhongShader = assetDatabase.Get<Shader>("BlinnPhongShader");
    Ref<Texture> checkerTexture = assetDatabase.Get<Texture>("CheckerTexPlaceholder");

    Ref<Material> material = device->CreateMaterial(blinnPhongShader);
    material->SetTexture("diffuseTex", checkerTexture);
    assetDatabase.AddAsset(material, "CubeMaterial");

    Ref<StaticMesh> cubeMesh = assetDatabase.CreateAsset<StaticMesh>("CubeMesh");
    cubeMesh->LoadFromFile(R"(C:\Users\Tiwann\Desktop\Cube.glb)", false);
    cubeMesh->SetMaterial(0, material);

    Scene* scene = new Scene(this, "Main Scene");
    scene->OnInit();

    audio = scene->CreateEntity("Audio");
    audio->AddComponent<AudioListener>();
    AudioSource* audioSource = audio->AddComponent<AudioSource>();
    audioSource->SetAudioClip(m_Clip);

    EntityHandle spectrumEntity = scene->CreateEntity("Spectrum");
    Spectrum* spectrumComponent = spectrumEntity->AddComponent<Spectrum>();
    spectrumComponent->SetAudioSource(audioSource);
    spectrumComponent->SetStaticMesh(cubeMesh);
    spectrumComponent->InitializeSpectrum();

    EntityHandle eclipseEntity = scene->CreateEntity("Eclipse");
    EclipseVisualizer* eclipseVisualizer = eclipseEntity->AddComponent<EclipseVisualizer>();
    eclipseVisualizer->SetAudioSource(audioSource);

    EntityHandle cameraEntity = scene->CreateEntity("Camera");
    Camera* camera = cameraEntity->AddComponent<Camera>();
    camera->SetFieldOfView(45.0f);
    FreeFlyCameraComponent* cameraMovement = cameraEntity->AddComponent<FreeFlyCameraComponent>();
    cameraMovement->SetCamera(camera);

    EntityHandle light = scene->CreateEntity("Light");
    light->AddComponent<DirectionalLight>();
    AmbientLight* ambient = light->AddComponent<AmbientLight>();
    ambient->SetIntensity(0.2f);

    if (m_Clip->IsValid())
        audioSource->Play();

    SceneManager* sceneManager = GetSceneManager();
    sceneManager->LoadScene(scene);
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
