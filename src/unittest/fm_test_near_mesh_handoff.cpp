// SPDX-License-Identifier: GPL-3.0-or-later

#include "client/fm_near_mesh_handoff.h"
#include "client/fm_far_mesh_clip.h"
#include "client/fm_far_mesh_bounds.h"
#include "client/fm_mesh_priority.h"
#include "client/mesh_generator_thread.h"
#include "test.h"
#include <CMeshBuffer.h>
#include <SMesh.h>
#include <array>

namespace
{
class TestFmNearMeshHandoff : public TestBase
{
public:
	TestFmNearMeshHandoff() { TestManager::registerTestModule(this); }
	const char *getName() override { return "TestFmNearMeshHandoff"; }
	void runTests(IGameDef *gamedef) override
	{
		TEST(testFarBoundsSelectReadyChunks);
		TEST(testFarBoundsIncludeLightPoint);
		TEST(testClipPartialSurface);
		TEST(testClipBoundaryFaces);
		TEST(testClipOversizedFarGeometry);
		TEST(testClipLights);
		TEST(testClipVertexAttributes);
		TEST(testClipTeleportFallback);
		TEST(testClipBufferLimit);
		TEST(testSparseTerrainHandoff);
		TEST(testUnfinishedChunkAboveSurface);
		TEST(testMissingVisibleSurface);
		TEST(testChunkBoundaryFaces);
		TEST(testDisconnectedGeometry);
		TEST(testOccludedOmissionIsTemporary);
		TEST(testFarLightsNeedCoverage);
		TEST(testEmptyCompletedChunks);
		TEST(testIdleRequestCanRetry, gamedef);
		TEST(testPendingResults);
		TEST(testMeshPriorityAfterMove);
	}

	static bool unknown(const v3bpos_t &) { return false; }

	static void addQuad(scene::SMesh &mesh, u8 axis, int sign, float plane, float u0,
			float u1, float v0, float v1)
	{
		if (!mesh.getMeshBufferCount()) {
			auto *buffer = new scene::SMeshBuffer();
			mesh.addMeshBuffer(buffer);
			buffer->drop();
		}
		v3opos_t normal;
		normal[axis] = sign;
		std::array<video::S3DVertex, 4> vertices;
		for (size_t i = 0; i < vertices.size(); ++i) {
			v3opos_t p;
			p[axis] = plane;
			p[(axis + 1) % 3] = (i == 1 || i == 2) ? u1 : u0;
			p[(axis + 2) % 3] = i >= 2 ? v1 : v0;
			vertices[i] = video::S3DVertex(v3f::from(p * BS), v3f::from(normal),
					video::SColor(0xffffffff), v2f{});
		}
		const u16 indices[] = {0, 1, 2, 0, 2, 3};
		mesh.getMeshBuffer(0)->append(vertices.data(), vertices.size(), indices, 6);
	}

	static double area(const std::vector<irr_ptr<scene::SMeshBuffer>> &buffers)
	{
		double result = 0;
		for (const auto &buffer : buffers) {
			const auto &indices = buffer->Indices->Data;
			for (size_t i = 0; i + 2 < indices.size(); i += 3) {
				const auto a = buffer->getPosition(indices[i]);
				const auto b = buffer->getPosition(indices[i + 1]);
				const auto c = buffer->getPosition(indices[i + 2]);
				result += (b - a).crossProduct(c - a).getLength() / (2 * BS * BS);
			}
		}
		return result;
	}

