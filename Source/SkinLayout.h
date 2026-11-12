// Gerado por Assets/make_skin.py - posicoes medidas na arte aprovada (pixels da arte, 1619 x 971).
#pragma once

namespace skin
{
constexpr float kArtW = 1619.0f, kArtH = 971.0f;
constexpr float kKnobSpriteRim = 82.6f;      // raio do aro dentro de knob.png
constexpr int   kButtonMargin = 6;
constexpr float kSegX0 = 571.0f, kSegPitch = 28.0f, kSegW = 24.5f, kSegY0 = 24.0f, kSegY1 = 51.0f;
constexpr int   kSegCount = 36, kSegHot = 5;
constexpr float kGrX0 = 572.5f, kGrX20 = 1552.5f, kGrY = 85.5f;
struct KnobPos { const char* id; float cx, cy, rim, valueY; };
constexpr KnobPos kKnobs[] = {
    { "link", 513.0f, 304.0f, 34.7f, 368.0f },
    { "thresh", 769.0f, 310.0f, 42.5f, 385.5f },
    { "ratio", 909.0f, 312.0f, 43.0f, 385.5f },
    { "attack", 1049.0f, 312.0f, 43.0f, 385.5f },
    { "release", 1189.0f, 312.0f, 43.4f, 384.5f },
    { "satAmount", 1447.0f, 303.0f, 36.8f, 368.5f },
    { "scHpf", 448.0f, 466.0f, 34.3f, 525.5f },
    { "scTilt", 582.0f, 465.0f, 34.3f, 525.5f },
    { "makeup", 769.0f, 496.0f, 42.5f, 568.5f },
    { "knee", 909.0f, 496.0f, 42.5f, 568.0f },
    { "range", 1049.0f, 495.0f, 43.0f, 568.0f },
    { "compMix", 1189.0f, 495.0f, 42.5f, 568.0f },
    { "satTone", 1374.0f, 464.0f, 34.7f, 525.5f },
    { "satMix", 1516.0f, 465.0f, 34.3f, 525.5f },
    { "air", 473.0f, 825.0f, 40.9f, 893.5f },
    { "width", 636.0f, 824.0f, 41.3f, 893.5f },
    { "monoBass", 800.0f, 824.0f, 41.7f, 893.5f },
    { "output", 1489.0f, 823.0f, 41.7f, 895.0f },
};
struct RectPos { const char* id; float x0, y0, x1, y1; };
constexpr RectPos kButtons[] = {
    { "extSc", 443.0f, 580.0f, 590.0f, 618.0f },
    { "autoRel", 946.0f, 608.0f, 1079.0f, 646.0f },
    { "sync", 1092.0f, 608.0f, 1250.0f, 646.0f },
    { "satTom", 1313.0f, 563.0f, 1440.0f, 601.0f },
    { "satHarm", 1454.0f, 563.0f, 1582.0f, 601.0f },
    { "satPre", 1313.0f, 612.0f, 1582.0f, 651.0f },
    { "spatOn", 946.0f, 807.0f, 1064.0f, 851.0f },
    { "compOn", 1085.0f, 807.0f, 1202.0f, 851.0f },
    { "satOn", 1223.0f, 807.0f, 1339.0f, 851.0f },
};
constexpr RectPos kPreset { "preset", 37.0f, 842.0f, 329.0f, 886.0f };
} // namespace skin
