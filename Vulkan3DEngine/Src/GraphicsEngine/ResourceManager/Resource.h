#pragma once
#include "../../Prerequisites.h"
#include <string>

class Resource
{
public:
	Resource();
	Resource(const char* full_path);
	virtual ~Resource();

	virtual void Reload() = 0;
protected:
	std::string m_full_path;
};

