#pragma once

#include <volk.h>
#include <stdexcept>

// Shared low-level Vulkan helpers used by UiRenderer and UiFontManager.

inline uint32_t UiFindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits,
                                 VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    throw std::runtime_error("UiRenderer: no suitable memory type");
}

// Uploads CPU pixel data to a new device-local VkImage via a staging buffer.
// Submits a one-time command buffer and waits for completion before returning.
// The caller owns outMemory and must free it with vkFreeMemory on teardown.
VkImage UiUploadImage(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkQueue queue, VkCommandBuffer commandBuffer,
                      const void *pixels, VkDeviceSize dataSize,
                      uint32_t width, uint32_t height,
                      VkFormat format, VkImageUsageFlags usage,
                      VkDeviceMemory &outMemory);
