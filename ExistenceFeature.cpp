#include "ExistenceFeature.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>

// ==================== WorldCoordinateSystem 实现 ====================
WorldCoordinateSystem world_coords; // 全局世界坐标系实例

void WorldCoordinateSystem::setOrigin(const Vector3D& pos, double time) {
    origin = pos;
    last_update_time = time;
    std::cout << "世界坐标系建立，原点: (" << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
}

Vector3D WorldCoordinateSystem::worldToEgo(const Vector3D& world_pos) const {
    return Vector3D(world_pos.x - origin.x, world_pos.y - origin.y, world_pos.z - origin.z);
}

Vector3D WorldCoordinateSystem::egoToWorld(const Vector3D& ego_pos) const {
    return Vector3D(ego_pos.x + origin.x, ego_pos.y + origin.y, ego_pos.z + origin.z);
}

// ==================== ExistenceFeature 实现 ====================
ExistenceFeature::ExistenceFeature(int id, const Vector3D& world_pos) 
    : id(id), world_position(world_pos), last_ego_position(world_pos), 
      last_seen_time(0), first_seen_time(0), appearance_count(1), 
      confidence(1.0), is_active(true) {
    
    // 设置时间戳
    first_seen_time = last_seen_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
    
    // 添加初始轨迹点
    trajectory.push_back(world_pos);
    
    // 随机生成颜色
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 255);
    color.r = dis(gen);
    color.g = dis(gen);
    color.b = dis(gen);
}

void ExistenceFeature::update(const Vector3D& new_ego_pos, const std::vector<int64_t>& contour, double time) {
    // 计算新的世界坐标
    Vector3D new_world_pos = world_coords.egoToWorld(new_ego_pos);
    
    // 计算速度（基于世界坐标）
    double time_diff = time - last_seen_time + 0.001; // 避免除零
    velocity = Vector3D(
        (new_world_pos.x - world_position.x) / time_diff,
        (new_world_pos.y - world_position.y) / time_diff,
        (new_world_pos.z - world_position.z) / time_diff
    );
    
    // 更新位置和状态
    world_position = new_world_pos;
    last_ego_position = new_ego_pos;
    last_seen_time = time;
    appearance_count++;
    confidence = std::min(1.0, confidence + 0.05);
    
    // 更新轨迹和轮廓历史
    trajectory.push_back(new_world_pos);
    contour_history.push_back(contour);
    
    // 限制历史数据长度
    if (trajectory.size() > 50) {
        trajectory.erase(trajectory.begin());
    }
    if (contour_history.size() > 50) {
        contour_history.erase(contour_history.begin());
    }
    
    is_active = true;
}

bool ExistenceFeature::shouldTerminate(double current_time, double timeout) const {
    if (!is_active) return false;
    return (current_time - last_seen_time) > timeout;
}

void ExistenceFeature::terminate() {
    is_active = false;
    std::cout << "存在 ID:" << id << " 长时间未见，跟踪终止（出现 " << appearance_count << " 次）\n";
}

void ExistenceFeature::calculateAverageVelocity() {
    if (trajectory.size() < 2) {
        velocity = Vector3D(0, 0, 0);
        return;
    }
    
    Vector3D sum_vel(0, 0, 0);
    for (size_t i = 1; i < trajectory.size(); ++i) {
        auto vel = trajectory[i] - trajectory[i-1];
        sum_vel.x += std::abs(vel.x);
        sum_vel.y += std::abs(vel.y);
        sum_vel.z += std::abs(vel.z);
    }
    
    velocity = Vector3D(
        sum_vel.x / (trajectory.size() - 1),
        sum_vel.y / (trajectory.size() - 1),
        sum_vel.z / (trajectory.size() - 1)
    );
}

ExistenceMemory::ExistenceMemory() : next_id(1) {}

std::shared_ptr<ExistenceFeature> ExistenceMemory::getOrCreate(int temp_id, const Vector3D& pos, 
                                                              const std::vector<int64_t>& contour, double time) {
    
    // 清理长时间未见的存在（智能跟踪终止）
    for (auto it = database.begin(); it != database.end();) {
        if (it->second->shouldTerminate(time)) {
            it->second->terminate();
            it = database.erase(it);
        } else {
            ++it;
        }
    }
    
    // 将自我坐标转换为世界坐标
    Vector3D world_pos = world_coords.egoToWorld(pos);
    
    // 查找最匹配的历史存在（基于世界坐标+轮廓相似度）
    double best_score = 0.7;
    std::shared_ptr<ExistenceFeature> best_match = nullptr;
    
    for (auto& pair : database) {
        auto& mem = pair.second;
        
        double pos_dist = mem->world_position.distance(world_pos);
        double contour_sim = 0.0;
        if (!mem->contour_history.empty()) {
            contour_sim = contourSimilarity(mem->contour_history.back(), contour);
        }
        double time_weight = 1.0 / (1.0 + (time - mem->last_seen_time));
        double score = (1.0 / (1.0 + pos_dist)) * contour_sim * time_weight * mem->confidence;
        
        if (score > best_score && pos_dist < 1.5) { // 距离阈值1.5米
            best_score = score;
            best_match = mem;
        }
    }
    
    if (best_match) {
        best_match->update(pos, contour, time);
        return best_match;
    } else {
        // 发现新存在
        auto new_mem = std::make_shared<ExistenceFeature>(next_id++, world_pos);
        new_mem->update(pos, contour, time);
        database[new_mem->id] = new_mem;
        std::cout << "发现新存在！分配ID:" << new_mem->id 
                  << " 世界坐标:(" << world_pos.x << "," << world_pos.y << "," << world_pos.z << ")\n";
        return new_mem;
    }
}

double ExistenceMemory::calculateMatchScore(std::shared_ptr<ExistenceFeature> memory, const Vector3D& pos, 
                                           const std::vector<int64_t>& contour, double time) {
    if (memory->trajectory.empty()) {
        return 0.0;
    }
    
    double pos_dist = memory->trajectory.back().distance(pos);
    if (pos_dist >= 1.0) { // 距离阈值
        return 0.0;
    }
    
    double contour_sim = 0.0;
    if (!memory->contour_history.empty()) {
        contour_sim = contourSimilarity(memory->contour_history.back(), contour);
    }
    
    double score = (1.0 / (1.0 + pos_dist)) * contour_sim * memory->confidence;
    return score;
}

double ExistenceMemory::contourSimilarity(const std::vector<int64_t>& a, const std::vector<int64_t>& b) {
    if (a.size() != b.size() || a.empty()) {
        return 0.0;
    }
    
    double sum = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        sum += 1.0 / (1.0 + std::abs(a[i] - b[i]));
    }
    
    return sum / a.size();
}