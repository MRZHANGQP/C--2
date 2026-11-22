# 运行与验证说明

本文档面向第一次接手仓库的同学。按下面的顺序执行，可以复现核心库公开测试、Q4 调试任务和 ROS 2 演示。

## 1. 本地环境

- Ubuntu 24.04（或任意支持 C++17 的 Linux 环境）
- CMake 3.16 或更高版本
- 支持 C++17 的编译器
- Q2 需要 ROS 2 Jazzy

## 2. 本地构建与测试

在仓库根目录执行：

```bash
cmake -S . -B build
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

也可以直接运行项目提供的一键检查脚本：

```bash
tools/check.sh
```

### 2.1 公开测试

```bash
./build/public_tests
```

预期输出：

```text
PUBLIC 2/2
```

如果程序返回非零值，说明公开测试失败。先查看终端中的 `FAIL` 行，再回到对应实现文件检查状态更新顺序。

### 2.2 演示程序

```bash
./build/core_demo
```

预期输出：

```text
AIM OOP CW2 1.0
```

### 2.3 结构检查

```bash
python3 tools/validate_structure.py
```

预期输出：

```text
Structure OK
```

结构检查会验证头文件保护、命名空间边界、领域层反向依赖，以及 ROS 2 包是否复用核心源码。

## 3. Q4 调试程序

```bash
./build/debug_task
```

修复前的错误输出：

```text
S1 no -1 yes 0
```

修复后的目标输出：

```text
S1 no 5 yes 0
```

五个输出字段依次表示：

1. 订单 O1 被分配到的站点。
2. 订单 O2 是否被接受。
3. 拒绝 O2 后站点 S2 的库存。
4. 完成 O1 后订单 O3 是否被接受。
5. 最终站点 S1 的库存。

如果输出不是目标值，请先检查 `src/debug/main.cpp` 中 `submit` 的校验顺序，详细分析见 `docs/DESIGN.md`。

## 4. ROS 2 构建与运行

以下命令在 Ubuntu 24.04 + ROS 2 Jazzy 中执行：

```bash
source /opt/ros/jazzy/setup.bash
cd ros2_ws
colcon build --symlink-install
source install/setup.bash
ros2 launch warehouse_dispatch warehouse_demo.launch.py
```

启动后，模拟节点每 300 毫秒发布一条事件；调度节点调用 Q1 核心库处理事件，并把结果发布到 `/dispatch_results`。六条事件处理完成后，两个节点自动退出。

## 5. ROS 2 预期输出

| 输入事件 | 预期输出 | 状态变化 |
| --- | --- | --- |
| `ORDER O1 SKU-A 1` | `ACCEPT O1 S1` | S1 库存 3→2，busy，当前订单 O1 |
| `ORDER O2 SKU-A 1` | `ACCEPT O2 S2` | S2 库存 2→1，busy，当前订单 O2 |
| `ORDER O3 SKU-A 2` | `REJECT O3` | 两个站点都 busy，无变化 |
| `COMPLETE O1` | `COMPLETE O1` | S1 恢复 idle，清空当前订单，库存保持 2 |
| `ORDER O4 SKU-A 3` | `REJECT O4` | S1 库存 2 小于 3，S2 busy，无变化 |
| `WAIT 1` | `WAITED 1` | 不修改库存和机器人状态，随后退出 |

评分时比较的是去掉首尾空白后的六条核心输出。`RCLCPP_INFO` 日志和启动提示不属于核心输出。

## 6. 常见问题排查

### 6.1 找不到 `ros2` 或 `colcon`

先确认 ROS 2 环境已经加载：

```bash
source /opt/ros/jazzy/setup.bash
echo $ROS_DISTRO
```

预期输出 `jazzy`。如果仍然找不到命令，检查 ROS 2 Jazzy 是否已经安装。

### 6.2 `colcon build` 找不到 `rclcpp`

通常是编译前没有加载 ROS 2 环境。重新执行：

```bash
source /opt/ros/jazzy/setup.bash
cd ros2_ws
rm -rf build install log
colcon build --symlink-install
```

### 6.3 启动后没有自动退出

先查看节点是否还在运行：

```bash
ros2 node list
```

正常情况下只应看到 `warehouse_simulator` 和 `warehouse_dispatcher`，并在 `WAIT 1` 处理完后消失。如果节点残留，检查调度节点是否真的收到 `WAIT 1`，以及 `handle_event` 是否调用了 `rclcpp::shutdown()`。

### 6.4 公开测试仍然失败

先确认没有残留的 TODO：

```bash
grep -R "TODO: implement" -n include src
```

然后重新构建：

```bash
cmake --build build --parallel 4
./build/public_tests
```

提交前不要提交 `build/`、`install/`、`log/` 等生成目录。
