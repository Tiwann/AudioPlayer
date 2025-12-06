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
#include "Rendering/Shader.h"
#include <imgui.h>

#include "Visualizers/CubeVisualizer.h"
#include "Editor/HierarchyWindow.h"
#include "Editor/InspectorWindow.h"
#include "Visualizers/BarsVisualizer.h"

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
        LoadAudioFile(args.GetArgument(1));

    Ref<DesktopWindow> window = GetWindow();
    window->OnDropEvent.Bind([this](const Array<StringView>& filepaths)
    {
        LoadAudioFile(filepaths[0]);

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

    if (false)
    {
        EntityHandle spectrumEntity = scene->CreateEntity("Spectrum");
        Spectrum* spectrumComponent = spectrumEntity->AddComponent<Spectrum>();
        spectrumComponent->SetAudioSource(audioSource);
        spectrumComponent->SetStaticMesh(cubeMesh);
        spectrumComponent->InitializeSpectrum();
    }


    EntityHandle visualizerEntity = scene->CreateEntity("2D Bars");
    BarsVisualizer* visualizer = visualizerEntity->AddComponent<BarsVisualizer>();
    visualizer->SetAudioSource(audioSource);

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
        if (window->GetKeyDown(KeyCode::F11))
        {
            ReloadAllShaders();
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
                LoadAudioFile(filepath);

                AudioSource* audioSource = audio->GetComponent<AudioSource>();
                auto fft = m_Clip->GetFFTAudioNode();
                fft->SetFFTWindow(FFTWindow::BlackmanHarris);

                audioSource->Stop();
                audioSource->SetAudioClip(m_Clip);
                audioSource->Play();
            }
            if (ImGui::MenuItem("Exit")) Exit();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Editor"))
        {
            Ref<InspectorWindow> inpectorWindow = GetEditorWindow<InspectorWindow>();
            Ref<HierarchyWindow> hierarchyWindow = GetEditorWindow<HierarchyWindow>();
            ImGui::MenuItem("Inspector", nullptr, inpectorWindow->GetShownPointer());
            ImGui::MenuItem("Hierarchy", nullptr, hierarchyWindow->GetShownPointer());
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("About"))
        {
            ImGui::TextLinkOpenURL("Github", "https://github.com/tiwann");
            ImGui::TextLinkOpenURL("Nova Engine", "https://github.com/tiwann/novaengine");
            ImGui::TextLinkOpenURL("Instagram", "https://instagram.com/prodtiwann");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Shaders"))
        {
            if (ImGui::MenuItem("Reload All"))
                ReloadAllShaders();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void AudioPlayerApplication::OnDestroy()
{
    Application::OnDestroy();
}

void AudioPlayerApplication::LoadAudioFile(const StringView filepath)
{
    if (filepath.IsEmpty())
        Exit();

    if (!m_Clip->LoadFromFile(filepath, AudioPlaybackFlagBits::Music | AudioPlaybackFlagBits::ComputeFFT))
        Exit();
}

void AudioPlayerApplication::ReloadAllShaders()
{
    const SceneManager* sceneManager = GetSceneManager();
    Scene* scene = sceneManager->GetActiveScene();
    for (const Entity* entity : scene->GetEntities())
    {
        if (Visualizer* visualizer = entity->GetComponent<Visualizer>())
            visualizer->ReloadPipelines();
    }
}
