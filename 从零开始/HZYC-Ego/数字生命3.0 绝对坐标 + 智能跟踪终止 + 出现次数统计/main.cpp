// 在您现有的 main.cpp 中，替换/新增以下内容
// 关键升级：绝对坐标系统 + 智能跟踪终止 + 出现次数

// ==================== 全局绝对坐标系管理 ====================
class WorldCoordinateSystem {
public:
    Vector3D origin{0, 0, 0};                    // 世界原点（可由SLAM初始化）
    Vector3D gravity_axis{0, 0, 1};             // 重力方向（IMU校正）
    double last_update_time = 0;
    
    // 设置世界原点（例如：启动时相机位置）
    void setOrigin(const Vector3D& pos, double time) {
        origin = pos;
        last_update_time = time;
        std::cout << "世界坐标系建立，原点: (" 
                  << pos.x << ", " << pos.y << ", " << pos.z << ")\n";
    }
    
    // 世界坐标 → 自我坐标（减去原点）
    Vector3D worldToEgo(const Vector3D& world_pos) const {
        return Vector3D(world_pos.x - origin.x, world_pos.y - origin.y, world_pos.z - origin.z);
    }
    
    // 自我坐标 → 世界坐标
    Vector3D egoToWorld(const Vector3D& ego_pos) const {
        return Vector3D(ego_pos.x + origin.x, ego_pos.y + origin.y, ego_pos.z + origin.z);
    }
};

WorldCoordinateSystem world_coords;  // 全局世界坐标系

// ==================== 升级版存在特征库（支持绝对坐标 + 出现次数 + 智能终止）===================
struct ExistenceFeature {
    int id;
    Vector3D world_position;              // 绝对世界坐标（核心！）
    Vector3D last_ego_position;           // 上一次在自我坐标系中的位置（用于匹配）
    Vector3D size;
    Vector3D velocity;
    std::vector<Vector3D> trajectory;     // 世界坐标轨迹
    std::vector<std::vector<int64_t>> contour_history;
    double last_seen_time = 0;
    double first_seen_time = 0;
    int appearance_count = 0;             // 出现次数（关键！）
    double confidence = 1.0;
    bool is_active = true;                // 是否仍在跟踪
    std::string concept_code = "";
    cv::viz::Color color;
    
    ExistenceFeature(int i, const Vector3D& world_pos) : id(i), world_position(world_pos), last_ego_position(world_pos) {
        first_seen_time = last_seen_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
        appearance_count = 1;
        trajectory.push_back(world_pos);
        
        std::random_device rd; std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(100, 255);
        color = cv::viz::Color(dis(gen), dis(gen), dis(gen));
    }
    
    void update(const Vector3D& new_ego_pos, const std::vector<int64_t>& contour, double time) {
        Vector3D new_world_pos = world_coords.egoToWorld(new_ego_pos);
        
        velocity = Vector3D(
            (new_world_pos.x - world_position.x) / (time - last_seen_time + 0.001),
            (new_world_pos.y - world_position.y) / (time - last_seen_time + 0.001),
          (new_world_pos.z - world_position.z) / (time - last_seen_time + 0.001)
        );
        
        world_position = new_world_pos;
        last_ego_position = new_ego_pos;
        last_seen_time = time;
        appearance_count++;
        confidence = std::min(1.0, confidence + 0.05);
        
        trajectory.push_back(new_world_pos);
        contour_history.push_back(contour);
        if (trajectory.size() > 50) trajectory.erase(trajectory.begin());
        if (contour_history.size() > 50) contour_history.erase(contour_history.begin());
        
        is_active = true;
    }
    
    // 判断是否长时间未更新（终止跟踪）
    bool shouldTerminate(double current_time, double timeout = 3.0) const {
        if (!is_active) return false;
        return (current_time - last_seen_time) > timeout;
    }
    
    void terminate() {
        is_active = false;
        std::cout << "存在 ID:" << id << " 长时间未见，跟踪终止（出现 " << appearance_count << " 次）\n";
    }
};

// ==================== 全局记忆库升级版 ====================
class ExistenceMemory {
public:
    std::map<int, std::shared_ptr<ExistenceFeature>> database;
    int next_id = 1;
    double current_time = 0;
    
    std::shared_ptr<ExistenceFeature> getOrCreate(const Vector3D& ego_pos, const std::vector<int64_t>& contour) {
        current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
        
        // 清理长时间未见的存在
        for (auto it = database.begin(); it != database.end();) {
            if (it->second->shouldTerminate(current_time)) {
                it->second->terminate();
                it = database.erase(it);
            } else {
                ++it;
            }
        }
        
        Vector3D world_pos = world_coords.egoToWorld(ego_pos);
        double best_score = 0.7;
        std::shared_ptr<ExistenceFeature> best_match = nullptr;
        
        for (auto& pair : database) {
            auto& mem = pair.second;
            double pos_dist = mem->world_position.distance(world_pos);
            double contour_sim = contourSimilarity(mem->contour_history.back(), contour);
            double time_weight = 1.0 / (1.0 + (current_time - mem->last_seen_time));
            double score = (1.0 / (1.0 + pos_dist)) * contour_sim * time_weight * mem->confidence;
            
            if (score > best_score && pos_dist < 1.5) {
                best_score = score;
                best_match = mem;
            }
        }
        
        if (best_match) {
            best_match->update(ego_pos, contour, current_time);
            return best_match;
        } else {
            auto new_mem = std::make_shared<ExistenceFeature>(next_id++, world_pos);
            new_mem->update(ego_pos, contour, current_time);
            database[new_mem->id] = new_mem;
            std::cout << "发现新存在！分配ID:" << new_mem->id << " 世界坐标:(" 
                      << world_pos.x << "," << world_pos.y << "," << world_pos.z << ")\n";
            return new_mem;
        }
    }
    
private:
    double contourSimilarity(const std::vector<int64_t>& a, const std::vector<int64_t>& b) {
        if (a.size() != b.size()) return 0.0;
        double sum = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            sum += 1.0 / (1.0 + std::abs(a[i] - b[i]) / 1000.0);
        }
        return sum / a.size();
    }
};

