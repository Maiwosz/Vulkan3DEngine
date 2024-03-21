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

    Device* getDevice() { return m_device; };
    Renderer* getRenderer() { return m_renderer; };
private:
    //Resources
    static GraphicsEngine* m_engine;
    Device* m_device = nullptr;
    Renderer* m_renderer = nullptr;
};

