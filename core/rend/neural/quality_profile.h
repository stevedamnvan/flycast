// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <string>

namespace flycast::rend::neural {

enum class QualityProfile : int {
	FaithfulDreamcastRemaster = 0,
	EnhancedMaterials = 1,
	PhotorealExperimental = 2,
	UncannyCinematic = 3,
};

enum class StyleFamily : int {
	Auto = 0,
	Realistic3D = 1,
	Stylized3D = 2,
	CelShaded = 3,
	RacingFastCamera = 4,
	ParticleHeavyArcade = 5,
	SpriteHeavy2D = 6,
	Mixed3DVideo = 7,
};

struct QualityProfileDescriptor {
	QualityProfile profile = QualityProfile::FaithfulDreamcastRemaster;
	StyleFamily style = StyleFamily::Auto;
	const char *name = "Faithful Dreamcast Remaster";
	const char *styleName = "Automatic / unclassified";
	std::string externalRecommendation;
	bool faithful = true;
	bool conservativeTemporalMask = true;
	bool protectCharacters = true;
	bool bypassGenerative = false;
};

QualityProfileDescriptor ResolveQualityProfile(int profile, int style);

} // namespace flycast::rend::neural
