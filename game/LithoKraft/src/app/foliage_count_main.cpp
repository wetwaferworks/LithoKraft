// foliage_count: headless foliage-meshing counter (Crew Canopy, F-B, 2026-10-18).
// Builds the playtest test world (buildTestWorld plants an 8-tree crystal-leaf
// grove: solid leaf-cube canopies), meshes every sub-chunk in a box around the
// grove, and prints total / leaf quads, verts and upload bytes for:
//   legacy        render.foliage_density = 0 (today's meshing)
//   sparse, near  density = 1, focus = the grove's own column (all near)
//   sparse, far   density = 1, focus = grove column + <arg> chunks in +x
// A measuring tool, not a test: always exits 0. Usage: foliage_count [farOffsetChunks]
#include <cstdio>
#include <cstdlib>
#include <memory>

#include "lithokraft/content/appearance.h"
#include "lithokraft/content/blocks.h"
#include "lithokraft/content/test_world.h"
#include "litholattice/render/block_materials.h"
#include "litholattice/render/mesher.h"
#include "litholattice/render/renderer.h"
#include "litholattice/world/chunk_map.h"
#include "litholattice/world/mesh_input.h"

namespace {
struct Acc {
  uint64_t quads = 0, leafQuads = 0, verts = 0;
};

Acc meshBox(const litholattice::ChunkMap& map, const litholattice::cBlockReg& reg,
            const litholattice::MeshContext& ctx, int cx0, int cz0, int cx1, int cz1) {
  Acc a;
  for (int cx = cx0; cx <= cx1; ++cx)
    for (int cz = cz0; cz <= cz1; ++cz)
      for (int sy = 0; sy < litholattice::kSubChunks; ++sy) {
        auto in = litholattice::snapshotForMeshing(map, reg, cx, sy, cz, 0, 0);
        if (!in) continue;
        const litholattice::ChunkMesh m = litholattice::meshSubChunk(*in, ctx);
        a.quads += m.verts.size() / 4;
        a.verts += m.verts.size();
        for (size_t q = 0; q + 3 < m.verts.size(); q += 4)
          if ((m.verts[q].mode & litholattice::kShadeModeMask) == litholattice::kShadeLeaf) ++a.leafQuads;
      }
  return a;
}

void printRow(const char* name, const Acc& a, const Acc* base) {
  const uint64_t bytes = a.verts * sizeof(litholattice::PackedVertex);
  std::printf("%-14s quads=%-8llu leafQuads=%-8llu verts=%-9llu bytes=%-10llu", name,
              (unsigned long long)a.quads, (unsigned long long)a.leafQuads,
              (unsigned long long)a.verts, (unsigned long long)bytes);
  if (base && base->verts)
    std::printf("  vs legacy: verts %+0.1f%%, leafQuads %+0.1f%%",
                100.0 * (double(a.verts) / double(base->verts) - 1.0),
                100.0 * (double(a.leafQuads) / double(base->leafQuads) - 1.0));
  std::printf("\n");
}
}  // namespace

int main(int argc, char** argv) {
  using namespace litholattice;
  const int farOff = argc > 1 ? std::atoi(argv[1]) : 8;
  const cBlockReg& reg = lithokraft::blockRegistry();
  const BlockMaterials mats(reg.size());

  ChunkMap map;
  map.setRegistry(&reg);
  for (int cx = -8; cx <= 8; ++cx)
    for (int cz = -8; cz <= 8; ++cz) {
      auto col = std::make_unique<ChunkColumn>();
      col->cx = cx;
      col->cz = cz;
      map.insert(std::move(col));
    }
  const lithokraft::TestWorldLayout tw = lithokraft::buildTestWorld(map, 0, 64, 0, 32);
  const int gcx = tw.leafDemoX >> 4, gcz = tw.leafDemoZ >> 4;
  std::printf("foliage_count: grove at blocks (%d, %d) -> column (%d, %d); sizeof(PackedVertex)=%zu\n",
              tw.leafDemoX, tw.leafDemoZ, gcx, gcz, sizeof(PackedVertex));

  MeshContext ctx{reg, mats, &lithokraft::blockAppearance()};
  setLeavesFancy(true);  // the sparse model applies to the fancy leaf path

  setFoliageDensity(0);
  const Acc legacy = meshBox(map, reg, ctx, gcx - 2, gcz - 2, gcx + 2, gcz + 2);
  printRow("legacy", legacy, nullptr);

  setFoliageDensity(1);
  setFoliageFocus(gcx, gcz);
  const Acc near = meshBox(map, reg, ctx, gcx - 2, gcz - 2, gcx + 2, gcz + 2);
  printRow("sparse, near", near, &legacy);
  std::printf("near-ring control: %s (sparse with the grove inside the near ring must equal legacy)\n",
              (near.quads == legacy.quads && near.verts == legacy.verts) ? "IDENTICAL" : "DIFFERS");

  setFoliageFocus(gcx + farOff, gcz);
  const Acc far = meshBox(map, reg, ctx, gcx - 2, gcz - 2, gcx + 2, gcz + 2);
  printRow("sparse, far", far, &legacy);
  return 0;
}
