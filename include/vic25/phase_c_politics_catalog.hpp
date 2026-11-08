#pragma once
// VIC2.5 source-qualified political catalogue.
//
// The 26 group names and their law identifiers are a compact *compatibility*
// catalogue informed by the Victoria 3 Laws/Ideologies wiki and 1.15 data.
// Numeric gameplay hooks below are VIC2.5 rules, not a verbatim port of every
// proprietary Victoria 3 modifier. No alternate treasury/POP/world registry.
#include "vic25/phase_c_p1.hpp"
#include "vic25/phase_c_p2.hpp"
#include "vic25/phase_c_p3.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace vic25::phasec::politics {
inline std::string bare_group(std::string s) {
    if(s.rfind("lawgroup_",0)==0)s.erase(0,9);
    return s;
}
inline std::string bare_law(std::string s) {
    if(s.rfind("law_",0)==0)s.erase(0,4);
    return s;
}
struct Law {
    std::string group,id,category;
};
struct Stance {
    std::string ideology,law_group,law;
    std::int8_t strength{};
};
struct Catalogue {
    std::map<std::string,Law> laws;
    std::map<std::string,std::set<std::string>> by_group;
    std::map<std::string,std::map<std::string,std::int8_t>> ideologies;
    std::map<std::string,std::string> categories;
    [[nodiscard]] bool has_group(const std::string& group)const {
        return by_group.contains(bare_group(group));
    }
    [[nodiscard]] bool has(const std::string& group,
                           const std::string& law)const {
        const auto id=bare_law(law);
        const auto it=laws.find(id);
        return it!=laws.end()&&it->second.group==bare_group(group);
    }
    [[nodiscard]] bool knows_ideology(const std::string& ideology)const {
        return ideologies.contains(ideology);
    }
    [[nodiscard]] std::optional<int> stance(const std::string& ideology,
                             const std::string& group,const std::string& law)const {
        const auto it=ideologies.find(ideology);
        if(it==ideologies.end())return std::nullopt;
        const auto p=it->second.find(bare_group(group)+"/"+bare_law(law));
        if(p==it->second.end())return std::nullopt;
        return static_cast<int>(p->second);
    }
};
// The game combines common and national/flavour laws. These are law
// identifiers, not a copy of scripted conditions, artwork or descriptions.
// Only a single selected law per group belongs in the Phase-C P2 legal ledger.
#include "vic25/phase_c_politics_reference_ideologies.hpp"

