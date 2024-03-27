#pragma once
#include <memory>

class Window;
class GraphicsEngine;
class Device;
class Renderer;
class SwapChain;
class GraphicsPipeline;

typedef std::shared_ptr<Window> WindowPtr;
typedef std::shared_ptr<Device> DevicePtr;
typedef std::shared_ptr<Renderer> RendererPtr;
typedef std::shared_ptr<SwapChain> SwapChainPtr;
typedef std::shared_ptr<GraphicsPipeline> GraphicsPipelinePtr;
