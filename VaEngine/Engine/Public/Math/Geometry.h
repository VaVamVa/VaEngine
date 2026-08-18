#pragma once

#include "Math/Container.h"
#include <algorithm>

// 중심(center)과 반지름(radius)으로 정의되는 구.
struct Sphere
{
	Vector3 center;
	float radius;

	/*
	* 로컬 공간 Sphere를 월드 공간으로 변환한다.
	
	* 비균등 스케일을 적용하면 구는 타원체가 되므로,
	  그 타원체를 완전히 포함하는 반지름은 축 스케일 중 최댓값을 곱한 값이다.
	*/
	static Sphere Transform(const Sphere& sphere, const Matrix4x4& transform)
	{
		Vector3 transformedCenter = TransformPoint(sphere.center, transform);
		// row-vector 컨벤션(Container.h)이라 각 축의 스케일은 transform의 해당 행(m[i][0..2])의 길이다.
		float scaleX = std::sqrt(transform.m[0][0] * transform.m[0][0] + transform.m[0][1] * transform.m[0][1] + transform.m[0][2] * transform.m[0][2]);
		float scaleY = std::sqrt(transform.m[1][0] * transform.m[1][0] + transform.m[1][1] * transform.m[1][1] + transform.m[1][2] * transform.m[1][2]);
		float scaleZ = std::sqrt(transform.m[2][0] * transform.m[2][0] + transform.m[2][1] * transform.m[2][1] + transform.m[2][2] * transform.m[2][2]);
		float maxScale = std::max({ scaleX, scaleY, scaleZ });
		float transformedRadius = sphere.radius * maxScale;

		return Sphere{ transformedCenter, transformedRadius };
	}

	// 두 Sphere를 모두 포함하는 근사 구를 계산한다.
	// 한쪽이 다른 한쪽을 완전히 포함하면 그 큰 쪽을 그대로 반환하고, 그 외에는 두 중심을 잇는
	// 직선상에서 각 구의 가장 먼 표면점을 잇는 선분의 중점·절반 길이를 새 중심·반지름으로 삼는다.
	static Sphere Merge(const Sphere& a, const Sphere& b)
	{
		Vector3 delta = b.center - a.center;
		float dist = delta.Length();

		if (dist + b.radius <= a.radius) return a;
		if (dist + a.radius <= b.radius) return b;

		float newRadius = (dist + a.radius + b.radius) * 0.5f;
		Vector3 newCenter = a.center + delta * ((newRadius - a.radius) / dist);
		return Sphere{ newCenter, newRadius };
	}
};

// 최소점(min)과 최대점(max)으로 정의되는, 월드 좌표축에 정렬된 상자.
struct AABB
{
	Vector3 min;
	Vector3 max;

	// 로컬 공간 AABB를 월드 공간으로 변환한다.
	// 8개 꼭짓점을 각각 변환하지 않고, 중심(center)·반너비(halfExtents) 표현으로 바꾼 뒤
	// 회전/스케일 성분의 각 축 절댓값을 반너비에 곱해 새 반너비를 구한다(Arvo's method).
	static AABB Transform(const AABB& aabb, const Matrix4x4& transform)
	{
		Vector3 center = (aabb.min + aabb.max) * 0.5f;
		Vector3 halfExtents = (aabb.max - aabb.min) * 0.5f;

		Vector3 transformedCenter = TransformPoint(center, transform);

		// row-vector 컨벤션이라 world 축 i의 반너비는 transform의 i번째 열(column) 성분들의 가중합이다.
		Vector3 transformedHalfExtents;
		transformedHalfExtents.x = std::abs(transform.m[0][0]) * halfExtents.x + std::abs(transform.m[1][0]) * halfExtents.y + std::abs(transform.m[2][0]) * halfExtents.z;
		transformedHalfExtents.y = std::abs(transform.m[0][1]) * halfExtents.x + std::abs(transform.m[1][1]) * halfExtents.y + std::abs(transform.m[2][1]) * halfExtents.z;
		transformedHalfExtents.z = std::abs(transform.m[0][2]) * halfExtents.x + std::abs(transform.m[1][2]) * halfExtents.y + std::abs(transform.m[2][2]) * halfExtents.z;

		return AABB{ transformedCenter - transformedHalfExtents, transformedCenter + transformedHalfExtents };
	}
};