	void testFarBoundsSelectReadyChunks()
	{
		// Reproduce the live cell: its aggregate bounds were zero even though
		// its geometry spans four completed near chunks away from its origin.
		const v3bpos_t origin(-16, 0, -19396);
		scene::SMesh mesh;
		addQuad(mesh, 1, 1, 43.5f, -0.5f, 63.5f, -0.5f, 63.5f);
		UASSERT(mesh.getBoundingBox().MinEdge == v3f());
		UASSERT(mesh.getBoundingBox().MaxEdge == v3f());
		farmesh::updateMeshBounds(mesh);
		const auto &bounds = mesh.getBoundingBox();
		UASSERTEQ(float, bounds.MinEdge.Y, 43.5f * BS);
		UASSERTEQ(float, bounds.MaxEdge.X, 63.5f * BS);
		UASSERT(!farmesh::intersectsNearChunk(bounds, origin, origin, 32));

		farmesh::FarMeshClipMask mask(origin, 1, 2);
		size_t selected = 0;
		for (pos_t z : {0, 2})
			for (pos_t x : {0, 2}) {
				const auto ready = origin + v3bpos_t(x, 2, z);
				if (farmesh::intersectsNearChunk(bounds, origin, ready, 32)) {
					mask.addChunk(ready);
					++selected;
				}
			}
		UASSERTEQ(size_t, selected, 4);
		// The previous zero bounds selected none of these chunks, leaving the
		// entire far surface visible over already drawn near terrain.
		UASSERT(mask.clip(*mesh.getMeshBuffer(0)).empty());
	}

	void testFarBoundsIncludeLightPoint()
	{
		scene::SMesh mesh;
		auto *buffer = new scene::SMeshBuffer();
		buffer->setPrimitiveType(scene::EPT_POINTS);
		// Far lights append directly to the vertex array, bypassing even the
		// buffer's bounds. A single light is not an empty mesh.
		buffer->Vertices->Data.emplace_back(
				v3f(40 * BS, -20 * BS, 10 * BS), v3f{}, video::SColor(0xffffffff), v2f{});
		buffer->Indices->Data.push_back(0);
		mesh.addMeshBuffer(buffer);
		buffer->drop();
		farmesh::updateMeshBounds(mesh);
		const auto point = buffer->getPosition(0);
		UASSERT(mesh.getBoundingBox().MinEdge == point);
		UASSERT(mesh.getBoundingBox().MaxEdge == point);
		UASSERT(buffer->getBoundingBox().MinEdge == point);
		UASSERT(farmesh::intersectsNearChunk(mesh.getBoundingBox(), {}, {2, -2, 0}, 32));
		farmesh::FarMeshClipMask mask({}, 1, 2);
		mask.addChunk({2, -2, 0});
		UASSERT(mask.clip(*buffer).empty());

		mesh.clear();
		farmesh::updateMeshBounds(mesh);
		UASSERT(mesh.getBoundingBox().MinEdge == v3f());
		UASSERT(mesh.getBoundingBox().MaxEdge == v3f());
	}

	void testClipPartialSurface()
	{
		// One ready near chunk must appear while the other three surface
		// chunks remain completely absent. Chunk size >=4 needs no occlusion.
		for (pos_t cell_size : {1, 2, 4, 8}) {
			const float width = cell_size * MAP_BLOCKSIZE;
			const v3bpos_t origin(-32, -16, 64);
			scene::SMesh mesh;
			addQuad(mesh, 1, 1, width / 2 - 0.5f, -0.5f, 2 * width - 0.5f, -0.5f,
					2 * width - 0.5f);
			auto &source = *mesh.getMeshBuffer(0);
			farmesh::FarMeshClipMask mask(origin, 1, cell_size);
			mask.addChunk(origin);
			auto clipped = mask.clip(source);
			UASSERT(std::abs(area(clipped) - 3 * width * width) < 0.01);
			// Clipping must preserve the cached far source for later movement.
			UASSERTEQ(u32, source.getIndexCount(), 6);
			mask.addChunk(origin + v3bpos_t(cell_size, 0, 0));
			UASSERT(std::abs(area(mask.clip(source)) - 2 * width * width) < 0.01);
			mask.addChunk(origin + v3bpos_t(0, 0, cell_size));
			mask.addChunk(origin + v3bpos_t(cell_size, 0, cell_size));
			UASSERT(mask.clip(source).empty());
		}
	}

