#pragma once
#include "VramHandle.h"
#include "AssetLib.h"
#include "ShaderModuleManager.h"

struct VramMesh {
    VramHandle vertexBuffer;
    VramHandle indexBuffer;
};

struct VramTexture {
    VramHandle image;
};

