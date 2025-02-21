// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/vulkan/swapchain_vk.h"

#include "fml/logging.h"
#include "impeller/base/validation.h"

namespace impeller {

std::unique_ptr<SwapchainVK> SwapchainVK::Create(vk::Device device,
                                                 vk::SurfaceKHR surface,
                                                 SwapchainDetailsVK& details) {
  vk::SurfaceFormatKHR surface_format = details.PickSurfaceFormat();
  vk::PresentModeKHR present_mode = details.PickPresentationMode();
  vk::Extent2D extent = details.PickExtent();

  vk::SwapchainCreateInfoKHR create_info;
  create_info.surface = surface;
  create_info.imageFormat = surface_format.format;
  create_info.imageColorSpace = surface_format.colorSpace;
  create_info.presentMode = present_mode;
  create_info.imageExtent = extent;

  create_info.minImageCount = details.GetImageCount();
  create_info.imageArrayLayers = 1;
  create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  create_info.preTransform = details.GetTransform();
  create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  create_info.clipped = VK_TRUE;

  create_info.imageSharingMode = vk::SharingMode::eExclusive;
  {
    // TODO (kaushikiska): This needs to change if graphics queue is not the
    // same as present queue which is rare.
    create_info.queueFamilyIndexCount = 0;
    create_info.pQueueFamilyIndices = nullptr;
  }

  create_info.oldSwapchain = nullptr;

  auto swapchain_res = device.createSwapchainKHRUnique(create_info);
  if (swapchain_res.result != vk::Result::eSuccess) {
    VALIDATION_LOG << "Failed to create swapchain: "
                   << vk::to_string(swapchain_res.result);
    return nullptr;
  }

  auto swapchain = std::make_unique<SwapchainVK>(std::move(swapchain_res.value),
                                                 surface_format.format, extent);
  if (!swapchain->CreateSwapchainImages(device)) {
    VALIDATION_LOG << "Failed to create swapchain images.";
    return nullptr;
  }

  return swapchain;
}

bool SwapchainVK::CreateSwapchainImages(vk::Device device) {
  FML_DCHECK(swapchain_images_.empty()) << "Swapchain images already created";
  auto res = device.getSwapchainImagesKHR(*swapchain_);
  if (res.result != vk::Result::eSuccess) {
    FML_CHECK(false) << "Failed to get swapchain images: "
                     << vk::to_string(res.result);
    return false;
  }

  std::vector<vk::Image> images = res.value;
  for (const auto& image : images) {
    vk::ImageViewCreateInfo create_info;
    create_info.image = image;
    create_info.viewType = vk::ImageViewType::e2D;
    create_info.format = image_format_;

    create_info.components.r = vk::ComponentSwizzle::eIdentity;
    create_info.components.g = vk::ComponentSwizzle::eIdentity;
    create_info.components.b = vk::ComponentSwizzle::eIdentity;
    create_info.components.a = vk::ComponentSwizzle::eIdentity;

    // TODO (kaushikiska): This has to match up to the texture we will be
    // rendering to (TextureVK).
    create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    auto img_view_res = device.createImageViewUnique(create_info);
    if (img_view_res.result != vk::Result::eSuccess) {
      VALIDATION_LOG << "Failed to create image view: "
                     << vk::to_string(img_view_res.result);
      return false;
    }

    swapchain_images_.push_back(std::make_unique<SwapchainImageVK>(
        image, std::move(img_view_res.value), image_format_, extent_));
  }

  return true;
}

SwapchainVK::SwapchainVK(vk::UniqueSwapchainKHR swapchain,
                         vk::Format image_format,
                         vk::Extent2D extent)
    : swapchain_(std::move(swapchain)),
      image_format_(image_format),
      extent_(extent) {}

SwapchainVK::~SwapchainVK() = default;

SwapchainImageVK::SwapchainImageVK(vk::Image image,
                                   vk::UniqueImageView image_view,
                                   vk::Format image_format,
                                   vk::Extent2D extent)
    : image_(image),
      image_view_(std::move(image_view)),
      image_format_(image_format),
      extent_(extent) {}

SwapchainImageVK::~SwapchainImageVK() = default;

}  // namespace impeller