// 중심(center)·반너비(halfExtents)·정규직교 축(axis) 3개로 정의되는, 임의 방향을 가질 수 있는 상자.
struct OBB
{
	Vector3 center;
	Vector3 halfExtents;
	Vector3 axis[3];

	// 로컬 공간 OBB를 월드 공간으로 변환한다.
	// 각 축을 방향 벡터로 변환한 뒤, 그 길이를 해당 축의 스케일로 반너비에 곱하고 축 자체는 다시 정규화한다.
	static OBB Transform(const OBB& obb, const Matrix4x4& transform)
	{
		Vector3 worldAxisX = TransformDirection(obb.axis[0], transform);
		Vector3 worldAxisY = TransformDirection(obb.axis[1], transform);
		Vector3 worldAxisZ = TransformDirection(obb.axis[2], transform);

		float scaleX = worldAxisX.Length();
		float scaleY = worldAxisY.Length();
		float scaleZ = worldAxisZ.Length();

		OBB result;
		result.center = TransformPoint(obb.center, transform);
		result.halfExtents = { obb.halfExtents.x * scaleX, obb.halfExtents.y * scaleY, obb.halfExtents.z * scaleZ };
		result.axis[0] = worldAxisX.Normalized();
		result.axis[1] = worldAxisY.Normalized();
		result.axis[2] = worldAxisZ.Normalized();
		return result;
	}
};

// 두 끝점(p0, p1)을 잇는 선분에 반지름(radius)만큼 두께를 준 도형.
struct Capsule
{
	Vector3 p0;
	Vector3 p1;
	float radius;

	// 로컬 공간 Capsule을 월드 공간으로 변환한다.
	// 두 끝점은 점으로 변환하고, 반지름은 Sphere와 동일하게 축 스케일 중 최댓값을 곱한다.
	static Capsule Transform(const Capsule& capsule, const Matrix4x4& transform)
	{
		Vector3 transformedP0 = TransformPoint(capsule.p0, transform);
		Vector3 transformedP1 = TransformPoint(capsule.p1, transform);

		float scaleX = std::sqrt(transform.m[0][0] * transform.m[0][0] + transform.m[0][1] * transform.m[0][1] + transform.m[0][2] * transform.m[0][2]);
		float scaleY = std::sqrt(transform.m[1][0] * transform.m[1][0] + transform.m[1][1] * transform.m[1][1] + transform.m[1][2] * transform.m[1][2]);
		float scaleZ = std::sqrt(transform.m[2][0] * transform.m[2][0] + transform.m[2][1] * transform.m[2][1] + transform.m[2][2] * transform.m[2][2]);
		float maxScale = std::max({ scaleX, scaleY, scaleZ });

		return Capsule{ transformedP0, transformedP1, capsule.radius * maxScale };
	}
};

// 한 코너(origin)에서 두 변 방향 벡터(row, col)로 정의되는 유한 사각형. 길이는 각 벡터의 크기.
struct Quad
{
	Vector3 origin;
	Vector3 row;
	Vector3 col;

	// 로컬 공간 Quad를 월드 공간으로 변환한다.
	// origin은 점, row/col은 방향+길이를 가진 벡터라 이동을 적용하지 않는다.
	static Quad Transform(const Quad& quad, const Matrix4x4& transform)
	{
		Vector3 transformedOrigin = TransformPoint(quad.origin, transform);
		Vector3 transformedRow = TransformDirection(quad.row, transform);
		Vector3 transformedCol = TransformDirection(quad.col, transform);
		return Quad{ transformedOrigin, transformedRow, transformedCol };
	}
};

