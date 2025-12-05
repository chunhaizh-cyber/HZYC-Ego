#include <iostream>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include <iomanip>
#include <random>
#include <algorithm>
#include <thread>
#include <cstdlib>

// 由于环境限制，我们创建一个纯控制台版本的数字生命系统
// 移除OpenCV依赖，使用纯C++实现

#include "Vector3D.h"
#include "ExistenceFeature.h"
#include "PointCloudProcessor.h"

// 全局变量
std::vector<std::shared_ptr<ExistenceFeature>> currentExistences;
ExistenceMemory global_memory;
PointCloudProcessor point_cloud_processor;

int frame_id = 0;
bool system_running = true;
std::shared_ptr<ExistenceFeature> locked_target = nullptr;

// 模拟数字生命状态
struct DigitalLife {
    double safety = 0.5;
    double curiosity = 0.5;
    std::vector<std::shared_ptr<ExistenceFeature>> existences;
    
    void updateSafetyAndCuriosity() {
        // 根据环境中目标的数量和距离更新安全度和好奇度
        if (existences.empty()) {
            safety = std::min(1.0, safety + 0.01);
            curiosity = std::max(0.0, curiosity - 0.01);
        } else {
            double avg_distance = 0;
            for (const auto& existence : existences) {
                if (!existence->trajectory.empty()) {
                    avg_distance += existence->trajectory.back().distance(Vector3D(0, 0, 0));
                }
            }
            avg_distance /= existences.size();
            
            if (avg_distance < 2.0) {
                safety = std::max(0.0, safety - 0.02);
                curiosity = std::min(1.0, curiosity + 0.02);
            } else {
                safety = std::min(1.0, safety + 0.01);
                curiosity = std::max(0.0, curiosity - 0.01);
            }
        }
    }
};

DigitalLife life;

// 模拟RealSense数据生成
std::vector<Vector3D> generateSimulatedPointCloud() {
    std::vector<Vector3D> cloud;
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    // 生成几个模拟目标
    int num_targets = 3 + (rand() % 3);  // 3-5个目标
    
    for (int target = 0; target < num_targets; ++target) {
        // 每个目标的中心位置
        std::uniform_real_distribution<> x_dis(-3.0, 3.0);
        std::uniform_real_distribution<> y_dis(-2.0, 2.0);
        std::uniform_real_distribution<> z_dis(1.0, 5.0);
        
        Vector3D center(x_dis(gen), y_dis(gen), z_dis(gen));
        
        // 生成围绕中心的点云
        int num_points = 50 + (rand() % 200);  // 50-250个点
        std::normal_distribution<> point_noise(0.0, 0.1);
        
        for (int i = 0; i < num_points; ++i) {
            Vector3D point(
                center.x + point_noise(gen),
                center.y + point_noise(gen),
                center.z + point_noise(gen)
            );
            cloud.push_back(point);
        }
    }
    
    return cloud;
}

// 生成模拟轮廓特征
std::vector<int64_t> generateSimulatedContour() {
    std::vector<int64_t> contour(81);
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 1000);
    
    std::generate(contour.begin(), contour.end(), [&]() { return dis(gen); });
    return contour;
}

// 主运行函数
// 控制台可视化函数（v3.0版本）
void displayConsoleVisualization() {
    std::cout << "\n=== 数字生命 v3.0 视觉系统 ===\n";
    std::cout << "时间: " << frame_id << "帧 | ";
    std::cout << "存在目标: " << currentExistences.size() << " | ";
    std::cout << "安全度: " << std::fixed << std::setprecision(2) << life.safety << " | ";
    std::cout << "好奇度: " << life.curiosity << "\n";
    std::cout << "世界坐标系: 原点(" << world_coords.origin.x << "," 
              << world_coords.origin.y << "," << world_coords.origin.z << ")\n";
    
    if (currentExistences.empty()) {
        std::cout << "【环境感知】未发现目标，系统处于扫描模式...\n";
        return;
    }
    
    std::cout << "\n【目标识别】发现 " << currentExistences.size() << " 个目标:\n";
    std::cout << "┌────┬──────┬────────┬────────┬────────┬──────────────┬────────┐\n";
    std::cout << "│ ID │ 类型 │ 距离(m)│ 出现次数│ 置信度 │ 世界坐标     │  状态  │\n";
    std::cout << "├────┼──────┼────────┼────────┼────────┼──────────────┼────────┤\n";
    
    for (const auto& existence : currentExistences) {
        if (!existence->is_active) continue; // 跳过已终止的跟踪
        
        Vector3D world_pos = existence->world_position;
        double distance = world_pos.distance(Vector3D(0, 0, 0));
        std::string type = (existence->trajectory.size() > 100) ? "人形" : "物体";
        std::string status = (existence == locked_target) ? "【锁定】" : 
                           (existence->is_active ? "监测中" : "[已消失]");
        
        std::cout << "│ " << std::setw(2) << existence->id << " │ "
                  << std::setw(4) << type << " │ "
                  << std::setw(6) << std::fixed << std::setprecision(2) << distance << " │ "
                  << std::setw(6) << existence->appearance_count << " │ "
                  << std::setw(6) << std::fixed << std::setprecision(2) << existence->confidence << " │ "
                  << "(" << std::setw(4) << std::fixed << std::setprecision(1) << world_pos.x << ","
                  << std::setw(4) << world_pos.y << "," << std::setw(4) << world_pos.z << ") │ "
                  << status << " │\n";
    }
    
    std::cout << "└────┴──────┴────────┴────────┴────────┴──────────────┴────────┘\n";
    
    // 显示锁定目标详细信息（v3.0版本）
    if (locked_target && locked_target->is_active) {
        double age = frame_id * 0.1; // 简化的观察时间
        std::cout << "\n【目标锁定】持续观察 ID:" << locked_target->id << "\n";
        std::cout << "   世界坐标: (" << locked_target->world_position.x << ","
                  << locked_target->world_position.y << "," 
                  << locked_target->world_position.z << ")\n";
        std::cout << "   出现次数: " << locked_target->appearance_count << " 次\n";
        std::cout << "   观察时长: " << std::fixed << std::setprecision(1) << age << "秒\n";
        std::cout << "   轨迹记录: " << locked_target->trajectory.size() << " 个数据点\n";
        std::cout << "   平均速度: " << locked_target->velocity.magnitude() << " m/s\n";
        std::cout << "   记忆强度: " << (locked_target->confidence > 0.8 ? "高" : 
                                        locked_target->confidence > 0.5 ? "中" : "低") << "\n";
    }
}

