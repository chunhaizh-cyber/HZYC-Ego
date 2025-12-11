export module 主信息定义模块;

import 模板模块;
import 基础数据类型模块;

export class 基础信息基类 {
public:
    virtual ~基础信息基类() = default;
    枚举_主信息类型 类型{};
    std::wstring    主键;
};

// 前向声明（断开循环依赖）
export class 世界树类;
export class 场景节点类;
export class 词性节点类;

export class 存在节点主信息类 : public 基础信息基类 {
public:
    词性节点类* 名称 = nullptr;
    场景节点类* 所属场景 = nullptr;
    存在节点主信息类() { 类型 = 枚举_主信息类型::存在; }
};

export class 特征节点主信息类 : public 基础信息基类 {
public:
    词性节点类* 名称 = nullptr;
    double      值 = 0.0;
    存在节点类* 主体 = nullptr;
    特征节点主信息类() { 类型 = 枚举_主信息类型::特征; }
};

export class 场景节点主信息类 : public 基础信息基类 {
public:
    词性节点类* 名称 = nullptr;
    std::vector<存在节点类*> 包含存在;
    std::vector<class 二次特征节点类*> 二次特征列表;
    std::vector<class 复合二次特征节点类*> 复合特征列表;
    场景节点主信息类() { 类型 = 枚举_主信息类型::场景; }
};

export class 二次特征主信息类 : public 基础信息基类 {
public:
    存在节点类* 基准状态 = nullptr;
    存在节点类* 比较对象状态 = nullptr;
    class 特征节点类* 比较结果 = nullptr;
    词性节点类* 比较特征 = nullptr;   // 可空：整体比较
    double 偏差值 = 0.0;
    二次特征主信息类() { 类型 = 枚举_主信息类型::二次特征; }
};

export class 复合二次特征主信息类 : public 基础信息基类 {
public:
    词性节点类* 名称 = nullptr;
    std::vector<基础信息节点类*> 来源特征;
    std::vector<double> 权重;
    double 当前值 = 0.0;
    double 阈值 = 0.6;
    复合二次特征主信息类() { 类型 = 枚举_主信息类型::复合二次特征; }
};

// 词系统
export class 词主信息类 : public 基础信息基类 {
public:
    std::wstring 词;
    explicit 词主信息类(const std::wstring& w) : 词(w) { 类型 = 枚举_主信息类型::词; }
};

export class 词性主信息类 : public 基础信息基类 {
public:
    枚举_词性 词性 = 枚举_词性::未定义;
    词主信息类* 对应词 = nullptr;
    词性主信息类() { 类型 = 枚举_主信息类型::词性; }
};

// using 节点类（唯一出口）
export using 存在节点类 = 链表模板<存在节点主信息类*>::节点类;
export using 特征节点类 = 链表模板<特征节点主信息类*>::节点类;
export using 场景节点类 = 链表模板<场景节点主信息类*>::节点类;
export using 二次特征节点类 = 链表模板<二次特征主信息类*>::节点类;
export using 复合二次特征节点类 = 链表模板<复合二次特征主信息类*>::节点类;
export using 词节点类 = 链表模板<词主信息类*>::节点类;
export using 词性节点类 = 链表模板<词性主信息类*>::节点类;