export module 点簇分割模块;

import 相机接口模块;
import 基础数据类型模块;

import <vector>;
import <cstdint>;
import <cmath>;
import <algorithm>;
import <limits>;
import <array>;



export class 点簇分割类 {
private:
    // ===== 内部通用工具 =====
    static inline int 索引(int u, int v, int w) {
        return v * w + u;
    }

    static inline bool 深度有效(float z, const 点簇分割参数& p) {
        return (z >= p.最小深度 && z <= p.最大深度);
    }

    static inline bool 点有效(const Vector3D& P) {
        return (P.z > 0.0f);
    }

    static inline bool 邻接连通(
        const 结构体_原始场景帧& 帧,
        int idxA,
        int idxB,
        const 点簇分割参数& p,
        bool 点云可用)
    {
        const float zA = 帧.深度[idxA];
        const float zB = 帧.深度[idxB];

        if (!深度有效(zA, p) || !深度有效(zB, p))
            return false;

        if (点云可用) {
            const auto& A = 帧.点云[idxA];
            const auto& B = 帧.点云[idxB];

            if (p.忽略无效点 && (!点有效(A) || !点有效(B)))
                return false;

            const double dx = A.x - B.x;
            const double dy = A.y - B.y;
            const double dz = A.z - B.z;
            const double d2 = dx * dx + dy * dy + dz * dz;

            const double thr2 = static_cast<double>(p.邻域最大三维距离) * p.邻域最大三维距离;
            return d2 <= thr2;
        }

        // 点云不可用：退化为深度差
        return std::fabs(zA - zB) <= p.邻域最大深度差;
    }

    // ===== 计算簇的 3D 中心与包围盒尺寸 =====
    static void 计算簇3D信息(
        const 结构体_原始场景帧& 帧,
        const 点簇& 簇,
        Vector3D& out中心,
        Vector3D& out尺寸)
    {
        if (簇.empty()) return;

        double minx = std::numeric_limits<double>::infinity();
        double maxx = -std::numeric_limits<double>::infinity();
        double miny = std::numeric_limits<double>::infinity();
        double maxy = -std::numeric_limits<double>::infinity();
        double minz = std::numeric_limits<double>::infinity();
        double maxz = -std::numeric_limits<double>::infinity();

        double sumx = 0.0, sumy = 0.0, sumz = 0.0;
        int validCnt = 0;

        const int w = 帧.宽度;

        for (const auto& pix : 簇) {
            const int idx = 索引(pix.u, pix.v, w);
            if ((unsigned)idx >= (unsigned)帧.点云.size()) continue;

            const Vector3D& p = 帧.点云[idx];
            if (p.z <= 0.0f) continue;

            sumx += p.x; sumy += p.y; sumz += p.z;

            minx = std::min(minx, (double)p.x); maxx = std::max(maxx, (double)p.x);
            miny = std::min(miny, (double)p.y); maxy = std::max(maxy, (double)p.y);
            minz = std::min(minz, (double)p.z); maxz = std::max(maxz, (double)p.z);

            ++validCnt;
        }

        if (validCnt == 0) return;

        out中心 = {
            static_cast<float>(sumx / validCnt),
            static_cast<float>(sumy / validCnt),
            static_cast<float>(sumz / validCnt)
        };

        out尺寸 = {
            static_cast<float>(maxx - minx),
            static_cast<float>(maxy - miny),
            static_cast<float>(maxz - minz)
        };

        // 防止尺寸为0导致后续除零
        constexpr float MIN_SIZE = 0.01f;
        if (out尺寸.x < MIN_SIZE) out尺寸.x = MIN_SIZE;
        if (out尺寸.y < MIN_SIZE) out尺寸.y = MIN_SIZE;
        if (out尺寸.z < MIN_SIZE) out尺寸.z = MIN_SIZE;
    }

