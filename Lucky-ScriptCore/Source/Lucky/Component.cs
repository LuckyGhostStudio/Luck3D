namespace Lucky
{
    /// <summary>
    /// 组件基类：所有 C# 侧组件代理的公共父类
    /// </summary>
    public abstract class Component
    {
        public Entity Entity { get; internal set; }
    }

    /// <summary>
    /// Transform 组件代理：读写实体的世界坐标位置
    /// </summary>
    public class TransformComponent : Component
    {
        public Vector3 Position
        {
            get
            {
                InternalCalls.TransformComponent_GetPosition(Entity.ID, out Vector3 result);
                return result;
            }
            set
            {
                InternalCalls.TransformComponent_SetPosition(Entity.ID, ref value);
            }
        }
    }
}
