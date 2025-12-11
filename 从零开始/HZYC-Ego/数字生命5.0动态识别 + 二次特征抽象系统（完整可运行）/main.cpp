// 在您现有 main.cpp 基础上新增/替换以下内容
// 核心：状态 → 最小动态 → 二次特征 → 动态概念编码

// ==================== 1. 带时间戳的状态 ====================
struct TimedState {
    double timestamp;                          // 秒（高精度）
    Vector3D position;                         // 世界坐标
    std::vector<int64_t> contour;              // N=9 轮廓特征（L=3, 81个）
    Vector3D velocity;                         // 当前速度（可计算）
    
    TimedState(double t, const Vector3D& p, const std::vector<int64_t>& c)
        : timestamp(t), position(p), contour(c), velocity(0,0,0) {}
};

// ==================== 2. 最小动态（相邻状态差分）===================
struct MinimalDynamic {
    double start_time;
    double duration;
    Vector3D displacement;                     // Δposition
    Vector3D avg_velocity;
    std::vector<int64_t> contour_change;       // 轮廓变化趋势（简化版）
    
    MinimalDynamic(const TimedState& s1, const TimedState& s2) 
        : start_time(s1.timestamp), duration(s2.timestamp - s1.timestamp) {
        displacement = s2.position - s1.position;
        avg_velocity = displacement / duration;
        
        // 轮廓变化（取平均差）
        contour_change.resize(s1.contour.size());
        for (size_t i = 0; i < s1.contour.size(); ++i) {
            contour_change[i] = s2.contour[i] - s1.contour[i];
        }
    }
};

// ==================== 3. 二次特征抽象器（忽略微小扰动）===================
class SecondaryFeatureExtractor {
public:
    // 量化位数（控制抽象程度）
    static constexpr int QUANT_BITS = 6;       // 6位量化 → 64个桶，忽略微小扰动
    static constexpr double MAX_VEL = 5.0;     // 最大速度5m/s（自动驾驶场景）
    static constexpr double MAX_DISP = 2.0;    // 单帧最大位移2米
    
    // 生成二次特征指纹（64位整数）
    static uint64_t generateFingerprint(const MinimalDynamic& dyn) {
        uint64_t fingerprint = 0;
        
        // 速度方向量化（8个方向）
        double angle = std::atan2(dyn.avg_velocity.y, dyn.avg_velocity.x);
        int dir_bucket = static_cast<int>((angle + M_PI) / (M_PI / 4)) % 8;
        fingerprint |= (static_cast<uint64_t>(dir_bucket) << 56);
        
        // 速度大小量化（6位）
        double speed = dyn.avg_velocity.distance(Vector3D());
        int speed_bucket = static_cast<int>(std::min(speed / MAX_VEL, 1.0) * 63);
        fingerprint |= (static_cast<uint64_t>(speed_bucket) << 50);
        
        // 位移大小量化
        double disp = dyn.displacement.distance(Vector3D());
        int disp_bucket = static_cast<int>(std::min(disp / MAX_DISP, 1.0) * 63);
        fingerprint |= (static_cast<uint64_t>(disp_bucket) << 44);
        
        // 轮廓变化主成分（取前3个最大变化）
        std::vector<std::pair<int64_t, size_t>> changes;
        for (size_t i = 0; i < dyn.contour_change.size(); ++i) {
            changes.emplace_back(std::abs(dyn.contour_change[i]), i);
        }
        std::sort(changes.rbegin(), changes.rend());
        for (int i = 0; i < 3 && i < changes.size(); ++i) {
            int idx = changes[i].second;
            int bucket = static_cast<int>((dyn.contour_change[idx] + 1000) / 2000.0 * 63);
            fingerprint |= (static_cast<uint64_t>(bucket) << (42 - i*6));
        }
        
        return fingerprint;
    }
    
    // 指纹相似度（汉明距离）
    static double fingerprintSimilarity(uint64_t a, uint64_t b) {
        uint64_t xor_val = a ^ b;
        int diff_bits = __builtin_popcountll(xor_val);
        return 1.0 - (diff_bits / 64.0);
    }
};

// ==================== 4. 动态记忆库（支持重复识别）===================
struct DynamicConcept {
    std::wstring code;                         // 动态编码，如"D001"=走路
    uint64_t fingerprint;
    int occurrence = 0;
    double confidence = 0.0;
    std::vector<MinimalDynamic> examples;
};

