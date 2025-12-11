export module 语素模块;

import 模板模块;
import 主信息定义模块;

export class 语素类 : public 链表模板<语素基类*> {
public:
    词节点类* 添加词(const std::wstring& 词) {
        // 前缀树实现（已修正类型安全）
        语素节点类* 游标 = 根指针;
        std::wstring 前缀;
        for (wchar_t c : 词) {
            前缀 += c;
            词主信息类 临时(前缀);
            auto* 候选 = 查找节点_当前链(游标, &临时, 枚举_比较字段::语素_词_词, 枚举_比较条件::相等);
            if (!候选) {
                auto* 新节点 = new 词节点类();
                新节点->主信息 = new 词主信息类(前缀);
                候选 = 添加节点(游标, 新节点);
            }
            游标 = 候选;
        }
        return dynamic_cast<词节点类*>(游标);
    }

    static std::wstring 获取词名(const 词性节点类* 节点) {
        if (!节点 || !节点->父) return L"";
        auto* 词节点 = dynamic_cast<词节点类*>(节点->父);
        if (!词节点 || !词节点->主信息) return L"";
        return static_cast<词主信息类*>(词节点->主信息)->词;
    }
};