	void testClipBoundaryFaces()
	{
		for (u8 axis = 0; axis < 3; ++axis)
			for (int sign : {-1, 1}) {
				scene::SMesh mesh;
				addQuad(mesh, axis, sign, 31.5f, -0.5f, 31.5f, -0.5f, 31.5f);
				v3bpos_t owner;
				if (sign < 0)
					owner[axis] = 2;
				farmesh::FarMeshClipMask mask({}, 1, 2);
				mask.addChunk(owner);
				UASSERT(mask.clip(*mesh.getMeshBuffer(0)).empty());
				owner[axis] = 2 - owner[axis];
				farmesh::FarMeshClipMask adjacent({}, 1, 2);
				adjacent.addChunk(owner);
				UASSERT(std::abs(area(adjacent.clip(*mesh.getMeshBuffer(0))) - 1024) <
						0.01);
			}
	}

	void testClipOversizedFarGeometry()
	{
		// Scaled far nodes can reach below their cell's origin. A near chunk
		// there must remove that geometry without affecting its neighbors.
		scene::SMesh mesh;
		addQuad(mesh, 1, 1, -16.5f, -0.5f, 63.5f, -0.5f, 63.5f);
		farmesh::FarMeshClipMask mask({}, 1, 2);
		mask.addChunk({0, 0, 0});
		UASSERT(std::abs(area(mask.clip(*mesh.getMeshBuffer(0))) - 4096) < 0.01);
		mask.addChunk({0, -2, 0});
		UASSERT(std::abs(area(mask.clip(*mesh.getMeshBuffer(0))) - 3072) < 0.01);
	}

	void testClipLights()
	{
		scene::SMeshBuffer source;
		source.setPrimitiveType(scene::EPT_POINTS);
		for (float x : {10.0f, 40.0f}) {
			video::S3DVertex vertex(
					v3f(x * BS, 10 * BS, 10 * BS), {}, video::SColor(0xffffffff), {});
			const u16 index = 0;
			source.append(&vertex, 1, &index, 1);
		}
		farmesh::FarMeshClipMask mask({}, 1, 2);
		mask.addChunk({});
		const auto clipped = mask.clip(source);
		UASSERTEQ(size_t, clipped.size(), 1);
		UASSERTEQ(u32, clipped[0]->getIndexCount(), 1);
		UASSERTEQ(float, clipped[0]->getPosition(0).X, 40 * BS);
	}

	void testClipVertexAttributes()
	{
		scene::SMesh mesh;
		addQuad(mesh, 1, 1, 15.5f, -0.5f, 63.5f, -0.5f, 63.5f);
		auto &source = *static_cast<scene::SMeshBuffer *>(mesh.getMeshBuffer(0));
		for (auto &vertex : source.Vertices->Data) {
			vertex.TCoords = v2f(vertex.Pos.X / (64 * BS), vertex.Pos.Z / (64 * BS));
			vertex.Aux = 37; // Texture-array layer must survive interpolation.
		}
		farmesh::FarMeshClipMask mask({}, 1, 2);
		mask.addChunk({});
		for (const auto &buffer : mask.clip(source))
			for (const auto &vertex : buffer->Vertices->Data) {
				UASSERTEQ(u16, vertex.Aux, 37);
				UASSERT(std::abs(vertex.TCoords.X - vertex.Pos.X / (64 * BS)) < 0.0001);
				UASSERT(std::abs(vertex.TCoords.Y - vertex.Pos.Z / (64 * BS)) < 0.0001);
				UASSERTEQ(float, vertex.Normal.Y, 1);
			}
	}