void run() {
    std::cout << "数字生命 v3.0 启动：绝对世界坐标 + 智能跟踪终止 + 出现次数统计\n";
    std::cout << "控制命令: [l] 锁定最近目标  [q] 退出系统  [其他键] 继续运行\n\n";
    
    double lock_start_time = 0;
    bool world_origin_set = false;
    Vector3D first_camera_pos;
    
    while (system_running) {
        double current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
        
        // 设置世界原点（第一帧相机位置）
        if (!world_origin_set) {
            first_camera_pos = Vector3D(0, 0, 0); // 相机初始位置
            world_coords.setOrigin(first_camera_pos, current_time);
            world_origin_set = true;
        }
        
        // 生成模拟点云数据
        std::vector<Vector3D> cloud = generateSimulatedPointCloud();
        std::vector<std::vector<int64_t>> contours;
        
        // 为每个聚类生成模拟轮廓特征
        int num_clusters = cloud.size() / 100 + 1; // 估算聚类数量
        for (int i = 0; i < num_clusters; ++i) {
            contours.push_back(generateSimulatedContour());
        }
        
        // 点云聚类
        auto clusters = point_cloud_processor.clusterPoints(cloud);
        
        // 清空当前存在列表
        currentExistences.clear();
        
        // 处理每个聚类
        int temp_id = 0;
        for (const auto& cluster : clusters) {
            if (temp_id >= contours.size()) break;
            
            // 计算聚类中心（自我坐标）
            Vector3D ego_center = cluster.center;
            
            // 特征记忆与跟踪（使用绝对坐标系统）
            auto memory = global_memory.getOrCreate(temp_id, ego_center, contours[temp_id], current_time);
            currentExistences.push_back(memory);
            temp_id++;
        }
        
        // 更新数字生命状态
        life.existences = currentExistences;
        life.updateSafetyAndCuriosity();
        
        // 控制台可视化（v3.0版本，显示绝对坐标和出现次数）
        displayConsoleVisualization();
        
        // 显示状态信息（v3.0版本）
        std::cout << "\n【系统状态】帧:" << ++frame_id 
                  << " | 存在:" << life.existences.size() 
                  << " | 安全度:" << std::fixed << std::setprecision(2) << life.safety 
                  << " | 好奇度:" << life.curiosity 
                  << (locked_target ? " | 锁定目标中..." : "")
                  << "\n";
        
        // 键盘控制
        std::cout << "\n请输入命令 [l/q]: ";
        std::string input;
        std::getline(std::cin, input);
        
        if (!input.empty()) {
            char key = input[0];
            if (key == 'q' || key == 'Q') {
                system_running = false;
                break;
            } else if ((key == 'l' || key == 'L') && !currentExistences.empty()) {
                // 锁定最近的目标（基于世界坐标）
                auto nearest = *std::min_element(currentExistences.begin(), currentExistences.end(),
                    [](const auto& a, const auto& b) {
                        if (!a->is_active || !b->is_active) return false;
                        return a->world_position.distance(Vector3D(0, 0, 0)) < 
                               b->world_position.distance(Vector3D(0, 0, 0));
                    });
                locked_target = nearest;
                lock_start_time = current_time;
                std::cout << "\n>>> 目标锁定！ID:" << locked_target->id 
                          << " 世界坐标:(" << locked_target->world_position.x << ","
                          << locked_target->world_position.y << "," 
                          << locked_target->world_position.z << ")"
                          << " 出现次数:" << locked_target->appearance_count << "\n";
            }
        }
        
        // 模拟帧率延迟
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 清屏（可选）
        #ifdef _WIN32
            system("cls");
        #else
            system("clear");
        #endif
    }
    
    std::cout << "\n【数字生命系统关闭】\n";
    std::cout << "运行统计：\n";
    std::cout << "- 总帧数: " << frame_id << "\n";
    std::cout << "- 识别目标: " << global_memory.database.size() << " 个不同存在\n";
    std::cout << "- 系统状态: 安全度 " << life.safety << " | 好奇度 " << life.curiosity << "\n";
    std::cout << "- 世界坐标系: 原点 (" << world_coords.origin.x << "," 
              << world_coords.origin.y << "," << world_coords.origin.z << ")\n";
}

int main() {
    try {
        run();
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}