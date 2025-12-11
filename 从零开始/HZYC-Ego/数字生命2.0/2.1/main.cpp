// main.cpp - 数字生命 + RealSense D455 + OpenCV 实时3D点云可视化
// 编译命令（Linux）：
// g++ -std=c++17 -O3 main.cpp -o digital_life_viz -lrealsense2 -lopencv_core -lopencv_highgui -lopencv_viz

#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <algorithm>
#include <map>
#include <chrono>
#include <thread>

// RealSense
#include <librealsense2/rs.hpp>

// OpenCV
#include <opencv2/opencv.hpp>
#include <opencv2/viz.hpp>
#include <opencv2/highgui.hpp>

// 您的数字生命核心结构（保持不变）
struct Vector3D {
    double x, y, z;
    Vector3D(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    double distance(const Vector3D& other) const {
        return std::sqrt((x-other.x)*(x-other.x) + (y-other.y)*(y-other.y) + (z-other.z)*(z-other.z));
    }
    Vector3D operator-(const Vector3D& other) const {
        return Vector3D(x-other.x, y-other.y, z-other.z);
    }
};

class ExistenceNode {
private:
    int id;
    Vector3D position;
    Vector3D dimensions;
    double distanceToSelf;
    int pixelArea;
    std::vector<Vector3D> trajectory;
    cv::viz::Color color; // 为每个存在分配颜色

public:
    ExistenceNode(int id, const Vector3D& pos, const Vector3D& dims, double dist, int area)
        : id(id), position(pos), dimensions(dims), distanceToSelf(dist), pixelArea(area) {
        trajectory.push_back(pos);
        // 随机颜色（更醒目）
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.3, 1.0);
        color = cv::viz::Color(dis(gen)*255, dis(gen)*255, dis(gen)*255);
    }

    int getId() const { return id; }
    Vector3D getPosition() const { return position; }
    Vector3D getDimensions() const { return dimensions; }
    double getDistanceToSelf() const { return distanceToSelf; }
    const cv::viz::Color& getColor() const { return color; }

    void updatePosition(const Vector3D& newPos) {
        position = newPos;
        trajectory.push_back(newPos);
    }

    Vector3D getVelocity() const {
        if (trajectory.size() < 2) return Vector3D(0,0,0);
        const auto& current = trajectory.back();
        const auto& previous = trajectory[trajectory.size()-2];
        return Vector3D(current.x-previous.x, current.y-previous.y, current.z-previous.z);
    }
};

class SelfNode {
private:
    Vector3D position = Vector3D(0,0,0);
    double safetyLevel = 0.5;
    double curiosityLevel = 0.5;

public:
    Vector3D getPosition() const { return position; }
    double getSafetyLevel() const { return safetyLevel; }
    double getCuriosityLevel() const { return curiosityLevel; }
    void setSafetyLevel(double v) { safetyLevel = std::max(0.0, std::min(1.0, v)); }
    void setCuriosityLevel(double v) { curiosityLevel = std::max(0.0, std::min(1.0, v)); }
};

// 数字生命系统（接入 D455 + OpenCV 可视化）
class DigitalLifeSystem {
private:
    std::shared_ptr<SelfNode> self;
    std::vector<std::shared_ptr<ExistenceNode>> currentExistences;
    int frameCount = 0;

    // RealSense
    rs2::pipeline pipe;
    rs2::config cfg;
    rs2::pointcloud pc;
    rs2::points points;

    // OpenCV 3D 可视化窗口
    cv::viz::Viz3d viz_window;
    bool show_window = true;

public:
    DigitalLifeSystem() : self(std::make_shared<SelfNode>()), viz_window("数字生命 - RealSense D455 实时点云") {
        // D455 配置：深度 + 颜色（用于点云上色）
        cfg.enable_stream(RS2_STREAM_DEPTH, 848, 480, RS2_FORMAT_Z16, 30);
        cfg.enable_stream(RS2_STREAM_COLOR, 1280, 720, RS2_FORMAT_BGR8, 30);
        auto profile = pipe.start(cfg);

        // 点云映射（深度→3D + 颜色）
        pc.map_to(profile.get_stream(RS2_STREAM_COLOR).as<rs2::video_stream_profile>());

        // 设置可视化窗口
        viz_window.setBackgroundColor(cv::viz::Color::black());
        viz_window.showWidget("Coordinate", cv::viz::WCoordinateSystem(1.0));
        viz_window.showWidget("Camera", cv::viz::WCameraPosition(0.5));
        viz_window.setWindowSize(cv::Size(1280, 720));
        viz_window.spinOnce(1, true);
    }

    ~DigitalLifeSystem() {
        pipe.stop();
    }

