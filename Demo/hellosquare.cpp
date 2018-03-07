#include "framework/application.hpp"
#include "framework/assets.hpp"
#include "framework/common.hpp"
#include "framework/context.hpp"
#include "framework/math.hpp"
#include "platform/platform.hpp"
#include <string.h>

using namespace MaliSDK;
using namespace std;
using namespace glm;

struct Backbuffer{
    VkImage image;
    VkImageView view;
    VkFramebuffer framebuffer;
};

struct Buffer
{
    VkBuffer buffer;
    VkDeviceMemory memory;
};

struct Vertex{
    vec4 position;
    vec4 color;
};

class HelloSquare : public VulkanApplication{
public:
    virtual bool initialize(Context * pContext);
    virtual void updateSwapchain(const std::vector<VkImage> &backbuffers,
                                 const Platform::SwapchainDimensions &dimensions);
    virtual void render(unsigned swapchainIndex, float deltaTime);
    virtual void terminate();

private:
    Context * pContext;

    vector<Backbuffer> backbuffers;
    unsigned width, height;

    VkRenderPass renderPass;

    VkPipeline pipeline;

    VkPipelineCache  pipelineCache;

    VkPipelineLayout pipelineLayout;

    Buffer vertexBuffer;
    Buffer vertexBuffer2;

    Buffer createBuffer(const void * pInitialData, size_t size, VkFlags usage);
    uint32_t findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements);

    void initRenderPass(VkFormat format);
    void termBackbuffers();

    void initVertexBuffer();
    void initPipeline();
};

uint32_t HelloSquare::findMemoryTypeFromRequirements(uint32_t deviceRequirements,
                                                     uint32_t hostRequirements) {
    const VkPhysicalDeviceMemoryProperties & props = pContext->getPlatform().getMemoryProperties();
    for(uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++){
        if(deviceRequirements & (1u << i)){
            if((props.memoryTypes[i].propertyFlags & hostRequirements) == hostRequirements){
                return i;
            }
        }
    }

    LOGE("Failed to obtain suitable memory type.");
    abort();
}

bool HelloSquare::initialize(Context *pContext) {
    this->pContext = pContext;

    initVertexBuffer();

    VkPipelineCacheCreateInfo pipelineCacheInfo = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
    VK_CHECK(vkCreatePipelineCache(pContext->getDevice(), &pipelineCacheInfo, nullptr, &pipelineCache));

    return true;
}

void HelloSquare::render(unsigned swapchainIndex, float deltaTime) {
    Backbuffer &backbuffer = backbuffers[swapchainIndex];

    VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkClearValue clearValue;
    clearValue.color.float32[0] = 0.1f;
    clearValue.color.float32[1] = 0.1f;
    clearValue.color.float32[2] = 0.2f;
    clearValue.color.float32[3] = 1.0f;

    VkRenderPassBeginInfo rpBegin = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rpBegin.renderPass = renderPass;
    rpBegin.framebuffer = backbuffer.framebuffer;
    rpBegin.renderArea.extent.width = width;
    rpBegin.renderArea.extent.height = height;
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearValue;
    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkViewport vp = {0};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = float(width);
    vp.height = float(height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D scissor;
    memset(&scissor, 0, sizeof(scissor));
    scissor.extent.width = width;
    scissor.extent.height = height;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer2.buffer, &offset);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    pContext->submitSwapchain(cmd);
}

void HelloSquare::termBackbuffers() {
    VkDevice  device = pContext->getDevice();

    if(!backbuffers.empty()){
        vkQueueWaitIdle(pContext->getGraphicsQueue());
        for(auto &backbuffer : backbuffers){
            vkDestroyFramebuffer(device, backbuffer.framebuffer, nullptr);
            vkDestroyImageView(device, backbuffer.view, nullptr);
        }
        backbuffers.clear();
        vkDestroyRenderPass(device, renderPass, nullptr);
        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
}

void HelloSquare::terminate() {
    vkDeviceWaitIdle(pContext->getDevice());

    VkDevice device = pContext->getDevice();
    vkFreeMemory(device, vertexBuffer.memory, nullptr);
    vkDestroyBuffer(device, vertexBuffer.buffer, nullptr);
    vkFreeMemory(device, vertexBuffer2.memory, nullptr);
    vkDestroyBuffer(device, vertexBuffer2.buffer, nullptr);

    termBackbuffers();
    vkDestroyPipelineCache(device, pipelineCache, nullptr);
}

void HelloSquare::updateSwapchain(const std::vector<VkImage> &newBackBuffers,
                                  const Platform::SwapchainDimensions &dimensions) {
    VkDevice  device = pContext->getDevice();
    this->width = dimensions.width;
    this->height = dimensions.height;

    termBackbuffers();

    initRenderPass(dimensions.format);
    initPipeline();

    for(auto image : newBackBuffers){
        Backbuffer backbuffer;
        backbuffer.image = image;

        VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = dimensions.format;
        view.image = image;
        view.subresourceRange.baseMipLevel = 0;
        view.subresourceRange.baseArrayLayer = 0;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.components.r = VK_COMPONENT_SWIZZLE_R;
        view.components.g = VK_COMPONENT_SWIZZLE_G;
        view.components.b = VK_COMPONENT_SWIZZLE_B;
        view.components.a = VK_COMPONENT_SWIZZLE_A;

        VK_CHECK(vkCreateImageView(device, &view, nullptr, &backbuffer.view));

        VkFramebufferCreateInfo fbInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &backbuffer.view;
        fbInfo.width = width;
        fbInfo.height = height;
        fbInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &backbuffer.framebuffer));

        backbuffers.push_back(backbuffer);
    }
}

