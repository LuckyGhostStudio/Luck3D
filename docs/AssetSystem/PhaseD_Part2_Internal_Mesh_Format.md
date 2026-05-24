# Phase D - Part 2：内部 Mesh 格式（.lmesh）

## 目录

- [一、概述](#一概述)
  - [1.1 本文档范围](#11-本文档范围)
  - [1.2 设计目标](#12-设计目标)
  - [1.3 前置依赖](#13-前置依赖)
  - [1.4 术语定义](#14-术语定义)
- [二、当前问题](#二当前问题)
- [三、.lmesh 文件格式设计](#三lmesh-文件格式设计)
- [四、MeshSerializer 设计](#四meshserializer-设计)
- [五、导入流程改造](#五导入流程改造)
- [六、内置图元持久化](#六内置图元持久化)
- [七、MeshImporter 改造](#七meshimporter-改造)
- [八、涉及的文件清单](#八涉及的文件清单)
- [九、分步实施策略](#九分步实施策略)
- [十、验证清单](#十验证清单)
- [十一、已知限制与后续扩展](#十一已知限制与后续扩展)

---

## 一、概述

### 1.1 本文档范围

本文档是 Phase D 高优先级功能的 **Part 2**，专注于设计和实现 Luck3D 内部 Mesh 格式（.lmesh）。

| 原始文档 | 拆分来源 |
|---------|---------|
| `PhaseD_High_Priority_Features.md` | 第三章：Part 2：内部 Mesh 格式（.lmesh） |

### 1.2 设计目标

1. ? 设计 `.lmesh` 内部二进制格式（快速加载）
2. ? 实现 `MeshSerializer`（序列化/反序列化 Mesh 到 `.lmesh`）
3. ? 导入外部模型时，转换为 `.lmesh` 内部格式
4. ? 内置图元可保存为 `.lmesh` 文件
5. ? `MeshImporter`（资产系统层）改为加载 `.lmesh` 文件

### 1.3 前置依赖

| 依赖 | 状态 | 说明 |
|------|------|------|
| Phase A 资产系统核心框架 | ? 已完成 | AssetHandle / AssetRegistry / AssetManager / AssetImporter |
| ModelLoader (Assimp) | ? 已完成 | 外部模型文件解析（原 MeshImporter，已重命名） |
| Mesh 类 | ? 已完成 | 顶点/索引/SubMesh 数据结构 |
| MeshFactory | ? 已完成 | 内置图元生成 |

### 1.4 术语定义

| 术语 | 定义 |
|------|------|
| **.lmesh** | Luck3D 引擎唯一认识的内部 Mesh 资产格式（二进制），所有 Mesh 引用（MeshFilter、AssetField\<Mesh\>、脚本等）都指向它 |
| **源文件** | 外部导入的原始文件（.fbx/.obj 等），不进入 Assets 目录，仅在导入时从外部路径读取 |
| **ModelLoader** | 底层模型解析器（Assimp 封装），负责解析外部 3D 模型文件，类似于 stbi_load 对图片格式的解析 |
| **MeshImporter** | 资产系统层的 Mesh 导入器（继承 AssetImporter），负责加载 .lmesh 文件 |

---

## 二、当前问题

当前 Mesh 资产直接引用外部模型文件（`.fbx/.obj`），每次加载都需要 Assimp 重新解析：

```cpp
// 当前 MeshImporter.cpp（资产系统层）
Ref<void> MeshImporter::Load(const AssetMetadata& metadata)
{
    ModelLoadResult result = ModelLoader::Load(absolutePath);  // 每次都调用 Assimp
    return result.MeshData;
}
```

| 问题 | 影响 |
|------|------|
| 每次加载都需 Assimp 解析 | 加载速度慢（Assimp 解析 .fbx 需要数百毫秒） |
| 无法序列化内置图元 | Cube/Sphere 等内置图元无法保存为资产文件 |
| 依赖外部文件格式 | 外部文件格式复杂，解析可能有兼容性问题 |
| 无法存储引擎特有数据 | 如预计算的切线、LOD 数据等 |

---

## 三、.lmesh 文件格式设计

### 方案 A：纯二进制格式（? 推荐）

```
┌─────────────────────────────────────────────────┐
│ Header (固定大小)                                │
│   Magic: "LMSH" (4 bytes)                       │
│   Version: uint32_t (4 bytes)                   │
│   VertexCount: uint32_t (4 bytes)               │
│   IndexCount: uint32_t (4 bytes)                │
│   SubMeshCount: uint32_t (4 bytes)              │
│   NameLength: uint32_t (4 bytes)                │
│   Flags: uint32_t (4 bytes, 预留)               │
├─────────────────────────────────────────────────┤
│ Name (NameLength bytes, UTF-8)                  │
├─────────────────────────────────────────────────┤
│ Vertices (VertexCount × sizeof(Vertex))         │
│   每个 Vertex:                                   │
│     Position: vec3 (12 bytes)                   │
│     Color: vec4 (16 bytes)                      │
│     Normal: vec3 (12 bytes)                     │
│     TexCoord: vec2 (8 bytes)                    │
│     Tangent: vec4 (16 bytes)                    │
│   共 64 bytes/vertex                            │
├─────────────────────────────────────────────────┤
│ Indices (IndexCount × sizeof(uint32_t))         │
├─────────────────────────────────────────────────┤
│ SubMeshes (SubMeshCount × sizeof(SubMeshData))  │
│   每个 SubMeshData:                              │
│     IndexOffset: uint32_t (4 bytes)             │
│     IndexCount: uint32_t (4 bytes)              │
│     VertexCount: uint32_t (4 bytes)             │
│     MaterialIndex: uint32_t (4 bytes)           │
│   共 16 bytes/submesh                           │
└─────────────────────────────────────────────────┘
```

**优点**：
- 加载速度极快（直接 memcpy 到内存）
- 文件体积最小
- 无解析开销

**缺点**：
- 不可人类阅读
- 调试困难
- 版本升级时需要处理兼容性（通过 Version 字段）
- Git diff 不友好

### 方案 B：YAML 格式

```yaml
Mesh:
  Name: Cube
  Vertices:
    - Position: [0.5, 0.5, 0.5]
      Normal: [0, 0, 1]
      TexCoord: [1, 1]
      ...
  Indices: [0, 1, 2, 2, 3, 0, ...]
  SubMeshes:
    - IndexOffset: 0
      IndexCount: 36
      VertexCount: 24
      MaterialIndex: 0
```

**优点**：人类可读，Git 友好，调试方便
**缺点**：文件体积巨大，加载速度慢，高精度模型文件可达数十 MB

### 方案 C：混合格式（YAML Header + 二进制 Body）

```
[YAML Header]
LMesh:
  Version: 1
  Name: Cube
  VertexCount: 24
  IndexCount: 36
  SubMeshCount: 1
  SubMeshes:
    - IndexOffset: 0
      IndexCount: 36
      VertexCount: 24
      MaterialIndex: 0
---BINARY---
<顶点数据二进制>
<索引数据二进制>
```

**优点**：Header 可读，Body 高效
**缺点**：实现复杂度高，需要自定义解析器

### 方案推荐

| 方案 | 推荐度 | 理由 |
|------|--------|------|
| **方案 A：纯二进制** | ??? 最优 | Mesh 数据量大，性能是首要考虑；Version 字段保证向前兼容；调试可通过日志输出元信息 |
| 方案 C：混合格式 | ?? | 折中但实现复杂 |
| 方案 B：YAML | ? | 性能不可接受，仅适合极小的测试 Mesh |

**推荐方案 A**。

---

## 四、MeshSerializer 设计

```cpp
// MeshSerializer.h
#pragma once

#include "Lucky/Renderer/Mesh.h"

namespace Lucky
{
    /// <summary>
    /// .lmesh 文件头
    /// </summary>
    struct LMeshHeader
    {
        char Magic[4] = { 'L', 'M', 'S', 'H' };   // 魔数
        uint32_t Version = 1;                       // 格式版本
        uint32_t VertexCount = 0;                   // 顶点数量
        uint32_t IndexCount = 0;                    // 索引数量
        uint32_t SubMeshCount = 0;                  // 子网格数量
        uint32_t NameLength = 0;                    // 名称长度（字节）
        uint32_t Flags = 0;                         // 预留标志位
    };

    /// <summary>
    /// Mesh 序列化器：负责 .lmesh 二进制文件的读写
    /// 全静态方法，与 MaterialSerializer 风格一致
    /// </summary>
    class MeshSerializer
    {
    public:
        /// <summary>
        /// 将 Mesh 序列化到 .lmesh 文件
        /// </summary>
        /// <param name="mesh">要序列化的 Mesh</param>
        /// <param name="filepath">输出文件路径</param>
        /// <returns>是否成功</returns>
        static bool Serialize(const Ref<Mesh>& mesh, const std::string& filepath);

        /// <summary>
        /// 从 .lmesh 文件反序列化 Mesh
        /// </summary>
        /// <param name="filepath">文件路径</param>
        /// <returns>反序列化的 Mesh（失败返回 nullptr）</returns>
        static Ref<Mesh> Deserialize(const std::string& filepath);
    };
}
```

### 完整实现

```cpp
// MeshSerializer.cpp
#include "lcpch.h"
#include "MeshSerializer.h"

#include <fstream>
#include <filesystem>

namespace Lucky
{
    bool MeshSerializer::Serialize(const Ref<Mesh>& mesh, const std::string& filepath)
    {
        if (!mesh)
        {
            LF_CORE_ERROR("MeshSerializer::Serialize - Mesh is null!");
            return false;
        }

        // 确保目录存在
        std::filesystem::path path(filepath);
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }

        std::ofstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            LF_CORE_ERROR("MeshSerializer::Serialize - Failed to open file: {0}", filepath);
            return false;
        }

        const auto& vertices = mesh->GetVertices();
        const auto& subMeshes = mesh->GetSubMeshes();
        const std::string& name = mesh->GetName();

        // 写入 Header
        LMeshHeader header;
        header.VertexCount = mesh->GetVertexCount();
        header.IndexCount = mesh->GetVertexIndexCount();
        header.SubMeshCount = mesh->GetSubMeshCount();
        header.NameLength = static_cast<uint32_t>(name.size());

        file.write(reinterpret_cast<const char*>(&header), sizeof(LMeshHeader));

        // 写入名称
        file.write(name.data(), name.size());

        // 写入顶点数据
        file.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(Vertex));

        // 写入索引数据
        const auto& indices = mesh->GetIndices();
        file.write(reinterpret_cast<const char*>(indices.data()), indices.size() * sizeof(uint32_t));

        // 写入 SubMesh 数据
        for (const auto& subMesh : subMeshes)
        {
            file.write(reinterpret_cast<const char*>(&subMesh), sizeof(SubMesh));
        }

        file.close();

        LF_CORE_INFO("MeshSerializer: Saved mesh '{0}' to '{1}' ({2} vertices, {3} indices, {4} submeshes)",
            name, filepath, header.VertexCount, header.IndexCount, header.SubMeshCount);
        return true;
    }

    Ref<Mesh> MeshSerializer::Deserialize(const std::string& filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - File not found: {0}", filepath);
            return nullptr;
        }

        std::ifstream file(filepath, std::ios::binary);
        if (!file.is_open())
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - Failed to open file: {0}", filepath);
            return nullptr;
        }

        // 读取 Header
        LMeshHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(LMeshHeader));

        // 验证魔数
        if (header.Magic[0] != 'L' || header.Magic[1] != 'M' ||
            header.Magic[2] != 'S' || header.Magic[3] != 'H')
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - Invalid file format: {0}", filepath);
            return nullptr;
        }

        // 验证版本
        if (header.Version != 1)
        {
            LF_CORE_ERROR("MeshSerializer::Deserialize - Unsupported version {0}: {1}", header.Version, filepath);
            return nullptr;
        }

        // 读取名称
        std::string name(header.NameLength, '\0');
        file.read(name.data(), header.NameLength);

        // 读取顶点数据
        std::vector<Vertex> vertices(header.VertexCount);
        file.read(reinterpret_cast<char*>(vertices.data()), header.VertexCount * sizeof(Vertex));

        // 读取索引数据
        std::vector<uint32_t> indices(header.IndexCount);
        file.read(reinterpret_cast<char*>(indices.data()), header.IndexCount * sizeof(uint32_t));

        // 读取 SubMesh 数据
        std::vector<SubMesh> subMeshes(header.SubMeshCount);
        for (uint32_t i = 0; i < header.SubMeshCount; ++i)
        {
            file.read(reinterpret_cast<char*>(&subMeshes[i]), sizeof(SubMesh));
        }

        file.close();

        // 创建 Mesh
        Ref<Mesh> mesh = CreateRef<Mesh>(vertices, indices, subMeshes);
        mesh->SetName(name);

        LF_CORE_INFO("MeshSerializer: Loaded mesh '{0}' from '{1}' ({2} vertices, {3} indices)",
            name, filepath, header.VertexCount, header.IndexCount);
        return mesh;
    }
}
```

---

## 五、导入流程改造

导入外部模型时，流程变为：

```
外部 .fbx/.obj（不在 Assets 目录中）
    ↓ ModelLoader::Load()（Assimp 解析）
Mesh 内存对象
    ↓ MeshSerializer::Serialize()
Assets/Meshes/xxx.lmesh（引擎内部 Mesh 资产，唯一注册到 Registry 的文件）
```

**关键设计决策**：
- 原始 `.fbx/.obj` 文件**不复制到 Assets 目录**，导入时直接从外部路径读取并转换
- Assets 目录中只存在 `.lmesh` 文件，它是引擎唯一认识的 Mesh 格式
- 所有 Mesh 引用（MeshFilter、场景序列化、AssetField 等）都通过 AssetHandle 指向 `.lmesh` 文件
- 如果未来需要重新导入（如美术修改了模型），用户需重新指定外部 `.fbx` 路径
- 资产扫描流程**不识别** `.fbx/.obj` 等外部模型格式??即使用户手动将 `.fbx` 放入 Assets 目录，扫描时也会被忽略（当作引擎不认识的文件，不注册）
- 模型导入**只能通过编辑器主动触发**（菜单/拖拽到编辑器窗口），不支持被动扫描导入
- 这与 Texture 不同：`.png/.jpg` 是引擎可直接使用的格式，可以被动扫描注册；`.fbx` 不是，必须先转换为 `.lmesh`

### EditorLayer::ImportModel 改造

```cpp
void EditorLayer::ImportModel(const std::filesystem::path& externalFilepath)
{
    // 1. 通过 ModelLoader 解析外部模型文件（不在 Assets 目录中）
    ModelLoadResult result = ModelLoader::Load(externalFilepath.string());
    if (!result.Success) { /* 错误处理 */ return; }
    
    // 2. 确定 .lmesh 输出路径（在 Assets 目录中）
    std::string meshName = externalFilepath.stem().string();
    std::filesystem::path lmeshPath = "Assets/Meshes/" + meshName + ".lmesh";
    
    std::string absoluteLmeshPath = std::filesystem::absolute(lmeshPath).string();
    MeshSerializer::Serialize(result.MeshData, absoluteLmeshPath);
    
    // 3. 注册 .lmesh 文件到资产系统（原始 .fbx 不注册）
    std::string normalizedPath = lmeshPath.generic_string();
    AssetHandle meshHandle = AssetManager::ImportAsset(normalizedPath, AssetType::Mesh);
    
    // 4. 设置 Handle
    result.MeshData->SetHandle(meshHandle);
    
    // 5. 材质处理（同 Phase B）
    // ...
}
```

---

## 六、内置图元持久化

### 方案 A：启动时自动生成（? 推荐）

```cpp
// AssetManager::Init() 中
void AssetManager::InitBuiltinMeshAssets()
{
    const std::string builtinDir = "Assets/Meshes/Builtin/";
    
    struct BuiltinMeshDef
    {
        PrimitiveType Type;
        const char* Name;
    };
    
    BuiltinMeshDef builtins[] = {
        { PrimitiveType::Cube, "Cube" },
        { PrimitiveType::Sphere, "Sphere" },
        { PrimitiveType::Plane, "Plane" },
        { PrimitiveType::Cylinder, "Cylinder" },
        { PrimitiveType::Capsule, "Capsule" },
    };
    
    for (const auto& def : builtins)
    {
        std::string filepath = builtinDir + def.Name + ".lmesh";
        
        // 如果文件不存在则生成
        if (!std::filesystem::exists(filepath))
        {
            Ref<Mesh> mesh = MeshFactory::CreatePrimitive(def.Type);
            mesh->SetName(def.Name);
            MeshSerializer::Serialize(mesh, filepath);
        }
        
        // 注册到资产系统
        ImportAsset(filepath, AssetType::Mesh);
    }
}
```

**优点**：
- 内置图元也有 AssetHandle，可被场景引用
- 统一管理
- 可在 ProjectAssetsPanel 中浏览

**缺点**：
- 启动时有额外 I/O（仅首次）

### 方案 B：内置图元不持久化，运行时生成

使用固定的 Handle（硬编码），不保存为文件。

**优点**：无额外文件，启动更快
**缺点**：硬编码 Handle 不灵活，不在 Registry 中

### 方案推荐

| 方案 | 推荐度 | 理由 |
|------|--------|------|
| **方案 A：启动时自动生成** | ??? 最优 | 统一管理，内置图元也是资产，可在 ProjectAssetsPanel 中浏览 |
| 方案 B：运行时生成 | ?? | 简单但不统一 |

**推荐方案 A**。

---

## 七、MeshImporter 改造

> **注意**：此处的 `MeshImporter` 是资产系统层的导入器（继承 `AssetImporter`），
> 原名 `MeshAssetImporter`，已重命名以与 `TextureImporter`、`MaterialImporter` 等保持一致。
>
> `MeshImporter` **只负责加载 `.lmesh` 文件**，不兼容任何外部模型格式（`.fbx/.obj` 等）。
> 外部模型的解析由 `ModelLoader` 在导入阶段完成，导入完成后 Assets 中只存在 `.lmesh`。

```cpp
// MeshImporter.cpp 改造后（资产系统层）
#include "lcpch.h"
#include "MeshImporter.h"

#include "Lucky/Serialization/MeshSerializer.h"

#include <filesystem>

namespace Lucky
{
    Ref<void> MeshImporter::Load(const AssetMetadata& metadata)
    {
        std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

        // 只支持 .lmesh 格式（引擎内部 Mesh 资产格式）
        Ref<Mesh> mesh = MeshSerializer::Deserialize(absolutePath);
        if (!mesh)
        {
            LF_CORE_ERROR("MeshImporter: Failed to load .lmesh: '{0}'", absolutePath);
            return nullptr;
        }
        return mesh;
    }
}
```

---

## 八、涉及的文件清单

| 文件路径 | 修改内容 |
|---------|----------|
| `Lucky/Source/Lucky/Serialization/MeshSerializer.h`（新建） | MeshSerializer + LMeshHeader 声明 |
| `Lucky/Source/Lucky/Serialization/MeshSerializer.cpp`（新建） | Serialize / Deserialize 实现 |
| `Lucky/Source/Lucky/Renderer/Mesh.h` | 新增 `GetIndices()` 公有接口（暴露索引数据） |
| `Lucky/Source/Lucky/Asset/MeshImporter.cpp` | 改造：只加载 .lmesh 格式，不兼容外部模型格式 |
| `Lucky/Source/Lucky/Asset/AssetType.h` | 添加 `.lmesh` 扩展名映射 |
| `Lucky/Source/Lucky/Asset/AssetManager.cpp` | 新增 `InitBuiltinMeshAssets()` |
| `Luck3DApp/Source/EditorLayer.cpp` | ImportModel 改造：从外部路径读取模型，转换为 .lmesh 保存到 Assets |

---

## 九、分步实施策略

| 步骤 | 内容 | 依赖 | 预估工作量 |
|------|------|------|-----------|
| Step 1 | Mesh.h 新增 `GetIndices()` 公有接口 | 无 | 极小 |
| Step 2 | 创建 MeshSerializer.h/cpp | Step 1 | 中 |
| Step 3 | AssetType.h 添加 `.lmesh` 扩展名映射 | 无 | 极小 |
| Step 4 | MeshImporter.cpp 改造（支持 .lmesh） | Step 2, 3 | 小 |
| Step 5 | EditorLayer::ImportModel 改造（从外部路径导入，保存 .lmesh 到 Assets） | Step 2 | 小 |
| Step 6 | AssetManager 内置图元持久化 | Step 2 | 小 |
| Step 7 | 编译测试 + 验证 | 全部 | 小 |

---

## 十、验证清单

| # | 验证项 | 预期结果 |
|---|--------|--------|
| 1 | 编译通过 | 无编译错误 |
| 2 | 导入外部模型 | 从外部路径读取 .fbx，在 Assets/Meshes/ 下生成 .lmesh 文件，注册到 Registry |
| 3 | .lmesh 文件大小合理 | 二进制格式，体积远小于 YAML |
| 4 | 重启后加载 .lmesh | 从 .lmesh 快速加载（不调用 Assimp），模型正确显示 |
| 5 | 内置图元 .lmesh | Assets/Meshes/Builtin/ 下生成 5 个 .lmesh 文件 |
| 6 | 内置图元通过 AssetManager 加载 | Cube/Sphere 等有 AssetHandle，可在 Registry 中查到 |
| 7 | 场景保存/加载 | MeshFilter 的 AssetHandle 引用 .lmesh 文件，加载正确 |
| 8 | 原始 .fbx 不在 Assets 目录中 | Assets 目录下只有 .lmesh 文件，无 .fbx/.obj 等外部格式 |

---

## 十一、已知限制与后续扩展

| 限制 | 影响 | 后续优化方向 |
|------|------|-------------|
| .lmesh 无压缩 | 大模型文件体积较大 | 后续可添加 LZ4/Zstd 压缩 |
| .lmesh 无 LOD 数据 | 无多级细节 | 后续在 Header 中扩展 LOD 信息 |
| 无骨骼动画数据 | 不支持骨骼 Mesh | 后续扩展 Header 和数据段 |
| 内置图元文件可能被误删 | 用户可能在文件管理器中删除 | 启动时自动检查并重新生成（EnsureAsset 模式） |
| Git diff 不友好 | 二进制文件无法 diff | 可通过 .gitattributes 标记为 binary |
