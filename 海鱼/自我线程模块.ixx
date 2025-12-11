
module;

export module 自我线程模块;

import 模板模块;
import 基础数据类型模块;
import 主信息定义模块;
import 特征值模块;
import 世界树模块;
import 语素模块;
import 状态模块;

import 需求模块;
import 动态模块;


import <thread>;
import <mutex>;
import <condition_variable>;
import <functional>;

import<cassert>;
import <vector>;
import <string>;
import <cstdint>;  // for int64_t
import <deque>;
import <map>;
import <algorithm>;


// = (存在节点类*)世界树.添加存在("自我", "自我");

export  class 自我线程类 {
 public:
	 自我线程类() = default;
	 ~自我线程类() { 停止(); }
	 void 启动();
	 void 停止();
	 void 等待线程结束();
	 bool 是否正在运行() const;
	
 private:
	 std::thread* m_线程{ nullptr };
	 mutable std::mutex m_互斥锁;
	 std::condition_variable m_条件变量;
	 bool m_运行标志{ false };
	 void 主循环();
	 std::chrono::milliseconds 心跳周期_{ 200 };

	存在节点类*  自我指针;
	特征节点类* 安全值指针;
	特征节点类* 信任值指针;
	特征节点类* 当前安全值指针;
	特征节点类* 当前信任值指针;
	场景节点类* 自我所在场景指针;
	状态节点类* 自我状态根指针;
	动态节点类* 自我动态根指针;
	需求节点类* 自我需求根指针;
	 int64_t 自检间隔时间;

//	 
	词性节点类* 专有名词词指针;
	词性节点类* 抽象名词词指针;
	词性节点类* 名词词指针;
	词性节点类* 动词词指针;
	词性节点类* 形容词词指针;
	词性节点类* 介词词指针;
	词性节点类* 连词词指针;
	词性节点类* 数词词指针;
	词性节点类* 量词词指针;
	词性节点类* 代词词指针;
	词性节点类* 副词词指针;
	词性节点类* 叹词指针;
	词性节点类* 标点符号指针;
	词性节点类* 助词词指针;

	词性节点类* 自我词指针;
	词性节点类* 我词指针;
	词性节点类* 你词指针;
	词性节点类* 他词指针;
	词性节点类* 矢量词指针;
	词性节点类* 轮廓词指针;
	词性节点类* 安全值词指针;
	词性节点类* 信任值词指针;

	词性节点类* 矢量单位词指针;

	void 自我初始化();
	void 更新安全值(int64_t 增减量);
	void 更新需求权重(需求节点类* 需求节点);

	void 语素类初始化();
	void 特征值类初始化();
	void 特征类初始化();
	void 存在类初始化();
	void 场景类初始化();
	void 需求初始化();
	void 方法初始化();
	void 任务初始化();


	
	///////////////////////////////
  //  节点类* 距离词指针;
  //  节点类* 尺寸词指针;
  //  节点类* 轮廓词指针;
	
	//
	//私有消息信息
public:
	enum class 消息类型 {
		任务创建,        // payload: 任务
		任务开始,        // payload: task_id
		任务进度,        // payload: task_id, progress [0,1]
		任务完成,        // payload: task_id, 成功标志
		任务失败,        // payload: task_id
		需求观测,        // payload: demand_key, 强度
		权重外部调整,    // payload: demand_key, 新权重(可选)
		心跳             // 无payload，用于周期性维护（也可内部触发）
	};


	struct 任务消息 {
		消息类型 类型{};
		std::string 任务ID;     // 可空
		std::string 需求键;     // 可空
		double 数值 = 0.0;       // 复用：进度/观测强度/新权重
		bool 标志 = false;       // 复用：成功/失败等
	};
 private:
	struct 任务 {
		std::string id;
		std::string 名称;
		double 基础重要度 = 1.0;   // 固有重要度
		double 紧急度 = 0.0;       // 随时间/观测变化
		double 成本 = 1.0;         // 资源消耗估计(越大越劣)
		double 风险 = 0.0;         // 安全风险估计(越大越劣)
		double 进度 = 0.0;         // [0,1]
		bool   活跃 = true;

