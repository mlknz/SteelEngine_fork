#include "Engine/Render/Stages/GBufferStage.hpp"

#include "Engine/ConsoleVariable.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Render/Vulkan/Pipelines/GraphicsPipeline.hpp"
#include "Engine/Render/Vulkan/RenderPass.hpp"
#include "Engine/Render/Vulkan/VulkanHelpers.hpp"
#include "Engine/Render/Vulkan/Pipelines/MaterialPipelineCache.hpp"
#include "Engine/Render/Vulkan/Resources/ImageHelpers.hpp"
#include "Engine/Render/Vulkan/Resources/ResourceContext.hpp"
#include "Engine/Scene/Components/Components.hpp"
#include "Engine/Scene/Primitive.hpp"
#include "Engine/Scene/Scene.hpp"

namespace Details
{
    static std::unique_ptr<RenderPass> CreateRenderPass()
    {
        std::vector<RenderPass::AttachmentDescription> attachments(GBufferStage::kFormats.size());

        for (size_t i = 0; i < attachments.size(); ++i)
        {
            if (ImageHelpers::IsDepthFormat(GBufferStage::kFormats[i]))
            {
                attachments[i] = RenderPass::AttachmentDescription{
                    RenderPass::AttachmentUsage::eDepth,
                    GBufferStage::kFormats[i],
                    vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eStore,
                    vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    vk::ImageLayout::eShaderReadOnlyOptimal
                };
            }
            else
            {
                attachments[i] = RenderPass::AttachmentDescription{
                    RenderPass::AttachmentUsage::eColor,
                    GBufferStage::kFormats[i],
                    vk::AttachmentLoadOp::eClear,
                    vk::AttachmentStoreOp::eStore,
                    vk::ImageLayout::eGeneral,
                    vk::ImageLayout::eColorAttachmentOptimal,
                    vk::ImageLayout::eGeneral
                };
            }
        }

        const RenderPass::Description description{
            vk::PipelineBindPoint::eGraphics,
            vk::SampleCountFlagBits::e1,
            attachments
        };

        const std::vector<PipelineBarrier> followingDependencies{
            PipelineBarrier{
                SyncScope::kColorAttachmentWrite | SyncScope::kDepthStencilAttachmentWrite,
                SyncScope::kComputeShaderRead
            }
        };

        std::unique_ptr<RenderPass> renderPass = RenderPass::Create(description,
                RenderPass::Dependencies{ {}, followingDependencies });

        return renderPass;
    }

    static std::vector<RenderTarget> CreateRenderTargets()
    {
        const vk::Extent2D& extent = VulkanContext::swapchain->GetExtent();

        std::vector<RenderTarget> renderTargets(GBufferStage::kFormats.size());

        for (size_t i = 0; i < renderTargets.size(); ++i)
        {
            constexpr vk::ImageUsageFlags colorImageUsage
                    = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage;

            constexpr vk::ImageUsageFlags depthImageUsage
                    = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;

            const vk::Format format = GBufferStage::kFormats[i];

            const vk::SampleCountFlagBits sampleCount = vk::SampleCountFlagBits::e1;

            const vk::ImageUsageFlags imageUsage = ImageHelpers::IsDepthFormat(format)
                    ? depthImageUsage : colorImageUsage;

            renderTargets[i] = ResourceContext::CreateBaseImage({
                .format = format,
                .extent = extent,
                .sampleCount = sampleCount,
                .usage = imageUsage
            });
        }

        VulkanContext::device->ExecuteOneTimeCommands([&renderTargets](vk::CommandBuffer commandBuffer)
            {
                const ImageLayoutTransition colorLayoutTransition{
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eGeneral,
                    PipelineBarrier::kEmpty
                };

                const ImageLayoutTransition depthLayoutTransition{
                    vk::ImageLayout::eUndefined,
                    vk::ImageLayout::eDepthStencilAttachmentOptimal,
                    PipelineBarrier::kEmpty
                };

                for (size_t i = 0; i < renderTargets.size(); ++i)
                {
                    const vk::Image image = renderTargets[i].image;

                    const vk::Format format = GBufferStage::kFormats[i];

                    const vk::ImageSubresourceRange& subresourceRange = ImageHelpers::IsDepthFormat(format)
                            ? ImageHelpers::kFlatDepth : ImageHelpers::kFlatColor;

                    const ImageLayoutTransition& layoutTransition = ImageHelpers::IsDepthFormat(format)
                            ? depthLayoutTransition : colorLayoutTransition;

                    ImageHelpers::TransitImageLayout(commandBuffer, image, subresourceRange, layoutTransition);
                }
            });

        return renderTargets;
    }

    static vk::Framebuffer CreateFramebuffer(const RenderPass& renderPass,
            const std::vector<RenderTarget>& renderTargets)
    {
        const vk::Device device = VulkanContext::device->Get();

        const vk::Extent2D& extent = VulkanContext::swapchain->GetExtent();

        std::vector<vk::ImageView> views;
        views.reserve(renderTargets.size());

        for (const auto& [image, view] : renderTargets)
        {
            views.push_back(view);
        }

        return VulkanHelpers::CreateFramebuffers(device, renderPass.Get(), extent, {}, views).front();
    }

    static bool ShouldRenderMaterial(MaterialFlags flags)
    {
        static const CVarBool& forceForwardCVar = CVarBool::Get("r.ForceForward");

        if (forceForwardCVar.GetValue())
        {
            return false;
        }

        return !(flags & MaterialFlagBits::eAlphaBlend);
    }

    static std::unique_ptr<MaterialPipelineCache> CreatePipelineCache(const RenderPass& renderPass)
    {
        return std::make_unique<MaterialPipelineCache>(MaterialPipelineStage::eGBuffer, renderPass.Get());
    }

