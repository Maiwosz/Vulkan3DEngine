#pragma once

// Forward declaration
class Renderer;
class EngineCore;

/**
 * Abstract base for GPU command execution
 *
 * Provides the foundation for command pattern implementation,
 * allowing different types of GPU operations to be queued and executed
 * with unified interface while maintaining access to renderer state.
 */
class GpuCall {
public:
    virtual ~GpuCall() = default;

    /**
     * Execute the GPU command with access to renderer context
     *
     * @param renderer Reference to the renderer providing GPU context
     * @return true if command executed successfully
     */
    virtual bool execute(Renderer& renderer, EngineCore& engineCore) = 0;
};