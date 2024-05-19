#include "Resource.h"

Resource::Resource()
{
}

Resource::Resource(const std::filesystem::path& full_path) : m_full_path(full_path)
{
}

Resource::~Resource()
{
}