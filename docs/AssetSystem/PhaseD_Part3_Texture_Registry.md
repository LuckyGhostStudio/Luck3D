# Phase D - Part 3：Texture 纳入 Registry 管理

## 目录

- [一、概述](#一概述)
  - [1.1 本文档范围](#11-本文档范围)
  - [1.2 设计目标](#12-设计目标)
  - [1.3 前置依赖](#13-前置依赖)
- [二、当前问题](#二当前问题)
- [三、纹理注册方案设计](#三纹理注册方案设计)
- [四、材质文件中纹理引用改造](#四材质文件中纹理引用改造)
- [五、TextureImporter 改造](#五textureimporter-改造)
- [六、MaterialSerializer 改造](#六materialserializer-改造)
- [七、涉及的文件清单](#七涉及的文件清单)
- [八、分步实施策略](#八分步实施策略)
- [九、验证清单](#九验证清单)
- [十、已知限制与后续扩展](#十已知限制与后续扩展)

---

## 一、概述

### 1.1 本文档范围

本文档是 Phase D 高优先级功能的 **Part 3**，专注于将 Texture 纳入 Registry 统一管理。

| 原始文档 | 拆分来源 |
|---------|---------|
| `PhaseD_High_Priority_Features.md` | 第四章：Part 3：Texture 纳入 Registry 管理 |

### 1.2 设计目标

1. ? 纹理文件导入时自动注册到 Registry（获得 AssetHandle）
2. ? `.lmat` 文件中纹理引用改为 AssetHandle
3. ? 纹理通过 AssetManager 统一加载和缓存
4. ? 向后兼容：支持旧格式（路径引用）的回退加载

### 1.3 前置依赖

| 依赖 | 状态 | 说明 |
|------|------|------|
| Phase A 资产系统核心框架 | ? 已完成 | AssetHandle / AssetRegistry / AssetManager / AssetImporter |
| Phase B 独立材质文件 | ? 已完成 | MaterialSerializer::SerializeToFile/DeserializeFromFile |
| TextureImporter | ? 已完成 | 图片文件加载为 Texture2D |

---

## 二、当前问题

当前纹理在 `.lmat` 文件中通过相对路径引用：

```yaml
# 当前 .lmat 文件中的纹理引用
Properties:
  - Name: _AlbedoMap
    Type: Sampler2D
    Value: Assets/Textures/Metal_Albedo.png
```

| 问题 | 影响 |
|------|------|
| 纹理未注册到 Registry | 无法通过 AssetHandle 引用 |
| 路径引用 | 移动/重命名纹理文件后，所有引用该纹理的 .lmat 文件都失效 |
| 无统一缓存 | 同一纹理被多个材质引用时，可能创建多个 GPU 纹理对象 |
| 无法在 ProjectAssetsPanel 中管理 | 纹理不是"已注册资产" |

---

## 三、纹理注册方案设计

### 方案 A：自动注册（导入时）（? 推荐）

纹理在以下时机自动注册到 Registry：
1. **导入模型时**：模型引用的纹理自动注册
2. **创建/保存材质时**：材质引用的纹理自动注册
3. **AssetManager::Init 时**：扫描 Assets 目录中的图片文件自动注册

```cpp
// 纹理注册辅助函数
AssetHandle AssetManager::EnsureTextureRegistered(const std::string& texturePath)
{
    // 检查是否已注册
    AssetHandle existing = GetAssetHandle(texturePath);
    if (existing.IsValid())
    {
        return existing;
    }
    
    // 注册新纹理
    return ImportAsset(texturePath, AssetType::Texture2D);
}
```

**优点**：
- 用户无感知，自动完成
- 不需要手动导入纹理

**缺点**：
- 需要在多个地方调用注册逻辑
- 首次使用时可能有延迟

### 方案 B：手动注册（通过 ProjectAssetsPanel）

用户需要在 ProjectAssetsPanel 中右键 → Import Texture 来注册纹理。

**优点**：用户有完全控制权
**缺点**：用户体验差，操作繁琐

### 方案推荐

| 方案 | 推荐度 | 理由 |
|------|--------|------|
| **方案 A：自动注册** | ??? 最优 | 用户无感知，体验好，与 Unity 行为一致 |
| 方案 B：手动注册 | ? | 体验差，不推荐 |

**推荐方案 A**。

---

## 四、材质文件中纹理引用改造

### 方案 A：纯 AssetHandle 引用

```yaml
Properties:
  - Name: _AlbedoMap
    Type: Sampler2D
    Value: 3847562910384756    # AssetHandle
```

**优点**：
- 移动/重命名纹理文件后引用不断裂
- 完全统一的引用机制

**缺点**：
- 不可人类直接阅读（UUID 无语义）
- 依赖 Registry 正确性

### 方案 B：混合方案（Handle + Path）（? 推荐）

```yaml
Properties:
  - Name: _AlbedoMap
    Type: Sampler2D
    Value:
      Handle: 3847562910384756
      Path: Assets/Textures/Metal_Albedo.png
```

**优点**：
- Handle 失效时有路径兜底
- 人类可读（路径提供语义信息）
- 向后兼容

**缺点**：
- 格式稍复杂
- 需要维护两份引用的一致性

### 方案 C：保持路径引用，仅注册 Registry

```yaml
Properties:
  - Name: _AlbedoMap
    Type: Sampler2D
    Value: Assets/Textures/Metal_Albedo.png    # 仍然是路径
```

**优点**：.lmat 文件格式不变，改动最小
**缺点**：移动文件后仍然断裂

### 方案推荐

| 方案 | 推荐度 | 理由 |
|------|--------|------|
| **方案 B：混合方案** | ??? 最优 | 兼具可读性和健壮性，Handle 失效时路径兜底，向后兼容旧 .lmat 文件 |
| 方案 A：纯 Handle | ?? | 最统一但可读性差 |
| 方案 C：保持路径 | ? | 改动最小但未解决核心问题 |

**推荐方案 B**。

---

## 五、TextureImporter 改造

TextureImporter 当前实现已经可用，无需大改。主要确保通过 AssetManager 获取纹理时能正确利用缓存：

```cpp
// TextureImporter.cpp（无需修改，当前实现已满足）
Ref<void> TextureImporter::Load(const AssetMetadata& metadata)
{
    std::string absolutePath = std::filesystem::absolute(metadata.FilePath).string();

    if (!std::filesystem::exists(absolutePath))
    {
        LF_CORE_ERROR("TextureImporter: File not found: '{0}'", absolutePath);
        return nullptr;
    }

    return Texture2D::Create(absolutePath);
}
```

---

## 六、MaterialSerializer 改造

### SerializeToFile 中纹理属性序列化改造

```cpp
case ShaderUniformType::Sampler2D:
{
    const Ref<Texture2D>& texture = std::get<Ref<Texture2D>>(prop.Value);
    out << YAML::Key << "Value" << YAML::Value;
    out << YAML::BeginMap;
    
    if (texture && !texture->GetPath().empty())
    {
        // 确保纹理已注册到 Registry
        std::filesystem::path absPath(texture->GetPath());
        std::filesystem::path relPath = std::filesystem::relative(absPath);
        std::string texturePath = relPath.generic_string();
        
        AssetHandle textureHandle = AssetManager::EnsureTextureRegistered(texturePath);
        
        out << YAML::Key << "Handle" << YAML::Value << static_cast<uint64_t>(textureHandle);
        out << YAML::Key << "Path" << YAML::Value << texturePath;
    }
    else
    {
        out << YAML::Key << "Handle" << YAML::Value << static_cast<uint64_t>(0);
        out << YAML::Key << "Path" << YAML::Value << "";
    }
    
    out << YAML::EndMap;
    break;
}
```

### DeserializeFromFile 中纹理属性反序列化改造

```cpp
case ShaderUniformType::Sampler2D:
{
    if (valueNode.IsMap())
    {
        // 新格式：Handle + Path
        AssetHandle textureHandle;
        std::string texturePath;
        
        if (valueNode["Handle"])
        {
            textureHandle = AssetHandle(valueNode["Handle"].as<uint64_t>());
        }
        if (valueNode["Path"])
        {
            texturePath = valueNode["Path"].as<std::string>();
        }
        
        Ref<Texture2D> texture = nullptr;
        
        // 优先通过 Handle 加载
        if (textureHandle.IsValid())
        {
            texture = AssetManager::GetAsset<Texture2D>(textureHandle);
        }
        
        // Handle 失效时回退到路径加载
        if (!texture && !texturePath.empty())
        {
            std::filesystem::path path(texturePath);
            std::string absolutePath = std::filesystem::absolute(path).string();
            if (std::filesystem::exists(absolutePath))
            {
                texture = Texture2D::Create(absolutePath);
                LF_CORE_WARN("Texture loaded via fallback path: '{0}'", texturePath);
            }
        }
        
        if (texture)
        {
            material->SetTexture(propName, texture);
        }
    }
    else if (valueNode.IsScalar())
    {
        // 旧格式兼容：纯路径字符串
        std::string texturePath = valueNode.as<std::string>();
        if (!texturePath.empty())
        {
            std::filesystem::path path(texturePath);
            std::string absolutePath = std::filesystem::absolute(path).string();
            Ref<Texture2D> texture = Texture2D::Create(absolutePath);
            material->SetTexture(propName, texture);
        }
    }
    break;
}
```

---

## 七、涉及的文件清单

| 文件路径 | 修改内容 |
|---------|----------|
| `Lucky/Source/Lucky/Asset/AssetManager.h` | 新增 `EnsureTextureRegistered()` 接口 |
| `Lucky/Source/Lucky/Asset/AssetManager.cpp` | 实现 `EnsureTextureRegistered()` |
| `Lucky/Source/Lucky/Serialization/MaterialSerializer.cpp` | 纹理属性序列化改为 Handle+Path 混合格式；反序列化支持新旧两种格式 |
| `Luck3DApp/Source/EditorLayer.cpp` | ImportModel 时自动注册纹理 |

---

## 八、分步实施策略

| 步骤 | 内容 | 依赖 | 预估工作量 |
|------|------|------|-----------|
| Step 1 | AssetManager 新增 `EnsureTextureRegistered()` | 无 | 极小 |
| Step 2 | MaterialSerializer 纹理序列化改造（Handle+Path） | Step 1 | 小 |
| Step 3 | MaterialSerializer 纹理反序列化改造（支持新旧格式） | Step 1 | 小 |
| Step 4 | EditorLayer::ImportModel 中自动注册纹理 | Step 1 | 极小 |
| Step 5 | 编译测试 + 验证 | 全部 | 小 |

---

## 九、验证清单

| # | 验证项 | 预期结果 |
|---|--------|--------|
| 1 | 编译通过 | 无编译错误 |
| 2 | 创建材质并设置纹理 | 保存后 .lmat 文件中纹理为 Handle+Path 格式 |
| 3 | 纹理注册到 Registry | Assets.lcr 中包含纹理注册条目 |
| 4 | 重启后加载材质 | 纹理通过 Handle 从 AssetManager 加载（缓存命中） |
| 5 | 多个材质引用同一纹理 | 只创建一个 GPU 纹理对象（AssetManager 缓存） |
| 6 | 旧格式 .lmat 兼容 | 旧格式（纯路径）的 .lmat 文件仍可正确加载 |
| 7 | Handle 失效回退 | 手动删除 Registry 条目后，通过 Path 回退加载成功 |
| 8 | 导入模型时纹理自动注册 | 模型引用的纹理自动注册到 Registry |

---

## 十、已知限制与后续扩展

| 限制 | 影响 | 后续优化方向 |
|------|------|-------------|
| 纹理无 Import Settings | 无压缩格式/Mipmap 配置 | 后续添加 .meta 文件存储导入设置 |
| 无 TextureCube 注册 | Cubemap 纹理未纳入管理 | 后续扩展 TextureCubeImporter |
| 无资产删除同步 | 删除文件后 Registry 残留 | Phase C 中实现 Refresh 功能 |
| 无纹理格式转换 | 不支持 BC/ASTC 压缩纹理 | 后续添加纹理导入管线 |
| 路径兜底可能产生重复纹理 | Handle 失效后通过路径加载不走缓存 | 加载后重新注册并更新缓存 |
