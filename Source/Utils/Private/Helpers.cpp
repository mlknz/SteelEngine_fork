#include "Utils/Helpers.hpp"

namespace Details
{
    template <glm::length_t N>
    bool IsMatrixValid(const glm::mat<N, N, float, glm::defaultp>& matrix)
    {
        for (glm::length_t i = 0; i < N; ++i)
        {
            for (glm::length_t j = 0; j < N; ++j)
            {
                if (std::isnan(matrix[i][j]) || std::isinf(matrix[i][j]))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool IsQuatValid(const glm::quat& quaternion)
    {
        for (glm::length_t i = 0; i < glm::quat::length(); ++i)
        {
            if (std::isnan(quaternion[i]) || std::isinf(quaternion[i]))
            {
                return false;
            }
        }

        return true;
    }
}

bool Matrix4::IsValid(const glm::mat4& matrix)
{
    return Details::IsMatrixValid(matrix);
}

bool Matrix3::IsValid(const glm::mat3& matrix)
{
    return Details::IsMatrixValid(matrix);
}

bool Quat::IsValid(const glm::quat& quaternion)
{
    return Details::IsQuatValid(quaternion);
}

bool Math::IsNearlyZero(float value)
{
    return glm::epsilonEqual(value, 0.0f, glm::epsilon<float>());
}

float Math::GetRangePercentage(float min, float max, float value)
{
    const float size = max - min;

    if (IsNearlyZero(size))
    {
        return value >= max ? 1.0f : 0.0f;
    }

    return (value - min) / size;
}

float Math::RemapValueClamped(const glm::vec2& inputRange, const glm::vec2& outputRange, float value)
{
    const float percentage = GetRangePercentage(inputRange.x, inputRange.y, value);

    const float t = std::clamp(percentage, outputRange.x, outputRange.y);

    return std::lerp(outputRange.x, outputRange.y, t);
}

Bytes GetBytes(const std::vector<ByteView>& byteViews)
{
    size_t size = 0;
    for (const auto& byteView : byteViews)
    {
        size += byteView.size;
    }

    Bytes bytes(size);

    size_t offset = 0;
    for (const auto& byteView : byteViews)
    {
        std::memcpy(bytes.data() + offset, byteView.data, byteView.size);

        offset += byteView.size;
    }

    return bytes;
}
