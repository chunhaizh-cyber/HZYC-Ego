export module 存在提取模块;

import 相机接口模块;
import 点簇分割模块;
import 基础数据类型模块;

import <vector>;
import <cstdint>;
import <cmath>;
import <algorithm>;
import <limits>;



// ==============================
// 提取参数（补全：原文件引用但未定义）
// ==============================
export struct 观测提取参数 {
    int  最小有效点数 = 80;

    // 颜色：若帧里没有颜色数据，且要求颜色，则观测无效
    bool 要求颜色 = true;
    int  颜色采样步长 = 2;  // 1=全采样；2/3=跳采样加速

    // 轮廓：要求 enhanced.轮廓编码 必须是 64 维
    bool 要求轮廓编码 = true;
    bool 严格轮廓维度64 = true;

    // 有效性约束
    float 最小中心Z = 0.05f;     // 视野前方
    float 最小尺寸 = 0.01f;       // 米：防止 0 尺寸
};

// ==============================
// 存在提取类（清理无效代码后，保留“增强点簇 -> 观测”的主流程）
// ==============================
export class 存在提取类 {
private:
    static inline int idx(int u, int v, int w) { return v * w + u; }

    static inline bool is_finite3(const Vector3D& v) {
        return std::isfinite((double)v.x) && std::isfinite((double)v.y) && std::isfinite((double)v.z);
    }

    static inline bool is_finite_color(const Color& c) {
        // Color 通常是 uint8_t，无需 isfinite；这里保持接口一致性
        (void)c;
        return true;
    }

    static bool 计算平均颜色_从簇(
        const 结构体_原始场景帧& 帧,
        const 点簇& 簇,
        const 观测提取参数& p,
        Color& outColor)
    {
        const int w = 帧.宽度;
        const int h = 帧.高度;
        if (w <= 0 || h <= 0) return false;

        // 颜色数组必须与图像尺寸一致
        if ((int)帧.颜色.size() != w * h) return false;

        const int step = std::max(1, p.颜色采样步长);

        std::uint64_t sumR = 0, sumG = 0, sumB = 0;
        std::uint64_t cnt = 0;

        // 跳采样：用簇内点序号 % step 控制即可（不依赖 u/v）
        for (size_t i = 0; i < 簇.size(); i += (size_t)step) {
            const auto& pix = 簇[i];
            if ((unsigned)pix.u >= (unsigned)w || (unsigned)pix.v >= (unsigned)h) continue;

            const int id = idx(pix.u, pix.v, w);
            const Color& c = 帧.颜色[(size_t)id];

            sumR += (std::uint64_t)c.r;
            sumG += (std::uint64_t)c.g;
            sumB += (std::uint64_t)c.b;
            ++cnt;
        }

        if (cnt == 0) return false;

        outColor.r = (std::uint8_t)(sumR / cnt);
        outColor.g = (std::uint8_t)(sumG / cnt);
        outColor.b = (std::uint8_t)(sumB / cnt);
        return true;
    }

    static bool 校验观测(const 存在观测& obs, const 观测提取参数& p)
    {
        // 1) 中心/尺寸必须存在且为有限值
        if (!is_finite3(obs.中心) || !is_finite3(obs.尺寸)) return false;

        // 2) 中心必须在相机前方
        if (obs.中心.z < p.最小中心Z) return false;

        // 3) 尺寸必须为正且 >= 最小尺寸
        if (obs.尺寸.x < p.最小尺寸 || obs.尺寸.y < p.最小尺寸 || obs.尺寸.z < p.最小尺寸)
            return false;

        // 4) 轮廓编码必须有效
        if (p.要求轮廓编码) {
            if (obs.轮廓编码.empty()) return false;
            if (p.严格轮廓维度64 && obs.轮廓编码.size() != 64) return false;

            // 容忍编码里出现非0/1：这里做一次归一化检查（不通过则判无效）
            for (auto v : obs.轮廓编码) {
                if (!(v == 0 || v == 1)) return false;
            }
        }

        // 5) 颜色（如要求）
        if (p.要求颜色) {
            if (!is_finite_color(obs.平均颜色)) return false;
            // 颜色本身是 uint8_t，不会“空”，这里只是占位
        }

        return true;
    }

public:
    // ==============================
    // 主接口：从“点簇增强结果”提取单个观测
    // ==============================
    bool 从点簇增强提取存在观测(
        const 结构体_原始场景帧& 帧,
        const 点簇增强结果& enhanced,
        存在观测& out,
        const 观测提取参数& p = {}) const
    {
        // 必须：簇点数量够
        if ((int)enhanced.簇.size() < p.最小有效点数) return false;

        // 必须：中心/尺寸来自增强结果
        out.时间 = 结构体_时间戳::当前();
        out.中心 = enhanced.中心;
        out.尺寸 = enhanced.尺寸;

        // 必须：轮廓编码来自增强结果
        out.轮廓编码 = enhanced.轮廓编码;

        // 可选：轮廓3D（调试）
        out.轮廓3D = enhanced.轮廓3D;

        // 主方向先占位（后续可从 PCA/OBB 计算）
        out.主方向1 = { 1.0f, 0.0f, 0.0f };
        out.主方向2 = { 0.0f, 1.0f, 0.0f };
        out.主方向3 = { 0.0f, 0.0f, 1.0f };

        // 颜色：从“簇像素”在对齐后的颜色帧里平均
        if (p.要求颜色) {
            Color avg{};
            if (!计算平均颜色_从簇(帧, enhanced.簇, p, avg)) {
                return false; // 颜色是必须项 => 算不出来则整体无效
            }
            out.平均颜色 = avg;
        }
        else {
            // 不要求颜色：给默认值
            out.平均颜色.r = 255;
            out.平均颜色.g = 255;
            out.平均颜色.b = 255;
        }

        // 最终校验：任何必须项无效 => 整体无效
        if (!校验观测(out, p)) return false;

        return true;
    }

    // ==============================
    // 批量：从“增强点簇列表”提取观测（自动过滤无效项）
    // ==============================
    std::vector<存在观测> 从点簇增强列表提取观测(
        const 结构体_原始场景帧& 帧,
        const std::vector<点簇增强结果>& 簇列表,
        const 观测提取参数& p = {}) const
    {
        std::vector<存在观测> out;
        out.reserve(簇列表.size());

        for (const auto& enhanced : 簇列表) {
            存在观测 obs;
            if (从点簇增强提取存在观测(帧, enhanced, obs, p)) {
                out.push_back(std::move(obs));
            }
        }
        return out;
    }

    // ==============================
    // 一步到位：输入帧 -> 分割增强点簇 -> 输出观测（过滤无效项）
    // ==============================
    std::vector<存在观测> 从帧提取观测(
        const 结构体_原始场景帧& 帧,
        点簇分割类& 分割器,
        const 点簇分割参数& 分割参数,
        const 观测提取参数& 提取参数 = {}) const
    {
        auto enhanced = 分割器.分割点簇_增强(帧, 分割参数);
        return 从点簇增强列表提取观测(帧, enhanced, 提取参数);
    }
};
