#pragma once

#include "bit_flags.hpp"
#include "main.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace glm;

namespace Moxaic
{
    class Transform final
    {
    public:
        MXC_NO_VALUE_PASS(Transform);

        enum AxisFlags {
            X = 1 << 0,
            Y = 1 << 1,
            Z = 1 << 2,
        };

        vec3 position{zero<vec3>()};
        quat orientation{identity<quat>()};

        Transform() = default;
        ~Transform() = default;

        void LocalTranslate(vec3 delta)
        {
            position += rotate(orientation, delta);
        }

        void LocalTranslate(const vec3 delta, const BitFlags<AxisFlags> zeroOrientationAxis)
        {
            vec3 euler = eulerAngles(orientation);
            euler.x = zeroOrientationAxis.ContainsFlag(X) ? 0 : euler.x;
            euler.y = zeroOrientationAxis.ContainsFlag(Y) ? 0 : euler.y;
            euler.z = zeroOrientationAxis.ContainsFlag(Z) ? 0 : euler.z;
            const auto lockedOrientation = quat(euler);
            position += rotate(lockedOrientation, delta);
        }

        void LocalTranslate(const float x, const float y, const float z)
        {
            const auto delta = vec3(x, y, z);
            position += rotate(orientation, delta);
        }

        void Rotate(const float x, const float y, const float z)
        {
            const auto rotation = quat(vec3(
              radians(x),
              radians(y),
              radians(z)));
            orientation = rotation * orientation;
        }

        void Rotate(const vec3 euler)
        {
            const auto rotation = quat(euler);
            orientation = rotation * orientation;
        }

        void LocalRotate(const float x, const float y, const float z)
        {
            const auto rotation = quat(vec3(
              radians(x),
              radians(y),
              radians(z)));
            orientation = orientation * rotation;
        }

        auto ModelMatrix() const
        {
            const mat4 rot = toMat4(orientation);
            const mat4 pos = translate(identity<mat4>(), position);
            return pos * rot;
        }
    };
}// namespace Moxaic
