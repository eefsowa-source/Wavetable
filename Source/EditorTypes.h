#pragma once
#include <array>
#include <cstdint>

// The 90 selectable editor types are organised in three banks of 30:
//   A: 30 skins      (palette, type, knob rendering, background decoration)
//   B: 30 layouts    (6 skeletons x 5 density levels)
//   C: 30 curated    (skin + layout pairings, each skin/layout used once)
// Audio parameters and the editor's control set stay identical in every type;
// only presentation changes. All data here is plain and header-only.
namespace seoului
{
struct Skin
{
    const char* name;
    std::uint32_t backgroundTop, backgroundBottom;
    std::uint32_t panel, panelLine;
    std::uint32_t accent, accentAlt;
    std::uint32_t text, textDim;
    const char* fontFamily;
    float titleSize;
    int knobStyle;   // 0 arc, 1 dot, 2 bar, 3 needle, 4 split gauge
    int decoration;  // 0 skyline, 1 grid, 2 scanlines, 3 aurora, 4 minimal
    bool glow;
};

inline constexpr int onyxPrismKnobStyle = 5;

inline const std::array<Skin, 30>& skins()
{
    static const std::array<Skin, 30> list = {{
        { "Neon Han",        0xff101b2b, 0xff071018, 0xff0d2233, 0xff35516b, 0xff39d0d8, 0xffe14fd2, 0xffd8f8ff, 0xff91aabb, "Helvetica Neue", 30.0f, onyxPrismKnobStyle, 0, true  },
        { "Han River Dawn",  0xff1b2333, 0xff0b0f18, 0xff182135, 0xff3c4a66, 0xfff2b25c, 0xff6fc7e8, 0xfff5ecdc, 0xff9aa6bd, "Avenir Next",    30.0f, 1, 1, false },
        { "Terminal Mint",   0xff06120c, 0xff020806, 0xff0a1a10, 0xff1d4030, 0xff4ce0a0, 0xffd0ff00, 0xffc8f5dd, 0xff6f9c85, "Menlo",          28.0f, 3, 2, false },
        { "Ultraviolet",     0xff150a2e, 0xff080418, 0xff1d1040, 0xff3a2a66, 0xffa06bff, 0xffff5ad9, 0xffeadcff, 0xff9a8cc0, "Helvetica Neue", 30.0f, 4, 3, true  },
        { "Sakura Neon",     0xff1d1020, 0xff0d060f, 0xff241326, 0xff4a2c44, 0xffff7bac, 0xff7be0d8, 0xffffe8f1, 0xffb58ea0, "Snell Roundhand", 30.0f, 0, 3, true  },
        { "Concrete Mono",   0xff16181a, 0xff0a0b0c, 0xff1d2023, 0xff3c4147, 0xffe8e8e8, 0xff9aa0a6, 0xffe6ebee, 0xff8a929a, "Helvetica Neue", 30.0f, 2, 4, false },
        { "Deep Ocean",      0xff061627, 0xff02090f, 0xff0a2036, 0xff1c3d5c, 0xff3fa9f5, 0xff35e0c8, 0xffd8f0ff, 0xff7fa0b8, "Avenir Next",    30.0f, 1, 1, false },
        { "Sunset VCT",      0xff1f1020, 0xff0d060c, 0xff2a1430, 0xff4e2a52, 0xffff8a50, 0xffffd166, 0xffffe9dc, 0xffb58a94, "Baskerville",    30.0f, 0, 0, true  },
        { "Forest CRT",      0xff0a1408, 0xff030702, 0xff10200c, 0xff24401c, 0xff7ee081, 0xffd7ff8a, 0xffdff5da, 0xff7c9c76, "Menlo",          28.0f, 3, 2, false },
        { "Lava Tube",       0xff1a0a08, 0xff0b0403, 0xff24100c, 0xff48201a, 0xffff5030, 0xffffb03a, 0xffffe2d8, 0xffb08478, "Helvetica Neue", 30.0f, 4, 0, true  },
        { "Arctic Glass",    0xff0e1a22, 0xff060c10, 0xff14242e, 0xff2c4a5a, 0xffbfe9ff, 0xff6fd0e8, 0xffeef8ff, 0xff8fb0c0, "Helvetica Neue", 28.0f, 1, 4, false },
        { "Gold Leaf",       0xff171208, 0xff090703, 0xff201a0c, 0xff423818, 0xffe8c15a, 0xff8a6f2a, 0xfff8ecd0, 0xffa89a76, "Baskerville",    30.0f, 2, 4, false },
        { "Hunter Green",    0xff0c1710, 0xff050a06, 0xff12211a, 0xff26402c, 0xff5abf7a, 0xffc2e86f, 0xffddf2e0, 0xff84a68c, "Avenir Next",    30.0f, 0, 1, false },
        { "Crimson Booth",   0xff1c0a0e, 0xff0d0406, 0xff261016, 0xff4a2028, 0xffff4d5e, 0xffffa26f, 0xffffe4e6, 0xffb2828a, "Helvetica Neue", 30.0f, 3, 0, true  },
        { "Teal Circuit",    0xff081a1c, 0xff030b0c, 0xff0e262a, 0xff1e4a50, 0xff2ad8c8, 0xffa0f0e0, 0xffd8f5f2, 0xff7aa8a4, "Menlo",          28.0f, 1, 2, false },
        { "Slate Blueprint", 0xff101822, 0xff080c12, 0xff16212e, 0xff2c3e52, 0xff7fa8d8, 0xffd8e8ff, 0xffdbe6f2, 0xff8298ae, "Menlo",          28.0f, 2, 1, false },
        { "Sandstone",       0xff1c1812, 0xff0d0b08, 0xff26201a, 0xff463c30, 0xffe0b070, 0xfff0d8a8, 0xfff5ead8, 0xffa89478, "Baskerville",    30.0f, 4, 4, false },
        { "Midnight Radio",  0xff0a0f1e, 0xff04060e, 0xff101830, 0xff243052, 0xffe8b04a, 0xff6f8ac8, 0xffe6e8f5, 0xff8790b0, "Helvetica Neue", 30.0f, 3, 0, false },
        { "Toxic Rave",      0xff101406, 0xff060802, 0xff181f08, 0xff303f10, 0xffc8ff2a, 0xffff2ad8, 0xffeeffc8, 0xff97a86a, "Helvetica Neue", 28.0f, 4, 3, true  },
        { "Copper Wok",      0xff180f0a, 0xff0a0605, 0xff221610, 0xff442c1e, 0xffe08a50, 0xff70c8b0, 0xfff5e2d2, 0xffab8e78, "Avenir Next",    30.0f, 0, 4, false },
        { "Pastel Yume",     0xff1a1622, 0xff0b0910, 0xff242030, 0xff46405a, 0xffb8a8f0, 0xffffb8d8, 0xfff5f0fa, 0xffa49ab8, "Snell Roundhand", 30.0f, 1, 3, false },
        { "Noir Scarlet",    0xff121212, 0xff060606, 0xff1a1a1a, 0xff383838, 0xffe83040, 0xffd8d8d8, 0xffeeeaea, 0xff8f8a8a, "Helvetica Neue", 30.0f, 2, 2, false },
        { "Polar Night",     0xff0c1420, 0xff05080e, 0xff122032, 0xff28425e, 0xff8ac8ff, 0xffe8a0ff, 0xffe2f0fc, 0xff86a2bc, "Helvetica Neue", 28.0f, 1, 1, true  },
        { "Plasma Flow",     0xff140a1c, 0xff090410, 0xff1c1030, 0xff382058, 0xffe84af0, 0xff4ae8f0, 0xfff5e2ff, 0xffa288be, "Helvetica Neue", 30.0f, 4, 0, true  },
        { "Sepia Film",      0xff191410, 0xff0c0906, 0xff221c14, 0xff403428, 0xffd0a878, 0xff8fa88a, 0xffefe4d2, 0xffa09080, "Baskerville",    28.0f, 3, 2, false },
        { "Kyoto Maple",     0xff1c0f0c, 0xff0d0605, 0xff26140f, 0xff4c2a1e, 0xffff7a4a, 0xffe8c05a, 0xfffae8dc, 0xffb28c78, "Baskerville",    30.0f, 0, 0, false },
        { "Bleached Cyan",   0xff0e1a1c, 0xff070d0e, 0xff142428, 0xff2c4a50, 0xffa8f0e8, 0xff4ac8d8, 0xffecfbf8, 0xff8ab0b2, "Helvetica Neue", 28.0f, 1, 4, true  },
        { "Carbon Violet",   0xff121016, 0xff08070a, 0xff1a1822, 0xff343044, 0xffb08af0, 0xff6fd0c8, 0xffece8f5, 0xff948ea8, "Helvetica Neue", 30.0f, 2, 3, false },
        { "Haeundae Blue",   0xff0a1220, 0xff04080f, 0xff101c30, 0xff203856, 0xff4a90e8, 0xffffd25a, 0xffdce8fa, 0xff7e94b2, "Avenir Next",    30.0f, 3, 1, false },
        { "Void Prism",      0xff0b0b10, 0xff050507, 0xff14141c, 0xff2a2a38, 0xff8affd8, 0xffff8ad8, 0xffe8ecf2, 0xff8e93a2, "Menlo",          28.0f, 4, 2, true  },
    }};
    return list;
}

struct Layout
{
    const char* name;
    int skeleton; // 0 stack, 1 sidecar, 2 twin, 3 stage, 4 rail, 5 grid
    int density;  // 0 comfortable .. 4 dense
};

inline const std::array<Layout, 30>& layouts()
{
    static const std::array<Layout, 30> list = {{
        { "Studio Comfort", 0, 0 }, { "Studio Normal", 0, 1 }, { "Studio Compact", 0, 2 },
        { "Studio Tight",   0, 3 }, { "Studio Dense",   0, 4 },
        { "Sidecar Comfort", 1, 0 }, { "Sidecar Normal", 1, 1 }, { "Sidecar Compact", 1, 2 },
        { "Sidecar Tight",   1, 3 }, { "Sidecar Dense",   1, 4 },
        { "Twin Comfort", 2, 0 }, { "Twin Normal", 2, 1 }, { "Twin Compact", 2, 2 },
        { "Twin Tight",   2, 3 }, { "Twin Dense",   2, 4 },
        { "Stage Comfort", 3, 0 }, { "Stage Normal", 3, 1 }, { "Stage Compact", 3, 2 },
        { "Stage Tight",   3, 3 }, { "Stage Dense",   3, 4 },
        { "Rail Comfort", 4, 0 }, { "Rail Normal", 4, 1 }, { "Rail Compact", 4, 2 },
        { "Rail Tight",   4, 3 }, { "Rail Dense",   4, 4 },
        { "Grid Comfort", 5, 0 }, { "Grid Normal", 5, 1 }, { "Grid Compact", 5, 2 },
        { "Grid Tight",   5, 3 }, { "Grid Dense",   5, 4 },
    }};
    return list;
}

struct Combined
{
    const char* name;
    int skin, layout;
};

inline const std::array<Combined, 30>& combined()
{
    static const std::array<Combined, 30> list = {{
        { "Neon Studio",      0, 1  }, { "Dawn Sidecar",    1, 5  }, { "Mint Rail",       2, 24 },
        { "Ultraviolet Grid", 3, 27 }, { "Sakura Stage",    4, 15 }, { "Mono Twin",       5, 13 },
        { "Ocean Studio",     6, 0  }, { "Sunset Rail",     7, 22 }, { "Forest Grid",     8, 29 },
        { "Lava Twin",        9, 11 }, { "Arctic Sidecar", 10, 8  }, { "Gold Dense",     11, 4  },
        { "Hunter Grid",     12, 26 }, { "Crimson Stage",  13, 18 }, { "Teal Rail",      14, 21 },
        { "Blueprint Twin",  15, 10 }, { "Sand Sidecar",   16, 6  }, { "Radio Compact",  17, 2  },
        { "Rave Grid",       18, 28 }, { "Copper Twin",    19, 14 }, { "Yume Stage",     20, 16 },
        { "Noir Rail",       21, 23 }, { "Polar Sidecar",  22, 9  }, { "Plasma Stage",   23, 19 },
        { "Sepia Tight",     24, 3  }, { "Kyoto Sidecar",  25, 7  }, { "Bleached Rail",  26, 20 },
        { "Carbon Twin",     27, 12 }, { "Haeundae Stage", 28, 17 }, { "Prism Grid",     29, 25 },
    }};
    return list;
}

inline constexpr int typeCount() { return 90; }

inline int clampType (int type) { return type < 0 ? 0 : (type >= typeCount() ? typeCount() - 1 : type); }

inline int skinIndexForType (int type)
{
    type = clampType (type);
    if (type < 30)
        return type;
    if (type < 60)
        return 0; // The layout bank keeps the default Neon Han palette.
    return combined()[(size_t) (type - 60)].skin;
}

inline int layoutIndexForType (int type)
{
    type = clampType (type);
    return type < 30 ? type : (type < 60 ? type - 30 : combined()[(size_t) (type - 60)].layout);
}
}
