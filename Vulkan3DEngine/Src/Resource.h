#pragma once
#include "Prerequisites.h"
#include <filesystem>

class Resource
{
public:
	Resource();
	Resource(const std::filesystem::path& full_path);
	virtual ~Resource();

	virtual void Load(const std::filesystem::path& full_path) = 0;
	virtual void Reload() = 0;

	std::filesystem::path getName() const {
		return m_full_path.filename();
	}
protected:
	std::filesystem::path m_full_path;
};
