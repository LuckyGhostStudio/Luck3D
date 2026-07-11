#include "lcpch.h"
#include "DragDropContext.h"

#include <imgui/imgui.h>

#include <string_view>

namespace Lucky::UI
{
    namespace
    {
        /// <summary>
        /// 按 payload 类型隔离的 accept 帧号：
        /// key   = payload 类型字符串（例如 "ASSET_HANDLE" / "ENTITY_HIERARCHY"）
        /// value = 最近一次 NotifyTargetAccepts() 时的 ImGui 帧号
        /// 
        /// 说明：源与目标可能是同一控件，或源位于目标之前绘制（例如 Hierarchy 内部拖拽，
        /// 拖拽源节点自身在其"下方兄弟"之前绘制）。此时源端读取 IsRejected 发生在
        /// 目标端 NotifyTargetAccepts 之前，"同帧写入-同帧读取"的假设不成立。
        /// 因此判定采用"上一帧是否有目标接受"：只要距离最近一次 accept 不超过 1 帧，
        /// 就视为 accepted，源端 tooltip 显示允许图标。
        /// </summary>
        std::unordered_map<std::string_view, int> s_AcceptFrames;

        int GetAcceptFrame(const char* payloadType)
        {
            if (!payloadType)
            {
                return -1;
            }
            auto it = s_AcceptFrames.find(std::string_view(payloadType));
            return it == s_AcceptFrames.end() ? -1 : it->second;
        }
    }

    void DragDropContext::NotifyTargetAccepts(const char* payloadType)
    {
        if (!payloadType)
        {
            return;
        }
        s_AcceptFrames[std::string_view(payloadType)] = ImGui::GetFrameCount();
    }

    bool DragDropContext::IsRejected(const char* payloadType)
    {
        // 未激活拖拽，直接视为未 rejected（tooltip 也不会显示）
        const ImGuiPayload* payload = ImGui::GetDragDropPayload();
        if (payload == nullptr)
        {
            return false;
        }

        // 当前拖拽的 payload 类型与查询类型不一致：源端本身就不是这类 payload，不做判定
        // 保险：payloadType 空则视为不 rejected，避免异常
        if (payloadType == nullptr || !payload->IsDataType(payloadType))
        {
            return false;
        }

        // 官方 peek（AcceptBeforeDelivery）保证目标在本帧写入，但当源与目标在同一列表中
        // 且源在目标之前绘制时，源端本帧读取会早于目标本帧写入。
        // 为了兼容此情况，判据采用"距离最近一次 accept 是否超过 1 帧"：
        //   - accept-frame == currentFrame：本帧目标已接受（源在目标之后绘制的正常情况）
        //   - accept-frame == currentFrame - 1：上一帧目标已接受（源在目标之前绘制的情况）
        //   - accept-frame  < currentFrame - 1：连续 2 帧无目标接受，视为 rejected
        // 副作用：鼠标从合法目标移开到非法区域时，图标切换会滞后 1 帧（约 16ms），肉眼不可察。
        return GetAcceptFrame(payloadType) < ImGui::GetFrameCount() - 1;
    }
}
