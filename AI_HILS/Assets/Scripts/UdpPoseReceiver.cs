using System;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using UnityEngine;

[Serializable]
public struct PoseMsg
{
    public string id;
    public double t;
    public float x, y, z;
    public float qx, qy, qz, qw;
}

public class UdpPoseReceiver : MonoBehaviour
{
    [Header("UDP")]
    public string listenIP = "127.0.0.1";
    public int listenPort = 5008;

    [Header("Smoothing")]
    [Range(0f, 1f)] public float lerpPosition = 0.5f;
    [Range(0f, 1f)] public float slerpRotation = 0.5f;

    private UdpClient _udp;
    private Thread _thread;
    private volatile bool _running;

    private readonly ConcurrentQueue<PoseMsg> _queue = new ConcurrentQueue<PoseMsg>();
    private readonly ConcurrentDictionary<string, Transform> _targets = new ConcurrentDictionary<string, Transform>();

    // === 추가: 수신 카운터 ===
    private int _recvCount = 0;
    private float _lastLogTime = 0f;

    public void RegisterTarget(string id, Transform t) => _targets[id] = t;
    public void UnregisterTarget(string id) => _targets.TryRemove(id, out _);

    void Start()
    {
        Application.runInBackground = true;
        StartUdp();
        _lastLogTime = Time.time;
    }

    void StartUdp()
    {
        try
        {
            var ip = IPAddress.Parse(listenIP);
            _udp = new UdpClient(new IPEndPoint(ip, listenPort));
            _running = true;
            _thread = new Thread(ReceiveLoop) { IsBackground = true };
            _thread.Start();
            Debug.Log($"[UDP] Listening {listenIP}:{listenPort}");
        }
        catch (Exception e)
        {
            Debug.LogError($"[UDP] Init failed: {e}");
        }
    }

    void ReceiveLoop()
    {
        var remote = new IPEndPoint(IPAddress.Any, 0);
        while (_running)
        {
            try
            {
                var data = _udp.Receive(ref remote);
                var json = Encoding.UTF8.GetString(data);
                var msg = JsonUtility.FromJson<PoseMsg>(json);
                if (!string.IsNullOrEmpty(msg.id))
                {
                    _queue.Enqueue(msg);
                    // === 수신 카운트 증가 ===
                    System.Threading.Interlocked.Increment(ref _recvCount);
                }
            }
            catch (SocketException)
            {
                // closing
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"[UDP] Parse error: {ex.Message}");
            }
        }
    }

    void Update()
    {
        while (_queue.TryDequeue(out var m))
        {
            if (_targets.TryGetValue(m.id, out var tr) && tr != null)
            {
                var p = new Vector3(m.x, m.y, m.z);
                var q = new Quaternion(m.qx, -m.qy, m.qz, m.qw);
                tr.position = Vector3.Lerp(tr.position, p, lerpPosition);
                tr.rotation = Quaternion.Slerp(tr.rotation, q, slerpRotation);
            }
        }

        // === 초당 1회 로그 출력 ===
        if (Time.time - _lastLogTime >= 1.0f)
        {
            int count = System.Threading.Interlocked.Exchange(ref _recvCount, 0);
            Debug.Log($"[UDP] Received {count} packets in last second");
            _lastLogTime = Time.time;
        }
    }

    void OnDestroy()
    {
        _running = false;
        try { _udp?.Close(); } catch { }
        if (_thread != null && _thread.IsAlive) _thread.Join(100);
    }
}
