# AutoZWJ Catalog 登记草案

以下内容对应 AviUtl2 Catalog 的包登记表单。带有“提交时填写”的项目不能预先猜测，应以实际 GitHub Release 资产为准。

## 基本信息

| Catalog 字段 | 建议填写值 |
|---|---|
| ID | `tnotawa.autozwj` |
| 包名称 | `AutoZWJ` |
| 作者 | `TNOTawa` |
| 原作者名 | 留空 |
| 包角色 | 主包 |
| 类型 | `generalPlugin`（汎用插件） |
| 包网站 | `https://github.com/TNOTawa/AutoZWJ` |
| 源语言 | `ja` |
| 标签 | `AviUtl2`, `音MAD`, `対軌`, `MIDI`, `REAPER`, `RPP`, `インポート` |
| 概要 | `音MAD向け対軌支援プラグイン` |
| NicoNico Commons ID | 留空 |
| 依赖包 | 留空 |

当前 Catalog 表单的 ID 校验实际使用小写字母、数字、点、连字符组成的格式，因此这里使用小写 ID。不要使用带大写字母的 `TNOTawa.AutoZWJ`。

`legacyId` 不需要手动填写；提交构建时留空会自动使用当前 ID。`addedAt` 也由 Catalog 表单自动生成，不需要预先指定。

## 详细说明

日文详细说明直接使用仓库现有的 README，不另写 Catalog 专用 README：

```text
https://raw.githubusercontent.com/TNOTawa/AutoZWJ/main/docs/ja/README.md
```

在 Catalog 中选择“外部 MD 链接”，不要填写 GitHub 网页 URL；GitHub 根目录 `README.md` 继续作为项目的英语主说明。

更新历史和下载注意事项暂时留空；它们都是可选 Markdown 字段。Catalog 的其他语言内容暂时不添加，只登记日语内容。

## 许可证

添加一条许可证：

| 字段 | 值 |
|---|---|
| 类型 | `MIT` |
| 使用模板 | 是 |
| 版权年份 | `2026` |
| 版权所有者 | `TNOTawa` |

Logo 不是插件运行所需文件，也不应作为插件许可证内容提交。软件许可证范围与 Logo 的单独授权说明见仓库中的 [`LICENSE`](../../LICENSE)、[`LICENSE-CC-BY-NC-ND-4.0`](../../LICENSE-CC-BY-NC-ND-4.0) 和 [`docs/images/README.md`](../images/README.md)。

## 图片

图片为可选项。提交时将以下文件作为缩略图上传：

```text
docs/images/autozwj-catalog-gif-206.gif
```

该文件为 `206×206`、195 帧的循环 GIF。说明图片暂时留空。

Catalog 登记文档明确推荐 PNG/JPG；当前 Catalog 应用的文件选择器另外接受 GIF、WebP、SVG、BMP 等格式。本次使用 GIF；若正式提交时的 Catalog 版本拒绝 GIF，再将同一画面导出为静态 PNG 作为备用缩略图。当前未发现公开的文件大小上限。

## GitHub Release 安装器

| Catalog 字段 | 建议填写值 |
|---|---|
| 来源类型 | `githubRelease` |
| GitHub 账户名 | `TNOTawa` |
| 仓库名 | `AutoZWJ` |
| 下载文件名正则 | `^AutoZWJ\.aux2$` |

安装步骤按顺序填写：

1. `download`
2. `copy`：源 `{tmp}/AutoZWJ.aux2`，目标 `{pluginsDir}`

卸载步骤填写：

1. `delete`：`{pluginsDir}/AutoZWJ.aux2`

## 关系与可选字段

以下字段当前均留空：

- 原作者名
- NicoNico Commons ID
- 必需依赖、推荐包、冲突包、相似包、替代包、fork 来源包
- 弃用开关及弃用消息
- 说明图片

许可证只登记 AutoZWJ 软件的 MIT License；Catalog 缩略图不是插件安装文件。

## 提交前测试

这是新包登记，提交前需要在 Catalog 表单中分别运行并通过：

1. 安装测试：确认能从 `v0.3.0` GitHub Release 下载并将文件复制到 Plugin 目录，同时能检测到版本。
2. 删除测试：确认能删除 `{pluginsDir}/AutoZWJ.aux2`。

测试必须使用最终的 Release、安装步骤、卸载步骤和版本哈希；这些字段变化后需要重新测试。

## 版本

正式上架目标版本：

| 字段 | 提交时填写值 |
|---|---|
| 版本 | `v0.3.0` |
| 发布日期 | 正式 Release 发布日期 |
| 文件路径 | `{pluginsDir}/AutoZWJ.aux2` |
| XXH3-128 | 提交时从对应 GitHub Release 资产计算 |

当前分支尚未完成，不能把当前构建物直接作为正式 Catalog 版本提交。完成开发后，将 `CMakeLists.txt` 版本更新为 `0.3.0`，创建 `v0.3.0` Release，再填写正式发布日期和 Release 资产哈希。
