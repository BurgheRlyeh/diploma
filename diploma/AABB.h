#pragma once

#include <limits>

#include "framework.h"

#undef min
#undef max

struct AABB {
    DirectX::SimpleMath::Vector4 bmin{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        0
    };

    DirectX::SimpleMath::Vector4 bmax{
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::min(),
        std::numeric_limits<float>::min(),
        0
    };

    inline DirectX::SimpleMath::Vector4 getVert(int idx) const {
        return {
            idx & 1 ? bmax.x : bmin.x,
            idx & 2 ? bmax.y : bmin.y,
            idx & 4 ? bmax.z : bmin.z,
            0
        };
    }

    inline DirectX::SimpleMath::Vector4 extent() const {
        return bmax - bmin;
    }

    inline void grow(DirectX::SimpleMath::Vector4 point) {
        bmin = DirectX::SimpleMath::Vector4::Min(bmin, point);
        bmax = DirectX::SimpleMath::Vector4::Max(bmax, point);
    }

    inline void grow(const AABB& aabb) {
        if (aabb.bmin.x != std::numeric_limits<float>::max()) {
            grow(aabb.bmin);
            grow(aabb.bmax);
        }
    }

    inline float area() {
        DirectX::SimpleMath::Vector4 e{ bmax - bmin };
        return e.x * e.y + e.y * e.z + e.z * e.x;
    }
};
