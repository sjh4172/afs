using System.Collections.Generic;
using UnityEngine;

public class CameraSwitcher : MonoBehaviour
{
    [Tooltip("전환 대상 카메라들. 0번이 시작 카메라")]
    public List<Camera> cameras = new List<Camera>();

    [Tooltip("시작 시 0번만 활성화")]
    public bool enableOnlyFirstOnStart = true;

    [Tooltip("전환 키")]
    public KeyCode toggleKey = KeyCode.C;

    private int _index = 0;

    void Start()
    {
        if (cameras.Count == 0)
        {
            Debug.LogError("[CamSwitch] cameras 리스트가 비어있음.");
            enabled = false;
            return;
        }

        if (enableOnlyFirstOnStart)
        {
            for (int i = 0; i < cameras.Count; i++)
                SetActive(i, i == 0);
            _index = 0;
        }
        else
        {
            // 첫 활성 카메라 찾기
            _index = cameras.FindIndex(c => c && c.enabled);
            if (_index < 0) _index = 0;
            for (int i = 0; i < cameras.Count; i++)
                if (i != _index) SetActive(i, false);
        }
    }

    void Update()
    {
        if (Input.GetKeyDown(toggleKey))
            Next();
    }

    public void Next()
    {
        if (cameras.Count == 0) return;
        int next = (_index + 1) % cameras.Count;
        SetActive(_index, false);
        SetActive(next, true);
        _index = next;
        Debug.Log($"[CamSwitch] Active = {cameras[_index].name}");
    }

    private void SetActive(int i, bool on)
    {
        var cam = cameras[i];
        if (!cam) return;
        cam.enabled = on;

        // AudioListener 관리: 활성 카메라만 유지
        var listener = cam.GetComponent<AudioListener>();
        if (listener)
            listener.enabled = on;

        if (on)
        {
            // 비활성 카메라들의 AudioListener 끄기
            for (int k = 0; k < cameras.Count; k++)
            {
                if (k == i) continue;
                var other = cameras[k];
                if (!other) continue;
                var al = other.GetComponent<AudioListener>();
                if (al) al.enabled = false;
            }
        }
    }
}
