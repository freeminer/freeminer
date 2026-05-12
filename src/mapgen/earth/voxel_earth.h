#pragma once


// ---------- LoaderResult ----------
#include <vector>
struct LoaderResult {
    int triCount;
    std::vector<float> vx0, vy0, vz0;
    std::vector<float> vx1, vy1, vz1;
    std::vector<float> vx2, vy2, vz2;
    std::vector<float> tu0, tv0, tu1, tv1, tu2, tv2;
    std::vector<char> hasUV; // bool per tri-vertex (we will later convert to per-triangle)
    int texW=0, texH=0, texStride=0;
    std::vector<int> texPixels; // ARGB-packed rgb int (0xRRGGBB)
};

//Stats voxelizeSingleGLB(const string& glbPath, const string& outDir, int gridSize, bool tiles3d, bool verbose);
