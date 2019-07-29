/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <math.h>

#include <utils/Errors.h>
#include <utils/Log.h>

#include <ui/GraphicBuffer.h>

#include "LayerDim.h"
#include "SurfaceFlinger.h"
#include "DisplayDevice.h"
#include "RenderEngine/RenderEngine.h"

namespace android {
// ---------------------------------------------------------------------------

LayerDim::LayerDim(SurfaceFlinger* flinger, const sp<Client>& client,
        const String8& name, uint32_t w, uint32_t h, uint32_t flags)
    : Layer(flinger, client, name, w, h, flags) {
}

LayerDim::~LayerDim() {
}

void LayerDim::onDraw(const sp<const DisplayDevice>& hw,
        const Region& /* clip */, bool useIdentityTransform) const
{
    const State& s(getDrawingState());
    if (s.alpha>0) {
        Mesh mesh(Mesh::TRIANGLE_FAN, 4, 2);
        computeGeometry(hw, mesh, useIdentityTransform);
        RenderEngine& engine(mFlinger->getRenderEngine());
        engine.setupDimLayerBlending(s.alpha);
        engine.drawMesh(mesh);
        engine.disableBlending();
    }
}

bool LayerDim::isVisible() const {
    const Layer::State& s(getDrawingState());
    return !(s.flags & layer_state_t::eLayerHidden) && s.alpha;
}

static Rect reduce(const Rect& win, const Region& exclude) {
    if (CC_LIKELY(exclude.isEmpty())) {
        return win;
    }
    if (exclude.isRect()) {
        return win.reduce(exclude.getBounds());
    }
    return Region(win).subtract(exclude).getBounds();
}

FloatRect LayerDim::computeCrop(const sp<const DisplayDevice>& hw) const {
    // the content crop is the area of the content that gets scaled to the
    // layer's size.

    FloatRect crop(getContentCrop());

    // the active.crop is the area of the window that gets cropped, but not
    // scaled in any ways.
    const State& s(getDrawingState());

    // apply the projection's clipping to the window crop in
    // layerstack space, and convert-back to layer space.
    // if there are no window scaling involved, this operation will map to full
    // pixels in the buffer.
    // FIXME: the 3 lines below can produce slightly incorrect clipping when we have
    // a viewport clipping and a window transform. we should use floating point to fix this.

    Rect activeCrop(s.active.w, s.active.h);
    if (!s.active.crop.isEmpty()) {
        activeCrop = s.active.crop;
    }

    activeCrop = s.transform.transform(activeCrop);
    activeCrop.intersect(hw->getViewport(), &activeCrop);
    activeCrop = s.transform.inverse().transform(activeCrop);

    // paranoia: make sure the window-crop is constrained in the
    // window's bounds
    activeCrop.intersect(Rect(s.active.w, s.active.h), &activeCrop);

    // subtract the transparent region and snap to the bounds
    activeCrop = reduce(activeCrop, s.activeTransparentRegion);

    if (!activeCrop.isEmpty()) {
        // Transform the window crop to match the buffer coordinate system,
        // which means using the inverse of the current transform set on the
        // SurfaceFlingerConsumer.
        uint32_t invTransform = getContentTransform();
        int winWidth = s.active.w;
        int winHeight = s.active.h;
        if (invTransform & NATIVE_WINDOW_TRANSFORM_ROT_90) {
            invTransform ^= NATIVE_WINDOW_TRANSFORM_FLIP_V |
                    NATIVE_WINDOW_TRANSFORM_FLIP_H;
            winWidth = s.active.h;
            winHeight = s.active.w;
        }
        const Rect winCrop = activeCrop.transform(
                invTransform, s.active.w, s.active.h);

        // below, crop is intersected with winCrop expressed in crop's coordinate space
        float xScale = crop.getWidth()  / float(winWidth);
        float yScale = crop.getHeight() / float(winHeight);

        float insetL = winCrop.left                 * xScale;
        float insetT = winCrop.top                  * yScale;
        float insetR = (winWidth  - winCrop.right ) * xScale;
        float insetB = (winHeight - winCrop.bottom) * yScale;

        crop.left   += insetL;
        crop.top    += insetT;
        crop.right  -= insetR;
        crop.bottom -= insetB;
    }
    return crop;
}


void LayerDim::setGeometry(
    const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer)
{
    layer.setDefaultState();

    // enable this layer
    layer.setSkip(false);

    // blending mode
    layer.setBlending(HWC_BLENDING_DIM);

    // set dim value
    const State& s(getDrawingState());
    layer.setPlaneAlpha(s.alpha);

    if (isSecure() && !hw->isSecure()) {
        layer.setSkip(true);
    }
    // apply the layer's transform, followed by the display's global transform
    // here we're guaranteed that the layer's transform preserves rects
    Rect frame(s.transform.transform(computeBounds()));
    frame.intersect(hw->getViewport(), &frame);
    const Transform& tr(hw->getTransform());
    layer.setFrame(tr.transform(frame));
    layer.setCrop(computeCrop(hw));
}

void LayerDim::setPerFrameData(const sp<const DisplayDevice>& hw,
        HWComposer::HWCLayerInterface& layer) {
    // we have to set the visible region on every frame because
    // we currently free it during onLayerDisplayed(), which is called
    // after HWComposer::commit() -- every frame.
    // Apply this display's projection's viewport to the visible region
    // before giving it to the HWC HAL.
    const Transform& tr = hw->getTransform();
    Region visible = tr.transform(visibleRegion.intersect(hw->getViewport()));
    layer.setVisibleRegionScreen(visible);
    // leave it as it is for dim layer.
}

// ---------------------------------------------------------------------------

}; // namespace android
