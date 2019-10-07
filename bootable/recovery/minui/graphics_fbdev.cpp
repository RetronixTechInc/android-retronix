/*
 * Copyright (C) 2014 The Android Open Source Project
 * Copyright 2017 NXP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "graphics_fbdev.h"

#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "minui/minui.h"

MinuiBackendFbdev::MinuiBackendFbdev() : gr_draw(nullptr), displayed_buffer(0), fb_fd(-1) {}

void MinuiBackendFbdev::Blank(bool blank) {
  int ret = ioctl(fb_fd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
  if (ret < 0) perror("ioctl(): blank");
}

void MinuiBackendFbdev::SetDisplayedFramebuffer(unsigned n) {
  if (n > 2 || !triple_buffered) {
    printf("set_displayed_framebuffer failed n:%d\n", n);
    return;
  }

  vi.activate = FB_ACTIVATE_VBL;
  vi.yoffset = n * gr_framebuffer[n].height;
  if (ioctl(fb_fd, FBIOPAN_DISPLAY, &vi) < 0) {
    perror("active fb swap failed");
  }
  displayed_buffer = (n + 1)%3;
}

GRSurface* MinuiBackendFbdev::Init() {
  int fd = open("/dev/graphics/fb0", O_RDWR);
  if (fd == -1) {
    displayed_buffer = 0;
    perror("cannot open fb0");
    return nullptr;
  }

  fb_fix_screeninfo fi;
  if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
    perror("failed to get fb0 info");
    close(fd);
    return nullptr;
  }

  void* bits = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (bits == MAP_FAILED) {
    perror("failed to mmap framebuffer");
    close(fd);
    return nullptr;
  }

  memset(bits, 0, fi.smem_len);
  if (munmap(bits,fi.smem_len) < 0) {
    perror("failed to munmap framebuffer");
    close(fd);
    return nullptr;
  }

  if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
    perror("failed to get fb0 info");
    close(fd);
    return nullptr;
  }

  vi.red.offset     = 0;
  vi.red.length     = 8;
  vi.green.offset   = 8;
  vi.green.length   = 8;
  vi.blue.offset    = 16;
  vi.blue.length    = 8;
  vi.transp.offset  = 24;
  vi.transp.length  = 8;
  vi.bits_per_pixel = 32;
  vi.xres_virtual = vi.xres;
  vi.yres_virtual = vi.yres * 3;
  vi.activate = FB_ACTIVATE_NOW;

  if (ioctl(fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
    perror("failed to put fb0 info");
    close(fd);
    return nullptr;
  }

  if (ioctl(fd, FBIOGET_FSCREENINFO, &fi) < 0) {
    perror("failed to get fb0 info");
    close(fd);
    return nullptr;
  }

  if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) < 0) {
    perror("failed to get fb0 info");
    close(fd);
    return nullptr;
  }

  // We print this out for informational purposes only, but
  // throughout we assume that the framebuffer device uses an RGBX
  // pixel format.  This is the case for every development device I
  // have access to.  For some of those devices (eg, hammerhead aka
  // Nexus 5), FBIOGET_VSCREENINFO *reports* that it wants a
  // different format (XBGR) but actually produces the correct
  // results on the display when you write RGBX.
  //
  // If you have a device that actually *needs* another pixel format
  // (ie, BGRX, or 565), patches welcome...

  printf(
      "fb0 reports (possibly inaccurate):\n"
      "  vi.bits_per_pixel = %d\n"
      "  vi.red.offset   = %3d   .length = %3d\n"
      "  vi.green.offset = %3d   .length = %3d\n"
      "  vi.blue.offset  = %3d   .length = %3d\n",
      vi.bits_per_pixel, vi.red.offset, vi.red.length, vi.green.offset, vi.green.length,
      vi.blue.offset, vi.blue.length);

  bits = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (bits == MAP_FAILED) {
    perror("failed to mmap framebuffer");
    close(fd);
    return nullptr;
  }

  memset(bits, 0, fi.smem_len);

  gr_framebuffer[0].width = vi.xres;
  gr_framebuffer[0].height = vi.yres;
  gr_framebuffer[0].row_bytes = fi.line_length;
  gr_framebuffer[0].pixel_bytes = vi.bits_per_pixel / 8;
  gr_framebuffer[0].data = static_cast<uint8_t*>(bits);
  memset(gr_framebuffer[0].data, 0, gr_framebuffer[0].height * gr_framebuffer[0].row_bytes);

  /* check if we can use triple buffering */
  if (vi.yres * fi.line_length * 3 <= fi.smem_len) {
    triple_buffered = true;

    memcpy(gr_framebuffer + 1, gr_framebuffer, sizeof(GRSurface));
    memcpy(gr_framebuffer + 2, gr_framebuffer, sizeof(GRSurface));
    gr_framebuffer[1].data =
        gr_framebuffer[0].data + gr_framebuffer[0].height * gr_framebuffer[0].row_bytes;
    memset(gr_framebuffer[1].data, 0, gr_framebuffer[1].height * gr_framebuffer[1].row_bytes);
    gr_framebuffer[2].data =
        gr_framebuffer[1].data + gr_framebuffer[1].height * gr_framebuffer[1].row_bytes;
    memset(gr_framebuffer[2].data, 0, gr_framebuffer[2].height * gr_framebuffer[2].row_bytes);

    gr_draw = gr_framebuffer + displayed_buffer;

  } else {
    triple_buffered = false;

    // Without double-buffering, we allocate RAM for a buffer to
    // draw in, and then "flipping" the buffer consists of a
    // memcpy from the buffer we allocated to the framebuffer.

    gr_draw = static_cast<GRSurface*>(malloc(sizeof(GRSurface)));
    memcpy(gr_draw, gr_framebuffer, sizeof(GRSurface));
    gr_draw->data = static_cast<unsigned char*>(malloc(gr_draw->height * gr_draw->row_bytes));
    if (!gr_draw->data) {
      perror("failed to allocate in-memory surface");
      close(fd);
      return nullptr;
    }
  }

  memset(gr_draw->data, 0, gr_draw->height * gr_draw->row_bytes);
  fb_fd = fd;
  SetDisplayedFramebuffer(displayed_buffer);
  gr_draw = gr_framebuffer + displayed_buffer;

  printf("framebuffer: %d (%d x %d)\n", fb_fd, gr_draw->width, gr_draw->height);
  printf("triple_buffered: %s\n", triple_buffered ? "true" : "false");

  Blank(true);
  Blank(false);

  return gr_draw;
}

GRSurface* MinuiBackendFbdev::Flip() {
  if (triple_buffered) {
    // Change gr_draw to point to the buffer currently displayed,
    // then flip the driver so we're displaying the other buffer
    // instead.
    SetDisplayedFramebuffer(displayed_buffer);
    gr_draw = gr_framebuffer + displayed_buffer;
  } else {
    // Copy from the in-memory surface to the framebuffer.
    memcpy(gr_framebuffer[0].data, gr_draw->data, gr_draw->height * gr_draw->row_bytes);
  }
  return gr_draw;
}

MinuiBackendFbdev::~MinuiBackendFbdev() {
  close(fb_fd);
  fb_fd = -1;

  if (!triple_buffered && gr_draw) {
    free(gr_draw->data);
    free(gr_draw);
  }
  gr_draw = nullptr;
}
