#include "Object.h"

Object::Object(std::vector<Vertex> vertices)
{
	m_mesh = std::make_shared<Mesh>(vertices);
}

Object::~Object()
{
}

void Object::update()
{
}

void Object::draw()
{
	m_mesh->m_vertexBuffer->bind(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer());
}
