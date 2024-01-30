#pragma once

#include "bit_flags.hpp"
#include "main.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace glm;

namespace Moxaic
{
    class Transform
    {
    public:
        MXC_NO_VALUE_PASS(Transform);

        enum Axis {
            X = 1 << 0,
            Y = 1 << 1,
            Z = 1 << 2,
        };

        Transform() = default;
        virtual ~Transform() = default;

        void LocalTranslate(vec3 delta)
        {
            delta = rotate(orientation_, delta);
            position_ += delta;
        }

        void LocalTranslate(const vec3 delta, const BitFlags<Axis> zeroOrientationAxis)
        {
            vec3 euler = eulerAngles(orientation_);
            euler.x = zeroOrientationAxis.ContainsFlag(X) ? 0 : euler.x;
            euler.y = zeroOrientationAxis.ContainsFlag(Y) ? 0 : euler.y;
            euler.z = zeroOrientationAxis.ContainsFlag(Z) ? 0 : euler.z;
            const auto lockedOrientation = quat(euler);
            position_ += rotate(lockedOrientation, delta);
        }

        void LocalTranslate(const float x, const float y, const float z)
        {
            const auto delta = vec3(x, y, z);
            position_ += rotate(orientation_, delta);
        }

        void Rotate(const float x, const float y, const float z)
        {
            const auto rotation = quat(vec3(
              radians(x),
              radians(y),
              radians(z)));
            orientation_ = rotation * orientation_;
        }

        void Rotate(const vec3 euler)
        {
            const auto rotation = quat(euler);
            orientation_ = rotation * orientation_;
        }

        void LocalRotate(const float x, const float y, const float z)
        {
            auto rotation = quat(vec3(
              radians(x),
              radians(y),
              radians(z)));
            orientation_ = orientation_ * rotation;
        }

        vec3 position_{zero<vec3>()};
        quat orientation_{identity<quat>()};

        auto ModelMatrix() const
        {
            const mat4 rot = toMat4(orientation_);
            const mat4 pos = translate(identity<mat4>(), position_);
            return pos * rot;
        }
    };
}// namespace Moxaic
