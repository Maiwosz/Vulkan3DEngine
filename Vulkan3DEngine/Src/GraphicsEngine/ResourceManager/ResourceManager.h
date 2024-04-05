#pragma once
#include "../../Prerequisites.h"
#include "Resource.h"
#include <unordered_map>
#include <string>

class ResourceManager
{
public:
	ResourceManager();
	virtual ~ResourceManager();

	ResourcePtr createResourceFromFile(const char* file_path);
protected:
	virtual Resource* createResourceFromFileConcrete(const char* file_path) = 0;
	std::unordered_map<std::string, ResourcePtr> m_map_resources;
private:
	
};