	void testClipTeleportFallback()
	{
		// A huge old cell contains one loaded chunk near the new camera.
		// The mask stays sparse instead of iterating over (2^step)^3 chunks.
		const v3bpos_t origin(-8192, 0, -8192);
		const v3bpos_t near = origin + v3bpos_t(2048, 0, 4096);
		farmesh::FarMeshClipMask mask(origin, 16, 2);
		mask.addChunk(near);
		scene::SMesh mesh;
		const float x = 2048 * MAP_BLOCKSIZE - 0.5f;
		const float z = 4096 * MAP_BLOCKSIZE - 0.5f;
		addQuad(mesh, 1, 1, 15.5f, z, z + 64, x, x + 64);
		UASSERT(std::abs(area(mask.clip(*mesh.getMeshBuffer(0))) - 3072) < 0.01);
		farmesh::FarMeshClipMask moved(origin, 16, 2);
		moved.addChunk(near + v3bpos_t(8, 0, 0));
		UASSERT(std::abs(area(moved.clip(*mesh.getMeshBuffer(0))) - 4096) < 0.01);
	}

	void testClipBufferLimit()
	{
		scene::SMesh mesh;
		for (unsigned i = 0; i < 12000; ++i)
			addQuad(mesh, 1, 1, 15.5f, -0.5f, 63.5f, -0.5f, 63.5f);
		farmesh::FarMeshClipMask mask({}, 1, 2);
		mask.addChunk({});
		const auto clipped = mask.clip(*mesh.getMeshBuffer(0));
		UASSERT(clipped.size() > 1);
		for (const auto &buffer : clipped) {
			UASSERT(buffer->getVertexCount() <= U16_MAX);
			for (const auto index : buffer->Indices->Data)
				UASSERT(index < buffer->getVertexCount());
		}
		UASSERT(std::abs(area(clipped) - 12000.0 * 3072) < 0.01);
	}

	void testSparseTerrainHandoff()
	{
		// Sparse server coverage: four surface meshes, two unfinished
		// underground chunks, and two entirely omitted underground chunks.
		const v3bpos_t origin(32, 0, 72);
		farmesh::NearMeshHandoff handoff(origin, 1, 2);
		scene::SMesh mesh;
		addQuad(mesh, 1, 1, 43.5f, -0.5f, 63.5f, -0.5f, 63.5f);
		for (pos_t z : {0, 2})
			for (pos_t x : {0, 2})
				handoff.addChunk(origin + v3bpos_t(x, 2, z), true);
		for (pos_t z : {0, 2})
			handoff.addChunk(origin + v3bpos_t(2, 0, z), false);
		// Unfinished underground chunks do not replace any of this far surface.
		UASSERT(handoff.covers(mesh, unknown));
		for (pos_t z : {0, 2})
			handoff.addChunk(origin + v3bpos_t(2, 0, z), true);
		size_t omitted_checks = 0;
		UASSERT(handoff.covers(mesh, [&](const v3bpos_t &) {
			++omitted_checks;
			return false;
		}));
		UASSERTEQ(size_t, omitted_checks, 0);
	}

	void testUnfinishedChunkAboveSurface()
	{
		// The stalled cell observed in GDB: four surface chunks are complete,
		// but one chunk above them has only member blocks and no origin mesh.
		const v3bpos_t origin(-152, 0, -444);
		farmesh::NearMeshHandoff handoff(origin, 1, 2);
		scene::SMesh mesh;
		addQuad(mesh, 1, 1, 15.5f, -0.5f, 63.5f, -0.5f, 63.5f);
		for (pos_t z : {0, 2})
			for (pos_t x : {0, 2})
				handoff.addChunk(origin + v3bpos_t(x, 0, z), true);
		handoff.addChunk(origin + v3bpos_t(0, 2, 0), false);
		handoff.addChunk(origin + v3bpos_t(2, 2, 0), true);
		UASSERT(handoff.covers(mesh, unknown));

		// If that unfinished chunk actually has far geometry to replace, it
		// must still block the handoff, even if an occlusion callback says yes.
		addQuad(mesh, 1, 1, 47.5f, -0.5f, 31.5f, -0.5f, 31.5f);
		UASSERT(!handoff.covers(mesh, [](const v3bpos_t &) { return true; }));
		handoff.addChunk(origin + v3bpos_t(0, 2, 0), true);
		UASSERT(handoff.covers(mesh, unknown));
	}