    // ===== 8x8 填洞：从边界 flood-fill 背景0，未触达的0视为洞 => 填成1 =====
    static void 填洞_8x8(std::array<std::array<std::uint8_t, 8>, 8>& g)
    {
        constexpr int N = 8;
        std::array<std::array<std::uint8_t, N>, N> vis{};
        std::vector<std::pair<int, int>> st;
        st.reserve(64);

        auto push = [&](int x, int y) {
            if ((unsigned)x >= N || (unsigned)y >= N) return;
            if (g[y][x] != 0) return;
            if (vis[y][x]) return;
            vis[y][x] = 1;
            st.push_back({ x,y });
            };

        // 从四边的背景0开始
        for (int x = 0; x < N; x++) { push(x, 0); push(x, N - 1); }
        for (int y = 0; y < N; y++) { push(0, y); push(N - 1, y); }

        const int off4[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };
        while (!st.empty()) {
            auto [x, y] = st.back();
            st.pop_back();
            for (auto& d : off4) {
                push(x + d[0], y + d[1]);
            }
        }

        // 0 且未被背景 flood 到的 => 洞/内部，填成 1
        for (int y = 0; y < N; y++) {
            for (int x = 0; x < N; x++) {
                if (g[y][x] == 0 && vis[y][x] == 0)
                    g[y][x] = 1;
            }
        }
    }

    // ===== 计算单个点簇的 8×8 封闭轮廓二值图（内部已填充）=====
    static std::vector<std::int64_t> 计算封闭轮廓二值图(
        const 结构体_原始场景帧& 帧,
        const 点簇& 簇,
        const Vector3D& 中心3D,
        const Vector3D& 尺寸3D)
    {
        constexpr int GRID = 8;
        constexpr int TOTAL = GRID * GRID;

        std::vector<std::int64_t> 编码(TOTAL, 0);
        if (簇.size() < 20) return 编码;

        const double eps = 1e-6;
        const double min_x = 中心3D.x - 尺寸3D.x * 0.5 - eps;
        const double max_x = 中心3D.x + 尺寸3D.x * 0.5 + eps;
        const double min_y = 中心3D.y - 尺寸3D.y * 0.5 - eps;
        const double max_y = 中心3D.y + 尺寸3D.y * 0.5 + eps;

        const double range_x = std::max(max_x - min_x, 1e-9);
        const double range_y = std::max(max_y - min_y, 1e-9);

        std::array<std::array<std::uint8_t, GRID>, GRID> grid{};
        const int w = 帧.宽度;

        // (1) 投影点到 XY 平面，落到 8x8
        for (const auto& pix : 簇) {
            const int idx = 索引(pix.u, pix.v, w);
            if ((unsigned)idx >= (unsigned)帧.点云.size()) continue;

            const Vector3D& pt = 帧.点云[idx];
            if (pt.z <= 0.0f) continue;

            // 过滤深度离群点，减少干扰
            if (std::fabs(pt.z - 中心3D.z) > 尺寸3D.z * 0.7f) continue;

            const double nx = (pt.x - min_x) / range_x;
            const double ny = (pt.y - min_y) / range_y;

            int ix = (int)(nx * GRID);
            int iy = (int)(ny * GRID);

            ix = std::clamp(ix, 0, GRID - 1);
            iy = std::clamp(iy, 0, GRID - 1);

            grid[iy][ix] = 1;
        }

        // (2) 膨胀一次（闭操作的一部分）
        auto tmp = grid;
        for (int y = 0; y < GRID; y++) {
            for (int x = 0; x < GRID; x++) {
                if (!grid[y][x]) continue;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y + dy, nx = x + dx;
                        if ((unsigned)nx < GRID && (unsigned)ny < GRID)
                            tmp[ny][nx] = 1;
                    }
                }
            }
        }
        grid = tmp;

        // (3) 填洞（封闭内部）
        填洞_8x8(grid);

        // (4) 展平成 64 维
        int pos = 0;
        for (int y = 0; y < GRID; y++) {
            for (int x = 0; x < GRID; x++) {
                编码[pos++] = grid[y][x] ? 1 : 0;
            }
        }

        return 编码;
    }

    // ===== bbox 内掩码膨胀（8邻域）=====
    static void 膨胀8(std::vector<std::uint8_t>& m, int w, int h)
    {
        std::vector<std::uint8_t> src = m;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                if (!src[y * w + x]) continue;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if ((unsigned)nx < (unsigned)w && (unsigned)ny < (unsigned)h)
                            m[ny * w + nx] = 1;
                    }
                }
            }
        }
    }

    // ===== bbox 内填洞：从边界背景 flood-fill，未触达的0填成1 =====
    static void 填洞_封闭(std::vector<std::uint8_t>& m, int w, int h)
    {
        std::vector<std::uint8_t> vis((size_t)w * (size_t)h, 0);
        std::vector<int> st;
        st.reserve((size_t)w * (size_t)h / 8);

        auto push = [&](int x, int y) {
            if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
            int id = y * w + x;
            if (m[id] != 0) return;
            if (vis[id]) return;
            vis[id] = 1;
            st.push_back(id);
            };

        // 从四边背景0开始
        for (int x = 0; x < w; x++) { push(x, 0); push(x, h - 1); }
        for (int y = 0; y < h; y++) { push(0, y); push(w - 1, y); }

        const int off4[4][2] = { {-1,0},{1,0},{0,-1},{0,1} };
        while (!st.empty()) {
            int id = st.back(); st.pop_back();
            int x = id % w;
            int y = id / w;
            for (auto& d : off4) {
                push(x + d[0], y + d[1]);
            }
        }

        // 洞填成1
        for (int i = 0; i < w * h; i++) {
            if (m[i] == 0 && vis[i] == 0) m[i] = 1;
        }
    }

    // ===== 生成裁剪颜色 + 裁剪掩码（bbox 内）=====
    static void 生成裁剪输出(
        const 结构体_原始场景帧& 帧,
        const 点簇& 簇,
        const 点簇边界框& box,
        const 点簇分割参数& 参数,
        点簇增强结果& res)
    {
        if (!参数.输出裁剪图 && !参数.输出裁剪掩码) return;

        const int W = 帧.宽度;
        const int H = 帧.高度;

        int u0 = std::max(0, box.umin - 参数.裁剪边距);
        int v0 = std::max(0, box.vmin - 参数.裁剪边距);
        int u1 = std::min(W - 1, box.umax + 参数.裁剪边距);
        int v1 = std::min(H - 1, box.vmax + 参数.裁剪边距);

        const int cw = std::max(0, u1 - u0 + 1);
        const int ch = std::max(0, v1 - v0 + 1);
        if (cw <= 0 || ch <= 0) return;

        if (cw * ch > 参数.最大裁剪像素) return;

        res.裁剪宽 = cw;
        res.裁剪高 = ch;

        const bool color_ok = ((int)帧.颜色.size() == W * H);

        // (1) 裁剪颜色
        if (参数.输出裁剪图) {
            res.裁剪颜色.assign((size_t)cw * (size_t)ch, Color{ 0,0,0 });
            if (color_ok) {
                for (int y = 0; y < ch; y++) {
                    for (int x = 0; x < cw; x++) {
                        int su = u0 + x;
                        int sv = v0 + y;
                        res.裁剪颜色[(size_t)y * (size_t)cw + (size_t)x]
                            = 帧.颜色[(size_t)sv * (size_t)W + (size_t)su];
                    }
                }
            }
        }
        else {
            res.裁剪颜色.clear();
        }

        // (2) 裁剪掩码：簇点投影到 bbox 内
        if (参数.输出裁剪掩码) {
            res.裁剪掩码.assign((size_t)cw * (size_t)ch, 0);

            for (const auto& p : 簇) {
                if ((unsigned)p.u >= (unsigned)W || (unsigned)p.v >= (unsigned)H) continue;
                if (p.u < u0 || p.u > u1 || p.v < v0 || p.v > v1) continue;

                int x = p.u - u0;
                int y = p.v - v0;
                res.裁剪掩码[(size_t)y * (size_t)cw + (size_t)x] = 1;
            }

            if (参数.掩码膨胀一次) 膨胀8(res.裁剪掩码, cw, ch);
            if (参数.掩码填洞)     填洞_封闭(res.裁剪掩码, cw, ch);
        }
        else {
            res.裁剪掩码.clear();
        }
    }

