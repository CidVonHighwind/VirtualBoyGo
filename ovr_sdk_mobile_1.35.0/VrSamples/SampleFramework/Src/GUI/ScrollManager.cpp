/************************************************************************************

Filename    :   ScrollManager.cpp
Content     :
Created     :   December 12, 2014
Authors     :   Jonathan E. Wright, Warsam Osman, Madhu Kalva

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.


*************************************************************************************/

#include "ScrollManager.h"

#include <algorithm> // for min, max
#include "OVR_Math.h"

#include "VrApi_Input.h"

using OVR::Bounds3f;
using OVR::Matrix4f;
using OVR::Posef;
using OVR::Quatf;
using OVR::Vector2f;
using OVR::Vector3f;
using OVR::Vector4f;

namespace OVRFW {

static const float EPSILON_VEL = 0.00001f;

OvrScrollManager::OvrScrollManager(eScrollType type)
    : CurrentScrollType(type),
      MaxPosition(0.0f),
      TouchIsDown(false),
      Deltas(),
      CurrentScrollState(SMOOTH_SCROLL),
      Position(0.0f),
      Velocity(0.0f),
      AccumulatedDeltaTimeInSeconds(0.0f),
      AutoScrollInFwdDir(false),
      AutoScrollBkwdDir(false),
      PrevAutoScrollVelocity(0.0f),
      RestrictedScrolling(false),
      TouchDirectionLocked(NO_LOCK),
      ControllerDirectionLocked(NO_LOCK) {}

void OvrScrollManager::Frame(float deltaSeconds, std::uint32_t controllerState) {
    // --
    // Controller Input Handling
    bool forwardScrolling = false;
    bool backwardScrolling = false;

    if (CurrentScrollType == HORIZONTAL_SCROLL) {
        forwardScrolling = (controllerState & (ovrButton_Right)) != 0;
        backwardScrolling = (controllerState & (ovrButton_Left)) != 0;
    } else if (CurrentScrollType == VERTICAL_SCROLL) {
        forwardScrolling = (controllerState & (ovrButton_Down)) != 0;
        backwardScrolling = (controllerState & (ovrButton_Up)) != 0;
    }

    if (forwardScrolling || backwardScrolling) {
        CurrentScrollState = SMOOTH_SCROLL;
    }
    SetEnableAutoForwardScrolling(forwardScrolling);
    SetEnableAutoBackwardScrolling(backwardScrolling);

    // --
    // Logic update for the current scroll state
    switch (CurrentScrollState) {
        case SMOOTH_SCROLL: {
            // Auto scroll during wrap around initiation.
            if ((!RestrictedScrolling) || // Either not restricted scrolling
                (RestrictedScrolling &&
                 (TouchDirectionLocked !=
                  NO_LOCK))) // or restricted scrolling with touch direction locked
            {
                if (TouchIsDown) {
                    if ((Position < 0.0f) || (Position > MaxPosition)) {
                        // When scrolled out of bounds and touch is still down,
                        // auto-scroll to initiate wrap around easily.
                        float totalDistance = 0.0f;
                        for (int i = 0; i < Deltas.GetNum(); ++i) {
                            totalDistance += Deltas[i].d;
                        }

                        if (totalDistance < -EPSILON_VEL) {
                            Velocity = -1.0f;
                        } else if (totalDistance > EPSILON_VEL) {
                            Velocity = 1.0f;
                        } else {
                            Velocity = PrevAutoScrollVelocity;
                        }
                    } else {
                        Velocity = 0.0f;
                    }

                    PrevAutoScrollVelocity = Velocity;
                }
            }

            if (!IsScrolling() && IsOutOfBounds()) {
                CurrentScrollState = BOUNCE_SCROLL;
                const float bounceBackVelocity = ScrollBehavior.BounceBackVelocity;
                if (Position < 0.0f) {
                    Velocity = bounceBackVelocity;
                } else if (Position > MaxPosition) {
                    Velocity = -bounceBackVelocity;
                }
                Velocity = GetModifiedVelocity(Velocity);
            }
        } break;

        case BOUNCE_SCROLL: {
            if (!IsOutOfBounds()) {
                CurrentScrollState = SMOOTH_SCROLL;
                if (Velocity > 0.0f) // Pulled at the beginning
                {
                    Position = 0.0f;
                } else if (Velocity < 0.0f) // Pulled at the end
                {
                    Position = MaxPosition;
                }
                Velocity = 0.0f;
            }
        } break;
    }

    // --
    // Scrolling physics update
    AccumulatedDeltaTimeInSeconds += deltaSeconds;
    assert(AccumulatedDeltaTimeInSeconds <= 10.0f);
    while (true) {
        if (AccumulatedDeltaTimeInSeconds < 0.016f) {
            break;
        }

        AccumulatedDeltaTimeInSeconds -= 0.016f;
        if (CurrentScrollState == SMOOTH_SCROLL ||
            CurrentScrollState == BOUNCE_SCROLL) // While wrap around don't slow down scrolling
        {
            Velocity -= Velocity * ScrollBehavior.FrictionPerSec * 0.016f;
            if (fabsf(Velocity) < EPSILON_VEL) {
                Velocity = 0.0f;
                // This will guarantee that Position will stop at desired location when it halts
                Position = DesiredPositionForTargetPosition(Position);
            }
        }

        Position += Velocity * 0.016f;
        if (CurrentScrollState == BOUNCE_SCROLL) {
            if (Velocity > 0.0f) {
                Position = clamp<float>(Position, -ScrollBehavior.Padding, MaxPosition);
            } else {
                Position = clamp<float>(Position, 0.0f, MaxPosition + ScrollBehavior.Padding);
            }
        } else {
            Position = clamp<float>(
                Position, -ScrollBehavior.Padding, MaxPosition + ScrollBehavior.Padding);
        }
    }
}

void OvrScrollManager::TouchDown() {
    TouchIsDown = true;
    CurrentScrollState = SMOOTH_SCROLL;
    LastTouchPosistion = Vector3f(0.0f, 0.0f, 0.0f);
    Velocity = 0.0f;
    Deltas.Clear();
}

void OvrScrollManager::TouchUp() {
    TouchIsDown = false;

    if (Deltas.GetNum() == 0) {
        Velocity = 0.0f;
    } else {
        // accumulate total distance
        float totalDistance = 0.0f;
        for (int i = 0; i < Deltas.GetNum(); ++i) {
            totalDistance += Deltas[i].d;
        }

        // calculating total time spent
        delta_t const& head = Deltas.GetHead();
        delta_t const& tail = Deltas.GetTail();
        float totalTime = static_cast<float>(head.t - tail.t);

        // calculating velocity based on touches
        float touchVelocity =
            totalTime > MATH_FLOAT_SMALLEST_NON_DENORMAL ? totalDistance / totalTime : 0.0f;
        Velocity = GetModifiedVelocity(touchVelocity);
    }

    Deltas.Clear();

    return;
}

void OvrScrollManager::TouchRelative(Vector3f touchPos, const double timeInSeconds) {
    if (!TouchIsDown) {
        return;
    }

    // Check if the touch is valid for this( horizontal / vertical ) scrolling type
    bool isValid = false;
    if (fabsf(LastTouchPosistion.x - touchPos.x) > fabsf(LastTouchPosistion.y - touchPos.y)) {
        if (CurrentScrollType == HORIZONTAL_SCROLL) {
            isValid = true;
        }
    } else {
        if (CurrentScrollType == VERTICAL_SCROLL) {
            isValid = true;
        }
    }

    if (isValid) {
        float touchVal;
        float lastTouchVal;
        float curMove;
        if (CurrentScrollType == HORIZONTAL_SCROLL) {
            touchVal = touchPos.x;
            lastTouchVal = LastTouchPosistion.x;
            curMove = lastTouchVal - touchVal;
        } else {
            touchVal = touchPos.y;
            lastTouchVal = LastTouchPosistion.y;
            curMove = touchVal - lastTouchVal;
        }

        float const DISTANCE_SCALE = ScrollBehavior.DistanceScale;

        Position += curMove * DISTANCE_SCALE;

        float const DISTANCE_RAMP = 150.0f;
        float const ramp = fabsf(touchVal) / DISTANCE_RAMP;
        Deltas.Append(delta_t(curMove * DISTANCE_SCALE * ramp, timeInSeconds));
    }

    LastTouchPosistion = touchPos;
}

bool OvrScrollManager::IsOutOfBounds() const {
    return (Position < 0.0f || Position > MaxPosition);
}

void OvrScrollManager::SetEnableAutoForwardScrolling(bool value) {
    if (value) {
        float newVelocity = ScrollBehavior.AutoVerticalSpeed;
        Velocity = Velocity + (newVelocity - Velocity) * 0.3f;
    } else {
        if (AutoScrollInFwdDir) {
            // Turning off auto forward scrolling, adjust velocity so it stops at proper position
            Velocity = GetModifiedVelocity(Velocity);
        }
    }

    AutoScrollInFwdDir = value;
}

void OvrScrollManager::SetEnableAutoBackwardScrolling(bool value) {
    if (value) {
        float newVelocity = -ScrollBehavior.AutoVerticalSpeed;
        Velocity = Velocity + (newVelocity - Velocity) * 0.3f;
    } else {
        if (AutoScrollBkwdDir) {
            // Turning off auto forward scrolling, adjust velocity so it stops at proper position
            Velocity = GetModifiedVelocity(Velocity);
        }
    }

    AutoScrollBkwdDir = value;
}

float OvrScrollManager::DesiredPositionForTargetPosition(float targetPosition) {
    return nearbyintf(targetPosition);
}

float OvrScrollManager::GetModifiedVelocity(const float inVelocity) {
    // calculating distance its going to travel with touch velocity.
    float distanceItsGonnaTravel = 0.0f;
    float begginingVelocity = inVelocity;
    while (true) {
        begginingVelocity -= begginingVelocity * ScrollBehavior.FrictionPerSec * 0.016f;
        distanceItsGonnaTravel += begginingVelocity * 0.016f;

        if (fabsf(begginingVelocity) < EPSILON_VEL) {
            break;
        }
    }

    float currentTargetPosition = Position + distanceItsGonnaTravel;
    float desiredTargetPosition = DesiredPositionForTargetPosition(currentTargetPosition);

    // Calculating velocity to compute desired target position
    float desiredVelocity = EPSILON_VEL;
    if (Position >= desiredTargetPosition) {
        desiredVelocity = -EPSILON_VEL;
    }
    float currentPosition = Position;
    float prevPosition;

    while (true) {
        prevPosition = currentPosition;
        currentPosition += desiredVelocity * 0.016f;

        if ((prevPosition <= desiredTargetPosition && currentPosition >= desiredTargetPosition) ||
            (currentPosition <= desiredTargetPosition && prevPosition >= desiredTargetPosition)) {
            // Found the spot where it should end up.
            // Adjust the position so it can end up at desiredPosition with desiredVelocity
            Position += (desiredTargetPosition - prevPosition);
            break;
        }

        // This is inverse to Velocity_new = Velocity_old - Velocity_old *
        // ScrollBehavior.FrictionPerSec * 0.016f (used in OvrScrollManager::Frame function)
        desiredVelocity = desiredVelocity / (1.0f - ScrollBehavior.FrictionPerSec * 0.016f);
    }

    return desiredVelocity;
}

void OvrScrollManager::SetRestrictedScrollingData(
    bool isRestricted,
    eScrollDirectionLockType touchLock,
    eScrollDirectionLockType controllerLock) {
    RestrictedScrolling = isRestricted;
    TouchDirectionLocked = touchLock;
    ControllerDirectionLocked = controllerLock;
}

void OvrScrollManager::Reset() {
    Velocity = 0;
    Position = 0;
    AccumulatedDeltaTimeInSeconds = 0;
    CurrentScrollState = eScrollState::SMOOTH_SCROLL;

    Deltas.Clear();
}

bool IsInRage(float val, float startRange, float endRange) {
    return ((val >= startRange) && (val <= endRange));
}

} // namespace OVRFW