		// 优先级得分（每次重算）
		double 优先级 = 0.0;
	};

	struct 需求 {
		// 需求当前值与期望目标
		double 当前 = 0.0;          // 需求强度（越高越迫切）
		double 目标 = 0.0;          // 目标水平（0=完全满足）
		// 自适应权重（体现“此类需求对全局的影响力”）
		double 权重 = 1.0;
		// 近端统计（用于自适应）
		double 指数平滑观测 = 0.0;  // EMA观测
		double 最近回报 = 0.0;      // 完成任务对该需求改善程度的估计
		// 衰减时间戳
		std::chrono::steady_clock::time_point 上次更新 = std::chrono::steady_clock::now();
	};
	
	// ==== 内部状态 ====
	std::atomic<bool> stop_{ false };
	std::thread worker_{};

	// 线程安全消息队列
	std::mutex mtx_;
	std::condition_variable cv_;
	std::deque<任务消息> 队列_;

	// 自我安全值（0=危险，1=安全）
	double 安全值_ = 1.0;

	// 需求池：key -> 需求
	std::map<std::string, 需求> 需求表_;

	// 任务池：id -> 任务
	std::map<std::string, 任务> 任务表_;

	// ====== 可调参数 ======

	// 需求衰减（把未持续观测的需求逐步回落到目标）
	double 需求衰减速率_每秒_ = 0.05; // 每秒将 (当前-目标) 向目标靠近5%
	// 权重自适应步长
	double 权重学习率_ = 0.1;         // 越大，响应越快
	// 指数平滑
	double EMA_alpha_ = 0.2;
	// 优先级权重（线性模型系数）
	double W_需求合成_ = 1.0;         // 需求总权重项
	double W_紧急度_ = 0.6;
	double W_基础重要_ = 0.4;
	double W_成本_ = -0.2;
	double W_风险_ = -0.4;
	double W_安全_ = 0.5;         // 安全值越低，越需要偏保守（见下）


	 ////////////////////////自我的方法函数//////////////////////////////////////
	词性节点类* 查找特征(词性节点类* 主体, 词性节点类* 特征类型);
	词性节点类* 添加子特征(词性节点类* 主体, 词性节点类* 特征类型, int64_t 特征值);
 public:
	 场景节点类 词转换( 结构体_分词 信息体);

	 void 推送消息(struct 任务消息 msg);
 private:
	 bool 退出标识 = true;
	 int64_t 安全值归零步长 = 10000;
	 int64_t 安全值增减步长 = 10000;
	 int64_t 安全值下限阈值 = -INT_FAST64_MAX/2;
	 int64_t 安全值上线限阈值 = INT_FAST64_MAX / 2;
	 int64_t 需求值下限阈值 = -INT_FAST64_MAX / 2;
	 int64_t 需求值上线限阈值 = INT_FAST64_MAX / 2;
	 int64_t 归零时间间隔 = 10000;
	 int64_t 需求值归零步长 = 10000;	 
	 int64_t 需求值增减步长 = 10000;
	 int64_t 安全值当前阈值累积时间 = 0;
	 int64_t 需求值当前阈值累积时间 = 0;

	 // ====== 内部工具 ======
	 void 处理消息_任务(const 任务消息& m);
	 void 处理消息_需求(const 任务消息& m);
	 void 处理消息_权重(const 任务消息& m);

	 void 周期性维护();                 // 衰减需求、归一化权重、重算优先级等
	 void 更新安全值_基于任务(const 任务& t, bool 完成, bool 成功);
	 void 更新需求_基于观测(const std::string& key, double 强度);

	 void 归一化需求权重();
	 double 计算需求合成权重() const;   // sum(key: 需求.权重 * max(0, 需求.当前-需求.目标))
	 void 重算所有任务优先级();

	 // 安全裁剪
	 static double clip01(double x) { return std::clamp(x, 0.0, 1.0); }

};




 export 自我线程类 自我;

