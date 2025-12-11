#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <algorithm>
#include <map>
#include <rs2/rs2.hpp>  // librealsense SDK 头文件

// 您的原代码结构（Vector3D, ExistenceNode, SelfNode, StateNode, SceneNode, DigitalLifeSystem）保持不变
// ... (复制您的原代码到这里，包括所有类定义)

struct PointCloud {
    std::vector<Vector3D> points;
    void addPoint(const Vector3D& point) { points.push_back(point); }
    size_t size() const { return points.size(); }
    const Vector3D& operator[](size_t index) const { return points[index]; }
};

// 扩展 DigitalLifeSystem：接入 D455 实时深度帧
class DigitalLifeSystem {
private:
    std::shared_ptr<SelfNode> self;
    std::shared_ptr<SceneNode> currentScene;
    std::shared_ptr<StateNode> previousState;
    int currentTime;
    std::mt19937 rng;

    // D455 相机配置
    rs2::pipeline pipe;
    rs2::config cfg;
    rs2::frameset frames;

public:
    DigitalLifeSystem() : currentTime(0), rng(std::random_device{}()) {
        self = std::make_shared<SelfNode>();
        currentScene = std::make_shared<SceneNode>("实时场景001");
        
        // 初始化 D455 相机
        cfg.enable_stream(RS2_STREAM_DEPTH, 640, 480, RS2_FORMAT_Z16, 30);  // 深度流：640x480, 30 fps, Z16 格式
        cfg.enable_stream(RS2_STREAM_COLOR, 640, 480, RS2_FORMAT_BGR8, 30);  // RGB 流（可选，用于可视化）
        pipe.start(cfg);
        std::cout << "D455 相机初始化成功！深度分辨率: 640x480 @ 30 fps\n";
    }
    
    ~DigitalLifeSystem() {
        pipe.stop();  // 释放相机资源
    }
    
    // 扩展：实时获取 D455 深度帧（替换模拟 getDepthFrame）
    PointCloud getDepthFrame() {
        PointCloud cloud;
        if (!pipe.poll_for_frames(&frames)) {
            std::cout << "警告：未获取到帧，使用模拟数据\n";
            return simulateDepthFrame();  // 备用模拟
        }
        
        rs2::depth_frame depth = frames.get_depth_frame();  // 获取深度帧
        rs2::video_frame color = frames.get_color_frame();  // 获取 RGB 帧（可选）
        
        const uint16_t* depth_data = reinterpret_cast<const uint16_t*>(depth.get_data());
        const int width = depth.get_width();  // 640
        const int height = depth.get_height();  // 480
        const float depth_scale = depth.get_units();  // 深度单位（米）
        
        // 遍历像素，生成 3D 点云（采样率 10% 以优化性能）
        for (int y = 0; y < height; y += 8) {  // 每 8 像素采样一次
            for (int x = 0; x < width; x += 8) {
                int index = y * width + x;
                uint16_t depth_value = depth_data[index];
                if (depth_value > 0 && depth_value < 10000) {  // 有效深度（0-10米）
                    float depth_m = depth_value * depth_scale;  // 转换为米
                    
                    // 使用 intrinsics 计算 3D 坐标
                    rs2_intrinsics intrin = depth.get_profile().as<rs2::video_stream_profile>().get_intrinsics();
                    float pixel_x = (x - intrin.ppx) / intrin.fx;  // 归一化 x
                    float pixel_y = (y - intrin.ppy) / intrin.fy;  // 归一化 y
                    
                    // 3D 点（假设相机坐标系）
                    double world_x = pixel_x * depth_m;
                    double world_y = pixel_y * depth_m;
                    double world_z = depth_m;
                    
                    cloud.addPoint(Vector3D(world_x, world_y, world_z));
                }
            }
        }
        
        std::cout << "实时深度帧: " << cloud.size() << " 个点（采样率10%）\n";
        return cloud;
    }
    
    // 备用模拟深度帧（如果相机失败）
    PointCloud simulateDepthFrame() {
        PointCloud cloud;
        std::uniform_real_distribution<> dis(-5.0, 5.0);
        std::uniform_real_distribution<> z_dis(0.5, 10.0);
        for (int i = 0; i < 500; ++i) {  // 模拟500点
            cloud.addPoint(Vector3D(dis(rng), dis(rng), z_dis(rng)));
        }
        return cloud;
    }
    
    // 其他函数保持不变（segmentExistences, generateExistenceNode, etc.）
    // ... (复制您的原代码)
    
    // 主循环
    void mainLoop(int iterations = 10) {
        std::cout << "=== 数字生命系统启动（D455 实时接入） ===\n";
        
        for (int i = 0; i < iterations; ++i) {
            executeObservationTask();
            
            // 简单的任务调度逻辑
            if (self->getSafetyLevel() < 0.3) {
                std::cout << "安全度过低，继续观察...\n";
            } else if (self->getCuriosityLevel() > 0.7) {
                std::cout << "好奇度较高，探索欲望强烈\n";
            }
            
            std::cout << std::endl;
        }
        
        std::cout << "=== 数字生命系统运行完成 ===\n";
        std::cout << "场景状态数: " << currentScene->getStates().size() << std::endl;
        pipe.stop();  // 关闭相机
    }
};

int main() {
    try {
        DigitalLifeSystem system;
        system.mainLoop(10);  // 运行10次迭代
    } catch (const rs2::error& e) {
        std::cerr << "RealSense 错误: " << e.what() << std::endl;
        std::cerr << "请检查 D455 连接和 SDK 安装\n";
    } catch (const std::exception& e) {
        std::cerr << "系统错误: " << e.what() << std::endl;
    }
    return 0;
}