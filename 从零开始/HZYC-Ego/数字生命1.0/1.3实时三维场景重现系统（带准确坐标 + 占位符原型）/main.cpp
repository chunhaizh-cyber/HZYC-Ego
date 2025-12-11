// main.cpp - 数字生命实时三维场景重现系统（D455 + OpenCV Viz）
// 编译命令（Ubuntu）：
/*
sudo apt install librealsense2-dev libopencv-dev libopencv-viz-dev g++
g++ -std=c++17 -O3 main.cpp -o scene_reconstruct -lrealsense2 -lopencv_core -lopencv_highgui -lopencv_viz -pthread
*/

#include <iostream>
#include <vector>
#include <memory>
#include <map>
#include <random>
#include <thread>
#include <chrono>

// RealSense
#include <librealsense2/rs.hpp>

// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/viz.hpp>
#include <opencv2/highgui.hpp>

using namespace std::chrono_literals;

// ==================== 数字生命核心结构 ====================
struct Vector3D {
    double x, y, z;
    Vector3D(double x=0, double y=0, double z=0) : x(x), y(y), z(z) {}
    double distance(const Vector3D& o) const {
        return std::sqrt((x-o.x)*(x-o.x) + (y-o.y)*(y-o.y) + (z-o.z)*(z-o.z));
    }
};

class Existence {
public:
    int id;
    Vector3D position;
    Vector3D size;           // 包围盒尺寸
    Vector3D velocity;
    std::vector<Vector3D> trajectory;
    cv::viz::Color color;
    std::string type;        // "person", "car", "unknown" 等（可后续归纳）
    
    Existence(int i, Vector3D p, Vector3D s) : id(i), position(p), size(s), velocity(0,0,0), type("unknown") {
        trajectory.push_back(p);
        std::random_device rd; std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(100, 255);
        color = cv::viz::Color(dis(gen), dis(gen), dis(gen));
    }
    
    void update(const Vector3D& newPos) {
        velocity = Vector3D(newPos.x - position.x, newPos.y - position.y, newPos.z - position.z);
        position = newPos;
        trajectory.push_back(newPos);
        if (trajectory.size() > 50) trajectory.erase(trajectory.begin());
    }
};

class DigitalLife {
public:
    Vector3D ego_position{0,0,0};  // 自我（车辆）位置
    double safety = 0.7;
    double curiosity = 0.5;
    std::vector<std::shared_ptr<Existence>> existences;
    
    void updateSafetyAndCuriosity() {
        double min_dist = 100.0;
        int approaching = 0;
        for (const auto& e : existences) {
            double d = e->position.distance(ego_position);
            if (d < min_dist) min_dist = d;
            double speed = e->velocity.distance(Vector3D());
            if (speed > 0.05 && d < 5.0) approaching++;
        }
        
        if (min_dist < 1.0) safety -= 0.15;
        else if (min_dist > 3.0) safety += 0.02;
        if (approaching > 0) curiosity += 0.08;
        else curiosity -= 0.02;
        
        safety = std::max(0.0, std::min(1.0, safety));
        curiosity = std::max(0.0, std::min(1.0, curiosity));
    }
};

// ==================== 实时场景重现系统 ====================
class SceneReconstructor {
private:
    rs2::pipeline pipe;
    rs2::pointcloud pc;
    rs2::points points;
    cv::viz::Viz3d window;
    DigitalLife life;
    int frame_id = 0;
    std::map<int, std::shared_ptr<Existence>> tracked_objects;
    int next_id = 0;

public:
    SceneReconstructor() : window("数字生命 - 实时三维场景重现（D455）") {
        rs2::config cfg;
        cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 30);
        cfg.enable_stream(RS2_STREAM_COLOR, 1280, 720, RS2_FORMAT_BGR8, 30);
        auto profile = pipe.start(cfg);
        pc.map_to(profile.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>());
        
