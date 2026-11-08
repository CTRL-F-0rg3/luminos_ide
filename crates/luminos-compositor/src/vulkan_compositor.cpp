// src/vulkan_compositor.cpp (fragment)
#include "vulkan_compositor.hpp"
#include <vulkan/vulkan_core.h>

namespace Luminos {

VkImage VulkanCompositor::importClientBuffer(const WindowBuffer& buffer) {
    VkImage image;

    // 1. Tworzymy VkImage z odpowiednimi flagami external
    VkExternalMemoryImageCreateInfo extImageInfo = {};
    extImageInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
    extImageInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.pNext = &extImageInfo;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { (uint32_t)buffer.width, (uint32_t)buffer.height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM; // Uproszczenie, w rzeczywistości mapujemy DRM formaty
    imageInfo.tiling = VK_IMAGE_TILING_LINEAR; // DMA-BUF jest zwykle linear
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(device, &imageInfo, nullptr, &image);

    // 2. Pobieramy wymagania pamięci dla tego obrazu
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    // 3. Importujemy pamięć z FD (DMA-BUF)
    VkImportMemoryFdInfoKHR importInfo = {};
    importInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
    importInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
    importInfo.fd = buffer.dma_buf_fd;

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &importInfo;
    allocInfo.allocationSize = memRequirements.size;
    // Tutaj trzeba znaleźć pamięć, która obsługuje VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 
    // i jest kompatybilna z zewnętrznym FD (vkGetMemoryFdPropertiesKHR)

    VkDeviceMemory memory;
    vkAllocateMemory(device, &allocInfo, nullptr, &memory);

    // 4. Wiążemy pamięć z obrazem
    vkBindImageMemory(device, image, memory, 0);

    // UWAGA: W pełnej implementacji trzeba tu dodać VkImageView i transition 
    // z UNDEFINED do SHADER_READ_ONLY_OPTIMAL.

    return image;
}

} // namespace Luminos
