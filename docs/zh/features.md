# 功能详解

## 支持的导入格式

| 格式 | 扩展名 | 说明 |
|------|--------|------|
| REAPER 工程 | `.rpp` | 支持轨道文件夹层级、音视频素材、SECTION LOOP、TEXT/NOTES |
| 标准 MIDI | `.mid` | 支持多轨道、CC7 Volume、CC10 Pan、PitchBend、Tempo 变化 |
| LRC 歌词 | `.lrc` | 支持 `[mm:ss.xx]` / `[mm:ss:xx]` 时间戳、一行多时间点、JSON 行过滤 |

---

## 模板驱动生成

AutoZWJ 的核心设计是**模板驱动**：

1. 用户在时间轴上选中一个或多个已有物件
2. 插件读取这些物件的 alias，提取其 `[Object.0]` 或 `[0.0]` 起始的效果链
3. 生成时，以该效果链为基础，仅替换再生位置、速度、循环、帧范围等参数
4. 效果链中的隐藏键（如 `Group=1`、`Group2=1`）原样保留

### 模板 Alias 归一化

`get_object_alias()` 返回 `[Object]`/`[Object.N]`，而 `create_object_from_alias()` 接受 `[0]`/`[0.N]`。插件在提取模板链时会自动将 `[Object.N]` 归一化为 `[0.N]`。

---

## 多源映射策略

当选中 ≥2 个模板物件时，生成循环会为每个目标物件选择一个模板：

### 顺序轮替

按模板池索引循环分配。支持三种子模式：
- **顺序**：0 → 1 → 2 → 0 → ...
- **倒序**：2 → 1 → 0 → 2 → ...
- **洗牌**：基于工程文件路径哈希生成确定性洗牌序列

### 随机抽选

基于工程文件路径哈希 + 全局物件索引生成确定性随机数：
- 同一工程文件总是产生相同的随机分配结果
- 可选**禁止连续重复**：若随机到与上一个相同的模板，则自动取下一个

### 和弦映射

将同一帧上出现的多个音符视为一个和弦，按和弦中的位置分配模板：
- `chord.index`：该音符在 chord 中的位置（从 1 开始）
- `chord.count`：该 chord 中的音符总数
- 需要多音符策略设为"全部独立"才能正常工作

---

## 层分配算法

层分配采用**贪婪首次适配**策略：

```
对于每个物件：
  从 base_layer 开始遍历图层
  若当前图层的结束帧 < 新物件的开始帧：
    复用该图层
  否则：
    开辟新图层
```

### 层分配策略

- **优化模式（0）**：尽可能复用图层，使图层数最少
- **持续累加模式（1）**：每个新物件都尝试放在新图层，仅当所有图层都冲突时才复用
- **交替换行（2）**：奇数索引物件使用图层组 A，偶数索引使用图层组 B，各自内部优化

### 反转轨道顺序

将层分配结果倒序输出：最大图层号变最小，最小变最大。

---

## 帧计算

AviUtl2 帧编号从 **1** 开始：

```
sf = round(pos * fps + 1.0)   // 开始帧
ef = round(bf)                // 结束帧
```

其中 `bf = sf + length * fps - 1`（未取整的浮点结束帧）。层分配时比较的是浮点 `bf` 与 `obj_fp`，取整仅用于最终输出。

可选 **向上取整帧**（`use_round_up`）替代四舍五入。

---

## 效果链编辑器技术细节

### 效果链解析

`parse_effect_chain()` 从模板 alias 中提取效果链：
1. 查找 `[0.N]` section 标记
2. 读取 `effect.name=` 确定效果类型
3. 其余 `键=值` 对作为参数
4. 通过 `find_effect()` 查询 `effect_dict` 获取中文显示名

### 参数 Bake 注入

`inject_param_bakes()` 将用户设置的 bake 值注入效果链：
1. 定位到目标 `[0.N]` section
2. 查找参数名，存在则替换，不存在则追加
3. 运动参数（逗号分隔）特殊处理：仅替换起点/终点，保留方法、曲线等字段
4. 配对参数（`.1` 后缀）独立处理

### 预设注入

`inject_presets()` 将动态预设（如翻转效果块）插入到效果链的指定位置：
1. 解析现有效果链为 section 列表
2. 将预设效果块插入到用户拖拽指定的位置
3. 自动重新编号 section 索引
4. 稳定排序处理多个预设

### 表达式求值

`ExprEvaluator` 使用递归下降语法分析：

```
expr     = or_expr
or_expr  = and_expr ( "||" and_expr )*
and_expr = eq_expr  ( "&&" eq_expr )*
eq_expr  = rel_expr ( ( "==" | "!=" ) rel_expr )*
rel_expr = add_expr ( ( "<=" | ">=" | "<" | ">" ) add_expr )*
add_expr = mul_expr ( ( "+" | "-" ) mul_expr )*
mul_expr = unary    ( ( "*" | "/" | "%" ) unary )*
unary    = ( "!" | "-" ) unary | primary
primary  = number | identifier [ "(" args ")" ] | "(" expr ")"
```

支持的函数：
- 数学：`abs`, `floor`, `ceil`, `round`, `sin`, `cos`, `sqrt`, `pow`, `min`, `max`, `clamp`
- 随机：`rand(min, max)`, `rand_int(min, max)`（基于种子确定性）
- 专用：`map_pitch(lo, hi)`（基于 `note.pitch_ratio` 线性映射）

---

## 持久化系统

所有配置通过 `PROJECT_FILE` API 以 `rppinexo.*` 键名序列化到 AviUtl2 工程文件中：

| 键名前缀 | 内容 |
|----------|------|
| `rppinexo.file_path` | 当前加载的工程文件路径 |
| `rppinexo.history_N` | 第 N 条历史记录的文件路径 |
| `rppinexo.history_offset_N` | 第 N 条历史记录的基准时间偏移 |
| `rppinexo.alt_flip` / `flip_type` / `flip_counter_mode` | 翻转设置 |
| `rppinexo.mapping_strategy` / `mapping_sequential_order` / `mapping_no_consecutive` | 多源映射设置 |
| ... | 其余所有 OutputConfig 字段 |

跨工程隔离：任何 EDIT_SECTION 回调入口都会先执行 `g_project_state = ProjectState{}` 重置全局状态，再从 `PROJECT_FILE` 重新加载。

---

## 文件拖放支持（存在问题）

插件注册了文件拖放处理器（可能需要手动去`设置 - 导入插件设置`中设定文件后缀），支持将工程文件直接拖入 AviUtl2 窗口自动解析。拖放后会自动将文件加入历史记录。
