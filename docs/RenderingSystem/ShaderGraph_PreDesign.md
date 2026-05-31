# ShaderGraph 预设计文档

> **文档性质**：预设计 / 快速参考，暂不实施。待基础设施（Shader 单文件合并、资产系统）稳定后再启动。

---

## 一、概述

ShaderGraph 是一个**可视化 Shader 编辑器**，允许用户通过拖拽节点和连线来创建 Shader，无需编写 GLSL 代码。其本质是一个 **DAG（有向无环图）→ GLSL 代码生成器**。

**与手写 Shader 的关系**：两者共存。ShaderGraph 最终生成 `.glsl`（或 `.lshader`）文件，与手写 Shader 地位相同，都被 ShaderLibrary 加载。

---

## 二、参考引擎分析

### Unity ShaderGraph

- **核心思路**：节点图 → 拓扑排序 → 生成 HLSL 代码片段 → 嵌入 Shader 模板
- **Master Node**：定义最终输出（PBR Master、Unlit Master），内含固定的光照计算逻辑
- **节点粒度**：较细（Add、Multiply、Lerp、Sample Texture 等基础数学/纹理操作）
- **序列化**：`.shadergraph` 文件（JSON 格式，存储节点、连线、位置等）
- **特点**：用户只填充表面属性，光照模型由 Master Node 固定

### Blender Shader Nodes

- **核心思路**：节点树 → 渲染器内部编译（EEVEE 生成 GLSL，Cycles 生成 SVM/OSL）
- **Closure 概念**：BSDF 节点输出的是"光照响应描述"而非颜色值，可通过 Mix Shader 混合
- **节点粒度**：较粗（Principled BSDF 本身就是一个大节点）
- **EEVEE 的做法**：每个节点注册一个 `gpu_shader_*` 函数，输出 GLSL 片段，最终拼接为完整 fragment shader

### 对本项目的选择

采用 **Unity 风格**（细粒度节点 + Master Node 模板），原因：
1. 本项目已有完整的 PBR 光照管线（`Standard.frag`），可直接作为 PBR Master 模板
2. 细粒度节点更灵活，适合学习型引擎
3. 实现复杂度低于 Blender 的 Closure 系统

---

## 三、所需第三方库

| 库 | 用途 | 地址 |
|----|------|------|
| **imgui-node-editor** | 节点编辑器 UI（基于 ImGui） | https://github.com/thedmd/imgui-node-editor |

> 本项目已使用 ImGui，imgui-node-editor 可无缝集成。它提供：节点拖拽/缩放/平移、贝塞尔曲线连线、引脚连接交互、框选/多选、上下文菜单等。

**无需其他第三方库**，代码生成器、拓扑排序、序列化等均可自行实现。

---

## 四、核心架构

```
┌─────────────────────────────────────────────────────────────┐
│                     编辑器层 (ImGui)                          │
│  ┌──────────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ 节点编辑器 UI     │  │ 节点库面板    │  │ 实时预览窗口  │  │
│  │ (imgui-node-editor)│  │ (分类浏览)   │  │ (预览球)     │  │
│  └──────────────────┘  └──────────────┘  └──────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ?
┌─────────────────────────────────────────────────────────────┐
│                        数据层                                │
│  ┌──────────────────────────────────────────────────────┐   │
│  │ ShaderGraph 数据模型 (Node + Edge + Slot)             │   │
│  │ 序列化格式：.lshadergraph (JSON)                      │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                              │
                              ?
┌─────────────────────────────────────────────────────────────┐
│                        编译层                                │
│  ┌────────────┐  ┌────────────┐  ┌────────────────────┐    │
│  │ 图验证      │  │ 拓扑排序    │  │ GLSL 代码生成器    │    │
│  │ (类型/环)   │  │            │  │ + Shader 模板系统  │    │
│  └────────────┘  └────────────┘  └────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ?
                    输出 .glsl / .lshader 文件
                              │
                              ?
                    现有 Shader 编译系统加载
```

---

## 五、数据模型设计

### 端口类型

```cpp
enum class SlotType { Float, Float2, Float3, Float4, Texture2D, SamplerCube };
```

### 端口

```cpp
struct Slot
{
    uint32_t ID;
    std::string Name;
    SlotType Type;
    enum { Input, Output } Direction;
    std::variant<float, glm::vec2, glm::vec3, glm::vec4> DefaultValue;  // 未连接时的默认值
};
```

### 节点基类

