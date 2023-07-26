/************************************************************************************

Filename    :   ProgressBarComponent.h
Content     :   A reusable component implementing a progress bar.
Created     :   Mar 30, 2015
Authors     :   Warsam Osman

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "ProgressBarComponent.h"
#include "Appl.h"
#include "Misc/Log.h"
#include "VRMenuMgr.h"
#include "GuiSys.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

char const* OvrProgressBarComponent::TYPE_NAME = "OvrProgressBarComponent";

static const Vector3f FWD(0.0f, 0.0f, -1.0f);
static const Vector3f RIGHT(1.0f, 0.0f, 0.0f);
static const Vector3f DOWN(0.0f, -1.0f, 0.0f);

static const float THUMB_FROM_BASE_OFFSET = 0.001f;

//==============================
// OvrProgressBarComponent::OvrProgressBarComponent
OvrProgressBarComponent::OvrProgressBarComponent(
    const VRMenuId_t rootId,
    const VRMenuId_t baseId,
    const VRMenuId_t thumbId,
    const VRMenuId_t animId)
    : VRMenuComponent(VRMenuEventFlags_t(VRMENU_EVENT_FRAME_UPDATE)),
      Fader(0.0f),
      FadeInRate(1.0f / 0.2f),
      FadeOutRate(1.0f / 0.5f),
      Frac(-1.0f),
      ProgressBarBaseWidth(0.0f),
      ProgressBarBaseHeight(0.0f),
      ProgressBarCurrentbaseLength(0.0f),
      ProgressBarThumbWidth(0.0f),
      ProgressBarThumbHeight(0.0f),
      ProgressBarCurrentThumbLength(0.0f),
      ProgressBarRootId(rootId),
      ProgressBarBaseId(baseId),
      ProgressBarThumbId(thumbId),
      ProgressBarAnimId(animId),
      CurrentProgressBarState(PROGRESSBAR_STATE_HIDDEN) {}

void OvrProgressBarComponent::SetProgressFrac(
    OvrVRMenuMgr& menuMgr,
    VRMenuObject* self,
    const float frac) {
    // ALOG( "OvrProgressBarComponent::SetProgressFrac to %f", Frac  );
    if ((CurrentProgressBarState != PROGRESSBAR_STATE_VISIBLE) &&
        (CurrentProgressBarState != PROGRESSBAR_STATE_FADE_IN)) {
        return;
    }

    if (frac >= 0.0f && frac <= 1.0f) {
        if (Frac == frac) {
            return;
        }

        Frac = frac;
        ProgressBarCurrentThumbLength = ProgressBarThumbWidth * Frac;

        float thumbPos = ProgressBarThumbWidth - ProgressBarCurrentThumbLength;

        thumbPos -= thumbPos * 0.5f;
        thumbPos *= VRMenuObject::DEFAULT_TEXEL_SCALE;

        VRMenuObject* thumb = menuMgr.ToObject(self->ChildHandleForId(menuMgr, ProgressBarThumbId));
        if (thumb != NULL) {
            thumb->SetSurfaceDims(
                0, Vector2f(ProgressBarCurrentThumbLength, ProgressBarThumbHeight));
            thumb->RegenerateSurfaceGeometry(0, false);
            thumb->SetLocalPosition((-RIGHT * thumbPos) - (FWD * THUMB_FROM_BASE_OFFSET));
        }
    }
}

void OvrProgressBarComponent::OneTimeInit(
    OvrGuiSys& guiSys,
    VRMenuObject* self,
    const Vector4f& color) {
    // Set alpha on the base - move this to somewhere more explicit if needed
    VRMenuObject* base = guiSys.GetVRMenuMgr().ToObject(
        self->ChildHandleForId(guiSys.GetVRMenuMgr(), ProgressBarBaseId));
    if (base != NULL) {
        base->SetSurfaceColor(0, color);

        // Resize the base
        base->SetSurfaceDims(0, Vector2f(ProgressBarBaseWidth, ProgressBarBaseHeight));
        base->RegenerateSurfaceGeometry(0, false);
    }

    // start off hidden
    SetProgressFrac(
        guiSys.GetVRMenuMgr(),
        self,
        0.0f); // this forces the generation of the thumb at the right position.
    SetProgressbarState(self, PROGRESSBAR_STATE_HIDDEN);
}

void OvrProgressBarComponent::GetProgressBarParms(
    VRMenu& menu,
    const int width,
    const int height,
    const VRMenuId_t parentId,
    const VRMenuId_t rootId,
    const VRMenuId_t xformId,
    const VRMenuId_t baseId,
    const VRMenuId_t thumbId,
    const VRMenuId_t animId,
    const Posef& rootLocalPose,
    const Posef& xformPose,
    const char* baseImage,
    const char* barImage,
    const char* animImage,
    std::vector<const VRMenuObjectParms*>& outParms) {
    // Build up the Progressbar parms
    OvrProgressBarComponent* ProgressComponent =
        new OvrProgressBarComponent(rootId, baseId, thumbId, animId);

    ProgressComponent->SetProgressBarBaseWidth(width);
    ProgressComponent->SetProgressBarBaseHeight(height);
    ProgressComponent->SetProgressBarThumbWidth(width);
    ProgressComponent->SetProgressBarThumbHeight(height);

    // parms for the root object that holds all the Progressbar components
    {
        std::vector<VRMenuComponent*> comps;
        comps.push_back(ProgressComponent);
        std::vector<VRMenuSurfaceParms> surfParms;
        char const* text = "ProgressBarRoot";
        Vector3f scale(1.0f);
        Posef pose(rootLocalPose);
        Posef textPose(Quatf(), Vector3f(0.0f));
        Vector3f textScale(1.0f);
        VRMenuFontParms fontParms;
        VRMenuObjectFlags_t objectFlags(VRMENUOBJECT_DONT_HIT_ALL);
        objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
        VRMenuObjectInitFlags_t initFlags(VRMENUOBJECT_INIT_FORCE_POSITION);
        VRMenuObjectParms* itemParms = new VRMenuObjectParms(
            VRMENU_CONTAINER,
            comps,
            surfParms,
            text,
            pose,
            scale,
            textPose,
            textScale,
            fontParms,
            rootId,
            objectFlags,
            initFlags);
        itemParms->ParentId = parentId;
        outParms.push_back(itemParms);
    }

    // add parms for the object that serves as a transform
    {
        std::vector<VRMenuComponent*> comps;
        std::vector<VRMenuSurfaceParms> surfParms;
        char const* text = "ProgressBarTransform";
        Vector3f scale(1.0f);
        Posef pose(xformPose);
        Posef textPose(Quatf(), Vector3f(0.0f));
        Vector3f textScale(1.0f);
        VRMenuFontParms fontParms;
        VRMenuObjectFlags_t objectFlags(VRMENUOBJECT_DONT_HIT_ALL);
        objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
        VRMenuObjectInitFlags_t initFlags(VRMENUOBJECT_INIT_FORCE_POSITION);
        VRMenuObjectParms* itemParms = new VRMenuObjectParms(
            VRMENU_CONTAINER,
            comps,
            surfParms,
            text,
            pose,
            scale,
            textPose,
            textScale,
            fontParms,
            xformId,
            objectFlags,
            initFlags);
        itemParms->ParentId = rootId;
        outParms.push_back(itemParms);
    }

    // add parms for the base image that underlays the whole Progressbar
    {
        const char* text = "ProgressBase";
        std::vector<VRMenuComponent*> comps;
        std::vector<VRMenuSurfaceParms> surfParms;
        VRMenuSurfaceParms baseParms(
            text,
            baseImage,
            SURFACE_TEXTURE_DIFFUSE,
            NULL,
            SURFACE_TEXTURE_MAX,
            NULL,
            SURFACE_TEXTURE_MAX);
        surfParms.push_back(baseParms);
        Vector3f scale(1.0f);
        Posef pose(Quatf(), Vector3f(0.0f));
        Posef textPose(Quatf(), Vector3f(0.0f));
        Vector3f textScale(1.0f);
        VRMenuFontParms fontParms;
        VRMenuObjectFlags_t objectFlags(VRMENUOBJECT_DONT_HIT_ALL);
        objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
        VRMenuObjectInitFlags_t initFlags(VRMENUOBJECT_INIT_FORCE_POSITION);
        VRMenuObjectParms* itemParms = new VRMenuObjectParms(
            VRMENU_BUTTON,
            comps,
            surfParms,
            text,
            pose,
            scale,
            textPose,
            textScale,
            fontParms,
            baseId,
            objectFlags,
            initFlags);
        itemParms->ParentId = xformId;
        outParms.push_back(itemParms);
    }

    // add parms for the thumb image of the Progressbar
    {
        const char* text = "ProgressThumb";
        std::vector<VRMenuComponent*> comps;
        std::vector<VRMenuSurfaceParms> surfParms;
        VRMenuSurfaceParms thumbParms(
            text,
            barImage,
            SURFACE_TEXTURE_DIFFUSE,
            NULL,
            SURFACE_TEXTURE_MAX,
            NULL,
            SURFACE_TEXTURE_MAX);
        // thumbParms.Border = thumbBorder;
        surfParms.push_back(thumbParms);
        Vector3f scale(1.0f);
        Posef pose(Quatf(), -FWD * THUMB_FROM_BASE_OFFSET);
        // Since we use left aligned anchors on the base and thumb, we offset the root once to
        // center the Progressbar
        Posef textPose(Quatf(), Vector3f(0.0f));
        Vector3f textScale(1.0f);
        VRMenuFontParms fontParms;
        VRMenuObjectFlags_t objectFlags(VRMENUOBJECT_DONT_HIT_ALL);
        objectFlags |= VRMENUOBJECT_DONT_RENDER_TEXT;
        objectFlags |= VRMENUOBJECT_FLAG_POLYGON_OFFSET;
        VRMenuObjectInitFlags_t initFlags(VRMENUOBJECT_INIT_FORCE_POSITION);
        VRMenuObjectParms* itemParms = new VRMenuObjectParms(
            VRMENU_BUTTON,
            comps,
            surfParms,
            text,
            pose,
            scale,
            textPose,
            textScale,
            fontParms,
            thumbId,
            objectFlags,
            initFlags);
        itemParms->ParentId = xformId;
        outParms.push_back(itemParms);
    }

    // add parms for the progress animation
    {
        // TODO
    }
}

//==============================
// OvrProgressBarComponent::OnEvent_Impl
eMsgStatus OvrProgressBarComponent::OnEvent_Impl(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    switch (event.EventType) {
        case VRMENU_EVENT_FRAME_UPDATE:
            return OnFrameUpdate(guiSys, vrFrame, self, event);
        default:
            assert(false);
            return MSG_STATUS_ALIVE;
    }
}

const char* progressBarStateNames[] = {"PROGRESS_STATE_NONE",
                                       "PROGRESS_STATE_FADE_IN",
                                       "PROGRESS_STATE_VISIBLE",
                                       "PROGRESS_STATE_FADE_OUT",
                                       "PROGRESS_STATE_HIDDEN",
                                       "NUM_PROGRESS_STATES"};

const char* StateString(const OvrProgressBarComponent::eProgressBarState state) {
    assert(state >= 0 && state < OvrProgressBarComponent::PROGRESSBAR_STATES_COUNT);
    return progressBarStateNames[state];
}

//==============================
// OvrProgressBarComponent::SetProgressState
void OvrProgressBarComponent::SetProgressbarState(
    VRMenuObject* self,
    const eProgressBarState state) {
    eProgressBarState lastState = CurrentProgressBarState;
    CurrentProgressBarState = state;
    if (CurrentProgressBarState == lastState) {
        return;
    }

    switch (CurrentProgressBarState) {
        case PROGRESSBAR_STATE_NONE:
            break;
        case PROGRESSBAR_STATE_FADE_IN:
            if (lastState == PROGRESSBAR_STATE_HIDDEN || lastState == PROGRESSBAR_STATE_FADE_OUT) {
                ALOG("%s to %s", StateString(lastState), StateString(CurrentProgressBarState));
                Fader.StartFadeIn();
            }
            break;
        case PROGRESSBAR_STATE_VISIBLE:
            self->SetVisible(true);
            break;
        case PROGRESSBAR_STATE_FADE_OUT:
            if (lastState == PROGRESSBAR_STATE_VISIBLE || lastState == PROGRESSBAR_STATE_FADE_IN) {
                ALOG("%s to %s", StateString(lastState), StateString(CurrentProgressBarState));
                Fader.StartFadeOut();
            }
            break;
        case PROGRESSBAR_STATE_HIDDEN:
            self->SetVisible(false);
            break;
        default:
            assert(false);
            break;
    }
}

//==============================
// OvrProgressBarComponent::OnFrameUpdate
eMsgStatus OvrProgressBarComponent::OnFrameUpdate(
    OvrGuiSys& guiSys,
    ovrApplFrameIn const& vrFrame,
    VRMenuObject* self,
    VRMenuEvent const& event) {
    assert(self != NULL);
    if (Fader.GetFadeState() != Fader::FADE_NONE) {
        const float fadeRate = (Fader.GetFadeState() == Fader::FADE_IN) ? FadeInRate : FadeOutRate;
        Fader.Update(fadeRate, vrFrame.DeltaSeconds);
        const float CurrentFadeLevel = Fader.GetFinalAlpha();
        self->SetColor(Vector4f(1.0f, 1.0f, 1.0f, CurrentFadeLevel));
    } else if (Fader.GetFadeAlpha() == 1.0f) {
        SetProgressbarState(self, PROGRESSBAR_STATE_VISIBLE);
    } else if (Fader.GetFadeAlpha() == 0.0f) {
        SetProgressbarState(self, PROGRESSBAR_STATE_HIDDEN);
    }

    return MSG_STATUS_ALIVE;
}

} // namespace OVRFW
