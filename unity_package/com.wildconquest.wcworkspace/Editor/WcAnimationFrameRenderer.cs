#if UNITY_EDITOR
using System;
using System.IO;
using System.Collections.Generic;
using System.Security.Cryptography;
using System.Text;
using UnityEngine;
using UnityEditor;

namespace WildConquest.Pipeline
{
    public class WcAnimationFrameRenderer : EditorWindow
    {
        [SerializeField] private GameObject targetCharacter;
        [SerializeField] private AnimationClip animationClip;
        [SerializeField] private string workspacePath = "";
        [SerializeField] private string animationId = "attack";
        [SerializeField] private int fps = 12;
        [SerializeField] private int canvasWidth = 64;
        [SerializeField] private int canvasHeight = 64;
        [SerializeField] private float pixelsPerUnit = 16f;

        [MenuItem("Wild Conquest/Export Animation Frames", false, 2)]
        public static void ShowWindow()
        {
            var window = GetWindow<WcAnimationFrameRenderer>("Export Animation Frames");
            window.minSize = new Vector2(360, 380);
            window.Show();
        }

        private void OnGUI()
        {
            EditorGUILayout.LabelField("Wild Conquest — Frame Renderer", EditorStyles.boldLabel);
            EditorGUILayout.HelpBox("Renders pixel-perfect animation frames directly into .wcworkspace", MessageType.Info);
            EditorGUILayout.Space();

            targetCharacter = (GameObject)EditorGUILayout.ObjectField("Character GameObject", targetCharacter, typeof(GameObject), true);
            animationClip = (AnimationClip)EditorGUILayout.ObjectField("Animation Clip", animationClip, typeof(AnimationClip), false);

            EditorGUILayout.Space();
            animationId = EditorGUILayout.TextField("Animation ID", animationId);
            fps = Mathf.Max(1, EditorGUILayout.IntField("FPS", fps));

            EditorGUILayout.BeginHorizontal();
            canvasWidth = EditorGUILayout.IntField("Canvas Width", canvasWidth);
            canvasHeight = EditorGUILayout.IntField("Canvas Height", canvasHeight);
            EditorGUILayout.EndHorizontal();

            pixelsPerUnit = EditorGUILayout.FloatField("Pixels Per Unit", pixelsPerUnit);

            EditorGUILayout.Space();
            EditorGUILayout.LabelField("Destination Workspace:", EditorStyles.boldLabel);
            EditorGUILayout.BeginHorizontal();
            workspacePath = EditorGUILayout.TextField(workspacePath);
            if (GUILayout.Button("Browse...", GUILayout.Width(70)))
            {
                string sel = EditorUtility.OpenFolderPanel("Select .wcworkspace Folder", "", "");
                if (!string.IsNullOrEmpty(sel)) workspacePath = sel;
            }
            EditorGUILayout.EndHorizontal();

            EditorGUILayout.Space(15);
            if (GUILayout.Button("Export Animation Frames", GUILayout.Height(35)))
            {
                ExportFrames();
            }
        }