	void testMissingVisibleSurface()
	{
		farmesh::NearMeshHandoff handoff({}, 1, 2);
		scene::SMesh mesh;
		addQuad(mesh, 1, 1, 43.5f, -0.5f, 63.5f, -0.5f, 63.5f);
		handoff.addChunk({0, 2, 0}, true);
		handoff.addChunk({2, 2, 0}, true);
		handoff.addChunk({0, 2, 2}, true);
		// An unloaded surface is not evidence of air: retain the complete far
		// cell until its last visible chunk is covered, avoiding a new hole.
		UASSERT(!handoff.covers(mesh, unknown));
		handoff.addChunk({2, 2, 2}, true);
		UASSERT(handoff.covers(mesh, unknown));
	}

	void testChunkBoundaryFaces()
	{
		for (u8 axis = 0; axis < 3; ++axis)
			for (int sign : {-1, 1}) {
				const v3bpos_t origin(-4, 0, 4);
				farmesh::NearMeshHandoff handoff(origin, 1, 2);
				auto owner = origin;
				if (sign < 0)
					owner[axis] += 2;
				handoff.addChunk(owner, true);
				scene::SMesh mesh;
				addQuad(mesh, axis, sign, 31.5f, -0.5f, 31.5f, -0.5f, 31.5f);
				// A face lying on a boundary belongs to its solid side, and merely
				// touching another chunk along its edges does not require that chunk.
				UASSERT(handoff.covers(mesh, unknown));
			}
	}

	void testDisconnectedGeometry()
	{
		farmesh::NearMeshHandoff handoff({}, 2, 2);
		handoff.addChunk({0, 0, 0}, true);
		handoff.addChunk({6, 0, 0}, true);
		scene::SMesh mesh;
		// Two islands in the same material buffer must not require omitted
		// chunks in the empty gap of that buffer's combined bounding box.
		addQuad(mesh, 1, 1, 15.5f, -0.5f, 31.5f, -0.5f, 31.5f);
		addQuad(mesh, 1, 1, 15.5f, -0.5f, 31.5f, 95.5f, 127.5f);
		UASSERT(handoff.covers(mesh, unknown));
	}

	void testOccludedOmissionIsTemporary()
	{
		scene::SMesh mesh;
		addQuad(mesh, 1, 1, 15.5f, -0.5f, 31.5f, -0.5f, 63.5f);
		farmesh::NearMeshHandoff handoff({}, 1, 2);
		handoff.addChunk({}, true);
		size_t calls = 0;
		const auto occluded = [&](const v3bpos_t &pos) {
			++calls;
			return pos == v3bpos_t(2, 0, 0);
		};
		UASSERT(handoff.covers(mesh, occluded));
		UASSERT(handoff.covers(mesh, occluded));
		UASSERTEQ(size_t, calls, 1);
		// A camera move can expose the omitted cell. Do not reuse the previous
		// frame's occlusion proof, even though the far geometry is unchanged.
		farmesh::NearMeshHandoff moved({}, 1, 2);
		moved.addChunk({}, true);
		UASSERT(!moved.covers(mesh, unknown));
	}

	void testFarLightsNeedCoverage()
	{
		const v3bpos_t origin(-4, -4, -4);
		farmesh::NearMeshHandoff handoff(origin, 1, 2);
		handoff.addChunk(origin, true);
		scene::SMesh mesh;
		auto *buffer = new scene::SMeshBuffer();
		buffer->PrimitiveType = scene::EPT_POINTS;
		const video::S3DVertex vertex(
				v3f(40 * BS, 10 * BS, 10 * BS), v3f{}, video::SColor(0xffffffff), v2f{});
		const u16 index = 0;
		buffer->append(&vertex, 1, &index, 1);
		mesh.addMeshBuffer(buffer);
		buffer->drop();
		UASSERT(!handoff.covers(mesh, unknown));
		handoff.addChunk(origin + v3bpos_t(2, 0, 0), true);
		UASSERT(handoff.covers(mesh, unknown));
	}

