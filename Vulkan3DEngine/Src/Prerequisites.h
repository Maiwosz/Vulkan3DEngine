#pragma once
#include <memory>

class Window;
class GraphicsEngine;
class Device;
class Renderer;
class SwapChain;

typedef std::shared_ptr<Window> WindowPtr;
typedef std::shared_ptr<SwapChain> SwapChainPtr;
