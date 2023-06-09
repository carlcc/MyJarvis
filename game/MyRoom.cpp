// Copyright (c) 2008-2022 the Urho3D project
// License: MIT

#include "httplib.h"
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/Skybox.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Technique.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/ObjectAnimation.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/ValueAnimation.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Sprite.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include "MyRoom.h"

#include <Urho3D/DebugNew.h>

URHO3D_DEFINE_APPLICATION_MAIN(MyRoom)

MyRoom::MyRoom(Context* context)
    : Sample(context)
{
}

MyRoom::~MyRoom()
{
    httpServer_->stop();
    if (httpServerThread_.joinable())
    {
        httpServerThread_.join();
    }
}

void MyRoom::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the UI content
    CreateInstructions();

    CreateHttpServer();

    // Create the scene content
    CreateScene();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_RELATIVE);
}

void MyRoom::CreateHttpServer()
{
    httpServerThread_ = std::thread(
        [this]()
        {
            httpServer_ = std::make_unique<httplib::Server>();
            httpServer_->Post("/cmd",
                              [this](const httplib::Request& req, httplib::Response& res) { OnHttpRequest(req, res); });
            httpServer_->listen("0.0.0.0", 8888);
            httpServer_ = nullptr;
        });
}

void MyRoom::OnHttpRequest(const httplib::Request& req, httplib::Response& res)
{
    const String reqBodyString(req.body.c_str());
    JSONFile jsonFile(context_);
    if (!jsonFile.FromString(reqBodyString))
    {
        // Invalid
        return;
    }

    URHO3D_LOGDEBUG("Got request: " + reqBodyString);

    auto& json = jsonFile.GetRoot();
    // as a demo, we skip the validation here
    auto& jsonObj = json.GetObject();
    auto cmd = jsonObj["cmd"]->GetString();
    if (cmd == "lighton")
    {
        auto& target = jsonObj["target"]->GetString();
        eventQueue_.Push(
            [target, this]()
            {
                if (target == "sun")
                {
                    auto* scene = scene_.Get();
                    Node* zoneNode = scene->CreateChild("Zone");
                    zoneNode->AddTag(target);
                    auto* zone = zoneNode->CreateComponent<Zone>();
                    zone->SetBoundingBox(BoundingBox(-1000.0f, 1000.0f));
                    zone->SetAmbientColor(Color(0.5f, 0.5f, 0.5f));
                    zone->SetFogColor(Color(0.4f, 0.5f, 0.8f));
                    zone->SetFogStart(100.0f);
                    zone->SetFogEnd(300.0f);

                    auto* cache = GetSubsystem<ResourceCache>();
                    auto* skyNode = scene->CreateChild("SkyNode");
                    skyNode->AddTag(target);
                    skyNode->SetScale(500);
                    auto* skybox = skyNode->CreateComponent<Skybox>();
                    skybox->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
                    skybox->SetMaterial(cache->GetResource<Material>("Materials/Skybox.xml"));

                    Node* lightNode = scene_->CreateChild("DirectionalLight");
                    lightNode->SetDirection(Vector3(0.6f, -1.0f, 0.8f));
                    lightNode->AddTag(target);
                    auto* light = lightNode->CreateComponent<Light>();
                    light->SetLightType(LIGHT_DIRECTIONAL);
                    light->SetCastShadows(true);
                    light->SetColor(Color(0.5f, 0.5f, 0.5f));
                    light->SetShadowBias(BiasParameters(0.00025f, 0.5f));
                    light->SetShadowCascade(CascadeParameters(10.0f, 50.0f, 200.0f, 0.0f, 0.8f));
                }
                else if (target == "disco")
                {
                    CreateDiscoLight(target);
                }
                else if (target == "welcome")
                {
                    eventQueue_.Push([target, this]() { CreateWelcome(target); });
                }
            });
    }
    else if (cmd == "lightoff")
    {
        auto& target = jsonObj["target"]->GetString();
        eventQueue_.Push(
            [target, this]()
            {
                PODVector<Node*> nodes;
                scene_->GetNodesWithTag(nodes, target);
                for (auto* node : nodes)
                {
                    node->Remove();
                }
            });
    }
    res.set_header("content-type", "application/json");
    res.status = 200;
    res.body = R"json({"code": 0})json";
}

