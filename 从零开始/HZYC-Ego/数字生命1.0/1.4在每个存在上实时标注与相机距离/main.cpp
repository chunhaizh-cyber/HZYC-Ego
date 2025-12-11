// 在您原有的 main.cpp 中，只需替换 run() 函数和相关部分即可
// 关键新增：cv::viz::WText3D 文字标注 + 距离计算

void run() {
    std::cout << "数字生命启动 - 实时三维场景重现 + 距离标注（按 q 退出）\n";

    // 初始相机位置（D455 坐标系原点）
    Vector3D camera_pos(0, 0, 0);

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
        for (int i = 0; i < points.size(); i += 10) { // 10% 采样
            if (vertices[i].z <= 0 || std::isnan(vertices[i].z)) continue;
            cloud.emplace_back(vertices[i].x, vertices[i].y, vertices[i].z);
        }

        auto clusters = clusterPoints(cloud);
        currentExistences.clear();

        // 清空上一帧所有部件
        window.removeAllWidgets();
        window.showWidget("Coord", cv::viz::WCoordinateSystem(1.0));
        window.showWidget("Camera", cv::viz::WCameraPosition(0.5));

        int temp_id = 0;
        for (const auto& cluster : clusters) {
            Vector3D center(0,0,0), min_pt(1e9,1e9,1e9), max_pt(-1e9,-1e9,-1e9);
            for (size_t idx : cluster) {
                auto& p = cloud[idx];
                center.x += p.x; center.y += p.y; center.z += p.z;
                min_pt = Vector3D(std::min(min_pt.x, p.x), std::min(min_pt.y, p.y), std::min(min_pt.z, p.z));
                max_pt = Vector3D(std::max(max_pt.x, p.x), std::max(max_pt.y, p.y), std::max(max_pt.z, p.z));
            }
            center.x /= cluster.size(); center.y /= cluster.size(); center.z /= cluster.size();
            Vector3D size(max_pt.x-min_pt.x, max_pt.y-min_pt.y, max_pt.z-min_pt.z);

            // 计算与相机的真实距离（米）
            double distance = center.distance(camera_pos);
            std::string dist_text = "ID:" + std::to_string(next_id) + "  距离:" + 
                                  std::to_string(distance).substr(0, 4) + "m";

            // 匹配或新建存在
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
            } else {
                matched = std::make_shared<Existence>(next_id++, center, size);
                life.existences.push_back(matched);
            }

            currentExistences.push_back(matched);

            // 选择合适的占位符原型
            cv::viz::Widget widget;
            if (size.z > size.x * 1.8 && size.z > 1.0) {
                // 立着的 → 人
                widget = cv::viz::WCylinder(
                    cv::Point3d(center.x, center.y, center.z - size.z/2),
                    cv::Point3d(center.x, center.y, center.z + size.z/2),
                    size.x/2, 20, matched->color);
            } else if (size.x > 1.5 && size.y > 1.0) {
                // 长方形 → 车
                widget = cv::viz::WCube(
                    cv::Point3d(center.x - size.x/2, center.y - size.y/2, center.z - size.z/2),
                    cv::Point3d(center.x + size.x/2, center.y + size.y/2, center.z + size.z/2),
                    true, matched->color);
            } else {
                // 其他 → 球体
                widget = cv::viz::WSphere(
                    cv::Point3d(center.x, center.y, center.z),
                    std::max({size.x, size.y, size.z})/2, 20, matched->color);
            }
            window.showWidget("Obj_" + std::to_string(matched->id), widget);

            // 关键：实时距离标注（朝向屏幕，永不遮挡）
            cv::viz::WText3D text(dist_text,
                cv::Point3d(center.x, center.y, center.z + std::max({size.x, size.y, size.z})/2 + 0.1),
                0.08, true, cv::viz::Color::white()); // 白色文字，billboard
            window.showWidget("Label_" + std::to_string(matched->id), text);
        }

        life.updateSafetyAndCuriosity();

        std::cout << "\r帧:" << ++frame_id 
                  << " | 存在:" << life.existences.size()
                  << " | 安全度:" << std::fixed << std::setprecision(2) << life.safety
                  << " | 好奇度:" << life.curiosity << std::flush;

        window.spinOnce(1, true);
        auto end = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
        if (ms < 16) std::this_thread::sleep_for(16ms - std::chrono::milliseconds(ms));
    }
}