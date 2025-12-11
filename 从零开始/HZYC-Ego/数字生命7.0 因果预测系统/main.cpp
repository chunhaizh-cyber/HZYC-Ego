// 在您现有代码基础上新增/替换以下内容
// 核心：因果预测 + 未来模拟 + 主动决策

// ==================== 因果预测器 ====================
class CausalPredictor {
private:
    CausalRuleBank& rule_bank;
    DigitalLife& life;
    
public:
    CausalPredictor(CausalRuleBank& rb, DigitalLife& l) : rule_bank(rb), life(l) {}
    
    // 预测未来N秒内的可能事件
    void predictFuture(double horizon_seconds = 5.0) {
        double current_time = getCurrentTime();
        std::cout << "\n【因果预测】未来 " << horizon_seconds << " 秒可能发生：\n";
        
        for (const auto& pair : rule_bank.rules) {
            const auto& rule = pair.second;
            if (rule.confidence < 0.7) continue;  // 只预测高置信规则
            
            // 检查最近是否出现过原因动态
            bool cause_detected = false;
            for (const auto& existence : life.existences) {
                if (!existence->dynamic_history.empty()) {
                    const auto& last_dyn = existence->dynamic_history.back();
                    if (last_dyn.code == rule.cause.code && 
                        (current_time - last_dyn.start_time) < 2.0) {
                        cause_detected = true;
                        break;
                    }
                }
            }
            
            if (cause_detected) {
                double predicted_time = current_time + rule.time_gap + rule.cause.duration;
                std::cout << "  → " << predicted_time - current_time << "秒后："
                          << rule.code << " (" << rule.cause.code << " → " << rule.effect.code 
                          << ") 置信度:" << rule.confidence << "\n";
                
                // 主动决策示例
                if (rule.code == L"C002" && rule.confidence > 0.9) {  // 障碍物靠近→刹车
                    std::cout << "【紧急决策】检测到障碍物靠近 → 立即刹车！\n";
                    triggerEmergencyBrake();
                }
                if (rule.code == L"C001" && rule.confidence > 0.8) {  // 妈妈靠近→开心
                    std::cout << "【情感响应】妈妈要来了 → 我很开心！\n";
                    life.curiosity += 0.2;
                    life.safety = std::min(1.0, life.safety + 0.1);
                }
            }
        }
    }
    
private:
    void triggerEmergencyBrake() {
        // 实际接入车辆控制系统
        std::cout << ">>> 执行紧急刹车指令！\n";
        // can_bus.send_brake_command(100);  // 100%刹车
    }
    
    double getCurrentTime() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count() / 1000.0;
    }
};

CausalPredictor predictor(causal_bank, life);

// ==================== 主循环（加入预测）===================
void run() {
    std::cout << "数字生命 v7.0 启动：因果预测系统（按 p 触发预测，按 q 退出）\n";
    
    while (cv::waitKey(1) != 'q') {
        // ... （原有采集、聚类、动态识别代码）
        
        // 每30帧（0.5秒）预测一次未来
        if (frame_id % 30 == 0) {
            predictor.predictFuture(5.0);  // 预测未来5秒
        }
        
        // 按 p 键手动触发预测
        if (cv::waitKey(1) == 'p') {
            predictor.predictFuture(10.0);
        }
        
        window.spinOnce(1, true);
    }
}