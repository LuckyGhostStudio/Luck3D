using Lucky;

namespace Sandbox
{
    /// <summary>
    /// 最小示例脚本：每帧沿 X 轴推动实体，用于验证脚本系统 MVP 闭环
    /// </summary>
    public class PlayerControllerA : Entity
    {
        // 脚本字段测试样本（P2.5 / P2.6 验收需要）
        public float Speed = 3.0f;
        public bool LogPosition = false;
        public string Title = "player";
        public Vector3 Offset = new Vector3(0.0f, 0.0f, 0.0f);
        public int Score = 42;

        public long BigNumber = 5000000000;
        public Entity Target;
        
        void Awake()
        {
            Debug.Log("Hello Luck3D");
            Debug.Log("awake speed = " + Speed);
        }

        void Update(float deltaTime)
        {
            TransformComponent transform = GetComponent<TransformComponent>();
            if (transform == null)
                return;

            Vector3 position = transform.Position;
            position.x += deltaTime;
            transform.Position = position;
            
            Debug.Log("update speed = " + Speed);
        }
        
        void OnDestroy()
        {
            TransformComponent t = GetComponent<TransformComponent>();
            Debug.Log("OnDestroy, x = " + t.Position.x);
        }
    }
}
