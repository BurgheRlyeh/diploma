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
        0.f
    };

    DirectX::SimpleMath::Vector4 bmax{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        0.f
    };

    inline bool isEmpty() const {
        return bmin.x == std::numeric_limits<float>::max()
            && bmin.y == std::numeric_limits<float>::max()
            && bmin.z == std::numeric_limits<float>::max()
            && bmax.x == std::numeric_limits<float>::lowest()
            && bmax.y == std::numeric_limits<float>::lowest()
            && bmax.z == std::numeric_limits<float>::lowest();
    }

    inline bool isCorrect() const {
        return bmin.x + std::numeric_limits<float>::epsilon() < bmax.x
            && bmin.y + std::numeric_limits<float>::epsilon() < bmax.y
            && bmin.z + std::numeric_limits<float>::epsilon() < bmax.z;
    }

    inline DirectX::SimpleMath::Vector4 getVert(int idx) const {
        return {
            idx & 1 ? bmax.x : bmin.x,
            idx & 2 ? bmax.y : bmin.y,
            idx & 4 ? bmax.z : bmin.z,
            0
        };
    }

    inline DirectX::SimpleMath::Vector4 diagonal() const {
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

    inline float area() const {
        DirectX::SimpleMath::Vector4 e{ diagonal() };
        return 2 * (e.x * e.y + e.y * e.z + e.z * e.x);
    }

    inline int extentMax() const {
        DirectX::SimpleMath::Vector4 d{ diagonal() };
        if (d.x > d.y && d.x > d.z)
            return 0;
        else if (d.y > d.z)
            return 1;
        else
            return 2;
    }

    inline DirectX::SimpleMath::Vector4 relateVecPos(
        const DirectX::SimpleMath::Vector4& v
    ) const {
        DirectX::SimpleMath::Vector4 o{ v - bmin };
        if (bmax.x > bmin.x) o.x /= bmax.x - bmin.x;
        if (bmax.y > bmin.y) o.y /= bmax.y - bmin.y;
        if (bmax.z > bmin.z) o.z /= bmax.z - bmin.z;
        return o;
    }

    static inline AABB bbUnion(const AABB& bb1, const AABB& bb2) {
        return AABB{
            DirectX::SimpleMath::Vector4::Min(bb1.bmin, bb2.bmin),
            DirectX::SimpleMath::Vector4::Max(bb1.bmax, bb2.bmax)
        };
    }

    static inline AABB bbIntersection(const AABB& bb1, const AABB& bb2) {
        return {
            DirectX::SimpleMath::Vector4::Max(bb1.bmin, bb2.bmin),
            DirectX::SimpleMath::Vector4::Min(bb1.bmax, bb2.bmax)
        };
    }
};
