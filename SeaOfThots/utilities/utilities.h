#pragma once

#define Assert( _exp ) ((void)0)
#define PI           3.14159265358979323846  /* pi */
#include "memory/memory.hpp"
#include "vector/vector.hpp"
#include "../../SDK/include/SDK.hpp"
#include <cmath>
#include <stdarg.h>  // For va_start, etc.
#include <memory>    // For std::unique_ptr

using namespace SDK;

namespace Utilities {
	std::string string_format(const std::string fmt_str, ...);

	float DotProduct(FVector a, FVector b);
	struct vMatrix
	{
		vMatrix() {}
		vMatrix(
			float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23)
		{
			m_flMatVal[0][0] = m00;	m_flMatVal[0][1] = m01; m_flMatVal[0][2] = m02; m_flMatVal[0][3] = m03;
			m_flMatVal[1][0] = m10;	m_flMatVal[1][1] = m11; m_flMatVal[1][2] = m12; m_flMatVal[1][3] = m13;
			m_flMatVal[2][0] = m20;	m_flMatVal[2][1] = m21; m_flMatVal[2][2] = m22; m_flMatVal[2][3] = m23;
		}

		float* operator[](int i) { Assert((i >= 0) && (i < 3)); return m_flMatVal[i]; }
		const float* operator[](int i) const { Assert((i >= 0) && (i < 3)); return m_flMatVal[i]; }
		float* Base() { return &m_flMatVal[0][0]; }
		const float* Base() const { return &m_flMatVal[0][0]; }

		float m_flMatVal[3][4];
	};

	vMatrix Matrix(FVector rot, FVector origin);

	bool WorldToScreen(FVector pos, UWorld* worldRef, FVector2D* screen, AHUD* hud);
}