inline const Catalogue& catalogue() {
    static const Catalogue result=[] {
        Catalogue c;
        const std::array<std::pair<const char*,const char*>,26> groups{{
            {"governance_principles","chiefdom monarchy theocracy colonial_administration social_monarchy corporate_state presidential_republic parliamentary_republic council_republic"},
            {"distribution_of_power","autocracy bakufu neo_absolutism oligarchy organic_regulation technocracy elder_council landed_voting wealth_voting census_voting labor_suffrage single_party_state universal_suffrage anarchy"},
            {"citizenship","ethnostate national_supremacy subjecthood official_nationality racial_segregation cultural_exclusion multicultural"},
            {"caste_hegemony","hindu_caste_enforced hindu_caste_codified hindu_caste_not_enforced affirmative_action"},
            {"church_and_state","millet_system people_of_the_book caesaropapism state_religion freedom_of_conscience total_separation state_atheism"},
            {"bureaucracy","hereditary_bureaucrats crownland_diets military_bureaucrats appointed_bureaucrats elected_bureaucrats communal_administration"},
            {"army_model","peasant_levies warrior_caste national_militia professional_army mass_conscription revolutionary_guard"},
            {"navy_model","merchant_navy jeune_ecole diplomatic_fleet capital_fleet"},
            {"internal_security","no_home_affairs national_guard secret_police shinsengumi third_section okhrana guaranteed_liberties extraordinary_commission"},
            {"edo_status_system","rigid_status_order loosened_status_order lax_status_order"},
            {"colonial_affairs","no_colonial_affairs internal_colonization frontier_colonization colonial_resettlement colonial_exploitation"},
            {"economic_system","traditionalism industry_banned extraction_economy agrarianism interventionism laissez_faire command_economy cooperative_ownership"},
            {"education_system","no_schools religious_schools private_schools terakoya public_schools"},
            {"health_system","no_health_system charitable_health_system private_health_insurance public_health_insurance"},
            {"land_reform","serfdom manorialism tenant_farmers latifundias expanded_latifundias commercialized_agriculture homesteading peasant_proprietorship collectivized_agriculture"},
            {"policing","no_police local_police dedicated_police militarized_police"},
            {"taxation","consumption_based_taxation land_based_taxation per_capita_based_taxation proportional_taxation graduated_taxation"},
            {"trade_policy","canton_system sakoku isolationism mercantilism protectionism free_trade"},
            {"childrens_rights","child_labor_allowed restricted_child_labor compulsory_primary_school"},
            {"free_speech","outlawed_dissent censorship right_of_assembly protected_speech"},
            {"labor_rights","no_workers_rights regulatory_bodies worker_protections"},
            {"labor_associations","guild_system combination_acts corporatized_unions anti_strike_laws right_to_associate factory_councils"},
            {"migration","closed_borders migration_controls no_migration_controls"},
            {"rights_of_women","no_womens_rights women_in_the_fields women_own_property women_in_the_workplace womens_suffrage"},
            {"slavery","debt_slavery slave_trade legacy_slavery colonial_slavery slavery_banned"},
            {"welfare","sef_paternalism no_social_security poor_laws wage_subsidies old_age_pension"}
        }};
        const std::set<std::string> power={
            "governance_principles","distribution_of_power","citizenship",
            "caste_hegemony","church_and_state","bureaucracy","army_model",
            "navy_model","internal_security","edo_status_system"
        };
        const std::set<std::string> economy={
            "colonial_affairs","economic_system","education_system","health_system",
            "land_reform","policing","taxation","trade_policy"
        };
        for(const auto& [group,words]:groups) {
            const std::string category=power.contains(group)?"power_structure":
                economy.contains(group)?"economy":"human_rights";
            c.categories.emplace(group,category);
            std::istringstream list(words);
            for(std::string id;list>>id;) {
                if(!c.laws.emplace(id,Law{group,id,category}).second)
                    throw std::logic_error("POLITICS_DUPLICATE_LAW_ID");
                c.by_group[group].insert(id);
            }
            if(c.by_group.at(group).empty())
                throw std::logic_error("POLITICS_EMPTY_LAW_GROUP");
        }
        // Opinion scale: -2 strongly disapprove, -1 disapprove, 0 neutral,
        // +1 approve, +2 strongly approve. A missing stance is neutral by
        // omission; it is NOT inherited from the law's ordinal position.
        // These common templates are deliberately compact; country-specific
        // ideological profiles may override them in the game save.
        const std::array<std::pair<const char*,const char*>,39> ideological_rules{{
            {"paternalistic/governance_principles","monarchy:2 theocracy:1 parliamentary_republic:-1 council_republic:-2"},
            {"paternalistic/distribution_of_power","autocracy:2 landed_voting:2 oligarchy:1 universal_suffrage:-2"},
            {"paternalistic/economic_system","traditionalism:2 agrarianism:1 cooperative_ownership:-2 command_economy:-2"},
            {"laissez_faire/economic_system","laissez_faire:2 interventionism:1 agrarianism:0 command_economy:-2 cooperative_ownership:-2"},
            {"laissez_faire/trade_policy","free_trade:2 mercantilism:0 protectionism:-1 isolationism:-2"},
            {"individualist/health_system","private_health_insurance:2 charitable_health_system:0 public_health_insurance:-1"},
            {"individualist/education_system","private_schools:1 public_schools:0 religious_schools:0 no_schools:-1"},
            {"individualist/welfare","no_social_security:0 poor_laws:1 wage_subsidies:-1 old_age_pension:-2"},
            {"stratocratic/army_model","peasant_levies:1 professional_army:0 national_militia:0 mass_conscription:-1"},
            {"stratocratic/land_reform","serfdom:2 tenant_farmers:1 homesteading:-2 collectivized_agriculture:-2"},
            {"militarist/army_model","professional_army:2 mass_conscription:1 revolutionary_guard:-2 peasant_levies:-1"},
            {"militarist/policing","militarized_police:2 dedicated_police:1 no_police:-1"},
            {"patriotic/policing","dedicated_police:1 local_police:0 militarized_police:-1 no_police:-1"},
            {"patriotic/internal_security","national_guard:1 secret_police:0 guaranteed_liberties:-1"},
            {"moralist/governance_principles","monarchy:2 theocracy:1 council_republic:-2"},
            {"moralist/church_and_state","state_religion:2 freedom_of_conscience:0 total_separation:-2 state_atheism:-2"},
            {"pious/education_system","religious_schools:1 public_schools:-1 private_schools:-1"},
            {"pious/health_system","charitable_health_system:1 public_health_insurance:0 no_health_system:-1"},
            {"liberal/free_speech","protected_speech:2 right_of_assembly:1 censorship:0 outlawed_dissent:-1"},
            {"liberal/internal_security","guaranteed_liberties:2 no_home_affairs:0 national_guard:-1 secret_police:-1"},
            {"liberal/rights_of_women","women_own_property:1 women_in_the_workplace:0 womens_suffrage:0 no_womens_rights:-1"},
            {"anti_clerical/church_and_state","total_separation:2 freedom_of_conscience:1 state_religion:-1 state_atheism:0"},
            {"anti_clerical/education_system","public_schools:1 no_schools:-1 religious_schools:-1"},
            {"republican/governance_principles","presidential_republic:2 parliamentary_republic:1 monarchy:0 council_republic:-1"},
            {"republican/distribution_of_power","universal_suffrage:2 census_voting:2 oligarchy:-1 autocracy:-1 anarchy:-2"},
            {"reactionary/citizenship","national_supremacy:2 ethnostate:1 racial_segregation:1 multicultural:-1"},
            {"reactionary/migration","migration_controls:2 closed_borders:1 no_migration_controls:-1"},
            {"agrarian/economic_system","agrarianism:2 traditionalism:0 laissez_faire:-2 command_economy:-2"},
            {"agrarian/land_reform","homesteading:1 commercialized_agriculture:0 serfdom:-2"},
            {"populist/distribution_of_power","universal_suffrage:2 anarchy:1 oligarchy:-2 autocracy:-2"},
            {"populist/army_model","national_militia:2 mass_conscription:1 professional_army:-1"},
            {"proletarian/economic_system","cooperative_ownership:2 command_economy:1 traditionalism:-1 laissez_faire:-1"},
            {"proletarian/welfare","old_age_pension:2 wage_subsidies:1 poor_laws:0 no_social_security:-1"},
            {"egalitarian/labor_rights","worker_protections:2 regulatory_bodies:1 no_workers_rights:0"},
            {"egalitarian/childrens_rights","compulsory_primary_school:2 restricted_child_labor:1 child_labor_allowed:-1"},
            {"socialist/economic_system","cooperative_ownership:2 command_economy:1 interventionism:0 laissez_faire:-2 traditionalism:-2"},
            {"socialist/labor_associations","factory_councils:2 right_to_associate:1 combination_acts:-2"},
            {"socialist/welfare","old_age_pension:2 wage_subsidies:1 poor_laws:0 no_social_security:-1"},
            {"abolitionist/slavery","slavery_banned:2 legacy_slavery:-1 debt_slavery:-2 slave_trade:-2"}
        }};
        for(const auto& [scope,values]:ideological_rules) {
            const std::string key(scope);
            const auto slash=key.find('/');
            if(slash==std::string::npos)
                throw std::logic_error("POLITICS_BAD_IDEOLOGY_KEY");
            const auto ideology=key.substr(0,slash);
            const auto group=key.substr(slash+1);
            std::istringstream words(values);
            for(std::string piece;words>>piece;) {
                const auto colon=piece.rfind(':');
                if(colon==std::string::npos)
                    throw std::logic_error("POLITICS_BAD_IDEOLOGY_STANCE");
                const auto law=piece.substr(0,colon);
                const int rank=std::stoi(piece.substr(colon+1));
                if(rank<-2||rank>2||!c.has(group,law))
                    throw std::logic_error("POLITICS_UNKNOWN_IDEOLOGY_LAW");
                c.ideologies[ideology][group+"/"+law]=
                    static_cast<std::int8_t>(rank);
            }
        }
        for(const auto& alias:{"plutocratic","meritocratic","particularist",
                   "hierarchic","pacifist","corporatist","feminist",
                   "authoritarian","communist","anarchist","reformer",
                   "moderate","radical","industrialist","social_democrat",
                   "state_socialist"}) {
            c.ideologies.try_emplace(alias);
        }
        // Additional positions are original simplified VIC2.5 rules, not
        // inferred from socioeconomic background or asserted as exact V3.
        const std::array<std::pair<const char*,const char*>,12> extras{{
            {"plutocratic/distribution_of_power","oligarchy:1 wealth_voting:1 universal_suffrage:-2 autocracy:-1"},
            {"meritocratic/bureaucracy","elected_bureaucrats:1 appointed_bureaucrats:0 hereditary_bureaucrats:-1"},
            {"particularist/army_model","national_militia:2 professional_army:0 mass_conscription:-1"},
            {"hierarchic/distribution_of_power","autocracy:1 landed_voting:1 universal_suffrage:-2"},
            {"pacifist/army_model","professional_army:-1 mass_conscription:-2"},
            {"corporatist/labor_associations","corporatized_unions:2 factory_councils:0 combination_acts:-1"},
            {"feminist/rights_of_women","womens_suffrage:2 women_in_the_workplace:1 no_womens_rights:-2"},
            {"authoritarian/distribution_of_power","autocracy:1 oligarchy:1 universal_suffrage:-2"},
            {"communist/economic_system","command_economy:2 cooperative_ownership:1 laissez_faire:-2"},
            {"anarchist/distribution_of_power","anarchy:2 universal_suffrage:1 autocracy:-2"},
            {"reformer/free_speech","protected_speech:1 censorship:-1"},
            {"state_socialist/economic_system","command_economy:1 cooperative_ownership:1 laissez_faire:-2"}
        }};
        for(const auto& [scope,values]:extras) {
            const std::string id(scope);
            const auto slash=id.find('/');
            std::istringstream input(values);
            for(std::string token;input>>token;) {
                const auto pos=token.rfind(':');
                const auto g=id.substr(slash+1);
                const auto l=token.substr(0,pos);
                if(!c.has(g,l))
                    throw std::logic_error("POLITICS_UNKNOWN_IDEOLOGY_LAW");
                c.ideologies[id.substr(0,slash)][g+"/"+l]=
                    static_cast<std::int8_t>(std::stoi(token.substr(pos+1)));
            }
        }
        install_reference_ideologies(c);
        return c;
    }();
    return result;
}
inline std::vector<std::string> default_ideologies(P3GroupType type) {
    switch(type) {
        case P3GroupType::landowners:return {"paternalistic","stratocratic"};
        case P3GroupType::industrialists:return {"laissez_faire","individualist"};
        case P3GroupType::armed_forces:return {"militarist","stratocratic"};
        case P3GroupType::devout:return {"moralist","pious"};
        case P3GroupType::intelligentsia:return {"liberal","anti_clerical","republican"};
        case P3GroupType::petite_bourgeoisie:return {"patriotic","reactionary"};
        case P3GroupType::rural_folk:return {"agrarian","populist"};
        case P3GroupType::trade_unions:return {"proletarian","egalitarian"};
        case P3GroupType::custom:return {};
    }
    return {};
}
// An *operational* national legal fallback applies only when a real catalogued
// P2 law is active and no higher-priority explicit legal clause overrides it.
// These limits are VIC2.5 gameplay approximations; never claim the values are
// numerical matches to Victoria 3's taxes or welfare calculations.
inline std::optional<std::int64_t> law_clause(
        const std::string& group,const std::string& law,P2ClauseKind kind) {
    const auto g=bare_group(group),l=bare_law(law);
    if(!catalogue().has(g,l))return std::nullopt;
    using K=P2ClauseKind;
    if(kind==K::female_suffrage_allowed&&g=="rights_of_women")
        return l=="womens_suffrage"?1:0;
    if(kind==K::public_healthcare_right&&g=="health_system")
        return l=="public_health_insurance"?1:0;
    if(kind==K::political_association_allowed) {
        if(g=="free_speech")
            return l=="outlawed_dissent"?0:1;
        if(g=="labor_associations")
            return l=="combination_acts"?0:1;
    }
    if(kind==K::peaceful_strike_allowed&&g=="labor_associations")
        return l=="anti_strike_laws"||l=="combination_acts"||
               l=="guild_system"?0:1;
    if(kind==K::income_tax_max_ppm&&g=="taxation") {
        if(l=="consumption_based_taxation"||l=="land_based_taxation")
            return 0; // neither tax law has wage-income tax authority
        if(l=="per_capita_based_taxation")return 400'000;
        if(l=="proportional_taxation")return 600'000;
        if(l=="graduated_taxation")return 850'000;
    }
    if(kind==K::maximum_import_tariff_ppm&&g=="trade_policy") {
        if(l=="free_trade")return 0;
        if(l=="isolationism")return 0; // no import tariff if trade prohibited
        if(l=="protectionism")return 1'000'000;
        if(l=="mercantilism")return 750'000;
        if(l=="sakoku"||l=="canton_system")return 1'000'000;
    }
    return std::nullopt;
}
// The world has no duplicate jobs, inventing money, or fictional institutions.
// Existing P1 staffing/budget and Phase C security access remain authoritative.
inline std::int32_t institution_factor(const std::string& group,
                       const std::string& law,P1InstitutionKind kind) {
    const auto g=bare_group(group),l=bare_law(law);
    if(!catalogue().has(g,l))return 1'000'000;
    if(g=="education_system"&&kind==P1InstitutionKind::education)
        return l=="no_schools"?0:1'000'000;
    if(g=="health_system"&&kind==P1InstitutionKind::healthcare)
        return l=="no_health_system"?0:1'000'000;
    if(g=="welfare"&&kind==P1InstitutionKind::welfare)
        return l=="no_social_security"?0:1'000'000;
    if(g=="policing"&&kind==P1InstitutionKind::policing)
        return l=="no_police"?0:1'000'000;
    if(g=="internal_security"&&kind==P1InstitutionKind::policing)
        return l=="no_home_affairs"?800'000:1'000'000;
    return 1'000'000;
}
// Legal political-strength multipliers (per million); this is not another
// clout ledger. Multiply only actual national organization membership.
inline std::int32_t clout_bonus(const std::string& group,const std::string& law,
                                P3GroupType type) {
    const auto g=bare_group(group),l=bare_law(law);
    if(!catalogue().has(g,l))return 0;
    using T=P3GroupType;
    if(g=="governance_principles") {
        if(l=="monarchy"&&type==T::landowners)return 250'000;
        if(l=="theocracy"&&type==T::devout)return 250'000;
        if(l=="council_republic"&&type==T::trade_unions)return 250'000;
    } else if(g=="distribution_of_power") {
        if(l=="autocracy"&&(type==T::landowners||type==T::armed_forces))
            return 500'000;
        if(l=="oligarchy"&&(type==T::landowners||type==T::industrialists))
            return 250'000;
        if(l=="universal_suffrage"&&type==T::trade_unions)return 250'000;
    } else if(g=="land_reform") {
        if((l=="serfdom"||l=="tenant_farmers")&&type==T::landowners)
            return 250'000;
    } else if(g=="church_and_state") {
        if(l=="state_religion"&&type==T::devout)return 300'000;
        if(l=="caesaropapism"&&type==T::devout)return 150'000;
    } else if(g=="labor_associations") {
        if(type==T::trade_unions) {
            if(l=="combination_acts")return -250'000;
            if(l=="anti_strike_laws")return -100'000;
            if(l=="right_to_associate")return 100'000;
            if(l=="factory_councils")return 250'000;
        }
    } else if(g=="economic_system") {
        if(l=="laissez_faire"&&type==T::industrialists)return 250'000;
        if(l=="command_economy"&&type==T::intelligentsia)return 250'000;
    } else if(g=="policing") {
        if(l=="militarized_police"&&type==T::armed_forces)return 100'000;
        if(l=="local_police"&&type==T::landowners)return 50'000;
    }
    return 0;
}
} // namespace vic25::phasec::politics