```cpp
class ShaderNode
{
public:
    uint32_t ID;
    std::string TypeName;       // "Multiply", "SampleTexture2D", etc.
    glm::vec2 Position;         // 编辑器中的位置
    std::vector<Slot> Inputs;
    std::vector<Slot> Outputs;

    // 核心：生成该节点的 GLSL 代码片段
    virtual std::string GenerateCode(CodeGenContext& ctx) = 0;
};
```

### 连线

```cpp
struct Edge
{
    uint32_t ID;
    uint32_t FromNodeID, FromSlotID;
    uint32_t ToNodeID, ToSlotID;
};
```

### 图

```cpp
class ShaderGraph
{
    std::vector<Ref<ShaderNode>> Nodes;
    std::vector<Edge> Edges;
    Ref<MasterNode> OutputNode;     // PBR Master / Unlit Master
};
```

---

## 六、代码生成流程

```
1. 从 Master Node 开始，反向遍历所有连接的节点（BFS/DFS）
2. 收集所有被使用的节点（剔除孤立节点）
3. 拓扑排序：确保依赖节点在前，被依赖节点在后
4. 按顺序为每个节点生成局部变量和 GLSL 代码片段
5. 收集所有 uniform 声明（Texture、Property 等）
6. 将代码片段 + uniform 声明嵌入 Shader 模板
7. 输出完整的 .glsl 文件
```

### 节点代码生成示例

```cpp
// Multiply 节点
class MultiplyNode : public ShaderNode
{
    std::string GenerateCode(CodeGenContext& ctx) override
    {
        std::string varA = ctx.GetInputVariable(Inputs[0]);
        std::string varB = ctx.GetInputVariable(Inputs[1]);
        std::string outVar = ctx.DeclareVariable(Outputs[0], "multiply");
        return fmt::format("    {} {} = {} * {};", GetGLSLType(Outputs[0].Type), outVar, varA, varB);
    }
};
```

### 生成结果示例

用户连接：`[Texture2D] → [Sample] → [Multiply] → [PBR Master.BaseColor]`

生成的 GLSL 片段：
```glsl
// 自动生成的节点代码
vec4 _node1_texSample = texture(u_AlbedoMap, v_Input.TexCoord);
vec3 _node2_multiply = _node1_texSample.rgb * u_TintColor.rgb;

// Master Node 赋值
vec3 albedo = _node2_multiply;
```

---

## 七、Master Node 模板

Master Node 对应引擎已有的光照管线，定义 Shader 的最终输出接口：

### PBR Master

| 输入端口 | 类型 | 默认值 | 说明 |
|---------|------|--------|------|
| Base Color | Float3 | (1,1,1) | 基础颜色 |
| Normal | Float3 | 顶点法线 | 世界空间法线 |
| Metallic | Float | 0.0 | 金属度 |
| Roughness | Float | 0.5 | 粗糙度 |
| AO | Float | 1.0 | 环境光遮蔽 |
| Emission | Float3 | (0,0,0) | 自发光 |
| Alpha | Float | 1.0 | 透明度 |

模板内嵌引擎的 `CalcDirectAndIndirectLighting()` 等固定光照逻辑。

### Unlit Master

| 输入端口 | 类型 | 默认值 | 说明 |
|---------|------|--------|------|
| Color | Float4 | (1,1,1,1) | 最终颜色（无光照） |

---

## 八、节点库规划（最小集）

### Phase 1（最小可行）

| 分类 | 节点 |
|------|------|
| Input | UV、Time、Color Property、Float Property |
| Texture | Sample Texture 2D |
| Math | Add、Subtract、Multiply、Divide、Lerp、Clamp、Saturate |
| Vector | Split（拆分分量）、Combine（合并分量） |
| Output | Unlit Master |

### Phase 2（基础可用）

| 分类 | 节点 |
|------|------|
| Input | Position、Normal、View Direction、Camera |
| Texture | Sample Texture Cube |
| Math | Power、Sqrt、Abs、Negate、Step、Smoothstep、Fract、Floor |
| Vector | Dot、Cross、Normalize、Length、Reflect |
| Color | Blend（Overlay/Multiply/Screen）|
| Output | PBR Master |

### Phase 3（丰富）

| 分类 | 节点 |
|------|------|
| Procedural | Noise、Voronoi、Gradient |
| UV | Tiling & Offset、Rotate、Polar Coordinates |
| Normal | Normal Map Decode、Normal Blend |
| Utility | Preview、SubGraph（子图复用）、Custom Function |

---

## 九、序列化格式

