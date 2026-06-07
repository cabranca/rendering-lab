#pragma once

#include "VulkanContext.h"

namespace lab {

    class VulkanCore {
        public:
        void init();
        void shutdown();

        private:
        VulkanContext m_VulkanCtx;
    };
}