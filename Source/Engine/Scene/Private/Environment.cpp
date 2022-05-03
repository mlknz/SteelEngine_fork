#include "Engine/Scene/Environment.hpp"

#include "Engine/Engine.hpp"
#include "Engine/Render/RenderContext.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"
#include "Engine/Scene/DirectLighting.hpp"
#include "Engine/Scene/ImageBasedLighting.hpp"
#include "Engine/Filesystem/Filesystem.hpp"

namespace Details
{
    static constexpr vk::Extent2D kMaxEnvironmentExtent(1024, 1024);

    static vk::Extent2D GetEnvironmentExtent(const vk::Extent2D& panoramaExtent)
    {
        Assert(panoramaExtent.width / panoramaExtent.height == 2);

        const uint32_t height = panoramaExtent.height / 2;

        if (height < kMaxEnvironmentExtent.height)
        {
            return vk::Extent2D(height, height);
        }

        return kMaxEnvironmentExtent;
    }

    static Texture CreateEnvironmentTexture(const Texture& panoramaTexture)
    {
        const vk::Extent2D& panoramaExtent = VulkanHelpers::GetExtent2D(
                VulkanContext::imageManager->GetImageDescription(panoramaTexture.image).extent);

        const vk::Extent2D environmentExtent = GetEnvironmentExtent(panoramaExtent);

        return VulkanContext::textureManager->CreateCubeTexture(panoramaTexture, environmentExtent);
    }

    static std::vector<Environment::Data> CreateDemoEnvironments(const std::vector<Filepath>& paths)
    {
        std::vector<Environment::Data> environments;
        
        for (const auto& path : paths)
        {
            const Texture panoramaTexture = VulkanContext::textureManager->CreateTexture(path);

            const Environment::Data environment{
                Details::CreateEnvironmentTexture(panoramaTexture),
                RenderContext::directLighting->RetrieveDirectLight(panoramaTexture),
                path.GetBaseName()
            };

            environments.push_back(environment);

            VulkanContext::textureManager->DestroyTexture(panoramaTexture);
        }

        return environments;
    }
}

Environment::Environment(const std::vector<Filepath>& paths)
{
    const Texture panoramaTexture = VulkanContext::textureManager->CreateTexture(paths.front());

    texture = Details::CreateEnvironmentTexture(panoramaTexture);
    directLight = RenderContext::directLighting->RetrieveDirectLight(panoramaTexture);
    iblTextures = RenderContext::imageBasedLighting->GenerateTextures(texture);

    demoEnvironments = Details::CreateDemoEnvironments(paths);
    Engine::settings.environment.count = static_cast<int32_t>(demoEnvironments.size());

    VulkanContext::textureManager->DestroyTexture(panoramaTexture);
}

Environment::~Environment()
{
    VulkanContext::textureManager->DestroyTexture(texture);
    VulkanContext::textureManager->DestroyTexture(iblTextures.irradiance);
    VulkanContext::textureManager->DestroyTexture(iblTextures.reflection);
    for (const auto& demo : demoEnvironments)
    {
        VulkanContext::textureManager->DestroyTexture(demo.texture);
    }
}