ExistenceMemory global_memory;

// ==================== 主循环（绝对坐标 + 智能跟踪终止）===================
void run() {
    std::cout << "数字生命 v3.0 启动：绝对世界坐标 + 智能跟踪终止 + 出现次数统计（按 l 锁定，按 q 退出）\n";
    
    // 启动时建立世界坐标系（以第一帧相机位置为原点）
    bool world_origin_set = false;
    Vector3D first_camera_pos;
    
    while (cv::waitKey(1) != 'q') {
        auto start = std::chrono::high_resolution_clock::now();
        double current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
        
        rs2::frameset frames = pipe.wait_for_frames();
        rs2::depth_frame depth = frames.get_depth_frame();
        rs2::video_frame color = frames.get_color_frame();
        if (!depth || !color) continue;
        
        // 设置世界原点（第一帧）
        if (!world_origin_set) {
            first_camera_pos = Vector3D(0,0,0); // 相机初始位置
            world_coords.setOrigin(first_camera_pos, current_time);
            world_origin_set = true;
        }
        
        points = pc.calculate(depth);
        pc.map_to(color);
        auto vertices = points.get_vertices();
        
        std::vector<Vector3D> cloud;
        std::vector<std::vector<int64_t>> contours;
        for (int i = 0; i < points.size(); i += 10) {
            if (vertices[i].z <= 0) continue;
            Vector3D p(vertices[i].x, vertices[i].y, vertices[i].z);
            cloud.push_back(p);
            
            // 模拟轮廓特征（实际可用Canny+SIFT）
            std::vector<int64_t> fake_contour(81);
            std::generate(fake_contour.begin(), fake_contour.end(), []() { return std::rand() % 2000 - 1000; });
            contours.push_back(fake_contour);
        }
        
        auto clusters = clusterPoints(cloud);
        currentExistences.clear();
        
        window.removeAllWidgets();
        window.showWidget("Coord", cv::viz::WCoordinateSystem(2.0));
        window.showWidget("Camera", cv::viz::WCameraPosition(0.5));
        
        int temp_id = 0;
        for (const auto& cluster : clusters) {
            Vector3D center(0,0,0);
            for (size_t idx : cluster) {
                center.x += cloud[idx].x;
                center.y += cloud[idx].y;
                center.z += cloud[idx].z;
            }
            center.x /= cluster.size(); center.y /= cluster.size(); center.z /= cluster.size();
            
            // 关键：使用绝对坐标进行记忆与匹配
            auto memory = global_memory.getOrCreate(center, contours[temp_id++], current_time);
            currentExistences.push_back(memory);
            
            // 可视化
            Vector3D size(0.3,0.3,0.3);
            if (cluster.size() > 100) size = Vector3D(0.6,0.6,1.8);
            if (cluster.size() > 500) size = Vector3D(2.0,1.0,1.5);
            
            cv::viz::Widget widget;
            if (size.z > size.x * 1.8) {
                widget = cv::viz::WCylinder(
                    cv::Point3d(center.x, center.y, center.z - size.z/2),
                    cv::Point3d(center.x, center.y, center.z + size.z/2),
                    size.x/2, 20, memory->color);
            } else {
                widget = cv::viz::WCube(
                    cv::Point3d(center.x - size.x/2, center.y - size.y/2, center.z - size.z/2),
                    cv::Point3d(center.x + size.x/2, center.y + size.y/2, center.z + size.z/2),
                    true, memory->color);
            }
            window.showWidget("Obj_" + std::to_string(memory->id), widget);
            
            // 绝对距离标注（与相机）
            double dist = center.distance(Vector3D(0,0,0));
            std::string text = "ID:" + std::to_string(memory->id) + 
                              " 距离:" + std::to_string(dist).substr(0,4) + "m" +
                              " 出现:" + std::to_string(memory->appearance_count) + "次" +
                              (memory->is_active ? "" : " [已消失]");
            
            cv::viz::WText3D label(text,
                cv::Point3d(center.x, center.y, center.z + 0.4),
                0.08, true, cv::viz::Color::white());
            window.showWidget("Label_" + std::to_string(memory->id), label);
        }
        
        life.updateSafetyAndCuriosity();
        std::cout << "\r帧:" << ++frame_id 
                  << " | 存在:" << life.existences.size()
                  << " | 安全度:" << std::fixed << std::setprecision(2) << life.safety
                  << " | 好奇度:" << life.curiosity << std::flush;
        
        window.spinOnce(1, true);
    }
}