# 设计与调试记录

## 1. 命名空间与类职责

| 命名空间 | 主要类 / 函数 | 职责 |
| --- | --- | --- |
| `warehouse` | `Order`、`PickStation`、`Inventory` | 保存订单、站点、库存和机器人状态；负责原子扣减、完成匹配和库存不恢复规则 |
| `dispatch` | `DispatchPolicy`、`Scheduler` | 策略接口、priority/stock 两种选择规则，以及派单和完成调度流程 |
| `audit` | `Trace` | 按发生顺序保存成功派单和成功完成事件 |
| `app` | `normalize_sku`、`valid_station_id`、`valid_order_id` | 输入规范化、ID 校验和命令行入口 |

依赖方向是单向的：

```text
warehouse -> 标准库
audit     -> 标准库
dispatch  -> warehouse、audit
app       -> 标准库（命令行入口可组合 dispatch/warehouse）
```

`warehouse/domain.hpp` 不包含 `dispatch/` 或 ROS 头文件。调度策略只读取候选站点，不修改库存或机器人状态。

## 2. 两种调度策略

- `priority`：候选站点已经按站点 ID 升序排列，直接选择第一个。
- `stock`：选择当前库存最多的候选站点；库存相同时选择站点 ID 较小的一个。

同一个初始状态和同一张订单可能选出不同站点。例如：

```text
S1：库存 3，idle
S2：库存 4，idle
订单：数量 1

priority -> S1
stock    -> S2
```

## 3. 状态转移表

| 操作 | 库存变化 | 机器人状态 | 当前订单 ID | Trace |
| --- | --- | --- | --- | --- |
| 派单成功 | 选中站点库存减少订单数量，且不小于 0 | 选中站点从 idle 变为 busy | 记录订单 ID | 追加 `DISPATCH O S` |
| 派单失败 | 所有站点不变 | 所有站点不变 | 所有站点不变 | 不追加 |
| 完成成功 | 目标站点不变，不恢复库存 | 目标站点从 busy 变为 idle | 清空 | 追加 `COMPLETE O S` |
| 重复完成或错误完成 | 所有站点不变 | 所有站点不变 | 所有站点不变 | 不追加 |

其中 `O` 是订单 ID，`S` 是站点 ID。

## 4. Q4 调试证据

### 4.1 复现输入

调试程序使用初始库存 `S1=3`、`S2=5`，两个机器人都是 idle，然后依次提交：

```text
O1，数量 2
O2，数量 6
完成 O1
O3，数量 1
```

### 4.2 错误输出

```text
S1 no -1 yes 0
```

第三字段是 `-1`，说明 O2 被拒绝后，S2 的库存仍然被错误扣减了 6。

### 4.3 根因分析

旧代码在检查库存是否充足之前就执行了扣减：

```cpp
item.second -= quantity;
if (item.second < 0) {
    return {false, "", "insufficient stock"};
}
```

因此失败路径已经修改了库存，违反“失败派单必须无副作用”的规则。

### 4.4 修复前后差异

修复后先检查数量和库存，只有全部条件满足才修改状态：

```cpp
if (quantity <= 0) {
    return {false, "", "quantity must be positive"};
}
if (item.second < quantity) {
    return {false, "", "insufficient stock"};
}
item.second -= quantity;
busy_[station_id] = true;
orders_[station_id] = order_id;
```

修复后输出：

```text
S1 no 5 yes 0
```

## 5. 边界验证记录

| 场景 | 输入 | 预期结果 | 实际结果 |
| --- | --- | --- | --- |
| 库存不足 | S1 库存 2，提交数量 3 | 拒绝，库存仍为 2，Trace 不增加 | 通过 |
| 库存恰好等于数量 | S1 库存 2，提交数量 2 | 接受，库存变为 0，机器人 busy | 通过 |
| 忙碌站点被跳过 | S1 busy，S2 idle 且库存 1，提交数量 1 | 分配到 S2，S1 不变 | 通过 |
| 数量为 0 | 提交数量 0 | 拒绝，库存和状态不变 | 通过 |

以上边界由本地补充测试覆盖，运行结果为：

```text
ALL C2 TESTS PASSED
```

## 6. 文档与验证命令

完整构建、Q4 调试、ROS 2 启动和故障排查命令见 `docs/README.md`。每次修改核心库后，至少执行：

```bash
cmake --build build --parallel 4
./build/public_tests
./build/debug_task
python3 tools/validate_structure.py
```

预期结果分别是公开测试通过、`S1 no 5 yes 0` 和 `Structure OK`。