class DynamicMemoryBank {
public:
    std::map<std::wstring, DynamicConcept> concepts;
    std::wstring next_code = L"D001";
    
    std::wstring recognizeOrCreate(const MinimalDynamic& dyn) {
        uint64_t fp = SecondaryFeatureExtractor::generateFingerprint(dyn);
        std::wstring best_code;
        double best_sim = 0.85;  // 相似度阈值
        
        for (const auto& pair : concepts) {
            double sim = SecondaryFeatureExtractor::fingerprintSimilarity(fp, pair.second.fingerprint);
            if (sim > best_sim && sim > pair.second.confidence) {
                best_sim = sim;
                best_code = pair.first;
            }
        }
        
        if (!best_code.empty()) {
            auto& concept = concepts[best_code];
            concept.occurrence++;
            concept.confidence = std::min(1.0, concept.confidence + 0.05);
            concept.examples.push_back(dyn);
            if (concept.examples.size() > 50) concept.examples.erase(concept.examples.begin());
            std::cout << "【动态再识别】" << best_code << " 相似度:" << best_sim << " 出现:" << concept.occurrence << "次\n";
            return best_code;
        } else {
            // 新动态概念
            DynamicConcept new_concept;
            new_concept.code = next_code++;
            new_concept.fingerprint = fp;
            new_concept.occurrence = 1;
            new_concept.confidence = 0.6;
            new_concept.examples = {dyn};
            concepts[new_concept.code] = new_concept;
            std::cout << "【发现新动态】分配编码:" << new_concept.code << "\n";
            return new_concept.code;
        }
    }
};

DynamicMemoryBank dynamic_bank;

// ==================== 5. 主循环（完整动态识别）===================
void run() {
    std::cout << "数字生命 v5.0 启动：动态识别 + 二次特征抽象（按 q 退出）\n";
    
    // 为每个存在维护状态历史
    std::map<int, std::vector<TimedState>> state_history;
    
    while (cv::waitKey(1) != 'q') {
        auto start = std::chrono::high_resolution_clock::now();
        double current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
        
        // ... （原有点云采集、聚类代码保持不变）
        auto colored_points = getRealPointCloud();
        if (colored_points.empty()) continue;
        
        auto clusters = simpleClustering(colored_points);
        currentExistences.clear();
        
        window.removeAllWidgets();
        window.showWidget("Coord", cv::viz::WCoordinateSystem(2.0));
        window.showWidget("Camera", cv::viz::WCameraPosition(0.5));
        
        for (const auto& cluster : clusters) {
            // 计算质心等（同前）
            Vector3D center = computeCenter(cluster, colored_points);
            std::vector<int64_t> contour = generateContourFeatures(cluster, colored_points); // 模拟
            
            // 记忆与再识别
            auto memory = global_memory_bank.recognizeOrCreate(center, contour);
            
            // 更新状态历史
            TimedState state(current_time, center, contour);
            state_history[memory->id].push_back(state);
            if (state_history[memory->id].size() > 50) state_history[memory->id].erase(state_history[memory->id].begin());
            
            // 生成最小动态（每2帧）
            if (state_history[memory->id].size() >= 2) {
                const auto& s1 = state_history[memory->id][state_history[memory->id].size()-2];
                const auto& s2 = state_history[memory->id].back();
                MinimalDynamic min_dyn(s1, s2);
                
                // 动态识别！
                std::wstring dyn_code = dynamic_bank.recognizeOrCreate(min_dyn);
                memory->concept_code = dyn_code;  // 更新动态标签
            }
            
            // 可视化（同前，新增动态标签）
            // ... （绘制立方体/圆柱）
            std::string text = "ID:" + std::to_string(memory->id) + 
                              " Dist:" + std::to_string(center.distance(Vector3D(0,0,0))).substr(0,4) + "m" +
                              " Dyn:" + std::string(memory->concept_code.begin(), memory->concept_code.end());
            cv::viz::WText3D label(text, cv::Point3d(center.x, center.y, center.z + 0.4), 0.08, true, cv::viz::Color::white());
            window.showWidget("Label_" + std::to_string(memory->id), label);
        }
        
        window.spinOnce(1, true);
    }
}