// Live2D 1.0.00alpha5 runtime, written from docs/spec_characters.md sections 5 and 6.
// Parses a moc (model data) and evaluates it: warp and rotation deformers, meshes, parts.
#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace l2d {

struct ParamPivots {
    std::string id; int param = -1, count = 0;
    std::vector<float> pivots;
    int index = 0; float t = 0;          // result of the last key selection
};
using PivotMgr = std::vector<ParamPivots>;

struct Affine { float ox = 0, oy = 0, sx = 1, sy = 1, rot = 0; };

struct Deformer {
    int type = 0;                        // 1 rotation, 2 warp
    std::string id, target;
    bool base = true;                    // target is null or DST_BASE
    int parent = -1;                     // index into sorted deformers, -1 = missing
    PivotMgr piv;
    int rows = 0, cols = 0;              // warp
    std::vector<std::vector<float>> keys;
    std::vector<Affine> akeys;           // rotation
    std::vector<float> grid, world;      // warp evaluated points
    Affine a, wa;                        // rotation local and world
    bool ready = false, outside = false;
};

struct Mesh {
    std::string id, target;
    bool base = true;
    int parent = -1, part = 0;
    PivotMgr piv;
    std::vector<int> orderKeys;
    std::vector<float> opacityKeys;
    int texNo = 0, pointCount = 0, triCount = 0, blend = 0;
    std::vector<int> indices;
    std::vector<std::vector<float>> keys;
    std::vector<float> uv;
    int order = 0; float opacity = 1;
    std::vector<float> local, final_;    // x,y pairs
    bool available = true, outside = false;
};

struct Part { std::string id; float opacity = 0; };

class Model {
public:
    // false if the data does not parse
    bool parse(const uint8_t* d, size_t n);

    int canvasW = 0, canvasH = 0;
    std::vector<Deformer> deformers;     // topologically sorted
    std::vector<Mesh> meshes;
    std::vector<Part> parts;

    int paramIndex(const std::string& name);   // appends unknown params, default 0
    float value(int i) const { return i >= 0 && i < (int)val.size() ? val[i] : 0; }
    float value(const std::string& name) { return value(paramIndex(name)); }
    void setParam(int i, float v);
    void setParam(const std::string& n, float v) { setParam(paramIndex(n), v); }
    void addWeighted(const std::string& n, float v, float w);
    void loadParam();
    void saveParam();
    float paramMin(int i) const { return mn[i]; }
    float paramMax(int i) const { return mx[i]; }

    float partOpacity(const std::string& name) const;   // 0 for unknown parts
    void setPartOpacity(const std::string& name, float v);

    void update();                       // deformers, then meshes
    const std::vector<int>& drawOrder(); // stable by (order, index); includes hidden meshes
private:
    std::vector<float> val, saved, mn, mx;
    std::unordered_map<std::string, int> ids;
    std::unordered_map<std::string, int> partIdx;
    std::vector<int> order_;
    int addParam(const std::string& id, float lo, float hi, float def);
    void bind(PivotMgr& p);
};

}  // namespace l2d
