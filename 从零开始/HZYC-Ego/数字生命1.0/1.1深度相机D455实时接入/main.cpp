#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <algorithm>
#include <map>
#include <librealsense2/rs.hpp>  // librealsense SDK 头文件


// 您的原代码结构（Vector3D, ExistenceNode, SelfNode, StateNode, SceneNode, DigitalLifeSystem）保持不变
// ... (复制您的原代码到这里，包括所有类定义)
// 基础数据结构定义
struct Vector3D {
    double x, y, z;
    Vector3D(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    double distance(const Vector3D& other) const {
        return std::sqrt((x - other.x) * (x - other.x) +
            (y - other.y) * (y - other.y) +
            (z - other.z) * (z - other.z));
    }
    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(x - other.x, y - other.y, z - other.z);
    }
};

// 存在节点类 - 表示场景中的物体
class ExistenceNode {
private:
    int id;
    Vector3D position;  // 质心位置
    Vector3D dimensions; // 长宽高
    double distanceToSelf; // 与自我的距离
    int pixelArea; // 投影面积
    std::vector<Vector3D> trajectory; // 轨迹历史

public:
    ExistenceNode(int id, const Vector3D& pos, const Vector3D& dims, double dist, int area)
        : id(id), position(pos), dimensions(dims), distanceToSelf(dist), pixelArea(area) {
        trajectory.push_back(pos);
    }

    // 获取特征
    int getId() const { return id; }
    Vector3D getPosition() const { return position; }
    Vector3D getDimensions() const { return dimensions; }
    double getDistanceToSelf() const { return distanceToSelf; }
    int getPixelArea() const { return pixelArea; }

    // 更新位置并计算轨迹
    void updatePosition(const Vector3D& newPos) {
        position = newPos;
        trajectory.push_back(newPos);
    }

    // 计算速度向量
    Vector3D getVelocity() const {
        if (trajectory.size() < 2) return Vector3D(0, 0, 0);
        const auto& current = trajectory.back();
        const auto& previous = trajectory[trajectory.size() - 2];
        return Vector3D(current.x - previous.x, current.y - previous.y, current.z - previous.z);
    }

    // 获取形状特征
    double getAspectRatio() const {
        double maxDim = std::max({ dimensions.x, dimensions.y, dimensions.z });
        double minDim = std::min({ dimensions.x, dimensions.y, dimensions.z });
        return maxDim / (minDim + 0.001); // 避免除零
    }

    bool isPlanar() const {
        double minDim = std::min({ dimensions.x, dimensions.y, dimensions.z });
        return minDim < 0.1; // 假设小于0.1为平面
    }
};

// 自我节点类
class SelfNode {
private:
    Vector3D position;
    double fieldOfView;
    double maxDistance;
    double safetyLevel;
    double curiosityLevel;

public:
    SelfNode() : position(0, 0, 0), fieldOfView(60.0), maxDistance(10.0),
        safetyLevel(0.5), curiosityLevel(0.5) {
    }

    Vector3D getPosition() const { return position; }
    double getSafetyLevel() const { return safetyLevel; }
    double getCuriosityLevel() const { return curiosityLevel; }
    void setSafetyLevel(double level) { safetyLevel = std::max(0.0, std::min(1.0, level)); }
    void setCuriosityLevel(double level) { curiosityLevel = std::max(0.0, std::min(1.0, level)); }
    double getFOV() const { return fieldOfView; }
    double getMaxDistance() const { return maxDistance; }
};

// 状态节点类 - 场景快照
class StateNode {
private:
    int timestamp;
    std::vector<std::shared_ptr<ExistenceNode>> existences;
    std::shared_ptr<SelfNode> self;

public:
    StateNode(int time, std::shared_ptr<SelfNode> selfNode)
        : timestamp(time), self(selfNode) {
    }

    void addExistence(std::shared_ptr<ExistenceNode> existence) {
        existences.push_back(existence);
    }

    const std::vector<std::shared_ptr<ExistenceNode>>& getExistences() const {
        return existences;
    }

    std::shared_ptr<SelfNode> getSelf() const { return self; }
    int getTimestamp() const { return timestamp; }
};

// 场景节点类
class SceneNode {
private:
    std::string name;
    std::vector<std::shared_ptr<StateNode>> states;

public:
    SceneNode(const std::string& sceneName) : name(sceneName) {}

    void addState(std::shared_ptr<StateNode> state) {
        states.push_back(state);
    }

    const std::vector<std::shared_ptr<StateNode>>& getStates() const {
        return states;
    }

    std::string getName() const { return name; }
};

// 点云数据结构
struct PointCloud {
    std::vector<Vector3D> points;
    void addPoint(const Vector3D& point) { points.push_back(point); }
    size_t size() const { return points.size(); }
    const Vector3D& operator[](size_t index) const { return points[index]; }
};

