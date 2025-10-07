#pragma once
#include <typeindex>

// Forward declarations
class Renderer;
class EngineCore;
class RenderNode;

/**
 * Abstract base for GPU command execution
 *
 * Provides the foundation for command pattern implementation,
 * allowing different types of GPU operations to be queued and executed
 * with unified interface while maintaining access to renderer state and render node context.
 */
class GpuCall {
public:
    virtual ~GpuCall() = default;

    /**
     * Execute the GPU command with access to renderer and render node context
     *
     * @param renderer Reference to the renderer providing GPU context
     * @param engineCore Reference to engine core for resource access
     * @param renderNode Reference to the current render node being executed
     * @return true if command executed successfully
     */
    virtual bool execute(Renderer& renderer, EngineCore& engineCore, RenderNode& renderNode) = 0;

    // Type identification for filtering
    virtual std::type_index getTypeIndex() const = 0;
};