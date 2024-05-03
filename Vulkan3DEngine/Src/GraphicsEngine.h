#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "Renderer.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "ModelDataManager.h"

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
    ModelDataManagerPtr getModelDataManager() { return m_modelDataManager; };
private:
    //Resources
    static GraphicsEngine* m_engine;
    WindowPtr m_window;
    RendererPtr m_renderer;
    MeshManagerPtr m_meshManager;
    TextureManagerPtr m_textureManager;
    ModelDataManagerPtr m_modelDataManager;
};

