#pragma once

namespace Lucky::UI
{
    /// <summary>
    /// 拖拽反馈上下文
    /// 
    /// 提供拖拽源与拖拽目标之间的一帧内状态传递，用于源端 tooltip 图标切换：
    /// - 目标端在合法命中时调用 NotifyTargetAccepts(payloadType) 上报
    /// - 源端调用 IsRejected(payloadType) 决定 tooltip 显示允许图标还是禁止图标
    /// 
    /// 内部按 payload 类型隔离状态，避免同帧内不同类型 payload 互相干扰
    /// </summary>
    class DragDropContext
    {
    public:
        /// <summary>
        /// 目标端上报：本帧内当前 payload 已被本目标接受
        /// 应在 BeginDragDropTarget 内、payload 类型与业务校验都通过后调用
        /// </summary>
        /// <param name="payloadType">payload 类型字符串（与 SetDragDropPayload 使用的 type 一致）</param>
        static void NotifyTargetAccepts(const char* payloadType);

        /// <summary>
        /// 源端查询：当前拖拽的 payload 是否处于"未被任何目标接受"状态
        /// 返回 true 表示应显示禁止图标（拖拽悬停在非法目标或空白区域）
        /// 返回 false 表示应显示允许图标（拖拽悬停在合法目标上）
        /// </summary>
        /// <param name="payloadType">payload 类型字符串（与源端 SetDragDropPayload 使用的 type 一致）</param>
        static bool IsRejected(const char* payloadType);
    };
}
