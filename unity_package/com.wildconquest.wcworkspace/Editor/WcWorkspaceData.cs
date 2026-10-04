using System;
using System.Collections.Generic;
using UnityEngine;

namespace WildConquest.Pipeline
{
    [Serializable]
    public class WcCanvas
    {
        public int width = 64;
        public int height = 64;
    }

    [Serializable]
    public class WcVec2
    {
        public float x;
        public float y;
    }

    [Serializable]
    public class WcPart
    {
        public string id;
        public string name;
        public string asset;
        public WcVec2 pivot = new WcVec2();
        public int zIndex;
        public bool visible = true;
    }

    [Serializable]
    public class WcCharacterInfo
    {
        public string id;
        public string name;
    }

    [Serializable]
    public class WcSourceInfo
    {
        public string type = "aseprite";
    }

    [Serializable]
    public class WcWorkspaceConfig
    {
        public string format = "wcworkspace";
        public int version = 2;
        public string workspaceId;
        public WcCharacterInfo character = new WcCharacterInfo();
        public WcCanvas canvas = new WcCanvas();
        public int pixelScale = 1;
        public string facing = "right";
        public WcSourceInfo source = new WcSourceInfo();
    }

    [Serializable]
    public class WcCharacterData
    {
        public string id;
        public string name;
        public List<WcPart> parts = new List<WcPart>();
    }

    [Serializable]
    public class WcAnimationConfig
    {
        public string id;
        public string name;
        public int fps = 12;
        public int frameCount;
        public WcCanvas canvas = new WcCanvas();
        public string source = "unity";
        public string status = "not_generated";
    }

    [Serializable]
    public class WcAnimManifestItem
    {
        public string status = "not_generated";
        public string generatedHash = "";
        public string polishedHash = "";
        public string spriteSheetHash = "";
        public int frameCount;
    }

    [Serializable]
    public class WcSourcesManifest
    {
        // Dictionary deserialization helper or custom
    }
}

