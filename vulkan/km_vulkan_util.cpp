#include "km_vulkan_util.h"

VkCommandBuffer BeginOneTimeCommands(VkDevice device, VkCommandPool commandPool)
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void EndOneTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue queue, VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

uint32_t FindMemoryTypeIndex(VkPhysicalDeviceMemoryProperties properties,
                             uint32_t typeFilter, VkMemoryPropertyFlags propertyFlags)
{
    uint32_t memoryTypeIndex = properties.memoryTypeCount;
    for (uint32_t i = 0; i < properties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i) &&
            (properties.memoryTypes[i].propertyFlags & propertyFlags) == propertyFlags) {
            memoryTypeIndex = i;
            break;
        }
    }

    return memoryTypeIndex;
}

Vec2 ScreenPosToNdc(Vec2Int pos, Vec2Int screenSize)
{
	return Vec2 {
        2.0f * pos.x / screenSize.x - 1.0f,
        2.0f * pos.y / screenSize.y - 1.0f
    };
}

RectCoordsNdc ToRectCoordsNdc(Vec2Int pos, Vec2Int size, Vec2Int screenSize)
{
	return RectCoordsNdc {
        .pos = ScreenPosToNdc(pos, screenSize),
        .size = {
            2.0f * size.x / screenSize.x,
            2.0f * size.y / screenSize.y
        },
    };
}

RectCoordsNdc ToRectCoordsNdc(Vec2Int pos, Vec2Int size, Vec2 anchor, Vec2Int screenSize)
{
	RectCoordsNdc result;
	result.pos = { (float32)pos.x, (float32)pos.y };
	result.size = { (float32)size.x, (float32)size.y };
	result.pos.x -= anchor.x * result.size.x;
	result.pos.y -= anchor.y * result.size.y;
	result.pos.x = result.pos.x * 2.0f / screenSize.x - 1.0f;
	result.pos.y = result.pos.y * 2.0f / screenSize.y - 1.0f;
	result.size.x *= 2.0f / screenSize.x;
	result.size.y *= 2.0f / screenSize.y;
	return result;
}

QueueFamilyInfo GetQueueFamilyInfo(VkSurfaceKHR surface, VkPhysicalDevice physicalDevice, LinearAllocator* allocator)
{
    QueueFamilyInfo info;
    info.hasGraphicsFamily = false;
    info.hasPresentFamily = false;
    info.hasComputeFamily = false;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    Array<VkQueueFamilyProperties> queueFamilies = allocator->NewArray<VkQueueFamilyProperties>(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data);

    for (uint32 i = 0; i < queueFamilies.size; i++) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, (uint32_t)i, surface, &presentSupport);
        if (presentSupport) {
            info.hasPresentFamily = true;
            info.presentFamilyIndex = (uint32_t)i;
        }

        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            info.hasGraphicsFamily = true;
            info.graphicsFamilyIndex = (uint32_t)i;
        }

        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            info.hasComputeFamily = true;
            info.computeFamilyIndex = (uint32_t)i;
        }
    }

    return info;
}

bool CreateShaderModule(const Array<uint8> code, VkDevice device, VkShaderModule* shaderModule)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size;
    createInfo.pCode = (const uint32_t*)code.data;

    return vkCreateShaderModule(device, &createInfo, nullptr, shaderModule) == VK_SUCCESS;
}

bool CreateVulkanBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryPropertyFlags,
                        VkDevice device, VkPhysicalDevice physicalDevice, VulkanBuffer* buffer)
{
    VkBufferCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size = size;
    createInfo.usage = usage;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.flags = 0;

    if (vkCreateBuffer(device, &createInfo, nullptr, &buffer->buffer) != VK_SUCCESS) {
        LOG_ERROR("vkCreateBuffer failed\n");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer->buffer, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = FindMemoryTypeIndex(memProperties, memRequirements.memoryTypeBits, memoryPropertyFlags);
    if (memoryTypeIndex == memProperties.memoryTypeCount) {
        LOG_ERROR("Failed to find suitable memory type\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, &buffer->memory) != VK_SUCCESS) {
        LOG_ERROR("vkAllocateMemory failed\n");
        return false;
    }

    vkBindBufferMemory(device, buffer->buffer, buffer->memory, 0);

    return true;
}

void DestroyVulkanBuffer(VkDevice device, VulkanBuffer* buffer)
{
    vkDestroyBuffer(device, buffer->buffer, nullptr);
    vkFreeMemory(device, buffer->memory, nullptr);
}

void CopyBuffer(VkCommandBuffer commandBuffer, VkBuffer src, VkBuffer dst, VkDeviceSize size)
{
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, src, dst, 1, &copyRegion);
}

bool CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format,
                 VkImageTiling tiling, VkImageUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
                 VkImage* image, VkDeviceMemory* imageMemory)
{
    VkImageCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    createInfo.imageType = VK_IMAGE_TYPE_2D;
    createInfo.extent.width = width;
    createInfo.extent.height = height;
    createInfo.extent.depth = 1;
    createInfo.mipLevels = 1;
    createInfo.arrayLayers = 1;
    createInfo.format = format;
    createInfo.tiling = tiling;
    createInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    createInfo.usage = usageFlags;
    createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    createInfo.flags = 0;

    if (vkCreateImage(device, &createInfo, nullptr, image) != VK_SUCCESS) {
        LOG_ERROR("vkCreateImage failed\n");
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, *image, &memRequirements);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = FindMemoryTypeIndex(memProperties, memRequirements.memoryTypeBits, memoryPropertyFlags);
    if (memoryTypeIndex == memProperties.memoryTypeCount) {
        LOG_ERROR("Failed to find suitable memory type\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    if (vkAllocateMemory(device, &allocInfo, nullptr, imageMemory) != VK_SUCCESS) {
        LOG_ERROR("vkAllocateMemory failed\n");
        return false;
    }

    vkBindImageMemory(device, *image, *imageMemory, 0);

    return true;
}

void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage;
    VkPipelineStageFlags dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else {
        srcStage = 0;
        dstStage = 0;
        DEBUG_PANIC("Unsupported layout transition\n");
    }

    vkCmdPipelineBarrier(commandBuffer, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0, };
    region.imageExtent = { width, height, 1 };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

bool CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags,
                     VkImageView* imageView)
{
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspectFlags;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &createInfo, nullptr, imageView) != VK_SUCCESS) {
        LOG_ERROR("vkCreateImageView failed\n");
        return false;
    }

    return true;
}

