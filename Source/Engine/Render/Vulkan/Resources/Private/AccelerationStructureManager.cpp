#include "Engine/Render/Vulkan/Resources/AccelerationStructureManager.hpp"

#include "Engine/Render/Vulkan/Resources/ResourceContext.hpp"
#include "Engine/Render/Vulkan/VulkanContext.hpp"

namespace Details
{
    constexpr vk::BuildAccelerationStructureFlagsKHR kBlasBuildFlags
            = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;

    constexpr vk::BuildAccelerationStructureFlagsKHR kTlasBuildFlags
            = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;

    static vk::BuildAccelerationStructureFlagsKHR GetBuildFlags(vk::AccelerationStructureTypeKHR type)
    {
        return type == vk::AccelerationStructureTypeKHR::eTopLevel ? kTlasBuildFlags : kBlasBuildFlags;
    }

    static vk::AccelerationStructureBuildSizesInfoKHR GetBuildSizesInfo(vk::AccelerationStructureTypeKHR type,
            const vk::AccelerationStructureGeometryKHR& geometry, uint32_t primitiveCount)
    {
        const vk::AccelerationStructureBuildGeometryInfoKHR buildInfo(
                type, GetBuildFlags(type),
                vk::BuildAccelerationStructureModeKHR::eBuild,
                nullptr, nullptr, 1, &geometry, nullptr,
                vk::DeviceOrHostAddressKHR(), nullptr);

        return VulkanContext::device->Get().getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo,
                { primitiveCount });
    }

    static vk::Buffer CreateAccelerationStructureBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage)
    {
        const vk::Buffer buffer = ResourceContext::CreateBuffer({
            .size = size,
            .usage = usage | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            .scratchAlignment = static_cast<bool>(usage & vk::BufferUsageFlagBits::eStorageBuffer)
        });

        return buffer;
    }

    static vk::Buffer CreateEmptyInstanceBuffer(uint32_t instanceCount)
    {
        const vk::BufferUsageFlags usage = vk::BufferUsageFlagBits::eShaderDeviceAddress
                | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
                | vk::BufferUsageFlagBits::eTransferDst;

        const size_t size = instanceCount * sizeof(vk::AccelerationStructureInstanceKHR);

        const vk::Buffer buffer = ResourceContext::CreateBuffer({
            .size = size,
            .usage = usage,
            .stagingBuffer = true
        });

        return buffer;
    }
}

vk::AccelerationStructureKHR AccelerationStructureManager::GenerateBlas(const BlasGeometryData& geometryData)
{
    constexpr vk::AccelerationStructureTypeKHR type = vk::AccelerationStructureTypeKHR::eBottomLevel;

    constexpr vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eShaderDeviceAddress
            | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;

    const vk::Buffer vertexBuffer = ResourceContext::CreateBuffer({
        .usage = bufferUsage,
        .initialData = ByteView(geometryData.vertices)
    });

    const vk::Buffer indexBuffer = ResourceContext::CreateBuffer({
        .usage = bufferUsage,
        .initialData = ByteView(geometryData.indices)
    });

    const vk::AccelerationStructureGeometryTrianglesDataKHR trianglesData(
            geometryData.vertexFormat, VulkanContext::device->GetAddress(vertexBuffer),
            geometryData.vertexStride, geometryData.vertexCount - 1,
            geometryData.indexType, VulkanContext::device->GetAddress(indexBuffer), nullptr);

    const vk::AccelerationStructureGeometryKHR geometry(
            vk::GeometryTypeKHR::eTriangles, trianglesData,
            vk::GeometryFlagsKHR());

    const uint32_t primitiveCount = geometryData.indexCount / 3;

    const vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo
            = Details::GetBuildSizesInfo(type, geometry, primitiveCount);

    AccelerationStructureBuffers buffers;

    buffers.scratchBuffer = Details::CreateAccelerationStructureBuffer(
            buildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer);

    buffers.storageBuffer = Details::CreateAccelerationStructureBuffer(
            buildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);

    const vk::AccelerationStructureCreateInfoKHR createInfo({}, buffers.storageBuffer, 0,
            buildSizesInfo.accelerationStructureSize, type, vk::DeviceAddress());

    const auto [result, blas] = VulkanContext::device->Get().createAccelerationStructureKHR(createInfo);

    const vk::AccelerationStructureBuildGeometryInfoKHR buildInfo(
            type, vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
            vk::BuildAccelerationStructureModeKHR::eBuild,
            nullptr, blas, 1, &geometry, nullptr,
            VulkanContext::device->GetAddress(buffers.scratchBuffer));

    const vk::AccelerationStructureBuildRangeInfoKHR rangeInfo(primitiveCount, 0, 0, 0);
    const vk::AccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    VulkanContext::device->ExecuteOneTimeCommands([&](vk::CommandBuffer commandBuffer)
        {
            commandBuffer.buildAccelerationStructuresKHR({ buildInfo }, { pRangeInfo });
        });

    ResourceContext::DestroyResource(vertexBuffer);
    ResourceContext::DestroyResource(indexBuffer);

    accelerationStructures.emplace(blas, buffers);

    return blas;
}

