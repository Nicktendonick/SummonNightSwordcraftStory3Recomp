#pragma once

namespace swordcraft3 {
struct CustomFieldProfile {
    const char* name;
    unsigned columns,rows,animation_ticks;
    const char* hashes[3];
};
// Geometry is only a candidate selector. Every source layer must also match
// its content hash before this profile can authorize any host-only drawing.
inline const CustomFieldProfile* custom_field_profile(unsigned width,unsigned height) {
    static const CustomFieldProfile profiles[]={
        {"opening-lake",45,40,14,{"dc34d56b0559809a5ad980e658b18299f44f2531",
            "7d2be0fda83c63fe24a149af05bf7795e52a5272","f3f0043cb20ab7bcb891c6f069d2abcd704ba5a3"}},
        {"village-chief-outdoors",64,40,14,{"c8a382699f592a2ea9de9f0923ff3a826916bfa3",
            "3196eea29a36dc6595ff5442a77ce7ec7dcd0740","8f5088f296e9755e92c6003c485da50a4465a278"}},
        {"village-outdoors",64,50,8,{"051a4e1a63751ecd465f23f3f289cbc579cbf7d7",
            "738fb7db4068074aa0ce7d22c91ed3c7899c05a5","7f5501a9ca02165314780644fa26dd4a31fa17ca"}}
    };
    for(const auto& profile:profiles)
        if(width==profile.columns*8 && height==profile.rows*8) return &profile;
    return nullptr;
}
}
