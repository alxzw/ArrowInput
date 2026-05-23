# ArrowInput

ArrowInput 是一个面向 Windows 的中文输入法实验项目，基于 TSF（Text Services Framework）实现，目前只支持 x64。

项目现阶段的重点是把 TSF 外壳、候选窗、配置、词库和算法测试链路跑通，让输入算法可以独立迭代、独立验证。

## 当前状态

- 生成 x64 COM DLL：`build\x64\Debug\ArrowInputTextService.dll`
- 默认部署测试文件到：`D:\MyApp\ArrowInput`
- 运行时读取配置：`%APPDATA%\ArrowInput\config.ini`
- 已接入 TSF key sink、focus sink
- 支持预编辑文本、候选窗、翻页、鼠标选择、失焦清理
- 支持中英文模式、全角/半角、中文标点、Ctrl/Alt 快捷键透传
- 候选查询已经抽出算法边界：
  - `IInputAlgorithm::QueryCandidates(code)`
  - 通过 `dictionary_type` 选择候选源
  - 通过 `dictionary_path` 加载 TSV 或 SQLite 词库
  - 通过独立的 `AlgorithmProbe.exe` 测试算法

## 构建

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Configuration Debug
```

## 部署

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\deploy.ps1 -Configuration Debug
```

默认测试部署目录：

```text
D:\MyApp\ArrowInput
```

如果 DLL 正被系统或应用加载，部署脚本会写入：

```text
D:\MyApp\ArrowInput\ArrowInputTextService.dll.pending
```

退出登录，或用其他方式卸载当前 DLL 后，再应用 pending 文件。

## 安装

TSF profile 注册需要管理员权限。请以管理员身份运行 PowerShell：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\install.ps1 -Configuration Debug
```

## 卸载

同样需要管理员权限：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\uninstall.ps1 -Configuration Debug
```

注销 TSF profile 不会强制已经加载 DLL 的进程立刻卸载它。可以检查或停止仍占用部署 DLL 的进程：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\release_loaded_binaries.ps1
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\release_loaded_binaries.ps1 -StopLockingProcesses
```

也可以在卸载时一并尝试停止占用进程：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\uninstall.ps1 -StopLockingProcesses
```

## 算法探针

构建独立算法测试程序：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_algorithm_probe.ps1 -Configuration Debug
```

执行一次性查询：

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --type tsv --dict tools\AlgorithmProbe\sample_dictionary.tsv ni hao
```

限制返回候选数量：

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --type sqlite --dict D:\MyApp\ArrowInput\experiments\rime_luna.db --limit 20 yi nih
```

查询当前用户配置中选中的词库：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\probe_current_dictionary.ps1 -Limit 10 yi nih
```

对当前词库做健康检查：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\check_current_dictionary.ps1 -Limit 5 ni nih yi zhongguo
```

交互式查询：

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --dict tools\AlgorithmProbe\sample_dictionary.tsv --interactive
```

打印候选源统计：

```powershell
.\build\tools\AlgorithmProbe\x64\Debug\AlgorithmProbe.exe --type tsv --dict tools\AlgorithmProbe\sample_dictionary.tsv --stats
```

## 词库格式

目前实现了两种候选源：

- `dictionary_type=tsv`
- `dictionary_type=sqlite`

其他值会返回 `unsupported source type`。SQLite schema 见 `docs\sqlite_dictionary_schema.md`。

TSV 词库格式：

```text
code<Tab>text<Tab>comment<Tab>weight<Tab>user_weight
```

说明：

- 文件使用 UTF-8
- `#` 开头的行会被忽略
- `comment`、`weight`、`user_weight` 可选
- 候选排序按 `weight DESC`、`user_weight DESC`、文件顺序

运行时会通过 `config.ini` 里的 `max_candidates_per_query` 限制候选数量，默认值为 30。开启 `prefix_candidates=1` 后，输入 `nih` 时也可以匹配 `nihao` 这类以当前编码开头的候选。

连续拼音会先解析再查询。例如 `shurufa` 也可以查询词库中编码为 `shu ru fa` 的行。

短拼也会作为回退候选参与查询。例如：

- `zg` 可以匹配 `zhong guo`
- `srf` 可以匹配 `shu ru fa`
- `zguo`、`zhg` 可以匹配 `zhong guo`
- `srfa`、`shrf` 可以匹配 `shu ru fa`

