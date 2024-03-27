#pragma once
#include "..\Application\Application.h"
#include "..\Prerequisites.h"
#include "Device\Device.h"
#include "Renderer\Renderer.h"
#include "..\WindowSytem\Window.h"

#include <iostream>

class GraphicsEngine
{
private:
    GraphicsEngine(WindowPtr window);
    ~GraphicsEngine();
public:
    static GraphicsEngine* get();
    static void create(WindowPtr window);
    static void release();

    DevicePtr getDevice() { return m_device; };
    RendererPtr getRenderer() { return m_renderer; };

private:
    //Resources
    static GraphicsEngine* m_engine;
    WindowPtr m_window;
    DevicePtr m_device;
    RendererPtr m_renderer;
};

