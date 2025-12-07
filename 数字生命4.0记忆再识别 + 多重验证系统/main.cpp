// 在您现有的 main.cpp 中替换/新增以下内容
// 核心升级：记忆再识别 + 多重特征验证 + 置信度系统

// ==================== 升级版记忆特征（支持多重验证）===================
struct ExistenceMemory {
    int id;
    std::wstring code = L"";                    // 抽象存在树编码（如"001"=妈妈）
    Vector3D world_position;                    // 最后一次世界坐标
    Vector3D avg_size;                          // 平均尺寸
    std::vector<Vector3D> trajectory;           // 世界坐标轨迹（最多100帧）
    std::vector<std::vector<int64_t>> contour_history; // 轮廓特征历史
    std::vector<Vector3D> velocity_history;     // 速度历史
    double first_seen = 0, last_seen = 0;
    int appearance_count = 0;
    double recognition_confidence = 0.0;        // 再识别置信度（0-1）
    cv::viz::Color color;
    
    ExistenceMemory(int i) : id(i), recognition_confidence(0.0) {
        std::random_device rd; std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(100, 255);
        color = cv::viz::Color(dis(gen), dis(gen), dis(gen));
    }
    
    // 更新记忆（每次看到时）
    void update(const Vector3D& pos, const std::vector<int64_t>& contour, double time) {
        world_position = pos;
        last_seen = time;
        appearance_count++;
        recognition_confidence = std::min(1.0, recognition_confidence + 0.05);
        
        trajectory.push_back(pos);
        contour_history.push_back(contour);
        if (trajectory.size() > 100) trajectory.erase(trajectory.begin());
        if (contour_history.size() > 100) contour_history.erase(contour_history.begin());
        
        // 更新平均尺寸和速度
        avg_size = Vector3D(0.3,0.3,0.3); // 可从聚类计算
        if (trajectory.size() >= 2) {
            auto v = trajectory.back() - trajectory[trajectory.size()-2];
            velocity_history.push_back(v);
            if (velocity_history.size() > 50) velocity_history.erase(velocity_history.begin());
        }
    }
    
    // 多重特征相似度评分（0-1）
    double multiFeatureSimilarity(const Vector3D& pos, const std::vector<int64_t>& contour) const {
        double score = 0.0;
        double weight_sum = 0.0;
        
        // 1. 位置相似度（世界坐标）
        double pos_dist = world_position.distance(pos);
        double pos_score = 1.0 / (1.0 + pos_dist * 2.0); // 0.5米内接近1
        score += pos_score * 0.4;
        weight_sum += 0.4;
        
        // 2. 轮廓相似度
        if (!contour_history.empty()) {
            double contour_sim = contourSimilarity(contour_history.back(), contour);
            score += contour_sim * 0.3;
            weight_sum += 0.3;
        }
        
        // 3. 尺寸相似度
        double size_diff = avg_size.distance(Vector3D(0.3,0.3,0.3)); // 简化
        double size_score = 1.0 / (1.0 + size_diff * 10);
        score += size_score * 0.2;
        weight_sum += 0.2;
        
        // 4. 动态行为相似度（速度）
        if (!velocity_history.empty()) {
            Vector3D avg_vel(0,0,0);
            for (const auto& v : velocity_history) avg_vel = avg_vel + v;
            avg_vel.x /= velocity_history.size(); avg_vel.y /= velocity_history.size(); avg_vel.z /= velocity_history.size();
            double vel_diff = avg_vel.distance(Vector3D(0,0,0));
            double vel_score = 1.0 / (1.0 + vel_diff * 5);
            score += vel_score * 0.1;
            weight_sum += 0.1;
        }
        
        return score / weight_sum;
    }
    
private:
    double contourSimilarity(const std::vector<int64_t>& a, const std::vector<int64_t>& b) const {
        if (a.size() != b.size()) return 0.0;
        double sum = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            sum += 1.0 / (1.0 + std::abs(a[i] - b[i]) / 1000.0);
        }
        return sum / a.size();
    }
};

// ==================== 全局记忆库（支持再识别）===================
class GlobalMemoryBank {
public:
    std::map<int, std::shared_ptr<ExistenceMemory>> memory;
    int next_id = 1;
    double current_time = 0;
    
    std::shared_ptr<ExistenceMemory> recognizeOrCreate(const Vector3D& ego_pos, const std::vector<int64_t>& contour) {
        current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
        
        Vector3D world_pos = world_coords.egoToWorld(ego_pos);
        double best_score = 0.75;  // 再识别阈值
        std::shared_ptr<ExistenceMemory> best_match = nullptr;
        
        for (auto& pair : memory) {
            auto& mem = pair.second;
            double score = mem->multiFeatureSimilarity(world_pos, contour);
            if (score > best_score && (current_time - mem->last_seen) < 30.0) { // 30秒内有效
                best_score = score;
                best_match = mem;
            }
        }
        
        if (best_match) {
            best_match->update(ego_pos, contour, current_time);
            best_match->recognition_confidence = best_score;
            std::cout << "【再识别成功！】ID:" << best_match->id 
                      << " 置信度:" << best_score 
                      << " 出现:" << best_match->appearance_count << "次\n";
            return best_match;
        } else {
            auto new_mem = std::make_shared<ExistenceMemory>(next_id++);
            new_mem->update(ego_pos, contour, current_time);
            memory[new_mem->id] = new_mem;
            std::cout << "【发现新存在】分配ID:" << new_mem->id << "\n";
            return new_mem;
        }
    }
};

GlobalMemoryBank global_memory_bank;

// ==================== 主循环（带再识别）===================
void run() {
    std::cout << "数字生命 v4.0 启动：记忆再识别 + 多重验证（按 q 退出）\n";
    
    while (cv::waitKey(1) != 'q') {
        auto start = std::chrono::high_resolution_clock::now();
        
        rs2::frameset frames = pipe.wait_for_frames();
        rs2::depth_frame depth = frames.get_depth_frame();
        rs2::video_frame color = frames.get_color_frame();
        if (!depth || !color) continue;
        
        points = pc.calculate(depth);
        pc.map_to(color);
        auto vertices = points.get_vertices();
        
        std::vector<Vector3D> cloud;
        std::vector<std::vector<int64_t>> contours;
        for (int i = 0; i < points.size(); i += 10) {
            if (vertices[i].z <= 0) continue;
            Vector3D p(vertices[i].x, vertices[i].y, vertices[i].z);
            cloud.push_back(p);
            
            // 模拟轮廓特征
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
            
            // 关键：记忆再识别（多重特征验证）
            auto memory = global_memory_bank.recognizeOrCreate(center, contours[temp_id++]);
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
            
            // 距离+置信度标注
            double dist = center.distance(Vector3D(0,0,0));
            std::string text = "ID:" + std::to_string(memory->id) + 
                              " Dist:" + std::to_string(dist).substr(0,4) + "m" +
                              " Conf:" + std::to_string(memory->recognition_confidence).substr(0,4) +
                              " Seen:" + std::to_string(memory->appearance_count);
            
            cv::viz::WText3D label(text,
                cv::Point3d(center.x, center.y, center.z + 0.4),
                0.08, true, memory->recognition_confidence > 0.9 ? cv::viz::Color::green() : cv::viz::Color::yellow());
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