可选的模糊音可以通过 `fuzzy_pinyin=1` 开启，支持 `z/zh`、`c/ch`、`s/sh`、`an/ang`、`en/eng`、`in/ing` 等常见组合。

## 用户词和学习

SQLite 词库包含 `user_entries` 表，用于自动学习和手动用户词。选择候选后，ArrowInput 会给该候选增加用户权重，让常用词逐渐排到前面。学习事件也会被记录，因此最近一次自动学习可以撤销：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db learning-history
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db undo-learning
```

添加、列出、导出、导入用户词：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db add --code ceshi --text 测试 --comment "ce shi" --user-weight 5000
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db list
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db user-stats
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db export --output D:\MyApp\ArrowInput\experiments\user_entries.tsv
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db import --input D:\MyApp\ArrowInput\experiments\user_entries.tsv
```

如果当前配置已经指向 SQLite 词库，可以使用短包装脚本：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_current_user_dictionary.ps1 user-stats
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_current_user_dictionary.ps1 undo-delete
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_current_user_dictionary.ps1 undo-learning
```

不想看到的候选可以用用户 tombstone 隐藏；输入过程中也可以按 `Shift+Delete` 隐藏当前高亮候选并刷新候选窗。

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db delete --code ni --text 你
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db deleted
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db undo-delete
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\manage_user_dictionary.ps1 -DatabasePath D:\MyApp\ArrowInput\experiments\rime_luna.db restore --code ni --text 你
```

## 词库工具

验证 TSV 词库：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\validate_tsv_dictionary.ps1 -InputPath data\seed_pinyin.tsv
```

验证会拒绝重复的 `code+text` 行、非法整数权重、多余字段、首尾空白、非法编码、过长字段，以及超出范围的权重。编码目前允许小写 ASCII 字母和撇号。

把外部文本词库转换为 ArrowInput TSV：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\convert_text_dictionary_to_tsv.ps1 -InputPath D:\path\to\source.txt -OutputPath D:\path\to\converted.tsv
```

默认读取 `code text comment weight user_weight` 列。存在 Tab 时按 Tab 分列，否则按空白分列。如果外部词库列顺序不同，可以使用 `-Columns`，例如：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\convert_text_dictionary_to_tsv.ps1 -InputPath D:\path\to\source.txt -OutputPath D:\path\to\converted.tsv -Columns text,code,weight
```

也支持转换 Rime `.dict.yaml`。`...` 标记之后的条目会按 `text<Tab>code<Tab>weight` 读取，编码中的空格会被移除以便 ArrowInput 查询，原始编码会保存在 comment 中：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\convert_text_dictionary_to_tsv.ps1 -InputPath D:\path\to\luna_pinyin.dict.yaml -OutputPath D:\path\to\rime_converted.tsv -SourceFormat rime
```

新增一个实验 TSV 条目，并验证、重建 SQLite、查询新编码：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\add_tsv_entry.ps1 -DictionaryPath D:\MyApp\ArrowInput\seed_pinyin.tsv -Code ceshi -Text 测试 -Comment "ce shi" -Weight 1000 -DatabasePath D:\MyApp\ArrowInput\seed_pinyin.db
```

合并一批 TSV 到种子词库，跳过重复的 `code+text` 行，验证并重建 SQLite：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\merge_tsv_dictionary.ps1 -TargetPath D:\MyApp\ArrowInput\seed_pinyin.tsv -SourcePath D:\path\to\new_words.tsv -DatabasePath D:\MyApp\ArrowInput\seed_pinyin.db
```

预览合并，不修改目标词库：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\preview_tsv_merge.ps1 -TargetPath D:\MyApp\ArrowInput\experiments\trial1.tsv -SourcePath D:\path\to\new_words.tsv
```

把某个 TSV 中的候选提升到同编码其他候选之前：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\promote_tsv_candidates.ps1 -TargetPath D:\MyApp\ArrowInput\experiments\trial1.tsv -SourcePath D:\MyApp\ArrowInput\seed_pinyin.tsv
```

## 实验词库

从种子词库创建一个隔离的实验词库，并立刻切换到它：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\new_experiment_dictionary.ps1 -Name trial1 -UseNow
```

切换到已有实验：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\use_experiment_dictionary.ps1 -Name trial1
```

