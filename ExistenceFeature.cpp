#include "ExistenceFeature.h"
#include <algorithm>
#include <cmath>
#include <random>

ExistenceFeature::ExistenceFeature(int id) 
    : id(id), last_seen_time(0), confidence(1.0) {
    // 随机生成颜色（简化版本，不使用OpenCV）
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(100, 255);
    color.r = dis(gen);
    color.g = dis(gen);
    color.b = dis(gen);
}

void ExistenceFeature::update(const Vector3D& pos, const std::vector<int64_t>& contour, double time) {
    trajectory.push_back(pos);
    contour_history.push_back(contour);
    
    // 限制历史数据长度
    if (trajectory.size() > 50) {
        trajectory.erase(trajectory.begin());
    }
    if (contour_history.size() > 50) {
        contour_history.erase(contour_history.begin());
    }
    
    // 计算平均速度
    calculateAverageVelocity();
    
    last_seen_time = time;
    confidence = std::min(1.0, confidence + 0.1);
}

void ExistenceFeature::calculateAverageVelocity() {
    if (trajectory.size() < 2) {
        avg_velocity = Vector3D(0, 0, 0);
        return;
    }
    
    Vector3D sum_vel(0, 0, 0);
    for (size_t i = 1; i < trajectory.size(); ++i) {
        auto vel = trajectory[i] - trajectory[i-1];
        sum_vel.x += std::abs(vel.x);
        sum_vel.y += std::abs(vel.y);
        sum_vel.z += std::abs(vel.z);
    }
    
    avg_velocity = Vector3D(
        sum_vel.x / (trajectory.size() - 1),
        sum_vel.y / (trajectory.size() - 1),
        sum_vel.z / (trajectory.size() - 1)
    );
}

void ExistenceFeature::decay(double current_time) {
    double age = current_time - last_seen_time;
    confidence = std::max(0.3, confidence - age * 0.02); // 每秒衰减2%
}

ExistenceMemory::ExistenceMemory() : next_id(1) {}

std::shared_ptr<ExistenceFeature> ExistenceMemory::getOrCreate(int temp_id, const Vector3D& pos, 
                                                              const std::vector<int64_t>& contour, double time) {
    // 查找最匹配的历史存在（位置+轮廓相似度）
    double best_score = 0.5;
    std::shared_ptr<ExistenceFeature> best_match = nullptr;
    
    for (auto& pair : database) {
        auto& mem = pair.second;
        mem->decay(time); // 更新置信度衰减
        
        double score = calculateMatchScore(mem, pos, contour, time);
        if (score > best_score) {
            best_score = score;
            best_match = mem;
        }
    }
    
    if (best_match) {
        best_match->update(pos, contour, time);
        return best_match;
    } else {
        auto new_mem = std::make_shared<ExistenceFeature>(next_id++);
        new_mem->update(pos, contour, time);
        database[new_mem->id] = new_mem;
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