void MyRoom::CreateDiscoLight(const String& tag)
{
    {
        // Create a point light to the world so that we can see something.
        Node* lightNode = scene_->CreateChild("PointLight");
        lightNode->AddTag(tag);
        auto* light = lightNode->CreateComponent<Light>();
        light->SetLightType(LIGHT_POINT);
        light->SetRange(10.0f);

        // Create light animation
        SharedPtr<ObjectAnimation> MyRoom(new ObjectAnimation(context_));

        // Create light position animation
        SharedPtr<ValueAnimation> positionAnimation(new ValueAnimation(context_));
        // Use spline interpolation method
        positionAnimation->SetInterpolationMethod(IM_SPLINE);
        // Set spline tension
        positionAnimation->SetSplineTension(0.7f);
        positionAnimation->SetKeyFrame(0.0f, Vector3(-30.0f, 5.0f, -30.0f));
        positionAnimation->SetKeyFrame(1.0f, Vector3(30.0f, 5.0f, -30.0f));
        positionAnimation->SetKeyFrame(2.0f, Vector3(30.0f, 5.0f, 30.0f));
        positionAnimation->SetKeyFrame(3.0f, Vector3(-30.0f, 5.0f, 30.0f));
        positionAnimation->SetKeyFrame(4.0f, Vector3(-30.0f, 5.0f, -30.0f));
        // Set position animation
        MyRoom->AddAttributeAnimation("Position", positionAnimation);

        // Create light color animation
        SharedPtr<ValueAnimation> colorAnimation(new ValueAnimation(context_));
        colorAnimation->SetKeyFrame(0.0f, Color::WHITE);
        colorAnimation->SetKeyFrame(1.0f, Color::RED);
        colorAnimation->SetKeyFrame(2.0f, Color::YELLOW);
        colorAnimation->SetKeyFrame(3.0f, Color::GREEN);
        colorAnimation->SetKeyFrame(4.0f, Color::WHITE);
        // Set Light component's color animation
        MyRoom->AddAttributeAnimation("@Light/Color", colorAnimation);

        // Apply light animation to light node
        lightNode->SetObjectAnimation(MyRoom);
    }

    for (int i = 0; i < 20; ++i)
    {
        // Create a point light to the world so that we can see something.
        Node* lightNode = scene_->CreateChild("PointLight");
        lightNode->AddTag(tag);
        auto* light = lightNode->CreateComponent<Light>();
        light->SetLightType(LIGHT_POINT);
        light->SetRange(10.0f);

        // Create light animation
        SharedPtr<ObjectAnimation> MyRoom(new ObjectAnimation(context_));

        // Create light position animation
        SharedPtr<ValueAnimation> positionAnimation(new ValueAnimation(context_));
        // Use spline interpolation method
        positionAnimation->SetInterpolationMethod(Urho3D::IM_SPLINE);
        // Set spline tension
        positionAnimation->SetSplineTension(0.7f);
        float lastT = 0.0F;
        std::random_device rd;
        std::uniform_real_distribution<> dist(0.0F, 1.0F);
        for (int j = 20; j >= 0; --j)
        {
            Vector3 position{-60.0F + (float)dist(rd) * 120, 3.0F, -60.0F + (float)dist(rd) * 120};
            if (j == 0)
            {
                auto& f = positionAnimation->GetKeyFrames()[0];
                positionAnimation->SetKeyFrame(lastT, f.value_.GetVector3());
            }
            else
            {
                positionAnimation->SetKeyFrame(lastT, position);
            }
            lastT += (float)dist(rd) * 3.0F + 0.5F;
        }
        // Set position animation
        MyRoom->AddAttributeAnimation("Position", positionAnimation);

        // Create light color animation
        SharedPtr<ValueAnimation> colorAnimation(new ValueAnimation(context_));
        std::vector<Color> colors{Color::WHITE, Color::GRAY, Color::RED,     Color::GREEN,
                                  Color::BLUE,  Color::CYAN, Color::MAGENTA, Color::YELLOW};
        std::random_shuffle(colors.begin(), colors.end());
        for (std::size_t j = 0; j < colors.size(); ++j)
        {
            colorAnimation->SetKeyFrame(j + dist(rd) * 0.5F, colors[j]);
        }
        // Set Light component's color animation
        MyRoom->AddAttributeAnimation("@Light/Color", colorAnimation);

        // Apply light animation to light node
        lightNode->SetObjectAnimation(MyRoom);
    }
}

