#include "Object.h"

Object::Object(MeshPtr mesh):m_mesh(mesh)
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
}
