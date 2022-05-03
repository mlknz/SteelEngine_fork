#pragma once
#include "Engine/Scene/ImageBasedLighting.hpp"
#include "Engine/Render/Vulkan/Resources/TextureHelpers.hpp"
#include "Shaders/Common/Common.h"

class Filepath;

class Environment
{
public:
    struct Data
    {
        Texture texture;
        DirectLight directLight;
        std::string name;
    };

    Environment(const std::vector<Filepath>& paths);
    ~Environment();

    const Texture& GetTexture() const { return texture; }

    const std::vector<Data>& GetDemoData() const { return demoEnvironments; }

    const DirectLight& GetDirectLight() const { return directLight; }

    const Texture& GetIrradianceTexture() const { return iblTextures.irradiance; }

    const Texture& GetReflectionTexture() const { return iblTextures.reflection; }

private:
    Texture texture;

    DirectLight directLight;

    ImageBasedLighting::Textures iblTextures;

    std::vector<Data> demoEnvironments;
};
