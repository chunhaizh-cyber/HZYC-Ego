// 在您现有代码基础上新增以下完整模块
// 核心：反向因果 + 目标驱动行为 + 主动改变未来

// ==================== 反向因果推理器 ====================
class ReverseCausalEngine {
private:
    CausalRuleBank& rule_bank;
    DigitalLife& life;
    
public:
    ReverseCausalEngine(CausalRuleBank& rb, DigitalLife& l) : rule_bank(rb), life(l) {}
    
    // 反向推理：我想要“开心”，需要触发什么原因？
    void desireDrivenPlanning(const std::wstring& desired_effect_code) {
        std::cout << "\n【反向因果推理】我想要：" << desired_effect_code << "\n";
        
        std::vector<std::pair<std::wstring, double>> possible_causes;
        
        for (const auto& pair : rule_bank.rules) {
            if (