using Lucky;

namespace Sandbox
{
    /// <summary>
    /// 最小示例脚本：每帧沿 X 轴推动实体，用于验证脚本系统 MVP 闭环
    /// </summary>
    public class PlayerController : Entity
    {
        void Awake()
        {
            Debug.Log("Hello Luck3D");
        }

        void Update(float deltaTime)
        {
            TransformComponent transform = GetComponent<TransformComponent>();
            if (transform == null)
                return;

            Vector3 position = transform.Position;
            position.x += deltaTime;
            transform.Position = position;
        }
    }
}