Buffer HelloSquare::createBuffer(const void *pInitialData, size_t size, VkFlags usage) {
    Buffer buffer;
    VkDevice  device = pContext->getDevice();

    VkBufferCreateInfo info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    info.usage = usage;
    info.size = size;

    VK_CHECK(vkCreateBuffer(device, &info, nullptr, &buffer.buffer));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer.buffer, &memReqs);

    VkMemoryAllocateInfo alloc = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = memReqs.size;

    alloc.memoryTypeIndex = findMemoryTypeFromRequirements(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &buffer.memory));

    vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);

    if(pInitialData){
        void * pData;
        VK_CHECK(vkMapMemory(device, buffer.memory, 0, size, 0, &pData));
        memcpy(pData, pInitialData, size);
        vkUnmapMemory(device, buffer.memory);
    }

    return buffer;
}

void HelloSquare::initRenderPass(VkFormat format) {
    VkAttachmentDescription attachment = {0};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass = {0};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency = {0};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(pContext->getDevice(), &rpInfo, nullptr, &renderPass));
}

void HelloSquare::initVertexBuffer() {
    static const Vertex data[] = {
            {
                    vec4(-0.5f, -0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
            },
            {
                    vec4(-0.5f, +0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
            },
            {
                    vec4(+0.5f, +0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
            },
            {
                    vec4(+0.5f, -0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
            }
    };

    vertexBuffer = createBuffer(data, sizeof(data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    static const Vertex data2[] = {
            {
                    vec4(+0.5f, +0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
            },
            {
                    vec4(+0.5f, -0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
            },
            {
                vec4(-0.5f, -0.5f, 0.0f, 1.0f), vec4(1.0f, 0.0f, 0.0f, 1.0f),
            }
    };
    vertexBuffer2 = createBuffer(data2, sizeof(data2), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void HelloSquare::initPipeline() {
    VkDevice  device = pContext->getDevice();
    VkPipelineLayoutCreateInfo layoutInfo = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
    };
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkVertexInputAttributeDescription attributes[2] = {{0}};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[0].offset = 0;
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[1].offset = 4 * sizeof(float);

    VkVertexInputBindingDescription binding = {0};
    binding.binding = 0;
    binding.stride = sizeof(Vertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInput = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attributes;

    VkPipelineRasterizationStateCreateInfo raster = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthClampEnable = false;
    raster.rasterizerDiscardEnable = false;
    raster.depthBiasEnable = false;
    raster.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachment = {0};
    blendAttachment.blendEnable = false;
    blendAttachment.colorWriteMask = 0xf;

    VkPipelineColorBlendStateCreateInfo blend = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;

    VkPipelineViewportStateCreateInfo viewport = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = false;
    depthStencil.depthWriteEnable = false;
    depthStencil.depthBoundsTestEnable = false;
    depthStencil.stencilTestEnable = false;

    VkPipelineMultisampleStateCreateInfo multisample = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    static const VkDynamicState dynamics[] = {
            VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.pDynamicStates = dynamics;
    dynamic.dynamicStateCount = sizeof(dynamics) / sizeof(dynamics[0]);

    VkPipelineShaderStageCreateInfo shaderStages[2] = {
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
            { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
    };

    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = loadShaderModule(device, "shaders/triangle.vert.spv");
    shaderStages[0].pName = "main";
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = loadShaderModule(device, "shaders/triangle.frag.spv");
    shaderStages[1].pName = "main";

    VkGraphicsPipelineCreateInfo pipe = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipe.stageCount = 2;
    pipe.pStages = shaderStages;
    pipe.pVertexInputState = &vertexInput;
    pipe.pInputAssemblyState = &inputAssembly;
    pipe.pRasterizationState = &raster;
    pipe.pColorBlendState = &blend;
    pipe.pMultisampleState = &multisample;
    pipe.pViewportState = &viewport;
    pipe.pDepthStencilState = &depthStencil;
    pipe.pDynamicState = &dynamic;

    pipe.renderPass = renderPass;
    pipe.layout = pipelineLayout;

    VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipe, nullptr, &pipeline));

    vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
}

VulkanApplication * MaliSDK::createApplication() {
    return new HelloSquare();
}