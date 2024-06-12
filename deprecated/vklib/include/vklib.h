#pragma once

#include "vklib-base.hpp"
#include "vklib-buffer.hpp"
#include "vklib-command-pool.hpp"
#include "vklib-debug-utils.hpp"
#include "vklib-descriptor.hpp"
#include "vklib-device.hpp"
#include "vklib-framebuffer.hpp"
#include "vklib-image.hpp"
#include "vklib-instance.hpp"
#include "vklib-pipeline.hpp"
#include "vklib-renderpass.hpp"
#include "vklib-shader.hpp"
#include "vklib-surface.hpp"
#include "vklib-swapchain.hpp"
#include "vklib-synchronize.hpp"
#include "vklib-tools.hpp"
#include "vklib-utility-values.hpp"
#include "vklib-vma.hpp"
#include "vklib-window.hpp"

#if __SIZEOF_POINTER__ == 4
#error "32 bit architecture is not supported!"
#endif