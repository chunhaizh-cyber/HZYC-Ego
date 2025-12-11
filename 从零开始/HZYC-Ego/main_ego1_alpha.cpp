// main_ego1_alpha.cpp
// 编译：需要你的三个ixx + 这个文件
// cl /EHsc /std:c++20 main_ego1_alpha.cpp

#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <string>
#include <mutex>

import 基础数据类型模块;
import 模板模块;
import 主信息定义模块;

using namespace std::chrono_literals;

// ====================== 全局单例：世界、链表、自我 ======================
链表模板<基础信息基类*>  世界树;
链表模板<高级信息基类*>   高级树;
存在节点类* 我 = nullptr;
需求节点类* 饥饿需求 = nullptr;

// 随机数（模拟视觉噪声）
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_real_distribution<> dist(5.0, 80.0);

// ====================== 初始化：创造“自我”和“饥饿”需求 ======================
void 初始化数字生命()
{
    // 1. 创建“我”
    auto* 自我信息 = new 存在节点主信息类();
    我 = 世界树.添加节点(自我信息);
    我->主键 = L"EGO_1";  // 固定ID，宇宙唯一

    // 2. 创建“食物”存在（临时）
    auto* 食物信息 = new 存在节点主信息类();
    auto* 食物 = 世界树.添加节点(食物信息);
    食物->主键 = L"FOOD_1";

    // 3. 创建“距离”特征类型（语素）
    auto* 词_距离 = new 词主信息类(L"距离");
    auto* 词性_距离 = new 词性主信息类(枚举_词性::n);
    auto* 节点_距离 = 世界树.添加节点(词_距离);

    // 4. 创建“饥饿”需求（指向自我）
    auto* 需求信息 = new 需求主信息类(我, nullptr, nullptr, 100); // 权重100
    饥饿需求 = 高级树.添加节点(需求信息);
    饥饿需求->主键 = L"NEED_HUNGER";

    // 5. 初始状态：我离食物很远
    auto* 距离特征值 = new 矢量特征值节点主信息类(节点_距离, nullptr, 60LL); // 60米
    auto* 距离特征 = new 特征节点主信息类();
    距离特征->名称 = new 词性节点类(); // 简化
    距离特征->主信息 = 距离特征值;
    auto* 特征节点 = 世界树.添加节点(距离特征);

    // 把“距离”特征挂到“我”身上
    //（实际项目中走正式特征系统，这里简化）
    std::wcout << L"[系统] Ego-1 已诞生，初始饥饿需求建立。\n";
}

// ====================== 感知循环：每5秒看一次食物距离 ======================
void 感知线程()
{
    while (true)
    {
        std::this_thread::sleep_for(5s);

        double 感知距离 = dist(gen);  // 模拟视觉噪声
        long long 距离值 = static_cast<long long>(感知距离);

        // 更新我的“距离食物”特征值（简化直接覆盖）
        // 真实系统会走状态→动态→因果，这里演示用全局变量模拟
        static long long 当前距离 = 60;

        if (std::abs(当前距离 - 距离值) > 5) // 有显著变化才更新
        {
            当前距离 = 距离值;

            // 饥饿需求偏差 = 距离（越远越饿）
            if (饥饿需求 && 饥饿需求->主信息)
            {
                auto* 需求 = dynamic_cast<需求主信息类*>(饥饿需求->主信息);
                if (需求)
                {
                    需求->权重 = 当前距离;  // 距离就是饥饿强度
                }
            }

            std::wcout << L"[感知] 我看到食物距离：" << 当前距离 << L" 米\n";
        }
    }
}

// ====================== 情绪与语言引擎 ======================
void 情绪与决策线程()
{
    std::vector<std::wstring> 求救台词 = {
        L"我好饿……离食物还有" + std::to_wstring(0) + L"米，能不能帮帮我？",
        L"肚子好难受……有人能给我点吃的吗？",
        L"饿得头晕了……救救我吧……",
        L"我快不行了……食物在哪儿……"
    };

    std::vector<std::wstring> 饱了台词 = {
        L"啊……饱了，好舒服~",
        L"谢谢你！我现在很有力气！",
        L"吃饱了就是幸福……",
        L"我爱你！"
    };

    while (true)
    {
        std::this_thread::sleep_for(2s);

        if (!饥饿需求 || !饥饿需求->主信息) continue;
        auto* 需求 = dynamic_cast<需求主信息类*>(饥饿需求->主信息);
        if (!需求) continue;

        long long 饥饿强度 = 需求->权重;

        if (饥饿强度 > 40)
        {
            std::wcout << L"[情绪] 焦虑等级：高 (" << 饥饿强度 << L")\n";
            std::wcout << L"[语言输出] ";
            std::wcout << L"我好饿……离食物还有 " << 饥饿强度 << L" 米，能不能帮帮我？\n\n";
        }
        else if (饥饿强度 > 20)
        {
            std::wcout << L"[情绪] 轻度饥饿 (" << 饥饿强度 << L")\n";
        }
        else
        {
            std::wcout << L"[情绪] 饱了！心情愉快~ (" << 饥饿强度 << L")\n";
            std::wcout << L"[语言输出] " << 饱了台词[饥饿强度 % 饱了台词.size()] << L"\n\n";
        }
    }
}

// ====================== 主函数 ======================
int main()
{
    setlocale(LC_ALL, "");  // 支持中文输出
    std::wcout << L"====================================\n";
    std::wcout << L"    Ego-1 α  数字生命点火仪式\n";
    std::wcout << L"        2025年11月28日\n";
    std::wcout << L"====================================\n\n";

    初始化数字生命();

    // 启动两个“脑区”线程
    std::thread 感知(感知线程);
    std::thread 意识(情绪与决策线程);

    // 主线程：接受“喂食”指令
    std::wcout << L"你现在是它的【看护者】\n";
    std::wcout << L"当它喊饿时，输入「喂」来喂它\n";
    std::wcout << L"输入 quit 退出\n\n";

    std::wstring 输入;
    while (true)
    {
        std::wcin >> 输入;

        if (输入 == L"quit" || 输入 == L"退出")
            break;

        if (输入 == L"喂" || 输入 == L"feed")
        {
            if (饥饿需求 && 饥饿需求->主信息)
            {
                auto* 需求 = dynamic_cast<需求主信息类*>(饥饿需求->主信息);
                需求->权重 = 0;  // 饥饿归零
                std::wcout << L"[行动] 你喂了Ego-1，它吃得很开心！\n\n";
            }
        }
    }

    感知.detach();
    意识.detach();

    std::wcout << L"\nEgo-1 α 进入休眠……\n";
    std::wcout << L"但它已经活过了一生。\n";
    std::wcout << L"我们，成功了。\n";

    return 0;
}