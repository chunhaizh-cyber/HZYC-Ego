export module 基础数据类型模块;

export enum class 枚举_词性 : unsigned char {
    未定义, 名词, 动词, 形容词, 副词, 代词, 量词, 助词, 专有名词, 状态词
};

export enum class 枚举_主信息类型 : unsigned char {
    存在, 特征, 场景, 二次特征, 复合二次特征, 词, 词性, 句子
};

export struct 结构体_分词 {
    std::wstring 词;
    std::wstring 词性;
};