vk::AccelerationStructureKHR AccelerationStructureManager::CreateTlas(uint32_t instanceCount)
{
    constexpr vk::AccelerationStructureTypeKHR type = vk::AccelerationStructureTypeKHR::eTopLevel;

    AccelerationStructureBuffers buffers;

    buffers.sourceBuffer = Details::CreateEmptyInstanceBuffer(instanceCount);

    const vk::AccelerationStructureGeometryKHR geometry(
            vk::GeometryTypeKHR::eInstances,
            vk::AccelerationStructureGeometryInstancesDataKHR(),
            vk::GeometryFlagBitsKHR::eOpaque);

    const vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo
            = Details::GetBuildSizesInfo(type, geometry, instanceCount);

    buffers.scratchBuffer = Details::CreateAccelerationStructureBuffer(
            buildSizesInfo.buildScratchSize, vk::BufferUsageFlagBits::eStorageBuffer);

    buffers.storageBuffer = Details::CreateAccelerationStructureBuffer(
            buildSizesInfo.accelerationStructureSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR);

    const vk::AccelerationStructureCreateInfoKHR createInfo({}, buffers.storageBuffer, 0,
            buildSizesInfo.accelerationStructureSize, type, vk::DeviceAddress());

    const auto [result, tlas] = VulkanContext::device->Get().createAccelerationStructureKHR(createInfo);

    Assert(result == vk::Result::eSuccess);

    accelerationStructures.emplace(tlas, buffers);

    return tlas;
}

void AccelerationStructureManager::BuildTlas(vk::CommandBuffer commandBuffer,
        vk::AccelerationStructureKHR tlas, const TlasInstances& instances)
{
    constexpr vk::AccelerationStructureTypeKHR type = vk::AccelerationStructureTypeKHR::eTopLevel;

    AccelerationStructureBuffers& buffers = accelerationStructures.at(tlas);

    const BufferUpdate bufferUpdate{
        .data = GetByteView(instances),
        .blockedScope = SyncScope::kAccelerationStructureShaderRead
    };

    ResourceContext::UpdateBuffer(commandBuffer, buffers.sourceBuffer, bufferUpdate);

    const vk::AccelerationStructureGeometryInstancesDataKHR instancesData(
            false, VulkanContext::device->GetAddress(buffers.sourceBuffer));

    const vk::AccelerationStructureGeometryDataKHR geometryData(instancesData);

    const vk::AccelerationStructureGeometryKHR geometry(
            vk::GeometryTypeKHR::eInstances, geometryData,
            vk::GeometryFlagBitsKHR::eOpaque);

    const vk::AccelerationStructureBuildGeometryInfoKHR buildInfo(
            type, Details::GetBuildFlags(type),
            vk::BuildAccelerationStructureModeKHR::eBuild,
            nullptr, tlas, 1, &geometry, nullptr,
            VulkanContext::device->GetAddress(buffers.scratchBuffer));

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    const vk::AccelerationStructureBuildRangeInfoKHR rangeInfo(instanceCount, 0, 0, 0);
    const vk::AccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    commandBuffer.buildAccelerationStructuresKHR({ buildInfo }, { pRangeInfo });

    VulkanHelpers::InsertMemoryBarrier(commandBuffer, PipelineBarrier{
        SyncScope::kAccelerationStructureWrite,
        SyncScope::kRayTracingAccelerationStructureRead,
    });

    const vk::AccelerationStructureBuildSizesInfoKHR buildSizesInfo
            = VulkanContext::device->Get().getAccelerationStructureBuildSizesKHR(
                    vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, { instanceCount });

    const vk::DeviceSize scratchBufferSize = ResourceContext::GetBufferDescription(buffers.scratchBuffer).size;
    const vk::DeviceSize storageBufferSize = ResourceContext::GetBufferDescription(buffers.storageBuffer).size;

    Assert(buildSizesInfo.buildScratchSize <= scratchBufferSize);
    Assert(buildSizesInfo.accelerationStructureSize <= storageBufferSize);
}

void AccelerationStructureManager::DestroyAccelerationStructure(vk::AccelerationStructureKHR accelerationStructure)
{
    const auto it = accelerationStructures.find(accelerationStructure);
    Assert(it != accelerationStructures.end());

    const AccelerationStructureBuffers& buffers = it->second;

    VulkanContext::device->Get().destroyAccelerationStructureKHR(accelerationStructure);

    if (buffers.sourceBuffer)
    {
        ResourceContext::DestroyResource(buffers.sourceBuffer);
    }

    ResourceContext::DestroyResource(buffers.scratchBuffer);
    ResourceContext::DestroyResource(buffers.storageBuffer);

    accelerationStructures.erase(it);
}
