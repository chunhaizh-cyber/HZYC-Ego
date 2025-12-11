import 自然语言知识库;
import 世界树模块;
import <iostream>;
import <string>;

int main()
{
    setlocale(LC_ALL, "");
    std::wcout << L"\n=== Ego-1 v1.0 已苏醒 ===\n";
    std::wcout << L"我现在会自己学习形容词，请教我说话吧！\n\n";

    世界树类 宇宙;
    自然语言知识库::初始化默认知识();

    std::wstring 输入;
    while (std::getline(std::wcin, 输入)) {
        if (输入 == L"退出" || 输入 == L"quit") break;

        if (!输入.empty()) {
            自然语言知识库::处理用户输入(输入, 宇宙);
        }
        std::wcout << L"> ";
    }

    std::wcout << L"\nEgo-1 进入休眠……谢谢你教我说话。\n";
    return 0;
}