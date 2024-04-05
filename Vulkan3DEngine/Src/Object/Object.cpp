#include "Object.h"
#include "../GraphicsEngine/ResourceManager/MeshManager/Mesh.h"
#include "../GraphicsEngine/ResourceManager/TextureManager/Texture.h"

Object::Object(MeshPtr mesh, TexturePtr texture):m_mesh(mesh), m_texture(texture)
{

}

Object::~Object()
{
}

void Object::update()
{
}

void Object::draw()
{
	m_mesh->draw();
	m_texture->draw();

	GraphicsEngine::get()->getRenderer()->bindDescriptorSets();

	if (m_mesh->m_hasIndexedBuffer) {
		vkCmdDrawIndexed(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), static_cast<uint32_t>(m_mesh->m_indices.size()), 1, 0, 0, 0);
	}
	else {
		vkCmdDraw(GraphicsEngine::get()->getRenderer()->getCurrentCommandBuffer(), 3, 1, 0, 0);
	}
}