void MyRoom::CreateWelcome(const Urho3D::String& tag)
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* box = scene_->CreateChild("WelcomeNode");
    box->AddTag(tag);
    box->SetScale(10);
    auto* model = box->CreateComponent<StaticModel>();
    model->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
    SharedPtr<Material> renderMaterial(new Material(context_));
    auto* texture = cache->GetResource<Texture2D>("Textures/welcome.png");
    renderMaterial->SetTechnique(0, cache->GetResource<Technique>("Techniques/DiffNormal.xml"));
    renderMaterial->SetTexture(TU_DIFFUSE, texture);
    // Since the screen material is on top of the box model and may Z-fight, use negative depth
    // bias to push it forward (particularly necessary on mobiles with possibly less Z
    // resolution)
    renderMaterial->SetDepthBias(BiasParameters(-0.001f, 0.0f));
    model->SetMaterial(renderMaterial);
    model->SetCastShadows(true);

    SharedPtr<ObjectAnimation> objectAnimation(new ObjectAnimation(context_));
    {
        // Create light position animation
        SharedPtr<ValueAnimation> rotAnimation(new ValueAnimation(context_));
        Quaternion quat1, quat2, quat3;
        quat1.FromAngleAxis(0, Vector3::UP);
        quat2.FromAngleAxis(180, Vector3::UP);
        quat3.FromAngleAxis(360, Vector3::UP);

        // Use spline interpolation method
        rotAnimation->SetInterpolationMethod(Urho3D::IM_LINEAR);
        // Set spline tension
        rotAnimation->SetKeyFrame(0.0f, quat1);
        rotAnimation->SetKeyFrame(3.0f, quat2);
        rotAnimation->SetKeyFrame(6.0f, quat3);
        // Set position animation
        objectAnimation->AddAttributeAnimation("Rotation", rotAnimation);
    }
    {
        SharedPtr<ValueAnimation> posAnimation(new ValueAnimation(context_));
        posAnimation->SetInterpolationMethod(Urho3D::IM_SPLINE);
        posAnimation->SetSplineTension(0.5F);
        posAnimation->SetKeyFrame(0.0F, Vector3{0, 10, 0});
        posAnimation->SetKeyFrame(2.5F, Vector3{0, 20, 0});
        posAnimation->SetKeyFrame(5.F, Vector3{0, 10, 0});
        objectAnimation->AddAttributeAnimation("Position", posAnimation);
    }

    box->SetObjectAnimation(objectAnimation);
}

