#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL 
#include <glm/gtx/norm.hpp>
#include <numbers>

namespace aZero::Helpers
{
	inline uint16_t float_to_half(float f)
	{
		uint32_t x;
		std::memcpy(&x, &f, sizeof(x));

		uint32_t sign = (x >> 16) & 0x8000;
		int32_t exp = ((x >> 23) & 0xFF) - 127 + 15;
		uint32_t mant = x & 0x7FFFFF;

		if (exp <= 0)
		{
			if (exp < -10)
				return (uint16_t)sign;

			mant = (mant | 0x800000) >> (1 - exp);
			return (uint16_t)(sign | ((mant + 0x1000) >> 13));
		}
		else if (exp >= 31)
		{
			if (mant == 0)
				return (uint16_t)(sign | 0x7C00); // Infinity

			return (uint16_t)(sign | 0x7C00 | (mant >> 13)); // NaN
		}

		return (uint16_t)(sign | (exp << 10) | ((mant + 0x1000) >> 13));
	}
}