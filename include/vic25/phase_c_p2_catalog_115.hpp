#pragma once
// Victoria 3 1.15.0 base-game snapshot (2026-11-05).
// This is a fixed catalog of 26 groups, 148 law choices and 71 amendments.
// It is not a generic law-script interpreter. Unknown scripted conditions remain
// explicit, and may not be assumed true by enactment or event code.
#include <algorithm>
#include <map>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace vic25::phasec {
struct P2CatalogLaw {
    std::string_view id, group, parent, unlocking_technologies;
    std::string_view disallowing_laws, requires_law_or;
    int progressiveness{};
    bool has_can_enact{},has_is_visible{};
};
struct P2CatalogAmendment {
    std::string_view id,parent,allowed_laws;
    bool has_possible{},has_can_repeal{};
    std::string_view possible_country_tags;
    bool repeal_requires_legitimacy{},generic_sponsor{};
};
struct P2CatalogModifier {
    std::string_view source_id,key;
    double value{};
    bool boolean{},institution{};
    bool sponsor{}; // sponsor effects require a real P3 interest group
};
struct P2Catalog115 {
    std::vector<P2CatalogLaw> laws;
    std::vector<P2CatalogAmendment> amendments;
    std::vector<P2CatalogModifier> modifiers;
};
#include "vic25/phase_c_p2_catalog_115_a.inc"
#include "vic25/phase_c_p2_catalog_115_b.inc"
#include "vic25/phase_c_p2_catalog_115_amendments.inc"
#include "vic25/phase_c_p2_catalog_115_sponsor.inc"
inline const P2Catalog115& p2_catalog_115() {
    static const P2Catalog115 data=[] {
        P2Catalog115 c;
        c.laws.reserve(148);c.amendments.reserve(71);
        c.modifiers.reserve(1200);
        p2_catalog_append_laws_a(c.laws,c.modifiers);
        p2_catalog_append_laws_b(c.laws,c.modifiers);
        p2_catalog_append_amendments(c.amendments,c.modifiers);
        p2_catalog_append_sponsor_effects(c.modifiers);
        return c;
    }();
    return data;
}
inline std::string p2_catalog_canonical_choice(std::string_view value) {
    if(value.empty())return {};
    if(value.substr(0,4)=="law_")return std::string(value);
    return "law_"+std::string(value);
}
inline const P2CatalogLaw* p2_catalog_law(std::string_view value) {
    const auto key=p2_catalog_canonical_choice(value);
    for(const auto& law:p2_catalog_115().laws)
        if(law.id==key)return &law;
    return nullptr;
}
inline bool p2_catalog_group(std::string_view group) {
    for(const auto& law:p2_catalog_115().laws)
        if(law.group==group)return true;
    return false;
}
inline bool p2_catalog_list_contains(std::string_view list,
                                     std::string_view item) {
    while(!list.empty()) {
        const auto end=list.find(';');
        const auto token=list.substr(0,end);
        if(token==item)return true;
        if(end==std::string_view::npos)break;
        list.remove_prefix(end+1);
    }
    return false;
}
inline bool p2_catalog_is_variant_of(std::string_view choice,
                                     std::string_view ancestor) {
    if(p2_catalog_canonical_choice(choice)==
       p2_catalog_canonical_choice(ancestor))return true;
    auto* law=p2_catalog_law(choice);
    for(int i=0;i<8&&law&&!law->parent.empty();++i) {
        if(law->parent==p2_catalog_canonical_choice(ancestor))return true;
        law=p2_catalog_law(law->parent);
    }
    return false;
}
inline const P2CatalogAmendment* p2_catalog_amendment(std::string_view id) {
    for(const auto& a:p2_catalog_115().amendments)
        if(a.id==id)return &a;
    return nullptr;
}
inline bool p2_catalog_amendment_applies(const P2CatalogAmendment& a,
                                         std::string_view law_choice) {
    const auto key=p2_catalog_canonical_choice(law_choice);
    if(!a.allowed_laws.empty()) {
        for(const auto& l:p2_catalog_115().laws)
            if(p2_catalog_list_contains(a.allowed_laws,l.id)&&
               p2_catalog_is_variant_of(key,l.id))return true;
        return false;
    }
    // Generic political concessions have no parent or fixed target.
    if(a.generic_sponsor)return true;
    return !a.parent.empty()&&p2_catalog_is_variant_of(key,a.parent);
}
inline double p2_catalog_raw_modifier(std::string_view owner,
                                      std::string_view key,
                                      int institution_level=0,
                                      bool include_sponsor=false) {
    // Construct an immutable (law/amendment, modifier) index once. Daily
    // political ticks never rescan the complete 1.15 catalog.
    struct Aggregate {
        double direct{},per_institution{},sponsored{};
        bool direct_flag{},institution_flag{},sponsor_flag{};
    };
    static const auto index=[] {
        std::map<std::pair<std::string_view,std::string_view>,Aggregate> cache;
        for(const auto& m:p2_catalog_115().modifiers) {
            auto& v=cache[{m.source_id,m.key}];
            if(m.sponsor) {
                if(m.boolean)v.sponsor_flag|=m.value!=0;
                else v.sponsored+=m.value;
            } else if(m.institution) {
                if(m.boolean)v.institution_flag|=m.value!=0;
                else v.per_institution+=m.value;
            } else {
                if(m.boolean)v.direct_flag|=m.value!=0;
                else v.direct+=m.value;
            }
        }
        return cache;
    }();
    const auto it=index.find({owner,key});
    if(it==index.end())return 0;
    const auto& v=it->second;
    const bool enabled=v.direct_flag||
        (institution_level>0&&v.institution_flag)||
        (include_sponsor&&v.sponsor_flag);
    if(enabled)return 1;
    return v.direct+(institution_level>0?
        v.per_institution*institution_level:0)+
        (include_sponsor?v.sponsored:0);
}
} // namespace vic25::phasec
