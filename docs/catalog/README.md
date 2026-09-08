# AviUtl2 Catalog 上架资料

本目录保存 AutoZWJ 提交 AviUtl2 Catalog 时使用的资料。Catalog 的登记仍需在 Catalog 应用内完成；这里的文件用于保持登记内容与仓库、GitHub Release 和安装行为一致。

## 提交前确认

1. 将要提交的版本已推送到 `main`，并创建对应的 `vX.Y.Z` GitHub Release。
2. Release 中包含名为 `AutoZWJ.aux2` 的单文件资产。
3. 在 Catalog 登记页面选择该 Release 资产，计算并填写它的 XXH3-128 哈希。不要使用本地 Debug 构建的哈希替代 Release 资产哈希。
4. 按 [`submission.md`](submission.md) 填写包信息、安装步骤、卸载步骤和版本信息。
5. 使用仓库现有的 [`docs/ja/README.md`](../ja/README.md) 作为日文详细说明；不另写 Catalog 专用 README。

## 当前发布策略

- 包 ID：`tnotawa.autozwj`
- 下载来源：GitHub Releases
- Release 资产正则：`^AutoZWJ\.aux2$`
- 安装目标：`{pluginsDir}/AutoZWJ.aux2`
- 卸载目标：`{pluginsDir}/AutoZWJ.aux2`
- Catalog 登记语言：日语
- GitHub 主要语言：英语（根目录 `README.md`）
- 目标正式版本：`v0.3.0`
- 缩略图：`docs/images/autozwj-catalog-gif-206.gif`（206×206，GIF，195 帧）
- 依赖：无

## 版本哈希

Catalog 使用发布资产的 XXH3-128 哈希判断已安装文件和更新状态。正式发布 `v0.3.0` 后，要在 Catalog 登记页面对该 Release 下载的 `AutoZWJ.aux2` 重新计算哈希；版本号、日期、文件路径和哈希必须来自同一个 Release 资产。