    static void CreateDescriptors(const Scene& scene, DescriptorProvider& descriptorProvider)
    {
        const auto& renderComponent = scene.ctx().get<RenderContextComponent>();
        const auto& textureComponent = scene.ctx().get<TextureStorageComponent>();

        descriptorProvider.PushGlobalData("materials", renderComponent.materialBuffer);
        descriptorProvider.PushGlobalData("materialTextures", &textureComponent.textures);

        for (const auto& frameBuffer : renderComponent.frameBuffers)
        {
            descriptorProvider.PushSliceData("frame", frameBuffer);
        }

        descriptorProvider.FlushData();
    }

    static std::vector<vk::ClearValue> GetClearValues()
    {
        std::vector<vk::ClearValue> clearValues(GBufferStage::kAttachmentCount);

        for (size_t i = 0; i < clearValues.size(); ++i)
        {
            if (ImageHelpers::IsDepthFormat(GBufferStage::kFormats[i]))
            {
                clearValues[i] = VulkanHelpers::GetDefaultClearDepthStencilValue();
            }
            else
            {
                clearValues[i] = VulkanHelpers::kDefaultClearColorValue;
            }
        }

        return clearValues;
    }
}

GBufferStage::GBufferStage()
{
    renderPass = Details::CreateRenderPass();
    renderTargets = Details::CreateRenderTargets();

    framebuffer = Details::CreateFramebuffer(*renderPass, renderTargets);

    pipelineCache = Details::CreatePipelineCache(*renderPass);
}

GBufferStage::~GBufferStage()
{
    RemoveScene();

    for (const auto& texture : renderTargets)
    {
        ResourceContext::DestroyResource(texture);
    }

    VulkanContext::device->Get().destroyFramebuffer(framebuffer);
}

void GBufferStage::RegisterScene(const Scene* scene_)
{
    RemoveScene();

    scene = scene_;
    Assert(scene);

    uniquePipelines = RenderHelpers::CacheMaterialPipelines(
            *scene, *pipelineCache, &Details::ShouldRenderMaterial);

    if (!uniquePipelines.empty())
    {
        Details::CreateDescriptors(*scene, pipelineCache->GetDescriptorProvider());
    }
}

void GBufferStage::RemoveScene()
{
    if (!scene)
    {
        return;
    }

    uniquePipelines.clear();

    scene = nullptr;
}

void GBufferStage::Update()
{
    Assert(scene);

    if (scene->ctx().get<MaterialStorageComponent>().updated)
    {
        uniquePipelines = RenderHelpers::CacheMaterialPipelines(
                *scene, *pipelineCache, &Details::ShouldRenderMaterial);
    }

    if (!uniquePipelines.empty())
    {
        const auto& textureComponent = scene->ctx().get<TextureStorageComponent>();

        if (textureComponent.updated)
        {
            DescriptorProvider& descriptorProvider = pipelineCache->GetDescriptorProvider();

            descriptorProvider.PushGlobalData("materialTextures", &textureComponent.textures);

            descriptorProvider.FlushData();
        }
    }
}

void GBufferStage::Execute(vk::CommandBuffer commandBuffer, uint32_t imageIndex) const
{
    const vk::Rect2D renderArea = RenderHelpers::GetSwapchainRenderArea();
    const vk::Viewport viewport = RenderHelpers::GetSwapchainViewport();
    const std::vector<vk::ClearValue> clearValues = Details::GetClearValues();

    const vk::RenderPassBeginInfo beginInfo(
            renderPass->Get(), framebuffer,
            renderArea, clearValues);

    commandBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

    commandBuffer.setViewport(0, { viewport });
    commandBuffer.setScissor(0, { renderArea });

    DrawScene(commandBuffer, imageIndex);

    commandBuffer.endRenderPass();
}

void GBufferStage::Resize()
{
    for (const auto& texture : renderTargets)
    {
        ResourceContext::DestroyResource(texture);
    }

    VulkanContext::device->Get().destroyFramebuffer(framebuffer);

    renderTargets = Details::CreateRenderTargets();

    framebuffer = Details::CreateFramebuffer(*renderPass, renderTargets);
}

void GBufferStage::ReloadShaders() const
{
    pipelineCache->ReloadPipelines();
}

void GBufferStage::DrawScene(vk::CommandBuffer commandBuffer, uint32_t imageIndex) const
{
    Assert(scene);

    const auto& materialComponent = scene->ctx().get<MaterialStorageComponent>();
    const auto& geometryComponent = scene->ctx().get<GeometryStorageComponent>();

    for (const auto& materialFlags : uniquePipelines)
    {
        const GraphicsPipeline& pipeline = pipelineCache->GetPipeline(materialFlags);

        const DescriptorProvider& descriptorProvider = pipelineCache->GetDescriptorProvider();

        pipeline.Bind(commandBuffer);

        pipeline.BindDescriptorSets(commandBuffer, descriptorProvider.GetDescriptorSlice(imageIndex));

        for (auto&& [entity, tc, rc] : scene->view<TransformComponent, RenderComponent>().each())
        {
            for (const auto& ro : rc.renderObjects)
            {
                if (materialComponent.materials[ro.material].flags == materialFlags)
                {
                    pipeline.PushConstant(commandBuffer, "transform", tc.GetWorldTransform().GetMatrix());

                    pipeline.PushConstant(commandBuffer, "materialIndex", ro.material);

                    const Primitive& primitive = geometryComponent.primitives[ro.primitive];

                    primitive.Draw(commandBuffer);
                }
            }
        }
    }
}
