// TrackedObject.cs
using UnityEngine;

[RequireComponent(typeof(Transform))]
public class TrackedObject : MonoBehaviour
{
    [Tooltip("송신기 JSON의 id와 동일해야 함")]
    public string objectId = "drone1";

    private UdpPoseReceiver _receiver;

    void Awake()
    {
        _receiver = FindObjectOfType<UdpPoseReceiver>();
        if (_receiver == null) {
            Debug.LogError("UdpPoseReceiver가 씬에 필요합니다.");
            enabled = false;
            return;
        }
        _receiver.RegisterTarget(objectId, transform);
    }

    void OnDestroy()
    {
        _receiver?.UnregisterTarget(objectId);
    }
}
