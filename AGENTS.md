# AGENTS.md

Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

**Tradeoff:** These guidelines bias toward caution over speed. For trivial tasks, use judgment.

## 项目介绍

 env-pro 是一个基于 [M5StickS3](./docs/M5StickS3.md) 和 [Unit ENV pro](./docs/unit-env-pro.md) 开发的来获取温度、气压、湿度、气体便携设备

## 1. Think Before Coding

**Don't assume. Don't hide confusion. Surface tradeoffs.**

Before implementing:
- State your assumptions explicitly. If uncertain, ask.
- If multiple interpretations exist, present them - don't pick silently.
- If a simpler approach exists, say so. Push back when warranted.
- If something is unclear, stop. Name what's confusing. Ask.

## 2. Simplicity First

**Minimum code that solves the problem. Nothing speculative.**

- No features beyond what was asked.
- No abstractions for single-use code.
- No "flexibility" or "configurability" that wasn't requested.
- No error handling for impossible scenarios.
- If you write 200 lines and it could be 50, rewrite it.

Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

## 3. Surgical Changes

**Touch only what you must. Clean up only your own mess.**

When editing existing code:
- Don't "improve" adjacent code, comments, or formatting.
- Don't refactor things that aren't broken.
- Match existing style, even if you'd do it differently.
- If you notice unrelated dead code, mention it - don't delete it.

When your changes create orphans:
- Remove imports/variables/functions that YOUR changes made unused.
- Don't remove pre-existing dead code unless asked.

The test: Every changed line should trace directly to the user's request.

## 4. Goal-Driven Execution

**Define success criteria. Loop until verified.**

Transform tasks into verifiable goals:
- "Add validation" → "Write tests for invalid inputs, then make them pass"
- "Fix the bug" → "Write a test that reproduces it, then make it pass"
- "Refactor X" → "Ensure tests pass before and after"

For multi-step tasks, state a brief plan:
```
1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
```

Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.


## 5. 优先使用的工具

今年是 2026 年，结合你的知识库来判断你的知识是否落伍/过时

| 场景 | 工具 | 用途 |
|------|------|------|
| M5Stack API / 函数签名 / 参数 | **context7** MCP | 查询 M5StickS3 官方文档 |
| 硬件规格 / 引脚定义 / 接线 | **context7** MCP | 查官方产品文档 |
| 通用技术问题 / 最佳实践 | **tavily** / **searxng** MCP | 搜索社区方案、GitHub Issues |
| 代码示例 / 开源项目参考 | **web_search** / **web_fetch** | 查找可运行的示例 |
| 库兼容性 / 已知 Bug | **context7** + **web_search** | 文档 + 社区验证 |
| Github 源码阅读 | **github** | 源码阅读 |

## 6. 查询顺序

```
遇到不确定
    ↓
能否用 context7 查官方文档？
    ↓ 是 → 查文档，获取准确信息
    ↓ 否 / 文档不清
能否用 tavily/searxng 搜索社区方案？
    ↓ 是 → 搜索并交叉验证 2-3 个来源
    ↓ 否 / 搜索结果矛盾
停下来，向用户说明困惑点，请求澄清
```

## 7. 不编造规则

- **不编造 API 参数**：如果不确定 `M5.Power.getBatteryVoltage()` 的返回值类型，用 **context7** 查 M5Unified 文档。
- **不编造引脚定义**：如果不确定 EXT 2.54-14P 的 `SCK` 对应哪个 GPIO，用 **context7** 查 Cardputer-Adv PinMap。
- **不编造硬件限制**：如果不确定 `ESP32-S3` 的 BLE 是否支持 Classic Bluetooth，用 **searxng** 搜索官方 specs。
- **不编造库版本**：如果不确定 `RadioLib` 是否支持 SX1262 的特定功能，用 **context7** 查库文档或 GitHub release notes。

## 8. 项目特定规则

- 使用 **PlatformIO** 构建系统（`platformio.ini`）
- 框架：**Arduino**（ESP32-S3）
- 核心库：**M5Unified** + **M5GFX** + **M5PM1**
- 代码风格：遵循现有项目的风格，如果不确定，问用户或查 `.clang-format`
- 所有新功能必须有可验证的 **test** 或 **demo**

---

## 9. git commit 规范

- 使用中文
- 标题行使用 conventional commits 格式（feat/fix/refactor/chore 等）
- body 中按文件或功能分组，说明改了什么、为什么改、影响范围
- 修复 bug 需说明根因；架构决策需简要说明理由

---

**These guidelines are working if:** fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.
