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

	BoundingBox::BoundingBox(TESObjectREFR* a_ref) :
		pos(a_ref->GetPosition()),
		boundMin(a_ref->GetBoundMin()),
		boundMax(a_ref->GetBoundMax())
	{
		extents = boundMax - boundMin;
	}

	bool BSTransform::operator==(const BSTransform& a_rhs) const
	{
		return std::tie(rotate, translate, scale) == std::tie(a_rhs.rotate, a_rhs.translate, a_rhs.scale);
	}

	void BSTransform::ValidatePosition(TESObjectCELL* a_cell, TESObjectREFR* a_ref, const BoundingBox& a_refBB, const NiPoint3& a_spawnExtents)
	{
		if (!a_cell) {
			return;
		}

		const auto bhkWorld = a_cell->GetbhkWorld();
		if (!bhkWorld) {
			return;
		}

		const static auto worldScale = RE::bhkWorld::GetWorldScale();
		const NiPoint3    scaledExtents = a_spawnExtents * scale;

		NiPoint3 rayStart = a_refBB.pos;
		rayStart.z += a_refBB.extents.z;

		const NiPoint3 rayEnd = translate;
		const NiPoint3 rayVec = rayEnd - rayStart;

		hkpAllRayHitTempCollector collector;
		bhkPickData               pick;

		pick.rayInput.from = rayStart * worldScale;
		pick.rayInput.to = rayEnd * worldScale;
		pick.rayInput.enableShapeCollectionFilter = false;
		pick.rayInput.filterInfo.SetCollisionLayer(COL_LAYER::kLOS);
		pick.allRayHitTempCollector = &collector;

		if (bhkWorld->PickObject(pick)) {
			float                        minFraction = 1.0f;
			const hkpWorldRayCastOutput* bestHit = nullptr;

			for (const auto& hit : collector.hits) {
				if (hit.hitFraction >= minFraction) {
					continue;
				}

				if (!hit.rootCollidable) {
					continue;
				}

				if (const auto layer = hit.rootCollidable->GetCollisionLayer(); layer == COL_LAYER::kBiped ||
																				layer == COL_LAYER::kDeadBip ||
																				layer == COL_LAYER::kClutter ||
																				layer == COL_LAYER::kProjectile ||
																				layer == COL_LAYER::kSpell ||
																				layer == COL_LAYER::kWeapon ||
																				layer == COL_LAYER::kCloudTrap) {
					continue;
				}

				auto hitRef = TESHavokUtilities::FindCollidableRef(*hit.rootCollidable);
				if (hitRef && (hitRef == a_ref || IsInBoundingBox(hitRef->GetPosition(), a_refBB.boundMin, a_refBB.boundMax))) {
					continue;
				}

				minFraction = hit.hitFraction;
				bestHit = &hit;
			}

			if (bestHit) {
				NiPoint3 normal{
					bestHit->normal.quad.m128_f32[0],
					bestHit->normal.quad.m128_f32[1],
					bestHit->normal.quad.m128_f32[2]
				};

				NiMatrix3 rot(rotate);

				const float projectedDepth =
					(std::abs(normal.Dot(rot.GetVectorX())) * scaledExtents.x) +
					(std::abs(normal.Dot(rot.GetVectorY())) * scaledExtents.y) +
					(std::abs(normal.Dot(rot.GetVectorZ())) * scaledExtents.z);

				const NiPoint3 hitPos = rayStart + (rayVec * minFraction);
				translate = hitPos + (normal * (projectedDepth + 1.0f));
			}
		}
	}
}
