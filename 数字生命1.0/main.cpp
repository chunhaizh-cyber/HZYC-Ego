#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <algorithm>
#include <map>

// 来源：https://blog.csdn.net/qq_45118549/article/details/155409293
// 人工智能之数字生命-从零开始

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
        double maxDim = std::max({dimensions.x, dimensions.y, dimensions.z});
        double minDim = std::min({dimensions.x, dimensions.y, dimensions.z});
        return maxDim / (minDim + 0.001); // 避免除零
    }
    
    bool isPlanar() const {
        double minDim = std::min({dimensions.x, dimensions.y, dimensions.z});
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
                 safetyLevel(0.5), curiosityLevel(0.5) {}
    
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
        : timestamp(time), self(selfNode) {}
    
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
class DigitalLifeSystem {
private:
    std::shared_ptr<SelfNode> self;
    std::shared_ptr<SceneNode> currentScene;
    std::shared_ptr<StateNode> previousState;
    int currentTime;
    std::mt19937 rng;
    
public:
    DigitalLifeSystem() : currentTime(0), rng(std::random_device{}()) {
        self = std::make_shared<SelfNode>();
        currentScene = std::make_shared<SceneNode>("初始场景001");
    }
    
    // 获取一帧深度图（模拟）
    PointCloud getDepthFrame() {
        PointCloud cloud;
        
        // 生成多个物体簇，模拟真实场景
        std::uniform_int_distribution<> clusterCount(2, 4); // 2-4个物体簇
        std::uniform_int_distribution<> pointsPerCluster(15, 35); // 每个簇15-35个点
        
        int numClusters = clusterCount(rng);
        
        for (int c = 0; c < numClusters; ++c) {
            // 为每个簇生成中心位置
            std::uniform_real_distribution<> centerDis(-4.0, 4.0);
            std::uniform_real_distribution<> centerZ(1.0, 6.0);
            Vector3D clusterCenter(centerDis(rng), centerDis(rng), centerZ(rng));
            
            int clusterSize = pointsPerCluster(rng);
            
            // 生成围绕中心的点云
            std::normal_distribution<> pointDis(0.0, 0.3); // 正态分布，标准差0.3
            
            for (int i = 0; i < clusterSize; ++i) {
                double x = clusterCenter.x + pointDis(rng);
                double y = clusterCenter.y + pointDis(rng);
                double z = clusterCenter.z + pointDis(rng) * 0.5; // Z轴变化小一些
                
                // 确保点在合理范围内
                if (std::abs(x) <= 5.0 && std::abs(y) <= 5.0 && z >= 0.1 && z <= 8.0) {
                    cloud.addPoint(Vector3D(x, y, z));
                }
            }
        }
        
        // 添加一些噪声点
        std::uniform_real_distribution<> noiseDis(-5.0, 5.0);
        std::uniform_real_distribution<> noiseZ(0.1, 8.0);
        for (int i = 0; i < 20; ++i) {
            cloud.addPoint(Vector3D(noiseDis(rng), noiseDis(rng), noiseZ(rng)));
        }
        
        return cloud;
    }
    
    // 分割存在（点云聚类）
    std::vector<PointCloud> segmentExistences(const PointCloud& cloud) {
        std::vector<PointCloud> clusters;
        
        // 简化的聚类算法 - 基于距离的连通性
        std::vector<bool> processed(cloud.size(), false);
        
        for (size_t i = 0; i < cloud.size(); ++i) {
            if (processed[i]) continue;
            
            PointCloud cluster;
            std::vector<size_t> toProcess = {i};
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
    
    // 生成场景状态
    std::shared_ptr<StateNode> generateState(const std::vector<std::shared_ptr<ExistenceNode>>& existences) {
        auto state = std::make_shared<StateNode>(currentTime++, self);
        for (const auto& existence : existences) {
            state->addExistence(existence);
        }
        return state;
    }
    
    // 匹配前一帧
    void matchPreviousFrame(std::shared_ptr<StateNode> currentState) {
        if (!previousState) return;
        
        const auto& prevExistences = previousState->getExistences();
        const auto& currExistences = currentState->getExistences();
        
        // 简单的匹配算法 - 基于位置和尺寸相似性
        for (const auto& currExist : currExistences) {
            double bestScore = -1;
            std::shared_ptr<ExistenceNode> bestMatch = nullptr;
            
            for (const auto& prevExist : prevExistences) {
                double positionDiff = currExist->getPosition().distance(prevExist->getPosition());
                double sizeDiff = std::abs(currExist->getDimensions().x - prevExist->getDimensions().x) +
                                 std::abs(currExist->getDimensions().y - prevExist->getDimensions().y) +
                                 std::abs(currExist->getDimensions().z - prevExist->getDimensions().z);
                
                double score = 1.0 / (1.0 + positionDiff + sizeDiff * 0.1);
                
                if (score > bestScore && score > 0.7) { // 匹配阈值
                    bestScore = score;
                    bestMatch = prevExist;
                }
            }
            
            if (bestMatch) {
                // 更新轨迹
                const auto& trajectory = bestMatch->getPosition();
                std::const_pointer_cast<ExistenceNode>(currExist)->updatePosition(trajectory);
            }
        }
    }
    
    // 更新安全度
    void updateSafety(std::shared_ptr<StateNode> state) {
        const auto& existences = state->getExistences();
        if (existences.empty()) {
            self->setSafetyLevel(self->getSafetyLevel() + 0.05); // 安全环境
            return;
        }
        
        // 找到最近的存在
        double minDistance = std::numeric_limits<double>::max();
        int approachingCount = 0;
        
        for (const auto& existence : existences) {
            double distance = existence->getDistanceToSelf();
            if (distance < minDistance) {
                minDistance = distance;
            }
            
            // 检查是否正在靠近
            Vector3D velocity = existence->getVelocity();
            Vector3D directionToSelf = self->getPosition() - existence->getPosition();
            double approachRate = (velocity.x * directionToSelf.x + 
                                 velocity.y * directionToSelf.y + 
                                 velocity.z * directionToSelf.z) / 
                                 (directionToSelf.distance(Vector3D(0,0,0)) + 0.001);
            
            if (approachRate > 0.5) { // 正在快速靠近
                approachingCount++;
            }
        }
        
        // 更新安全度
        double currentSafety = self->getSafetyLevel();
        
        if (minDistance < 2.0) { // 太近
            currentSafety -= 0.2;
        } else if (minDistance > 5.0) { // 安全距离
            currentSafety += 0.05;
        }
        
        if (approachingCount > 0) {
            currentSafety -= 0.1 * approachingCount;
        }
        
        self->setSafetyLevel(currentSafety);
    }
    
    // 更新好奇度
    void updateCuriosity(std::shared_ptr<StateNode> state) {
        const auto& existences = state->getExistences();
        double currentCuriosity = self->getCuriosityLevel();
        
        // 基于未知区域和变化频率
        int unknownCount = 0;
        int highChangeCount = 0;
        
        for (const auto& existence : existences) {
            if (existence->getDistanceToSelf() > 7.0) { // 远距离未知
                unknownCount++;
            }
            
            Vector3D velocity = existence->getVelocity();
            double speed = std::sqrt(velocity.x * velocity.x + 
                                   velocity.y * velocity.y + 
                                   velocity.z * velocity.z);
            
            if (speed > 0.3) { // 高变化
                highChangeCount++;
            }
        }
        
        if (unknownCount > 0) {
            currentCuriosity += 0.05 * unknownCount;
        }
        
        if (highChangeCount > 0) {
            currentCuriosity += 0.03 * highChangeCount;
        } else {
            currentCuriosity -= 0.02; // 环境稳定，好奇心下降
        }
        
        self->setCuriosityLevel(currentCuriosity);
    }
    
    // 执行观察任务
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
        std::cout << "=== 数字生命系统启动 ===" << std::endl;
        
        for (int i = 0; i < iterations; ++i) {
            executeObservationTask();
            
            // 简单的任务调度逻辑
            if (self->getSafetyLevel() < 0.3) {
                std::cout << "安全度过低，继续观察..." << std::endl;
            } else if (self->getCuriosityLevel() > 0.7) {
                std::cout << "好奇度较高，探索欲望强烈" << std::endl;
            }
            
            std::cout << std::endl;
        }
        
        std::cout << "=== 数字生命系统运行完成 ===" << std::endl;
        std::cout << "场景状态数: " << currentScene->getStates().size() << std::endl;
    }
};

int main() {
    DigitalLifeSystem system;
    system.mainLoop(5); // 运行5次迭代
    
    return 0;
}