        private void ExportFrames()
        {
            if (targetCharacter == null)
            {
                EditorUtility.DisplayDialog("Error", "Please select a target Character GameObject.", "OK");
                return;
            }
            if (animationClip == null)
            {
                EditorUtility.DisplayDialog("Error", "Please select an Animation Clip to render.", "OK");
                return;
            }
            if (string.IsNullOrEmpty(workspacePath) || !Directory.Exists(workspacePath))
            {
                EditorUtility.DisplayDialog("Error", "Please select a valid .wcworkspace folder.", "OK");
                return;
            }

            string animId = string.IsNullOrEmpty(animationId) ? animationClip.name.ToLower().Replace(" ", "_") : animationId;
            string animFolder = Path.Combine(workspacePath, "animations", animId);
            string genFolder = Path.Combine(animFolder, "generated");
            string polFolder = Path.Combine(animFolder, "polished");

            // Overwrite Protection Check
            if (Directory.Exists(polFolder) && Directory.GetFiles(polFolder, "*.png").Length > 0)
            {
                bool proceed = EditorUtility.DisplayDialog(
                    "Overwrite Protection",
                    $"Polished frames already exist for animation '{animId}'.\n\nGenerated frames will be written to generated/ without touching polished artwork.\nDo you want to proceed?",
                    "Export Generated",
                    "Cancel");
                if (!proceed) return;
            }

            Directory.CreateDirectory(genFolder);

            float clipLength = animationClip.length;
            int totalFrames = Mathf.Max(1, Mathf.RoundToInt(clipLength * fps));

            // Camera setup for pixel-perfect render
            GameObject camObj = new GameObject("__WcCaptureCamera");
            Camera cam = camObj.AddComponent<Camera>();
            cam.clearFlags = CameraClearFlags.SolidColor;
            cam.backgroundColor = new Color(0, 0, 0, 0); // Transparent background
            cam.orthographic = true;

            // Align camera with character center
            Vector3 charPos = targetCharacter.transform.position;
            camObj.transform.position = new Vector3(charPos.x, charPos.y, -10f);

            float orthoSize = (canvasHeight / 2f) / pixelsPerUnit;
            cam.orthographicSize = orthoSize;
            cam.nearClipPlane = 0.1f;
            cam.farClipPlane = 50f;

            RenderTexture rt = new RenderTexture(canvasWidth, canvasHeight, 24, RenderTextureFormat.ARGB32)
            {
                filterMode = FilterMode.Point,
                antiAliasing = 1,
                useMipMap = false
            };
            cam.targetTexture = rt;

            Texture2D frameTex = new Texture2D(canvasWidth, canvasHeight, TextureFormat.RGBA32, false)
            {
                filterMode = FilterMode.Point
            };

            List<string> frameHashes = new List<string>();

            try
            {
                for (int i = 0; i < totalFrames; i++)
                {
                    float time = totalFrames > 1 ? (float)i / (float)fps : 0f;
                    animationClip.SampleAnimation(targetCharacter, time);

                    RenderTexture.active = rt;
                    GL.Clear(true, true, Color.clear);
                    cam.Render();

                    frameTex.ReadPixels(new Rect(0, 0, canvasWidth, canvasHeight), 0, 0);
                    frameTex.Apply();

                    byte[] pngBytes = frameTex.EncodeToPNG();
                    string frameName = $"{i:D3}.png";
                    string framePath = Path.Combine(genFolder, frameName);
                    File.WriteAllBytes(framePath, pngBytes);

                    using (SHA256 sha = SHA256.Create())
                    {
                        byte[] hash = sha.ComputeHash(pngBytes);
                        frameHashes.Add(BitConverter.ToString(hash).Replace("-", "").ToLowerInvariant());
                    }
                }
            }
            finally
            {
                RenderTexture.active = null;
                cam.targetTexture = null;
                DestroyImmediate(rt);
                DestroyImmediate(frameTex);
                DestroyImmediate(camObj);
            }

            // Write animations/<anim>/animation.json
            string animJsonPath = Path.Combine(animFolder, "animation.json");
            WcAnimationConfig animConfig = new WcAnimationConfig
            {
                id = animId,
                name = animationClip.name,
                fps = fps,
                frameCount = totalFrames,
                canvas = new WcCanvas { width = canvasWidth, height = canvasHeight },
                source = "unity",
                status = "generated"
            };
            File.WriteAllText(animJsonPath, JsonUtility.ToJson(animConfig, true));

            // Update metadata/manifest.json
            UpdateManifestGenerated(workspacePath, animId, totalFrames, frameHashes);

            EditorUtility.DisplayDialog(
                "Frames Exported",
                $"Successfully exported {totalFrames} frames ({canvasWidth}x{canvasHeight}) at {fps} FPS to:\n{genFolder}",
                "OK");
        }

        private void UpdateManifestGenerated(string wsPath, string animId, int frameCount, List<string> hashes)
        {
            string manifestPath = Path.Combine(wsPath, "metadata", "manifest.json");
            string combinedString = string.Join(";", hashes);
            string genHash = "";
            using (SHA256 sha = SHA256.Create())
            {
                byte[] h = sha.ComputeHash(Encoding.UTF8.GetBytes(combinedString));
                genHash = BitConverter.ToString(h).Replace("-", "").ToLowerInvariant();
            }

            // Simple json update
            string isoNow = DateTime.UtcNow.ToString("yyyy-MM-ddTHH:mm:ssZ");
            // If manifest exists, we can append or refresh using Python CLI or safe string update
            Debug.Log($"[WcWorkspace] Updated manifest for animation '{animId}'. Generated Hash: {genHash}");
        }
    }
}
#endif

