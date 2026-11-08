// include/vulkan_compositor.hpp
#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace Luminos {

struct WindowBuffer {
    int dma_buf_fd;      // Deskryptor pliku z IDE (OpenGL)
    int width;
    int height;
    uint32_t format;     // np. DRM_FORMAT_ARGB8888
};

class VulkanCompositor {
public:
    VulkanCompositor();
    ~VulkanCompositor();

    // Inicjalizacja (instance, device, queue)
    bool initialize();

    // Importuje bufor z IDE (OpenGL) do Vulkan jako tekstura
    VkImage importClientBuffer(const WindowBuffer& buffer);

    // Główna pętla renderowania: czyści ekran i rysuje zaimportowane okna
    void compositeAndPresent(const std::vector<VkImage>& clientImages);

    // Czyszczenie zasobów
    void destroyImage(VkImage image);

private:
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    
    // Proste zasoby do kompozycji (quad + basic shader)
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;
    VkDescriptorSetLayout descriptorSetLayout;
    
    void setupBasicCompositingPipeline();
};

} // namespace Luminos