        window.setBackgroundColor(cv::viz::Color::black());
        window.showWidget("Coord", cv::viz::WCoordinateSystem(1.0));
        window.showWidget("Camera", cv::viz::WCameraPosition(0.5));
        window.setWindowSize(cv::Size(1600, 900));
    }

    // 简单距离聚类
    std::vector<std::vector<size_t>> clusterPoints(const std::vector<Vector3D>& pts, double threshold = 0.15) {
        std::vector<std::vector<size_t>> clusters;
        std::vector<bool> visited(pts.size(), false);
        
        for (size_t i = 0; i < pts.size(); ++i) {
            if (visited[i]) continue;
            std::vector<size_t> cluster{i};
            visited[i] = true;
            std::vector<size_t> frontier{i};
            
            while (!frontier.empty()) {
                size_t curr = frontier.back(); frontier.pop_back();
                for (size_t j = 0; j < pts.size(); ++j) {
                    if (!visited[j] && pts[curr].distance(pts[j]) < threshold) {
                        visited[j] = true;
                        cluster.push_back(j);
                        frontier.push_back(j);
                    }
                }
            }
            if (cluster.size() > 30) clusters.push_back(cluster); // 过滤噪声
        }
        return clusters;
    }

    void run() {
        std::cout << "实时三维场景重现启动（按 q 退出）\n";
        
        while (cv::waitKey(1) != 'q') {
            auto start = std::chrono::high_resolution_clock::now();
            
            rs2::frameset frames = pipe.wait_for_frames();
            rs2::depth_frame depth = frames.get_depth_frame();
            rs2::video_frame color = frames.get_color_frame();
            if (!depth || !color) continue;
            
            points = pc.calculate(depth);
            pc.map_to(color);
            auto vertices = points.get_vertices();
            auto tex_coords = points.get_texture_coordinates();
            auto color_data = reinterpret_cast<const uint8_t*>(color.get_data());
            int w = color.get_width(), h = color.get_height();
            
            std::vector<Vector3D> cloud;
            for (int i = 0; i < points.size(); i += 10) { // 采样优化性能
                if (vertices[i].z <= 0) continue;
                cloud.emplace_back(vertices[i].x, vertices[i].y, vertices[i].z);
            }
            
            // 聚类得到存在
            auto clusters = clusterPoints(cloud);
            std::vector<std::shared_ptr<Existence>> current_frame_objects;
            
            // 清空上一帧可视化
            window.removeAllWidgets();
            window.showWidget("Coord", cv::viz::WCoordinateSystem(1.0));
            window.showWidget("Camera", cv::viz::WCameraPosition(0.5));
            
            int temp_id = 0;
            for (const auto& cluster : clusters) {
                Vector3D center(0,0,0), min_pt(1e9,1e9,1e9), max_pt(-1e9,-1e9,-1e9);
                for (size_t idx : cluster) {
                    auto& p = cloud[idx];
                    center.x += p.x; center.y += p.y; center.z += p.z;
                    min_pt.x = std::min(min_pt.x, p.x);
                    min_pt.y = std::min(min_pt.y, p.y);
                    min_pt.z = std::min(min_pt.z, p.z);
                    max_pt.x = std::max(max_pt.x, p.x);
                    max_pt.y = std::max(max_pt.y, p.y);
                    max_pt.z = std::max(max_pt.z, p.z);
                }
                center.x /= cluster.size(); center.y /= cluster.size(); center.z /= cluster.size();
                Vector3D size(max_pt.x-min_pt.x, max_pt.y-min_pt.y, max_pt.z-min_pt.z);
                
                // 简单匹配：找最近的历史存在
                std::shared_ptr<Existence> matched = nullptr;
                double best_dist = 1.0;
                for (auto& prev : life.existences) {
                    double d = prev->position.distance(center);
                    if (d < best_dist && d < 0.5) {
                        best_dist = d;
                        matched = prev;
                    }
                }
                
                if (matched) {
                    matched->update(center);
                    current_frame_objects.push_back(matched);
                } else {
                    auto new_obj = std::make_shared<Existence>(next_id++, center, size);
                    current_frame_objects.push_back(new_obj);
                }
                
                // 可视化：根据尺寸选择原型
                auto obj = current_frame_objects.back();
                cv::viz::Widget widget;
                if (size.z > size.x * 1.8) {
                    // 立着的 → 可能是人
                    widget = cv::viz::WCylinder(cv::Point3d(obj->position.x, obj->position.y, obj->position.z - size.z/2),
                                               cv::Point3d(obj->position.x, obj->position.y, obj->position.z + size.z/2),
                                               size.x/2, 20, obj->color);
                } else if (size.x > 1.5 && size.y > 1.0) {
                    // 长方形 → 可能是车
                    widget = cv::viz::WCube(cv::Point3d(obj->position.x - size.x/2, obj->position.y - size.y/2, obj->position.z - size.z/2),
                                           cv::Point3d(obj->position.x + size.x/2, obj->position.y + size.y/2, obj->position.z + size.z/2),
                                           true, obj->color);
                } else {
                    // 默认球体
                    widget = cv::viz::WSphere(cv::Point3d(obj->position.x, obj->position.y, obj->position.z),
                                             std::max({size.x, size.y, size.z})/2, 20, obj->color);
                }
                window.showWidget("Obj_" + std::to_string(obj->id), widget);
            }
            
            life.existences = current_frame_objects;
            life.updateSafetyAndCuriosity();
            
            // 信息面板
            std::cout << "\r帧:" << ++frame_id 
                      << " | 存在:" << life.existences.size()
                      << " | 安全度:" << std::fixed << std::setprecision(2) << life.safety
                      << " | 好奇度:" << life.curiosity << std::flush;
            
            window.spinOnce(1, true);
            auto end = std::chrono::high_resolution_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
            if (ms < 16) std::this_thread::sleep_for(std::chrono::milliseconds(16-ms)); // 稳定60fps
        }
    }
};

int main() {
    try {
        SceneReconstructor recon;
        recon.run();
    } catch (const rs2::error& e) {
        std::cerr << "RealSense错误: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
    return 0;
}