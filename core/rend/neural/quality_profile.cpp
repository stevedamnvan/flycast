// SPDX-License-Identifier: GPL-2.0-or-later
#include "quality_profile.h"

#include <algorithm>

namespace flycast::rend::neural {

QualityProfileDescriptor ResolveQualityProfile(int profileValue, int styleValue)
{
	QualityProfileDescriptor result;
	result.profile = static_cast<QualityProfile>(std::clamp(profileValue, 0, 2));
	result.style = static_cast<StyleFamily>(std::clamp(styleValue, 0, 7));
	static constexpr const char *styleNames[] = {
		"Automatic / unclassified", "Realistic 3D", "Stylized 3D", "Cel-shaded",
		"Racing / fast camera", "Particle-heavy arcade", "Sprite-heavy / 2D",
		"Mixed 3D and pre-rendered / video"
	};
	result.styleName = styleNames[static_cast<int>(result.style)];
	switch (result.profile)
	{
	case QualityProfile::EnhancedMaterials:
		result.name = "Enhanced Materials";
		result.externalRecommendation =
			"Tone Intensity zero or low; moderate Structure Intensity; user controlled";
		result.faithful = true;
		result.conservativeTemporalMask = false;
		result.protectCharacters = true;
		break;
	case QualityProfile::PhotorealExperimental:
		result.name = "Photoreal Experimental";
		result.externalRecommendation =
			"Higher Structure and Tone are optional; explicitly non-faithful and user controlled";
		result.faithful = false;
		result.conservativeTemporalMask = false;
		result.protectCharacters = false;
		break;
	default:
		result.profile = QualityProfile::FaithfulDreamcastRemaster;
		result.name = "Faithful Dreamcast Remaster";
		result.externalRecommendation =
			"Tone Intensity zero; lowest useful Structure Intensity; user controlled";
		result.faithful = true;
		result.conservativeTemporalMask = true;
		result.protectCharacters = true;
		break;
	}
	if (result.style == StyleFamily::SpriteHeavy2D)
	{
		result.bypassGenerative = true;
		result.externalRecommendation += "; generative Neural Rendering bypass recommended";
	}
	else if (result.style == StyleFamily::CelShaded)
		result.externalRecommendation += "; preserve flat shading with minimum Structure Intensity";
	else if (result.style == StyleFamily::RacingFastCamera
		|| result.style == StyleFamily::ParticleHeavyArcade)
		result.externalRecommendation += "; conservative reactive protection recommended";
	return result;
}

} // namespace flycast::rend::neural
