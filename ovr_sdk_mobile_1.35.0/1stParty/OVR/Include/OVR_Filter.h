/************************************************************************************

Filename    :   OVR_Filter.h
Content     :   Filter algorithms for OVR_Math types
Created     :
Authors     :

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/
#ifndef OVR_Filter_h
#define OVR_Filter_h

#include "OVR_Math.h"

namespace OVRF {
static inline float cuttoffToGain(float cutoff, float dt) {
    return dt / (1.0f / (2.0f * MATH_FLOAT_PI * cutoff) + dt);
}

template <typename T>
static inline T sum(T a, T b) {
    return a + b;
}

static inline OVR::Quatf sum(OVR::Quatf a, OVR::Quatf b) {
    return a * b;
}

template <typename T>
static inline T diff(T a, T b) {
    return a - b;
}

static inline OVR::Quatf diff(OVR::Quatf a, OVR::Quatf b) {
    return b * a.Inverted();
}

template <typename T>
static inline T scale(T a, float s) {
    return a * s;
}

static inline OVR::Quatf scale(OVR::Quatf a, float s) {
    // scales the angle about the same axis.
    // this is the same as Slerp from identity, but faster.
    return OVR::Quatf::FromRotationVector(a.ToRotationVector() * s);
}

template <typename T>
static inline T lerp(T start, T end, float progress) {
    return start + (end - start) * progress;
}

static inline OVR::Anglef lerp(OVR::Anglef start, OVR::Anglef end, float progress) {
    return start.Lerp(end, progress);
}

static inline OVR::Quatf lerp(OVR::Quatf start, OVR::Quatf end, float progress) {
    return start.Slerp(end, progress);
}

static inline float mag(float value) {
    return fabs(value);
}

static inline float mag(OVR::Vector3f value) {
    return value.Length();
}

static inline float mag(OVR::Quatf value) {
    return value.Angle(); // returns abs angle <= PI
}

static inline float mag(OVR::Anglef value) {
    return value.Abs(); // returns abs angle <= PI
}
} // namespace OVRF

namespace OVR {
template <typename T>
class IdentifyFilter {
   public:
    T Filter(T x, float dt) {
        return x;
    }
};

struct SingleExpoParams {
    SingleExpoParams() : cutoff(1) {}
    SingleExpoParams(float c) : cutoff(c) {}
    float cutoff;
};

template <typename T>
class SingleExpoFilter {
   public:
    SingleExpoFilter() : warm(false), last(T{}) {}
    SingleExpoFilter(SingleExpoParams p) : params(p), warm(false), last(T{}) {}
    SingleExpoFilter(float c) : SingleExpoFilter(SingleExpoParams(c)) {}

    T Filter(T x, float dt) {
        if (!warm) {
            warm = true;
            return last = x;
        }
        return last = OVRF::lerp(last, x, OVRF::cuttoffToGain(params.cutoff, dt));
    }

    void Reset() {
        warm = false;
    }

    constexpr T Last() const {
        return last;
    }

    // params
    SingleExpoParams params;

   private:
    // state
    bool warm;
    T last;
};

// This filter takes positions in and returns filtered velocities
template <typename T>
class VelocityFilter {
   public:
    VelocityFilter() : warm(false), last(T{}) {}
    VelocityFilter(SingleExpoParams p) : params(p), warm(false), last(T{}), vfilter(p) {}
    VelocityFilter(float c) : VelocityFilter(SingleExpoParams(c)) {}

    T Filter(T x, float dt) {
        vfilter.params = params;
        if (!warm) {
            warm = true;
            last = x;
            return T{};
        }

        float divdt = (1.0f / std::max(OVR::Math<float>::Tolerance(), dt)); // avoid divide by zero
        auto velocity = OVRF::scale(OVRF::diff(x, last), divdt);
        last = x;
        return vfilter.Filter(velocity, dt);
    }

    void Reset() {
        warm = false;
        vfilter.Reset();
    }

    constexpr T Last() const {
        return last;
    }

    // params
    SingleExpoParams params;

   private:
    // state
    bool warm;
    T last;
    SingleExpoFilter<T> vfilter;
};

struct DoubleExpoParams {
    DoubleExpoParams() : cutoff(1), vcutoff(1), vscale(1) {}
    DoubleExpoParams(float c, float vc, float vs) : cutoff(c), vcutoff(vc), vscale(vs) {}
    float cutoff;
    float vcutoff;
    float vscale;
};

template <typename T>
class DoubleExpoFilter {
   public:
    DoubleExpoFilter() : warm(false), last(T{}), velocity(T{}) {}
    DoubleExpoFilter(DoubleExpoParams p) : params(p), warm(false), last(T{}), velocity(T{}) {}
    DoubleExpoFilter(float c, float vc, float vs) : DoubleExpoFilter(DoubleExpoParams(c, vc, vs)) {}

    T Filter(T x, float dt) {
        if (!warm) {
            warm = true;
            return last = x;
        }
        T next = OVRF::lerp(OVRF::sum(last, velocity), x, OVRF::cuttoffToGain(params.cutoff, dt));
        velocity = OVRF::lerp(
            velocity,
            OVRF::scale(OVRF::diff(next, last), params.vscale),
            OVRF::cuttoffToGain(params.vcutoff, dt));
        return last = next;
    }

    constexpr T Last() const {
        return last;
    }
    constexpr T Velocity() const {
        return velocity;
    }

    void Reset() {
        warm = false;
    }

    // params
    DoubleExpoParams params;

   private:
    // state
    bool warm;
    T last;
    T velocity;
};

struct OneEuroParams {
    OneEuroParams() : cutoff(1), vcutoff(1), relax(1) {}
    OneEuroParams(float c, float vc, float r) : cutoff(c), vcutoff(vc), relax(r) {}
    float cutoff;
    float vcutoff;
    float relax;
};

// Source: http://cristal.univ-lille.fr/~casiez/1euro/
// This is a dyamic low pass filter, that increases the cutoff as speed increases
template <typename T>
class OneEuroFilter {
   public:
    OneEuroFilter() : warm(false), last(T{}), xfilter(), dxfilter() {}
    OneEuroFilter(OneEuroParams p)
        : params(p), warm(false), last(T{}), xfilter(p.cutoff), dxfilter(p.vcutoff) {}
    OneEuroFilter(float c, float vc, float r) : OneEuroFilter(OneEuroParams(c, vc, r)) {}

    T Filter(T x, float dt) {
        if (dt > 0.0f) {
            if (!warm) {
                warm = true;
                // prime the inner xfilter assuming no relax,
                // dxfilter must wait until we have another sample to compute intantanious velocity
                // from
                return last = xfilter.Filter(x, dt);
            }

            // velocity/speed
            dxfilter.params.cutoff = params.vcutoff;
            float divdt =
                (1.0f / std::max(OVR::Math<float>::Tolerance(), dt)); // avoid divide by zero
            T instantVelocity = OVRF::scale(OVRF::diff(x, last), divdt);
            float filteredSpeed = OVRF::mag(dxfilter.Filter(instantVelocity, dt));

            // dynamic filter based on speed
            xfilter.params.cutoff = params.cutoff + params.relax * filteredSpeed;
            // if (params.relax == 5.1f) ALOG("VrHands effective cutoff: %.3f speed: %.3f",
            // xfilter.params.cutoff, filteredSpeed);
            return last = xfilter.Filter(x, dt);
        } else
            return Last();
    }

    void Reset() {
        warm = false;
        xfilter.Reset();
        dxfilter.Reset();
    }

    constexpr T Last() const {
        return last;
    }

    // params
    OneEuroParams params;

   private:
    // state
    bool warm;
    T last;
    SingleExpoFilter<T> xfilter;
    SingleExpoFilter<T> dxfilter;
};

template <class T>
struct Latch {
   public:
    Latch(T initial) : Value(initial), HeldValue(T{}), Holding(false) {}

    T Update(T desired, bool hold) {
        if (hold && !Holding)
            HeldValue = Value; // hold the previous value
        Holding = hold;
        return Value = Holding ? HeldValue : desired;
    }

    T Value;
    T HeldValue;
    bool Holding;
};

// A Latch, but with a smooth transition upon release
template <class T>
struct SmoothLatch {
   public:
    SmoothLatch(T initial, float cutoff)
        : Value(initial), HeldValue(T{}), Holding(false), MixFilter(cutoff) {}

    T Update(T desired, bool hold, float dt) {
        if (hold && !Holding) // begin holding
        {
            HeldValue = Value;
            MixFilter = SingleExpoFilter<float>(MixFilter.params.cutoff);
        }
        Holding = hold;
        LerpFactor = MixFilter.Filter(Holding ? 0 : 1, dt);
        return Value = OVRF::lerp(HeldValue, desired, LerpFactor);
    }

    T Value;
    T HeldValue;
    bool Holding;
    float LerpFactor;
    SingleExpoFilter<float> MixFilter;
};
} // namespace OVR

#endif // OVR_Filter_h
