#pragma once
#include "Prerequisites.h"
#include "Instance.h"
#include "Window.h"
#include "Device.h"
#include "Renderer.h"
#include "MeshManager.h"
#include "TextureManager.h"
#include "ModelDataManager.h"

#include <iostream>

class GraphicsEngine
{
private:
    GraphicsEngine();
    ~GraphicsEngine();
public:
    static GraphicsEngine* get();
    static void create();
    static void release();
    void init();

    WindowPtr getWindow() { return m_window; };
    DevicePtr getDevice() { return m_device; };
    RendererPtr getRenderer() { return m_renderer; };
    MeshManagerPtr getMeshManager() { return m_meshManager; };
    TextureManagerPtr getTextureManager() { return m_textureManager; };
    ModelDataManagerPtr getModelDataManager() { return m_modelDataManager; };
private:
    //Resources
    static GraphicsEngine* m_engine;
    WindowPtr m_window;
    const uint32_t m_window_width = 1280;
    const uint32_t m_window_height = 720;
    DevicePtr m_device;
    RendererPtr m_renderer;
    MeshManagerPtr m_meshManager;
    TextureManagerPtr m_textureManager;
    ModelDataManagerPtr m_modelDataManager;
};