public:
    // ===== 基础分割：只返回点簇（u,v 列表）=====
    std::vector<点簇> 分割点簇(const 结构体_原始场景帧& 帧, const 点簇分割参数& 参数)
    {
        std::vector<点簇> 输出;

        const int w = 帧.宽度;
        const int h = 帧.高度;

        if (w <= 0 || h <= 0) return 输出;
        if ((int)帧.深度.size() != w * h) return 输出;

        const bool 点云可用 = ((int)帧.点云.size() == w * h);
        const int step = std::max(1, 参数.采样步长);

        std::vector<std::uint8_t> visited((size_t)w * (size_t)h, 0);
        std::vector<int> stack;
        stack.reserve(4096);

        // 前4个=4邻域，后4个=对角 => 可正确切换 4/8
        constexpr std::array<std::array<int, 2>, 8> offsets_all =
        { {{-1,0}, {1,0}, {0,-1}, {0,1}, {-1,-1}, {-1,1}, {1,-1}, {1,1}} };

        const size_t offset_count = 参数.使用8邻域 ? 8 : 4;

        for (int v = 0; v < h; v += step) {
            for (int u = 0; u < w; u += step) {
                const int start = 索引(u, v, w);
                if (visited[start]) continue;

                if (!深度有效(帧.深度[start], 参数)) {
                    visited[start] = 1;
                    continue;
                }

                if (参数.忽略无效点 && 点云可用) {
                    const auto& P = 帧.点云[start];
                    if (!点有效(P)) {
                        visited[start] = 1;
                        continue;
                    }
                }

                点簇 当前簇;
                当前簇.reserve(256);

                stack.clear();
                stack.push_back(start);
                visited[start] = 1;

                while (!stack.empty()) {
                    const int curIdx = stack.back();
                    stack.pop_back();

                    const int cu = curIdx % w;
                    const int cv = curIdx / w;
                    当前簇.push_back({ cu, cv });

                    for (size_t i = 0; i < offset_count; ++i) {
                        const auto& d = offsets_all[i];
                        const int nu = cu + d[0];
                        const int nv = cv + d[1];

                        if ((unsigned)nu >= (unsigned)w || (unsigned)nv >= (unsigned)h) continue;
                        if ((nu % step) != 0 || (nv % step) != 0) continue;

                        const int nidx = 索引(nu, nv, w);
                        if (visited[nidx]) continue;

                        if (邻接连通(帧, curIdx, nidx, 参数, 点云可用)) {
                            visited[nidx] = 1;
                            stack.push_back(nidx);
                        }
                    }
                }

                if ((int)当前簇.size() >= 参数.最小点数) {
                    输出.push_back(std::move(当前簇));
                }
            }
        }

        return 输出;
    }

    // ===== 返回点簇 + 边界框 =====
    std::vector<点簇结果> 分割点簇_带边界框(const 结构体_原始场景帧& 帧, const 点簇分割参数& 参数)
    {
        auto 簇列表 = 分割点簇(帧, 参数);

        std::vector<点簇结果> out;
        out.reserve(簇列表.size());

        for (auto& c : 簇列表) {
            点簇边界框 box;
            if (!c.empty()) {
                box.umin = box.umax = c[0].u;
                box.vmin = box.vmax = c[0].v;
                for (auto& p : c) {
                    box.umin = std::min(box.umin, p.u);
                    box.umax = std::max(box.umax, p.u);
                    box.vmin = std::min(box.vmin, p.v);
                    box.vmax = std::max(box.vmax, p.v);
                }
            }
            out.push_back({ std::move(c), box });
        }
        return out;
    }

    // ===== 推荐主接口：返回完整增强信息（含封闭轮廓二值图 + 裁剪原图 + 裁剪掩码）=====
    std::vector<点簇增强结果> 分割点簇_增强(const 结构体_原始场景帧& 帧, const 点簇分割参数& 参数)
    {
        auto 原始簇列表 = 分割点簇(帧, 参数);

        std::vector<点簇增强结果> 结果;
        结果.reserve(原始簇列表.size());

        for (auto& c : 原始簇列表) {
            if ((int)c.size() < 参数.最小点数) continue;

            点簇增强结果 res;
            res.簇 = std::move(c);

            // 像素边界框
            if (!res.簇.empty()) {
                res.边界.umin = res.边界.umax = res.簇[0].u;
                res.边界.vmin = res.边界.vmax = res.簇[0].v;
                for (const auto& p : res.簇) {
                    res.边界.umin = std::min(res.边界.umin, p.u);
                    res.边界.umax = std::max(res.边界.umax, p.u);
                    res.边界.vmin = std::min(res.边界.vmin, p.v);
                    res.边界.vmax = std::max(res.边界.vmax, p.v);
                }
            }

            // 3D 中心与尺寸
            计算簇3D信息(帧, res.簇, res.中心, res.尺寸);

            // 8×8 封闭轮廓二值图（内部填充）
            res.轮廓编码 = 计算封闭轮廓二值图(帧, res.簇, res.中心, res.尺寸);

            // 新增：根据轮廓/掩码裁剪原图并返回（bbox 内）
            生成裁剪输出(帧, res.簇, res.边界, 参数, res);

            结果.push_back(std::move(res));
        }

        return 结果;
    }
};
