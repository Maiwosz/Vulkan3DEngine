#pragma once
#include "../../Prerequisites.h"
#include <string>

class Resource
{
public:
	Resource();
	Resource(const char* full_path);
	virtual ~Resource();
protected:
	std::string m_full_path;
};

