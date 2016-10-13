/* Hisilicon Hibmc SoC drm driver
 *
 * Based on the bochs drm driver.
 *
 * Copyright (c) 2016 Huawei Limited.
 *
 * Author:
 *	Rongrong Zou <zourongrong@huawei>
 *	Rongrong Zou <zourongrong@gmail.com>
 *	Jianhua Li <lijianhua@huawei.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#ifndef HIBMC_DRM_DRV_H
#define HIBMC_DRM_DRV_H

#include <drm/drmP.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_gem_cma_helper.h>

struct hibmc_framebuffer {
	struct drm_framebuffer fb;
	struct drm_gem_cma_object *obj;
	bool is_fbdev_fb;
};

struct hibmc_fbdev {
	struct hibmc_framebuffer fb;
	struct drm_fb_helper helper;
	bool initialized;
};

struct hibmc_drm_device {
	/* hw */
	void __iomem   *mmio;
	void __iomem   *fb_map;
	unsigned long  fb_base;
	unsigned long  fb_size;

	/* drm */
	struct drm_device  *dev;
	struct drm_plane plane;
	struct drm_crtc crtc;
	struct drm_encoder encoder;
	struct drm_connector connector;
	bool mode_config_initialized;

	/* fbdev */
	struct hibmc_fbdev fbdev;
};

int hibmc_plane_init(struct hibmc_drm_device *hidev);
int hibmc_crtc_init(struct hibmc_drm_device *hidev);
int hibmc_encoder_init(struct hibmc_drm_device *hidev);
int hibmc_connector_init(struct hibmc_drm_device *hidev);
int hibmc_fbdev_init(struct hibmc_drm_device *hidev);
void hibmc_fbdev_fini(struct hibmc_drm_device *hidev);

#endif
