#include "RenderGraphTemplate.h"
#include "EngineCore.h"
#include "RenderNodeManager.h"

RenderGraphTemplate::RenderGraphTemplate(EngineCore& engineCore)
    : m_engineCore(engineCore), m_nodeManager(engineCore.renderNodeManager()) {
}