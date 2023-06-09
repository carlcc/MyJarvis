// Copyright (c) 2008-2022 the Urho3D project
// License: MIT

#pragma once

#include "Sample.h"
#include "SimpleThreadSafeQueue.h"
#include <memory>

namespace Urho3D
{

class Node;
class Scene;

} // namespace Urho3D

namespace httplib
{
class Server;
class Request;
class Response;
} // namespace httplib

/// Light animation example.
/// This sample is base on StaticScene, and it demonstrates:
///     - Usage of attribute animation for light color & UI animation
class MyRoom : public Sample
{
    URHO3D_OBJECT(MyRoom, Sample);

public:
    /// Construct.
    explicit MyRoom(Context* context);
    ~MyRoom() override;

    /// Setup after engine initialization and before running the main loop.
    void Start() override;

private:
    /// Create an http server to handle commands
    void CreateHttpServer();
    void OnHttpRequest(const httplib::Request& req, httplib::Response& res);
    void CreateDiscoLight(const String& tag);
    void CreateWelcome(const String& tag);

    /// Construct the scene content.
    void CreateScene();
    /// Construct an instruction text to the UI.
    void CreateInstructions();
    /// Set up a viewport for displaying the scene.
    void SetupViewport();
    /// Read input and moves the camera.
    void MoveCamera(float timeStep);
    /// Subscribe to application-wide logic update events.
    void SubscribeToEvents();
    /// Handle the logic update event.
    void HandleUpdate(StringHash eventType, VariantMap& eventData);

private:
    std::thread httpServerThread_{};
    std::unique_ptr<httplib::Server> httpServer_{nullptr};
    SimpleThreadSafeQueue<std::function<void()>> eventQueue_{};
};
