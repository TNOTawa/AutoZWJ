# AutoZWJ 领域语言

将 REAPER `.rpp` / MIDI `.mid` 工程以时间轴已有物件为模板，批量再生到 AviUtl2 时间轴的插件。本文件只记录领域名词，不含实现细节。

## 语言

**Template（模板）**:
时间轴上被选中的物件，作为样式源；其效果链被复用，仅替换位置/速度/循环等参数。
_Avoid_: 模板物件、源物件、样板

**Alias**:
宿主 `get_object_alias` 返回的原始文本，含 `[Object.N]` 等 section 头与隐藏键（如 `Group=1`）。
_Avoid_: 物件数据、alias 文本

**Template Chain（模板链）**:
从 Alias 中提取、section 头归一化为 `[0.N]` 的效果链文本块。生成引擎消费的是已提取的链，不碰原始 Alias。
_Avoid_: 效果链文本、chain

**ParamBake（参数 bake）**:
作用在模板某效果参数上的覆盖值；值可为字面量 / `$var$` 变量替换 / 表达式求值三种模式。
_Avoid_: 参数覆盖、bake 值

**Preset（预设）**:
插入到模板链指定位置的效果块（如翻转块 `build_flip_block` 的产物）。
_Avoid_: 效果块、动态效果

**ObjDict**:
解析 RPP/MIDI 工程得到的扁平数据——位置/长度/音高/播放率/MIDI 控制器等平行向量，外加 BPM 与轨数。
_Avoid_: 工程数据、解析结果

**Item（条目/音符）**:
ObjDict 中的一个元素：一个 MIDI 音符或一个 RPP 物件。空轨以 `pos == -1.0` 作分隔符。
_Avoid_: 事件、note（note 仅在变量名 `$note.*$` 语境中使用）

**Track（轨道）**:
RPP/MIDI 中的一条轨，含编号/层级/选中态/子轨。空轨保留可见但生成时跳过。

**Generation（生成）**:
由 ObjDict + 模板池 + bake/预设，按同步/层分配/翻转/多源映射策略，产出时间轴物件的过程。

**Generation Engine（生成引擎）**:
执行 Generation 的只读纯计算黑盒：输入一份打包数据，输出 Generated Object 列表；不调用宿主 SDK、不写全局。
_Avoid_: 生成器、generator

**Generated Object（生成物件规格）**:
生成引擎的输出契约：层/起始帧/结束帧/完整 alias 文本块/物件名。写入时间轴由引擎外的薄适配器完成。