    // 实时获取 D455 点云（带颜色）
    std::vector<std::pair<Vector3D, cv::Vec3b>> getRealPointCloud() {
        std::vector<std::pair<Vector3D, cv::Vec3b>> colored_points;

        rs2::frameset frames = pipe.wait_for_frames();
        rs2::depth_frame depth = frames.get_depth_frame();
        rs2::video_frame color = frames.get_color_frame();

        if (!depth || !color) return colored_points;

        // 生成点云
        points = pc.calculate(depth);
        pc.map_to(color);

        const rs2::vertex* vertices = points.get_vertices();
        const rs2::texture_coordinate* tex_coords = points.get_texture_coordinates();
        int width = color.get_width();
        int height = color.get_height();
        auto color_data = (const uint8_t*)color.get_data();

        for (int i = 0; i < points.size(); ++i) {
            if (vertices[i].z <= 0 || std::isnan(vertices[i].z)) continue;

            Vector3D point(vertices[i].x, vertices[i].y, vertices[i].z);

            // 获取颜色
            int tx = std::min(std::max(0, (int)(tex_coords[i].u * width)), width - 1);
            int ty = std::min(std::max(0, (int)(tex_coords[i].v * height)), height - 1);
            int idx = ty * width * 3 + tx * 3;
            cv::Vec3b color_bgr(color_data[idx+2], color_data[idx+1], color_data[idx]);

            colored_points.emplace_back(point, color_bgr);
        }

        return colored_points;
    }

    // 简单聚类（基于距离）
    std::vector<std::vector<size_t>> simpleClustering(const std::vector<std::pair<Vector3D, cv::Vec3b>>& points, double threshold = 0.15) {
        std::vector<std::vector<size_t>> clusters;
        std::vector<bool> visited(points.size(), false);

        for (size_t i = 0; i < points.size(); ++i) {
            if (visited[i]) continue;

            std::vector<size_t> cluster;
            std::vector<size_t> queue{i};
            visited[i] = true;

            while (!queue.empty()) {
                size_t idx = queue.back(); queue.pop_back();
                cluster.push_back(idx);

                for (size_t j = 0; j < points.size(); ++j) {
                    if (visited[j]) continue;
                    if (points[idx].first.distance(points[j].first) < threshold) {
                        queue.push_back(j);
                        visited[j] = true;
                    }
                }
            }
            if (cluster.size() > 20) clusters.push_back(cluster);
        }
        return clusters;
    }

    // 主循环（实时可视化）
    void run() {
        std::cout << "数字生命启动 - RealSense D455 + OpenCV 3D可视化 (按 'q' 退出)\n";

        while (show_window) {
            auto start = std::chrono::high_resolution_clock::now();

            // 获取带颜色的点云
            auto colored_points = getRealPointCloud();
            if (colored_points.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            // 聚类得到存在
            auto clusters = simpleClustering(colored_points);
            currentExistences.clear();

            // 可视化：清空上一帧
            viz_window.removeAllWidgets();
            viz_window.showWidget("Coordinate", cv::viz::WCoordinateSystem(1.0));
            viz_window.showWidget("Camera", cv::viz::WCameraPosition(0.5));

            int id = 0;
            for (const auto& cluster : clusters) {
                // 计算质心
                Vector3D center(0,0,0);
                cv::Vec3b avg_color(0,0,0);
                for (size_t idx : cluster) {
                    center.x += colored_points[idx].first.x;
                    center.y += colored_points[idx].first.y;
                    center.z += colored_points[idx].first.z;
                    avg_color += colored_points[idx].second;
                }
                center.x /= cluster.size();
                center.y /= cluster.size();
                center.z /= cluster.size();
                avg_color /= (int)cluster.size();

                auto existence = std::make_shared<ExistenceNode>(id++, center, Vector3D(0.3,0.3,0.3), 
                    center.distance(self->getPosition()), cluster.size());

                currentExistences.push_back(existence);

                // OpenCV Viz 显示包围盒
                cv::viz::WCube cube(cv::Point3d(center.x-0.15, center.y-0.15, center.z-0.15),
                                   cv::Point3d(center.x+0.15, center.y+0.15, center.z+0.15),
                                   true, cv::viz::Color(avg_color[2], avg_color[1], avg_color[0]));
                viz_window.showWidget("Cube" + std::to_string(id), cube);
            }

            // 更新安全度与好奇度
            updateSafetyAndCuriosity();

            // 显示信息
            std::cout << "\r帧: " << ++frameCount 
                      << " | 存在数量: " << currentExistences.size()
                      << " | 安全度: " << std::fixed << std::setprecision(2) << self->getSafetyLevel()
                      << " | 好奇度: " << self->getCuriosityLevel() << std::flush;

            // OpenCV 渲染
            viz_window.spinOnce(1, true);
            if (cv::waitKey(1) == 'q') break;
        }
    }

private:
    void updateSafetyAndCuriosity() {
        double min_dist = 10.0;
        int approaching = 0;
        for (const auto& e : currentExistences) {
            double d = e->getDistanceToSelf();
            if (d < min_dist) min_dist = d;
            Vector3D vel = e->getVelocity();
            if (vel.x*vel.x + vel.y*vel.y + vel.z*vel.z > 0.01) approaching++;
        }

        if (min_dist < 1.0) self->setSafetyLevel(self->getSafetyLevel() - 0.1);
        else if (min_dist > 3.0) self->setSafetyLevel(self->getSafetyLevel() + 0.02);
        if (approaching > 2) self->setCuriosityLevel(self->getCuriosityLevel() + 0.05);
        else self->setCuriosityLevel(self->getCuriosityLevel() - 0.01);

        self->setSafetyLevel(std::max(0.0, std::min(1.0, self->getSafetyLevel())));
        self->setCuriosityLevel(std::max(0.0, std::min(1.0, self->getCuriosityLevel())));
    }
};

int main() {
    try {
        DigitalLifeSystem system;
        system.run();
    }
    catch (const rs2::error& e) {
        std::cerr << "RealSense错误: " << e.what() << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
    }
    return 0;
}