#pragma once
#include "..\Application\Application.h"
#include "..\Prerequisites.h"
#include "Renderer\Renderer.h"
#include "..\WindowSytem\Window.h"
#include "ResourceManager\MeshManager\MeshManager.h"
#include "ResourceManager\TextureManager\TextureManager.h"

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

    RendererPtr getRenderer() { return m_renderer; };
    MeshManagerPtr getMeshManager() { return m_meshManager; };
    TextureManagerPtr getTextureManager() { return m_textureManager; };

private:
    //Resources
    static GraphicsEngine* m_engine;
    WindowPtr m_window;
    RendererPtr m_renderer;
    MeshManagerPtr m_meshManager;
    TextureManagerPtr m_textureManager;
};

