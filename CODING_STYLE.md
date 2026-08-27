# C++ Coding Style Guide

This document defines the preferred naming and formatting conventions for first-party C++ code. The goal is consistency, readability, and code that is easy to scan without excessive formatting noise.

## Naming

| Construct | Convention | Examples |
|---|---|---|
| Classes, structs, aliases, concepts, templates, namespaces | `UpperCamelCase` | `ProjectInfo`, `ProgramAnalysisManager` |
| Enum types | `EUpperCamelCase` | `EApplicationState`, `ELogLevel` |
| Enum values | `UpperCamelCase` | `EApplicationState::ProjectOpen` |
| Functions and methods | `UpperCamelCase` | `LoadProject()`, `ValidateMetadata()` |
| Public struct/data fields | `UpperCamelCase` | `DisplayName`, `CreatedUtc` |
| Private/protected members | `UpperCamelCase_` | `State_`, `ActiveProject_` |
| Parameters and local variables | `lowerCamelCase` | `projectInfo`, `displayName` |
| Named constants | `KUpperCamelCase` | `KMaxBlocks`, `KCurrentFormatVersion` |

Treat initialisms as words inside identifiers:

```text
Id Uuid Utc Ui Vm Ir Pdb Api Json Sqlite
```

Prefer:

```cpp
ProjectId
CreatedUtc
SqliteConnection
VirtualIrBuilder
```

rather than forms such as `ProjectID`, `CreatedUTC`, or `SQLITEConnection`.

Keep external names in their official form when appropriate, such as `Qt`, `FFmpeg`, or library APIs.

## Files and Directories

First-party C++ files and source directories use `UpperCamelCase`.

```text
ProgramAnalysis/
ProgramAnalysisManager.h
ProgramAnalysisManager.cpp
```

Do not rename third-party files or persisted interfaces merely to match source style.

## Function Parameters and Calls

Do not chop function parameters, prototypes, or call arguments across multiple lines under normal circumstances.

Prefer:

```cpp
Result<ProjectInfo> OpenProject(const std::filesystem::path& projectFile, const bool validateFirst);
```

and:

```cpp
auto result = OpenProject(projectFile, true);
```

rather than vertically splitting every parameter or argument.

Only split when a line would become genuinely unreasonable or the expression is otherwise substantially harder to read.

## Initializers

Short initializers may remain on one line:

```cpp
Point point{10, 20};
```

When an initializer is split across lines, place the opening brace on its own line:

```cpp
ProjectInfo projectInfo
{
    id,
    displayName,
    createdUtc,
    modifiedUtc,
    KCurrentFormatVersion
};
```

Do not write:

```cpp
ProjectInfo projectInfo{
    id,
    displayName,
    createdUtc
};
```

The same rule applies to larger aggregate, container, and designated initializers.

## Logical Spacing

Use blank lines to separate distinct logical groups.

Declarations and immediate setup should be visually separated from later behavioral logic.

Prefer:

```cpp
MyStruct value = {};
value.Field = 5;

DoStuff();
DoOtherStuff();

int count = 1;

for (;;)
{
    Process(count);
}

return;
```

Do not compress unrelated declarations and actions into one uninterrupted block.

Closely related declarations may remain together:

```cpp
const auto start = range.Start;
const auto end = range.End;
const auto duration = end - start;

PlayRange(start, end);
```

## Control Flow

Put a blank line before a control-flow statement when it follows other work, and a blank line after the complete control-flow unit before the next independent operation.

Prefer:

```cpp
PrepareState();

if (!IsValid())
{
    return;
}

CommitState();
```

Associated branches remain attached:

```cpp
if (condition)
{
    HandleTrue();
}
else
{
    HandleFalse();
}

ContinueWork();
```

The same applies to:

- `if` / `else if` / `else`
- `for`
- `while`
- `do` / `while`
- `switch`
- `try` / `catch`

Do not insert blank lines mechanically after every opening brace or before every closing brace.

## Declarations Inside Control Flow

Prefer declarations before the control statement rather than hiding them inside the condition.

Prefer:

```cpp
auto result = LoadProject();

if (!result)
{
    return std::unexpected(result.error());
}
```

rather than:

```cpp
if (auto result = LoadProject(); !result)
{
    return std::unexpected(result.error());
}
```

Likewise, avoid declarations in loop conditions when a separate declaration is clearer.

Prefer:

```cpp
QLayoutItem* item = layout->takeAt(0);

while (item != nullptr)
{
    RemoveItem(item);

    item = layout->takeAt(0);
}
```

## Terminal Flow

Separate terminal control flow from preceding work when it is not the only statement in the block.

```cpp
Cleanup();

return;
```

This applies to:

- `return`
- `throw`
- `break`
- `continue`

## Comments

Comments should explain:

- intent;
- ownership;
- invariants;
- ordering requirements;
- non-obvious constraints;
- why a design choice exists.

Avoid comments that simply restate the code.

Prefer concise comments at file, type, function, and logical-block boundaries. Use line-level comments only for genuinely subtle operations.

A competent C++ developer should be able to follow the important control and data flow without already knowing the subsystem.

## General Principles

- Prefer clear, explicit code over compressed cleverness.
- Keep one concept under one authority.
- Avoid speculative abstractions.
- Keep functions focused, but do not fragment simple logic into excessive helper functions.
- Preserve external and persisted naming conventions where compatibility matters.
- Make ownership and lifetime visible.
- Keep related code together and unrelated work visually separated.
- Optimize for readability when scanning real implementation code, not for minimizing line count.
