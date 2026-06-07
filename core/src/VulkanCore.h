#pragma once

#include "Swapchain.h"
#include "VulkanContext.h"

namespace lab {

    class VulkanCore {
        public:
        void init();
        void shutdown();

        private:
        VulkanContext m_VulkanCtx;
        Swapchain m_Swapchain;
    };
}