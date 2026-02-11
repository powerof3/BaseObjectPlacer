#include "Transform.h"

namespace RE
{
	bool Point3Range::operator==(const Point3Range& a_rhs) const
	{
		return std::tie(x, y, z) == std::tie(a_rhs.x, a_rhs.y, a_rhs.z);
	}

	NiPoint3 Point3Range::min() const
	{
		return { x.min, y.min, z.min };
	}

	NiPoint3 Point3Range::max() const
	{
		return { x.max.value_or(x.min), y.max.value_or(y.min), z.max.value_or(z.min) };
	}

	NiPoint3 Point3Range::value(std::size_t seed) const
	{
		return { x.value(seed), y.value(seed), z.value(seed) };
	}

	bool BSTransform::operator==(const BSTransform& a_rhs) const
	{
		return std::tie(rotate, translate, scale) == std::tie(a_rhs.rotate, a_rhs.translate, a_rhs.scale);
	}
}