// 数字生命核心系统



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
     // 分割存在（点云聚类）
    std::vector<PointCloud> segmentExistences(const PointCloud& cloud) {
        std::vector<PointCloud> clusters;

        // 简化的聚类算法 - 基于距离的连通性
        std::vector<bool> processed(cloud.size(), false);

        for (size_t i = 0; i < cloud.size(); ++i) {
            if (processed[i]) continue;

            PointCloud cluster;
            std::vector<size_t> toProcess = { i };
            processed[i] = true;

            while (!toProcess.empty()) {
                size_t current = toProcess.back();
                toProcess.pop_back();
                cluster.addPoint(cloud[current]);

                // 查找邻近点
                for (size_t j = 0; j < cloud.size(); ++j) {
                    if (processed[j]) continue;
                    if (cloud[current].distance(cloud[j]) < 1.0) { // 距离阈值
                        toProcess.push_back(j);
                        processed[j] = true;
                    }
                }
            }

            if (cluster.size() > 5) { // 最小聚类大小
                clusters.push_back(cluster);
            }
        }

        return clusters;
    }
    // 生成存在节点
    std::shared_ptr<ExistenceNode> generateExistenceNode(const PointCloud& cluster, int id) {
        // 计算质心
        Vector3D centroid(0, 0, 0);
        for (size_t i = 0; i < cluster.size(); ++i) {
            centroid.x += cluster[i].x;
            centroid.y += cluster[i].y;
            centroid.z += cluster[i].z;
        }
        centroid.x /= cluster.size();
        centroid.y /= cluster.size();
        centroid.z /= cluster.size();

        // 计算包围盒尺寸
        double minX = cluster[0].x, maxX = cluster[0].x;
        double minY = cluster[0].y, maxY = cluster[0].y;
        double minZ = cluster[0].z, maxZ = cluster[0].z;

        for (size_t i = 1; i < cluster.size(); ++i) {
            minX = std::min(minX, cluster[i].x);
            maxX = std::max(maxX, cluster[i].x);
            minY = std::min(minY, cluster[i].y);
            maxY = std::max(maxY, cluster[i].y);
            minZ = std::min(minZ, cluster[i].z);
            maxZ = std::max(maxZ, cluster[i].z);
        }

        Vector3D dimensions(maxX - minX, maxY - minY, maxZ - minZ);
        double distance = centroid.distance(self->getPosition());
        int pixelArea = cluster.size(); // 用点数模拟像素面积

        return std::make_shared<ExistenceNode>(id, centroid, dimensions, distance, pixelArea);
    }
    //3. 生成场景状态
    std::shared_ptr<StateNode> generateState(const std::vector<std::shared_ptr<ExistenceNode>>& existences) {
        auto state = std::make_shared<StateNode>(currentTime++, self);
        for (const auto& existence : existences) {
            state->addExistence(existence);
        }
        return state;
    }
    void executeObservationTask() {
        std::cout << "\n=== 执行观察任务 ===" << std::endl;

        // 1. 获取深度图
        PointCloud cloud = getDepthFrame();
        std::cout << "获取点云数据: " << cloud.size() << " 个点" << std::endl;

        // 2. 分割存在
        auto clusters = segmentExistences(cloud);
        std::cout << "分割得到 " << clusters.size() << " 个存在簇" << std::endl;

        // 3. 生成存在节点
        std::vector<std::shared_ptr<ExistenceNode>> existences;
        for (size_t i = 0; i < clusters.size(); ++i) {
            auto existence = generateExistenceNode(clusters[i], i);
            existences.push_back(existence);
            std::cout << "存在 " << i << ": 位置(" << existence->getPosition().x << ","
                << existence->getPosition().y << "," << existence->getPosition().z
                << ") 距离: " << existence->getDistanceToSelf() << std::endl;
        }
       
        // 4. 生成状态
        auto currentState = generateState(existences);

        // 5. 跨帧匹配
        matchPreviousFrame(currentState);

        // 6. 更新安全度和好奇度
        updateSafety(currentState);
        updateCuriosity(currentState);

        // 7. 添加到场景
        currentScene->addState(currentState);

        std::cout << "安全度: " << self->getSafetyLevel()
            << ", 好奇度: " << self->getCuriosityLevel() << std::endl;

        previousState = currentState;
    }
    // 主循环
    void mainLoop(int iterations = 10) {
        std::cout << "=== 数字生命系统启动（D455 实时接入） ===\n";
        
        for (int i = 0; i < iterations; ++i) {
            // 在 DigitalLifeSystem 类中补充 executeObservationTask 方法实现
            // 参考模拟版的实现，适配 D455 实时点云

            void executeObservationTask() {
                std::cout << "\n=== 执行观察任务 ===" << std::endl;

                // 1. 获取深度图（D455 实时点云）
                PointCloud cloud = getDepthFrame();
                std::cout << "获取点云数据: " << cloud.size() << " 个点" << std::endl;

                // 2. 分割存在
                auto clusters = segmentExistences(cloud);
                std::cout << "分割得到 " << clusters.size() << " 个存在簇" << std::endl;

                // 3. 生成存在节点
                std::vector<std::shared_ptr<ExistenceNode>> existences;
                for (size_t i = 0; i < clusters.size(); ++i) {
                    auto existence = generateExistenceNode(clusters[i], static_cast<int>(i));
                    existences.push_back(existence);
                    std::cout << "存在 " << i << ": 位置(" << existence->getPosition().x << ","
                        << existence->getPosition().y << "," << existence->getPosition().z
                        << ") 距离: " << existence->getDistanceToSelf() << std::endl;
                }

                // 4. 生成状态
                auto currentState = generateState(existences);
                // 5. 跨帧匹配                
                matchPreviousFrame(currentState);
                // 6. 更新安全度和好奇度
                updateSafety(currentState);
                updateCuriosity(currentState);

                // 7. 添加到场景
                currentScene->addState(currentState);

                std::cout << "安全度: " << self->getSafetyLevel()
                    << ", 好奇度: " << self->getCuriosityLevel() << std::endl;

                previousState = currentState;
            }
            
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