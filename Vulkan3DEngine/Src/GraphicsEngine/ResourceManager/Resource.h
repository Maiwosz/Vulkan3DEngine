#pragma once
#include "../../Prerequisites.h"
#include <string>

class Resource
{
public:
	Resource();
	Resource(const char* full_path);
	virtual ~Resource();

	virtual void Load(const char* full_path) = 0;
	virtual void Reload() = 0;
protected:
	std::string m_full_path;
};

