#pragma once
// Data-only second half of source law's unconditional disallowing_laws.
// 1.15 source revision 9fab6db71edc7df473aea00cd3ac81092b6cef20.
inline void install_vic3_115_disallows_b(Catalogue& c) {
    static const std::array<std::pair<const char*,const char*>,58> rows{{
        {"serfdom","interventionism laissez_faire command_economy"},
        {"manorialism",""},
        {"tenant_farmers",""},
        {"latifundias","council_republic"},
        {"expanded_latifundias","council_republic"},
        {"commercialized_agriculture","traditionalism"},
        {"peasant_proprietorship",""},
        {"homesteading",""},
        {"collectivized_agriculture",""},
        {"no_police",""},
        {"local_police",""},
        {"dedicated_police",""},
        {"militarized_police",""},
        {"consumption_based_taxation",""},
        {"land_based_taxation",""},
        {"per_capita_based_taxation","traditionalism"},
        {"proportional_taxation","traditionalism"},
        {"graduated_taxation","traditionalism"},
        {"mercantilism",""},
        {"protectionism",""},
        {"free_trade","command_economy"},
        {"isolationism","laissez_faire"},
        {"canton_system",""},
        {"sakoku",""},
        {"child_labor_allowed",""},
        {"restricted_child_labor",""},
        {"compulsory_primary_school","no_schools terakoya"},
        {"outlawed_dissent",""},
        {"censorship",""},
        {"right_of_assembly",""},
        {"protected_speech",""},
        {"no_workers_rights",""},
        {"regulatory_bodies",""},
        {"worker_protections",""},
        {"guild_system","laissez_faire"},
        {"combination_acts",""},
        {"anti_strike_laws",""},
        {"right_to_associate",""},
        {"corporatized_unions",""},
        {"factory_councils",""},
        {"no_migration_controls","sakoku"},
        {"migration_controls","sakoku"},
        {"closed_borders",""},
        {"no_womens_rights",""},
        {"women_in_the_fields",""},
        {"women_own_property",""},
        {"women_in_the_workplace",""},
        {"womens_suffrage",""},
        {"slavery_banned",""},
        {"colonial_slavery",""},
        {"debt_slavery","multicultural affirmative_action"},
        {"slave_trade","cultural_exclusion multicultural affirmative_action"},
        {"legacy_slavery",""},
        {"no_social_security",""},
        {"chiefs_distribute_aid",""},
        {"poor_laws",""},
        {"wage_subsidies",""},
        {"old_age_pension",""},
    }};
    for(const auto& [id,encoded]:rows) {
        auto& blocked=c.disallowed[id];
        std::istringstream words(encoded);
        for(std::string target;words>>target;)
            blocked.insert(bare_law(target));
    }
    if(c.disallowed.size()!=148)
        throw std::logic_error("POLITICS_115_DISALLOW_LIST_INCOMPLETE");
}
