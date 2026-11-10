#include "vic25/phase_c_p2_catalog_115.hpp"
#include <cassert>
#include <set>
#include <string_view>

int main() {
    using namespace vic25::phasec;
    const auto& catalog=p2_catalog_115();
    assert(catalog.laws.size()==148);
    assert(catalog.amendments.size()==71);
    std::set<std::string_view> groups,law_ids,amendment_ids;
    for(const auto& law:catalog.laws) {
        assert(!law.id.empty()&&!law.group.empty());
        assert(law_ids.insert(law.id).second);
        groups.insert(law.group);
        if(!law.parent.empty())assert(p2_catalog_law(law.parent));
    }
    for(const auto& a:catalog.amendments)
        assert(amendment_ids.insert(a.id).second);
    assert(groups.size()==26);
    assert(p2_catalog_law("law_monarchy"));
    assert(p2_catalog_law("monarchy"));
    assert(p2_catalog_law("communal_administration")->group=="bureaucracy");
    assert(p2_catalog_law("revolutionary_guard")->group=="army_model");
    assert(p2_catalog_law("extraordinary_commission")->group=="internal_security");
    assert(p2_catalog_is_variant_of("law_bakufu","law_autocracy"));
    assert(!p2_catalog_is_variant_of("law_universal_suffrage","law_autocracy"));
    assert(p2_catalog_raw_modifier("law_autocracy","country_authority_add")==200);
    assert(p2_catalog_raw_modifier("law_monarchy","country_authority_add")==200);
    assert(p2_catalog_raw_modifier("law_laissez_faire","country_authority_add")==0);
    const auto* general=p2_catalog_amendment(
        "amendment_generic_sponsor_polstr_increase");
    assert(general&&general->generic_sponsor);
    assert(p2_catalog_amendment_applies(*general,"law_autocracy"));
    assert(p2_catalog_raw_modifier(general->id,
           "interest_group_pol_str_mult",0,true)==0.2);
    assert(p2_catalog_raw_modifier(general->id,
           "interest_group_pol_str_mult",0,false)==0);
    const auto* historic=p2_catalog_amendment(
        "amendment_geheime_staatskonferenz_metternich");
    assert(historic&&p2_catalog_amendment_applies(*historic,"law_autocracy"));
    assert(!p2_catalog_amendment_applies(*historic,"law_universal_suffrage"));
    assert(historic->repeal_requires_legitimacy);
}
