#include "Transform.h"

namespace RE
{
	bool Point3Range::operator==(const Point3Range& a_rhs) const
	{
		return std::tie(x, y, z) == std::tie(a_rhs.x, a_rhs.y, a_rhs.z);
	}

	RE::NiPoint3 Point3Range::min() const
	{
		return RE::NiPoint3(x.min, y.min, z.min);
	}

	RE::NiPoint3 Point3Range::max() const
	{
		return RE::NiPoint3(x.max.value_or(x.min), y.max.value_or(y.min), z.max.value_or(z.min));
	}

	RE::NiPoint3 Point3Range::value(std::size_t seed) const
	{
		return RE::NiPoint3(x.value(seed), y.value(seed), z.value(seed));
	}

	Point3Range Point3Range::deg_to_rad() const
	{
		Point3Range tmp = *this;
		tmp.x.deg_to_rad();
		tmp.y.deg_to_rad();
		tmp.z.deg_to_rad();
		return tmp;
	}

	bool BSTransform::operator==(const BSTransform& a_rhs) const
	{
		return std::tie(rotate, translate, scale) == std::tie(a_rhs.rotate, a_rhs.translate, a_rhs.scale);
	}

	BSTransform BSTransformRange::value(std::size_t a_seed) const
	{
		return BSTransform(rotate.value(a_seed), translate.value(a_seed), scale.value(a_seed));
	}
}
