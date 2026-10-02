using UnityEngine;
using System.Collections.Generic;
using UnityEngine.EventSystems;

[RequireComponent(typeof(Camera))]
public class MultiTargetCameraOrbitV2 : MonoBehaviour
{
    [Header("Targets (중앙점 계산용)")]
    public List<Transform> targets = new List<Transform>();
    public Vector3 centerOffset = Vector3.zero; // 타깃 중심에서의 추가 오프셋

    [Header("Orbit / Pan / Zoom (즉시 반응)")]
    public float yawDeg = 0f;                 // 수평 회전(Yaw)
    public float pitchDeg = 25f;              // 수직 회전(Pitch)
    public float minPitch = -80f;
    public float maxPitch = 80f;

    [Tooltip("카메라-중심 거리(줌). FOV는 고정")]
    public float orbitRadius = 60f;           // 줌은 이 값을 변경
    public float zoomStep = 4f;               // 휠 한 칸당 거리 변화(m)
    public float zoomMin = 5f;
    public float zoomMax = 50000f;

    [Tooltip("좌클릭 드래그 감도(도/초)")]
    public float orbitSensitivity = 800f;
    [Tooltip("우클릭 드래그 감도(m/px)")]
    public float panSensitivity = 1.0f;

    [Header("입력 옵션")]
    public bool ignoreUI = true;              // UI 위에서 입력 무시
    public KeyCode resetKey = KeyCode.R;      // 시점 리셋 키(옵션)

    [Header("Auto Framing (옵션, 기본 OFF)")]
    public bool autoZoomToFit = false;        // 타깃 분산에 따라 거리 자동 조정
    public float fitPadding = 1.2f;           // 여유 배수(>1)
    public float fitBase = 10f;               // 최소 여유 거리

    private Camera cam;
    private Vector3 panOffset = Vector3.zero; // 마우스 팬 누적(월드공간)

    void Awake()
    {
        cam = GetComponent<Camera>();
        // FOV는 고정 운용 권장(예: 60)
        if (cam.fieldOfView < 1f) cam.fieldOfView = 60f;
    }

    void LateUpdate()
    {
        if (targets.Count == 0) return;

        // 1) 중앙점 계산(바운딩 박스 중심) + 오프셋 + 팬
        Vector3 center = GetCenter() + centerOffset + panOffset;

        // 2) 입력 처리 (UI 위 입력 무시 옵션)
        if (!ignoreUI || !EventSystem.current || !EventSystem.current.IsPointerOverGameObject())
            HandleMouse(center);

        // 3) (옵션) 자동 프레이밍: 분산 크기로 적정 orbitRadius 산출
        if (autoZoomToFit) ApplyAutoZoom(center);

        // 4) 즉시 카메라 위치/회전 반영 (FOV 고정, 거리 기반 줌)
        yawDeg = Mathf.Repeat(yawDeg, 360f);
        pitchDeg = Mathf.Clamp(pitchDeg, minPitch, maxPitch);
        orbitRadius = Mathf.Clamp(orbitRadius, zoomMin, zoomMax);

        Vector3 dir = Quaternion.Euler(pitchDeg, yawDeg, 0f) * Vector3.back; // 중심에서 뒤쪽 방향
        Vector3 camPos = center + dir * orbitRadius;

        transform.position = camPos;
        transform.rotation = Quaternion.LookRotation(center - camPos, Vector3.up);

        // 5) 리셋 키(옵션)
        if (resetKey != KeyCode.None && Input.GetKeyDown(resetKey))
        {
            panOffset = Vector3.zero;
        }
    }

    void HandleMouse(Vector3 currentCenter)
    {
        float dt = Time.unscaledDeltaTime;

        // Orbit: 좌클릭 드래그
        if (Input.GetMouseButton(0))
        {
            float dx = Input.GetAxisRaw("Mouse X");
            float dy = Input.GetAxisRaw("Mouse Y");
            yawDeg += dx * orbitSensitivity * dt;
            pitchDeg -= dy * orbitSensitivity * dt;
        }

        // Pan: 우클릭 드래그 (카메라 기준 우/위로 평면 이동)
        if (Input.GetMouseButton(1))
        {
            float dx = Input.GetAxisRaw("Mouse X");
            float dy = Input.GetAxisRaw("Mouse Y");
            var right = transform.right;
            var up = Vector3.ProjectOnPlane(transform.up, Vector3.up).normalized; // 수평 성분 위주
            float scale = panSensitivity * Mathf.Max(1f, orbitRadius * 0.05f);
            panOffset -= (right * dx + up * dy) * scale;
        }

        // Wheel: 거리 기반 줌
        float scroll = Input.mouseScrollDelta.y;
        if (Mathf.Abs(scroll) > 0.0001f)
        {
            orbitRadius = Mathf.Clamp(orbitRadius - scroll * zoomStep, zoomMin, zoomMax);
        }
    }

    void ApplyAutoZoom(Vector3 center)
    {
        Bounds b;
        if (!GetTargetsBounds(out b)) return;

        float span = Mathf.Max(b.size.x, b.size.z) * fitPadding; // 타깃 분산
        float desired = Mathf.Clamp(span + fitBase, zoomMin, zoomMax);
        orbitRadius = desired; // 즉시 반영 (스무딩 없음)
    }

    Vector3 GetCenter()
    {
        Bounds b;
        if (!GetTargetsBounds(out b))
            return targets[0] ? targets[0].position : Vector3.zero;
        return b.center;
    }

    bool GetTargetsBounds(out Bounds b)
    {
        b = new Bounds(Vector3.zero, Vector3.zero);
        bool first = true;
        foreach (var t in targets)
        {
            if (!t) continue;
            if (first) { b = new Bounds(t.position, Vector3.zero); first = false; }
            else b.Encapsulate(t.position);
        }
        return !first;
    }

    // 런타임에 타깃 교체/추가용
    public void SetTargets(List<Transform> list) => targets = list ?? new List<Transform>();
}