bool LoadVulkanImage(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                     uint32 width, uint32 height, uint32 channels, const uint8* data, VulkanImage* image)
{
    VkFormat format;
    switch (channels) {
        case 1: {
            format = VK_FORMAT_R8_UNORM;
        } break;
        case 4: {
            format = VK_FORMAT_R8G8B8A8_UNORM;
        } break;
        default: {
            LOG_ERROR("Unsupported image number of channels: %lu\n", channels);
            return false;
        } break;
    }

    const VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
    const VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageFormatProperties properties;
    const VkResult formatResult = vkGetPhysicalDeviceImageFormatProperties(physicalDevice, format, VK_IMAGE_TYPE_2D,
                                                                           tiling, usageFlags, 0, &properties);
    if (formatResult != VK_SUCCESS) {
        LOG_ERROR("vkGetPhysicalDeviceImageFormatProperties returned %d for image format %d", formatResult, format);
        return false;
    }

#if 0
    // TODO might want something like this eventually that covers all image formats we'll be using
    VkFormat monochromeFormats[] = {
        VK_FORMAT_R8_UNORM,
        VK_FORMAT_R8_SNORM,
        VK_FORMAT_R8_USCALED,
        VK_FORMAT_R8_SSCALED,
        VK_FORMAT_R8_UINT,
        VK_FORMAT_R8_SINT,
        VK_FORMAT_R8_SRGB,
    };
    for (int i = 0; i < C_ARRAY_LENGTH(monochromeFormats); i++) {
        VkImageFormatProperties properties;
        VkResult result = vkGetPhysicalDeviceImageFormatProperties(physicalDevice, monochromeFormats[i],
                                                                   VK_IMAGE_TYPE_2D, tiling, usageFlags, 0, &properties);
        LOG_INFO("%d - result %d\n", i, result);
    }
#endif

    // Create image
    if (!CreateImage(device, physicalDevice, width, height, format, tiling, usageFlags,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &image->image, &image->memory)) {
        LOG_ERROR("CreateImage failed\n");
        return false;
    }

    // Copy image data using a memory-mapped staging buffer
    const VkDeviceSize imageSize = width * height * channels;
    VulkanBuffer stagingBuffer;
    if (!CreateVulkanBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            device, physicalDevice, &stagingBuffer)) {
        LOG_ERROR("CreateBuffer failed for staging buffer\n");
        return false;
    }
    defer(DestroyVulkanBuffer(device, &stagingBuffer));

    void* memoryMappedData;
    vkMapMemory(device, stagingBuffer.memory, 0, imageSize, 0, &memoryMappedData);
    MemCopy(memoryMappedData, data, imageSize);
    vkUnmapMemory(device, stagingBuffer.memory);

    {
        SCOPED_VK_COMMAND_BUFFER(commandBuffer, device, commandPool, queue);

        TransitionImageLayout(commandBuffer, image->image,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(commandBuffer, stagingBuffer.buffer, image->image, width, height);
        TransitionImageLayout(commandBuffer, image->image,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Create image view
    if (!CreateImageView(device, image->image, format, VK_IMAGE_ASPECT_COLOR_BIT, &image->view)) {
        LOG_ERROR("CreateImageView failed\n");
        return false;
    }

    return true;
}

void DestroyVulkanImage(VkDevice device, VulkanImage* image)
{
    vkDestroyImageView(device, image->view, nullptr);
    vkDestroyImage(device, image->image, nullptr);
    vkFreeMemory(device, image->memory, nullptr);
}