文件扩展名：`.lshadergraph`，JSON 格式：

```json
{
  "version": 1,
  "masterType": "PBR",
  "nodes": [
    {
      "id": 1,
      "type": "SampleTexture2D",
      "position": [100, 200],
      "properties": { "textureProperty": "u_AlbedoMap" }
    },
    {
      "id": 2,
      "type": "Multiply",
      "position": [300, 200],
      "defaults": { "B": 0.8 }
    }
  ],
  "edges": [
    { "id": 1, "from": [1, 0], "to": [2, 0] },
    { "id": 2, "from": [2, 0], "to": [0, 0] }
  ],
  "properties": [
    { "name": "u_AlbedoMap", "type": "Texture2D", "default": "white" },
    { "name": "u_TintColor", "type": "Float4", "default": [1,1,1,1] }
  ]
}
```

---

## 十、实施步骤

### Phase 1：最小可行原型（预计 1-2 周）

1. **集成 imgui-node-editor**
   - 添加为 Vendor 子模块
   - 在 CMakeLists 中配置编译
   - 创建 `ShaderGraphPanel` 编辑器面板

2. **实现数据模型**
   - `ShaderNode` 基类 + `Slot` + `Edge`
   - `ShaderGraph` 容器类
   - 5 个基础节点（Color、Texture、Multiply、Add、UV）

3. **实现代码生成器**
   - 拓扑排序算法（Kahn's Algorithm）
   - `CodeGenContext`：变量命名、类型推导
   - Unlit Master 模板（最简单的输出）

4. **验证闭环**
   - 节点图 → 生成 GLSL → 编译 → 渲染预览

### Phase 2：基础可用（预计 2-3 周）

5. **PBR Master Node**
   - 基于现有 `Standard.frag` 提取模板
   - 接入引擎光照管线

6. **扩充节点库**（20+ 节点）

7. **实时预览**
   - 图变化时自动重新生成 + 编译
   - 预览球渲染

8. **序列化**
   - `.lshadergraph` JSON 读写
   - 与资产系统集成（注册 AssetType::ShaderGraph）

### Phase 3：生产可用（持续迭代）

9. **SubGraph 子图复用**
10. **自定义函数节点**（嵌入手写 GLSL 片段）
11. **Vertex Graph**（顶点变形支持）
12. **类型自动转换**（Float → Float3 隐式扩展等）
13. **错误提示**（编译失败时高亮问题节点）

---

## 十一、前置依赖

在启动 ShaderGraph 之前，建议先完成：

1. **Shader 单文件合并**（`#pragma stage` 方案）?? ShaderGraph 生成的代码需要输出为单文件
2. **资产系统稳定** ?? `.lshadergraph` 需要作为资产被管理
3. **Material 系统完善** ?? ShaderGraph 生成的 Shader 需要被 Material 引用

---

## 十二、文件组织建议

```
Lucky/Source/Lucky/
├── ShaderGraph/
│   ├── ShaderGraph.h/.cpp              // 图数据结构
│   ├── ShaderNode.h/.cpp               // 节点基类
│   ├── ShaderGraphCompiler.h/.cpp      // 代码生成器
│   ├── MasterNodes/
│   │   ├── PBRMasterNode.h/.cpp
│   │   └── UnlitMasterNode.h/.cpp
│   ├── Nodes/
│   │   ├── MathNodes.h/.cpp            // Add, Multiply, Lerp...
│   │   ├── TextureNodes.h/.cpp         // Sample Texture 2D...
│   │   ├── InputNodes.h/.cpp           // UV, Time, Property...
│   │   └── VectorNodes.h/.cpp          // Split, Combine, Dot...
│   └── ShaderGraphSerializer.h/.cpp    // JSON 序列化

Luck3DApp/Source/Panels/
└── ShaderGraphPanel.h/.cpp             // 编辑器面板（集成 imgui-node-editor）
```

---

## 十三、关键技术点备忘

1. **拓扑排序**：使用 Kahn's Algorithm（BFS），检测环的同时确定执行顺序
2. **类型推导**：输出端口类型由输入决定（如 Float * Float3 = Float3）
3. **变量命名**：`_node{ID}_{slotName}`，保证唯一性
4. **未连接端口**：使用 Slot 的 DefaultValue 生成常量
5. **实时编译**：图变化后延迟 200-500ms 再触发编译（防抖），避免频繁编译
6. **预览**：单独的 FBO + 预览球 Mesh，使用生成的 Shader 渲染
