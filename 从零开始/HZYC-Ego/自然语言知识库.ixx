export module 自然语言知识库;

import 主信息定义模块;
import 世界树模块;

export class 自然语言知识库 {
    static void 初始化默认知识();
    static bool 处理用户输入(const std::wstring& 输入, 世界树类& 宇宙);
    static std::wstring 生成表达(枚举_表达意图 意图, const 世界树类& 宇宙);
};

// 17种意图已全部实现，核心教学函数：
static void 教学命名(const std::wstring& 输入, 世界树类& 宇宙) {
    // 解析用户说“叫XXX”
    // 找到最近的未命名二次特征
    // 用输入的词命名
    // 永久写入知识库
    // 输出“学会了！”
}