编辑实验 TSV 后，验证并重建 SQLite：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\rebuild_experiment_dictionary.ps1 -Name trial1 -UseNow
```

备份、查看备份、恢复实验词库：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\backup_experiment_dictionary.ps1 -Name trial1
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\list_experiment_backups.ps1 -Name trial1
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\restore_experiment_dictionary.ps1 -Name trial1 -UseNow
```

导入外部词库到实验词库。脚本会按需转换源文件、验证、预览合并、备份实验、合并、重建 SQLite，并可选择切换到新 DB：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\import_external_dictionary_to_experiment.ps1 -Name trial1 -SourcePath D:\path\to\source.txt -Delimiter space -UseNow
```

常用参数：

- `-PreviewOnly`：只验证和预览合并，不修改实验词库
- `-SourceFormat rime`：直接导入 Rime 词库
- `-CreateIfMissing`：实验不存在时先从 `seed_pinyin.tsv` 创建
- `-PromoteSourceTsv`：把指定 TSV 中的候选提升到同编码导入候选之前

示例：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\import_external_dictionary_to_experiment.ps1 -Name trial1 -SourcePath D:\path\to\luna_pinyin.dict.yaml -SourceFormat rime -CreateIfMissing -PromoteSourceTsv D:\MyApp\ArrowInput\seed_pinyin.tsv -UseNow
```

## 测试

运行当前所有检查：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_all.ps1 -Configuration Debug
```

只运行算法探针回归测试：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_algorithm_probe.ps1 -Configuration Debug
```

构建一个 SQLite 词库：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\import_tsv_to_sqlite.ps1 -InputPath tools\AlgorithmProbe\sample_dictionary.tsv -OutputPath build\tools\AlgorithmProbe\sample_dictionary.db
```

运行 SQLite 导入冒烟测试：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\test_sqlite_import.ps1
```

查询生成的 SQLite 词库：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\query_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db ni hao
```

查看 SQLite 词库统计，以及某个编码下排名靠前的候选：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\inspect_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Limit 5 -Metadata ni hao
```

检查运行时看到的前缀查询：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\inspect_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Prefix -Limit 5 nih
```

测试精确查询或前缀查询延迟：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\benchmark_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Limit 30 -Iterations 100 ni shi yi
powershell -ExecutionPolicy Bypass -File .\scripts\benchmark_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Prefix -Limit 30 -Iterations 100 nih niha
```

加上 `-Explain` 可以输出 SQLite 查询计划，用来确认索引是否生效：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\benchmark_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -Prefix -Explain nih
```

查找候选冲突较多的编码：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\inspect_sqlite_dictionary.ps1 -DatabasePath build\tools\AlgorithmProbe\sample_dictionary.db -TopCodes 20
```

## 运行时配置

使用生成的 SQLite 词库：

```ini
dictionary_type=sqlite
dictionary_path=build\tools\AlgorithmProbe\sample_dictionary.db
```

对部署目录中的测试副本，可以切换当前用户配置：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\set_dictionary.ps1 -Type sqlite
```

测试实验种子词库：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\set_dictionary.ps1 -Type sqlite -Path D:\MyApp\ArrowInput\seed_pinyin.db
```

切回 TSV：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\set_dictionary.ps1 -Type tsv
```

`set_dictionary.ps1` 默认会检查目标词库路径是否存在。若是故意测试坏路径，可以传入 `-AllowMissing`。

检查部署文件、pending 二进制、当前用户配置和最近运行日志：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\status.ps1
```

退出登录或以其他方式卸载 ArrowInput 后，应用 pending DLL：

```powershell
powershell -ExecutionPolicy Bypass -File D:\MyApp\ArrowInput\apply_pending.ps1
```

## 项目结构

```text
src/                  TSF 输入法主体代码
scripts/              构建、部署、安装、词库和测试脚本
tools/AlgorithmProbe/ 独立算法探针
tools/DictionaryTools/词库处理工具
tests/                回归测试数据
data/                 种子词库和外部词库转换结果
docs/                 设计和数据结构文档
config/               默认配置
```

## 说明

这是一个处于实验和开发阶段的输入法项目。安装和卸载会触碰 Windows TSF 注册信息，建议先在测试环境或熟悉的开发机上使用。
