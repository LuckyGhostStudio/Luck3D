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
        /// 因为使用了 ImGuiDragDropFlags_AcceptBeforeDelivery 的官方 peek 机制，
        /// 源端与目标端保证在同一帧内完成写入-读取，因此只需比较是否等于 currentFrame。
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

        // 官方 peek（AcceptBeforeDelivery）保证目标在本帧写入，源端本帧读取
        // 因此只需比较当前帧号
        return GetAcceptFrame(payloadType) != ImGui::GetFrameCount();
    }
}
