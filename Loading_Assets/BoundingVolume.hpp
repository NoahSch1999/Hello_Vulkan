#pragma once
#include "Helpers.hpp"

namespace aZero::BoundingVolume
{
	struct BoundingSphere
	{
		glm::vec3 Origin;
		float Radius;
	};

	struct BoundingFrustum
	{
		glm::vec4 Rotation;

		glm::vec3 Position;
        float RightSlope;

        float LeftSlope;
        float TopSlope;
        float BottomSlope;
        float Near;

        float Far;
	};
}