void MyRoom::CreateScene()
{
    auto* cache = GetSubsystem<ResourceCache>();

    scene_ = new Scene(context_);

    // Create the Octree component to the scene. This is required before adding any drawable components, or else nothing
    // will show up. The default octree volume will be from (-1000, -1000, -1000) to (1000, 1000, 1000) in world
    // coordinates; it is also legal to place objects outside the volume but their visibility can then not be checked in
    // a hierarchically optimizing manner
    scene_->CreateComponent<Octree>();

    // Create a child scene node (at world origin) and a StaticModel component into it. Set the StaticModel to show a
    // simple plane mesh with a "stone" material. Note that naming the scene nodes is optional. Scale the scene node
    // larger (100 x 100 world units)
    Node* planeNode = scene_->CreateChild("Plane");
    planeNode->SetScale(Vector3(100.0f, 1.0f, 100.0f));
    auto* planeObject = planeNode->CreateComponent<StaticModel>();
    planeObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
    planeObject->SetMaterial(cache->GetResource<Material>("Materials/StoneTiled.xml"));

    //        auto* cache = GetSubsystem<ResourceCache>();
    // Create more StaticModel objects to the scene, randomly positioned, rotated and scaled. For rotation, we construct
    // a quaternion from Euler angles where the Y angle (rotation about the Y axis) is randomized. The mushroom model
    // contains LOD levels, so the StaticModel component will automatically select the LOD level according to the view
    // distance (you'll see the model get simpler as it moves further away). Finally, rendering a large number of the
    // same object with the same material allows instancing to be used, if the GPU supports it. This reduces the amount
    // of CPU work in rendering the scene.
    const unsigned NUM_OBJECTS = 200;
    for (unsigned i = 0; i < NUM_OBJECTS; ++i)
    {
        Node* mushroomNode = scene_->CreateChild("Mushroom");
        mushroomNode->SetPosition(Vector3(Random(90.0f) - 45.0f, 0.0f, Random(90.0f) - 45.0f));
        mushroomNode->SetRotation(Quaternion(0.0f, Random(360.0f), 0.0f));
        mushroomNode->SetScale(0.5f + Random(2.0f));
        auto* mushroomObject = mushroomNode->CreateComponent<StaticModel>();
        mushroomObject->SetModel(cache->GetResource<Model>("Models/Mushroom.mdl"));
        mushroomObject->SetMaterial(cache->GetResource<Material>("Materials/Mushroom.xml"));
        mushroomObject->SetCastShadows(true);
    }

    // Create a scene node for the camera, which we will move around
    // The camera will use default settings (1000 far clip distance, 45 degrees FOV, set aspect ratio automatically)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));
}

void MyRoom::CreateInstructions()
{
    auto* cache = GetSubsystem<ResourceCache>();
    //    auto* ui = GetSubsystem<UI>();
    //
    //    // Construct new Text object, set string to display and font to use
    //    auto* instructionText = ui->GetRoot()->CreateChild<Text>();
    //    instructionText->SetText("Use WASD keys and mouse/touch to move");
    //    auto* font = cache->GetResource<Font>("Fonts/Anonymous Pro.ttf");
    //    instructionText->SetFont(font, 15);
    //
    //    // Position the text relative to the screen center
    //    instructionText->SetHorizontalAlignment(HA_CENTER);
    //    instructionText->SetVerticalAlignment(VA_CENTER);
    //    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
    //
    //    // Animating text
    //    auto* text = ui->GetRoot()->CreateChild<Text>("animatingText");
    //    text->SetFont(font, 15);
    //    text->SetHorizontalAlignment(HA_CENTER);
    //    text->SetVerticalAlignment(VA_CENTER);
    //    text->SetPosition(0, ui->GetRoot()->GetHeight() / 4 + 20);
    //
    //    // Animating sprite in the top left corner
    //    auto* sprite = ui->GetRoot()->CreateChild<Sprite>("animatingSprite");
    //    sprite->SetPosition(8, 8);
    //    sprite->SetSize(64, 64);
}

void MyRoom::SetupViewport()
{
    auto* renderer = GetSubsystem<Renderer>();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the
    // camera at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward /
    // deferred) to use, but now we just use full screen and default render path configured in the engine command line
    // options
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void MyRoom::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    auto* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    // Use the Translate() function (default local space) to move relative to the node's orientation.
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void MyRoom::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(MyRoom, HandleUpdate));
}

void MyRoom::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    {
        std::function<void()> task;
        while (eventQueue_.Pop(task))
        {
            task();
        }
    }

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}