	void testEmptyCompletedChunks()
	{
		scene::SMesh empty;
		farmesh::NearMeshHandoff handoff({}, 1, 2);
		UASSERT(!handoff.covers(empty, unknown));
		handoff.addChunk({}, false);
		UASSERT(!handoff.covers(empty, unknown));
		handoff.addChunk({}, true);
		UASSERT(handoff.covers(empty, unknown));
		// Nothing is removed from this empty far mesh. Another unfinished chunk
		// must not prevent an already completed near chunk from being drawn.
		handoff.addChunk({2, 0, 0}, false);
		UASSERT(handoff.covers(empty, unknown));
		handoff.addChunk({2, 0, 0}, true);
		UASSERT(handoff.covers(empty, unknown));
	}

	void testIdleRequestCanRetry(IGameDef *gamedef)
	{
		const v3bpos_t origin(-152, 2, -444);
		MapBlock member({-151, 2, -443}, gamedef);
		member.updateMeshRevision(3707);
		UASSERT(member.tryMarkMeshRequested(0, member.getMeshRevision()));
		UASSERT(!member.tryMarkMeshRequested(0, member.getMeshRevision()));

		// Workers are intentionally not started. A surviving member's old
		// marker does not represent queued work after the origin is lost.
		MeshUpdateManager manager(nullptr);
		UASSERT(!manager.hasPending(origin));
		MeshUpdateResult result;
		result.p = origin;
		manager.putResult(std::move(result));
		UASSERT(manager.hasPending(origin));
		UASSERT(manager.getNextResult(result));
		// Whether the caller installs or rejects this result, an absent mesh
		// can be requested again without requiring a block revision change.
		UASSERT(!manager.hasPending(origin));
	}

	void testMeshPriorityAfterMove()
	{
		QueuedMeshUpdate a, b, c;
		a.p = {-20, 0, 0};
		b.p = {0, 0, 0};
		c.p = {20, 0, 0};
		a.urgent = true;
		std::vector<QueuedMeshUpdate *> queue{&a, &b, &c};
		const auto position = [](const auto *job) { return job->p; };
		sortMeshUpdatesNearFirst(queue, {}, position);
		UASSERT(queue[0] == &b);
		UASSERT(queue[1] == &a); // Equal distances preserve arrival order.
		sortMeshUpdatesNearFirst(queue, {21, 0, 0}, position);
		UASSERT(queue[0] == &c);
		UASSERT(queue[1] == &b);
		UASSERT(queue[2] == &a);
		UASSERT(a.urgent); // Explicit urgent edits retain their priority flag.
		sortMeshUpdatesNearFirst(queue, {-21, 0, 0}, position);
		UASSERT(queue[0] == &a);
		UASSERT(queue[1] == &b);
		UASSERT(queue[2] == &c);
	}

	void testPendingResults()
	{
		MeshUpdateManager manager(nullptr);
		const v3bpos_t origin(-4, 2, 8);
		for (bool urgent : {false, true}) {
			MeshUpdateResult result;
			result.p = origin;
			result.urgent = urgent;
			manager.putResult(std::move(result));
		}
		UASSERT(manager.hasPending(origin));
		UASSERT(!manager.hasPending(origin + v3bpos_t(2, 0, 0)));
		MeshUpdateResult result;
		UASSERT(manager.getNextResult(result));
		UASSERT(result.urgent);
		UASSERT(manager.hasPending(origin));
		UASSERT(manager.getNextResult(result));
		UASSERT(!result.urgent);
		UASSERT(!manager.hasPending(origin));

		// Clearing an unacknowledged result must also release the request.
		manager.putResult(std::move(result));
		UASSERT(manager.hasPending(origin));
		manager.clearAllQueues(false);
		UASSERT(!manager.hasPending(origin));
	}
};

TestFmNearMeshHandoff g_test_near_mesh_handoff;
} // namespace
