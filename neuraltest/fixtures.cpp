// SPDX-License-Identifier: GPL-2.0-or-later
#include "harness.h"

#include <array>
#include <cmath>

namespace neuraltest {
namespace {

constexpr std::array<const char *, 13> names = {
	"static-triangle", "translate-x4", "camera-translate", "deform-one-vertex",
	"topology-change", "rotate-quad", "textured-checker-edge", "opaque-pt-tr-stack",
	"particles", "modvol-shadow", "edge-clip", "naomi2-matrix", "wrong-history"
};

void AddTriangle(Fixture& f, Vertex a, Vertex b, Vertex c)
{
	const auto base = static_cast<std::uint16_t>(f.vertices.size());
	f.vertices.insert(f.vertices.end(), {a, b, c});
	f.indices.insert(f.indices.end(), {base, static_cast<std::uint16_t>(base + 1),
		static_cast<std::uint16_t>(base + 2)});
}

void AddQuad(Fixture& f, float left, float top, float right, float bottom, float z,
	float r, float g, float b, float a = 1.f)
{
	const Vertex tl{left, top, z, r, g, b, a};
	const Vertex tr{right, top, z, r, g, b, a};
	const Vertex bl{left, bottom, z, r, g, b, a};
	const Vertex br{right, bottom, z, r, g, b, a};
	AddTriangle(f, tl, tr, bl);
	AddTriangle(f, bl, tr, br);
}

Vertex Rotate(Vertex v, float radians)
{
	const float c = std::cos(radians);
	const float s = std::sin(radians);
	const float x = v.x;
	const float y = v.y;
	v.x = x * c - y * s;
	v.y = x * s + y * c;
	return v;
}

} // namespace

const std::vector<std::string>& FixtureNames()
{
	static const std::vector<std::string> value(names.begin(), names.end());
	return value;
}

bool MakeFixture(const std::string& name, std::uint32_t frame, Fixture& f, std::string& error)
{
	f = {};
	f.name = name;
	bool found = false;
	for (const char *candidate : names)
		found = found || name == candidate;
	if (!found && name != "ta-stream")
	{
		error = "unknown fixture: " + name;
		return false;
	}

	const float phase = static_cast<float>(frame);
	if (name == "static-triangle" || name == "ta-stream")
	{
		AddTriangle(f, {-0.72f, 0.62f, .25f, 1.f, .08f, .08f, 1.f},
			{0.70f, 0.48f, .25f, .08f, 1.f, .12f, 1.f},
			{-0.08f, -0.72f, .25f, .12f, .20f, 1.f, 1.f});
	}
	else if (name == "wrong-history")
	{
		if ((frame & 1u) == 0)
			AddTriangle(f, {-.92f, .72f, .2f, 1, .7f, .05f, 1},
				{-.08f, .38f, .2f, .8f, .1f, .6f, 1}, {-.65f, -.7f, .2f, .1f, .7f, 1, 1});
		else
			AddQuad(f, .12f, .72f, .92f, -.7f, .45f, .1f, .95f, .4f);
		f.dynamic = true;
	}
	else if (name == "translate-x4" || name == "camera-translate")
	{
		const float dx = phase * 4.f / 320.f;
		if (name == "camera-translate")
		{
			for (int y = 0; y < 3; ++y)
				for (int x = 0; x < 5; ++x)
					AddQuad(f, -0.9f + x * .38f + dx, 0.72f - y * .55f,
						-0.67f + x * .38f + dx, 0.42f - y * .55f, .2f + .02f * y,
						.15f + .15f * x, .25f + .2f * y, .8f - .1f * x);
		}
		else
			AddQuad(f, -.55f + dx, .5f, .35f + dx, -.5f, .25f, .95f, .35f, .08f);
		f.dynamic = true;
		f.hasMotionGroundTruth = true;
	}
	else if (name == "deform-one-vertex")
	{
		AddTriangle(f, {-.72f, .62f, .25f, 1, .2f, .1f, 1},
			{.65f + phase * .03f, .5f - phase * .02f, .25f, .1f, 1, .2f, 1},
			{-.1f, -.7f, .25f, .2f, .3f, 1, 1});
		f.dynamic = true;
		f.hasMotionGroundTruth = true;
	}
	else if (name == "topology-change")
	{
		AddQuad(f, -.65f, .6f, .6f, -.6f, .3f, .8f, .2f, .8f);
		if ((frame & 1u) != 0)
			AddTriangle(f, {-.65f, .6f, .29f, 1, 1, .1f, 1}, {.6f, -.6f, .29f, 1, .1f, 1, 1},
				{.82f, .45f, .29f, .1f, 1, 1, 1});
		f.dynamic = true;
	}
	else if (name == "rotate-quad" || name == "textured-checker-edge")
	{
		std::array<Vertex, 4> q{{{-.62f, .62f, .3f, 1, 1, 1, 1}, {.62f, .62f, .3f, .05f, .05f, .05f, 1},
			{-.62f, -.62f, .3f, .05f, .05f, .05f, 1}, {.62f, -.62f, .3f, 1, 1, 1, 1}}};
		const float angle = name == "rotate-quad" ? .19f + phase * .02f : .13f;
		for (auto& v : q) v = Rotate(v, angle);
		AddTriangle(f, q[0], q[1], q[2]);
		AddTriangle(f, q[2], q[1], q[3]);
		f.dynamic = name == "rotate-quad";
		f.hasMotionGroundTruth = f.dynamic;
	}
	else if (name == "opaque-pt-tr-stack")
	{
		AddQuad(f, -.85f, .75f, .55f, -.75f, .7f, .08f, .2f, .9f);
		AddQuad(f, -.55f, .55f, .75f, -.55f, .4f, .1f, .9f, .25f);
		AddQuad(f, -.25f, .35f, .9f, -.35f, .15f, .95f, .3f, .1f, .65f);
		f.hasDepthGroundTruth = true;
	}
	else if (name == "particles")
	{
		for (int i = 0; i < 18; ++i)
		{
			const float x = -.85f + static_cast<float>((i * 37 + frame * 11) % 170) / 100.f;
			const float y = -.75f + static_cast<float>((i * 53 + frame * 7) % 150) / 100.f;
			AddQuad(f, x, y + .08f, x + .08f, y, .2f, 1.f, .6f, .08f, .65f);
		}
		f.dynamic = true;
	}
	else if (name == "modvol-shadow")
	{
		AddQuad(f, -.78f, .68f, .78f, -.68f, .6f, .55f, .7f, .95f);
		AddTriangle(f, {-.45f, .48f, .25f, .12f, .12f, .18f, 1},
			{.58f, .22f, .25f, .12f, .12f, .18f, 1}, {-.15f, -.58f, .25f, .12f, .12f, .18f, 1});
		f.hasDepthGroundTruth = true;
	}
	else if (name == "edge-clip")
	{
		const float dx = phase * .05f;
		AddQuad(f, -.98f + dx, .65f, 1.12f + dx, -.65f, .3f, .2f, .85f, .95f);
		f.dynamic = true;
		f.hasMotionGroundTruth = true;
	}
	else if (name == "naomi2-matrix")
	{
		const float dx = phase * .025f;
		AddQuad(f, -.55f + dx, .62f, .55f + dx, -.58f, .2f, .9f, .55f, .1f);
		f.dynamic = true;
		f.hasMotionGroundTruth = true;
	}

	return true;
}

} // namespace neuraltest
