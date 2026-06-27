# ESP3DLib 支持 Marlin 2.1.x 适配计划

## 摘要

将 ESP3DLib 库修改为支持 Marlin 2.1.x，主要涉及宏定义更新、SD 卡代码重写和条件编译调整。

## 当前状态分析

### 代码结构

* **主要配置文件**: `src/esp3dlibconfig.h` - 包含 Marlin 路径宏和版本信息

* **通信模块**: `src/espcom.cpp` - LCD/UI 处理，已有 `__has_include` 检测

* **SD 卡模块**: `src/sd_ESP32.cpp` - 当前使用 SdFat API（2.1.x 已移除）

* **Web 服务器**: `src/web_server.cpp` - 包含 `#undef DISABLED/_BV` 处理宏冲突

### 现有兼容性代码

```cpp
// espcom.cpp 中已有
#if defined __has_include
#if __has_include (MARLIN_PATH(lcd/ultralcd.h))
#include MARLIN_PATH(lcd/ultralcd.h)
#endif
#if __has_include (MARLIN_PATH(lcd/marlinui.h))
#include MARLIN_PATH(lcd/marlinui.h)
#endif
#endif
```

### Marlin 2.1.x 主要变化

1. **SD 卡**: 移除 `SdFat` 相关头文件 (`sd/cardreader.h`, `sd/SdVolume.h`, `sd/SdFatStructs.h`, `sd/SdFile.h`)
2. **LCD/UI**: 已通过 `__has_include` 兼容
3. **配置版本**: `CONFIGURATION_H_VERSION` 格式 `0201xx00`
4. **宏冲突**: `DISABLED`/`_BV` 在新版本中可能已不存在冲突

***

## 实施计划

### 任务 1: 修改 esp3dlibconfig.h - 添加版本检测和宏适配

**文件**: `src/esp3dlibconfig.h`

**更改内容**:

1. 在包含 `MarlinConfigPre.h` 后添加版本检测:

```cpp
#include MARLIN_PATH(inc/MarlinConfigPre.h)
#undef DISABLED
#undef _BV

// 添加 Marlin 版本检测
#if defined(CONFIGURATION_H_VERSION)
  #if CONFIGURATION_H_VERSION >= 02010000
    #define MARLIN_VERSION_21X
  #endif
#endif
```

1. 调整宏处理顺序（如果需要）

**原因**: 需要检测 Marlin 版本以便条件编译

***

### 任务 2: 修改 espcom.cpp - 确认 LCD/UI 兼容性

**文件**: `src/espcom.cpp`

**当前状态**: 已有 `__has_include` 检测机制，应该已兼容 2.1.x

**检查项**:

* 确认 `HAS_GRAPHICAL_LCD` 宏在 2.1.x 中是否仍然存在

* 确认 `#include <U8glib.h>` 在 2.1.x 中的替代方式

**可能需要的更改**: 添加 `U8gllib.h` 的条件检测

***

### 任务 3: 重写 sd\_ESP32.cpp - 使用 Marlin 内置 SD 卡 API

**文件**: `src/sd_ESP32.cpp` 和 `src/sd_ESP32.h`

**问题**: Marlin 2.1.x 移除了 SdFat 相关头文件，无法直接使用 `SdFile`, `SdVolume` 等类

**重写方案**:
使用 Marlin 内置的 `card` 对象 API (在 `src/module/cardreader.h` 中定义)

主要更改:

1. 移除 SdFat 头文件包含:

```cpp
// 删除这些
// #include MARLIN_PATH(sd/cardreader.h)
// #include MARLIN_PATH(sd/SdVolume.h)
// #include MARLIN_PATH(sd/SdFatStructs.h)
// #include MARLIN_PATH(sd/SdFile.h)
```

1. 改为使用 Marlin 的 cardreader API:

```cpp
#include MARLIN_PATH(module/cardreader.h)
```

1. 替换 SdFat 调用为 Marlin API:

* `SdFile` → 使用 `card` 的成员函数

* `sd_volume` → 通过 `card.mediaStatus()` 等

* `workDir` → 使用 `card.getWorkDir()` 或类似方法

**关键 API 映射** (需要根据真实的 Marlin 2.1.x API 调整):

| 旧 SdFat API        | Marlin cardreader API    |
| ------------------ | ------------------------ |
| `file.isOpen()`    | `card.isFileOpen()`      |
| `file.open(...)`   | `card.openFile(...)`     |
| `card.mount()`     | `card.mount()`           |
| `card.isMounted()` | `card.isMounted()`       |
| `IS_SD_INSERTED()` | `card.mediaIsInserted()` |

***

### 任务 4: 修改 web\_server.cpp - 条件化宏冲突处理

**文件**: `src/web_server.cpp`

**问题**: `#undef DISABLED` 和 `#undef _BV` 在 Marlin 2.1.x 中可能不再需要或会产生问题

**更改**: 条件化这些 `#undef`:

```cpp
#ifdef DISABLED
#undef DISABLED
#endif
#ifdef _BV
#undef _BV
#endif
```

或者检测 Marlin 版本:

```cpp
#ifndef MARLIN_VERSION_21X
#undef DISABLED
#undef _BV
#endif
```

***

### 任务 5: 检查其他文件的头文件引用

**相关文件**:

* `command.cpp` - 包含 `MARLIN_PATH(inc/Version.h)`

* `sd_ESP32.cpp` - SD 相关头文件

* `web_server.cpp` - 注释掉的 queue.h 和 Version.h

**检查**: 确认这些文件在 2.1.x 中的存在性和路径

***

## 假设与决策

1. **SD 卡重写**: 用户确认需要重写 sd\_ESP32.cpp 以使用 Marlin 内置 API，而非条件编译保留旧代码
2. **版本检测**: 使用 `CONFIGURATION_H_VERSION` 宏进行版本检测
3. **向后兼容**: 同时保留对 Marlin 2.0.x 的支持（条件编译）
4. **宏冲突处理**: 使用条件 `#undef` 避免对不支持的宏进行操作

***

## 验证步骤

1. **编译验证**: 使用 Marlin 2.1.x 配置编译 ESP3DLib
2. **功能测试**:

   * WiFi 连接

   * Web 服务器

   * SD 卡访问

   * 串口通信
3. 不需要确保对 Marlin 2.0.x 的兼容性，这是一次激进的修改

***

## 风险与注意事项

1. **Marlin API 变化**: Marlin 2.1.x 的 cardreader API 可能与预期不同，需要根据实际代码调整
2. **SdFat 完全移除**: 2.1.x 中 SdFat 相关功能可能已重构，需要仔细研究新 API
3. **HAL 层变化**: 不同 HAL 实现可能需要不同的适配

