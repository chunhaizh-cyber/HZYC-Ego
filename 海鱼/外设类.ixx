
module;
export module 外设模块;
import <vector>;
import <cstdint>;
import 场景实时显示模块;
	

/*坐标系说明：
1. 坐标系设计：相机 = 自我坐标原点

先把坐标系定死，否则后面都乱：

自我坐标系（Ego / Self）

原点：D455 相机光心

轴向：

+Z：镜头前方

+X：右

+Y：下（或上，自己统一就好）

世界坐标系

暂时可以先 = 自我坐标系（相机固定不动）

以后如果“身体+相机”会移动，再加一个 自我姿态：R, T 做坐标变换：
P_world = R * P_camera + T
*/

export class 外设类 {

	public:
	外设类() = default;
	~外设类() = default;
	// 初始化外设
	
	// 获取当前帧点云数据
	void 相机开始获取信息();
	场景实时显示器 场景显示({
	   .显示点云 = true,
	   .点云采样步长 = 4,
	   .显示存在占位盒 = true,
	   .显示存在轮廓点云 = false
		});
	
};

