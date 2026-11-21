#pragma once
// Read-only 1.15-style acceptance derived from verified scenario culture
// traits, real Phase B POP identities and the active Phase C P2 law.
// No independent POP headcount, province identity or migration economy.
namespace vic25::phasec {
inline void IntegratedWorld::p2_seed_culture(P2CultureTraits culture) {
    ensure(stage_==-1&&commit_id_==0&&!culture.culture.empty()&&
           !culture.heritages.empty()&&!culture.languages.empty()&&
           !p2_cultures_.contains(culture.culture),
           "P2 culture requires explicit heritage and language evidence");
    p2_cultures_.emplace(culture.culture,std::move(culture));
}
inline void IntegratedWorld::p2_seed_religion(P2ReligionTraits religion) {
    ensure(stage_==-1&&commit_id_==0&&!religion.religion.empty()&&
           !p2_religions_.contains(religion.religion),
           "P2 religion requires source-qualified identity");
    p2_religions_.emplace(religion.religion,std::move(religion));
}
inline void IntegratedWorld::p2_seed_national_identity(P2NationalIdentity nation) {
    ensure(stage_==-1&&commit_id_==0&&!nation.country.empty()&&
           governments_.contains(nation.country)&&
           !nation.primary_cultures.empty()&&
           !p2_national_identities_.contains(nation.country),
           "P2 identity missing sovereign or primary culture");
    for(const auto& id:nation.primary_cultures)
        ensure(p2_cultures_.contains(id),
               "P2 primary culture must have sourced trait metadata");
    for(const auto& id:nation.state_religions)
        ensure(p2_religions_.contains(id),
               "P2 state religion must have sourced trait metadata");
    p2_national_identities_.emplace(nation.country,std::move(nation));
}
inline std::optional<P2AcceptanceView> IntegratedWorld::p2_acceptance(
        const std::string& country,Id slice)const {
    if(!people_.has_slice(slice)||!governments_.contains(country))
        return std::nullopt;
    const auto law=p2_selected_law(country,"citizenship");
    if(!law)return std::nullopt;
    // Use authored 1.15 direct citizenship terms, not the duplicate compact
    // catalogue or generic projection of unrelated national/local laws.
    using CulturalTerms=std::map<std::string_view,std::int64_t>;
    static const auto cultural_sources=[] {
        std::map<std::string_view,CulturalTerms> sources;
        for(const auto& row:p2_catalog_115().modifiers) {
            if(row.boolean||row.institution||row.sponsor||
               !row.source_id.starts_with("law_")||
               !row.key.starts_with("country_acceptance_")||
               row.key=="country_acceptance_not_homeland_add")continue;
            const auto* entry=p2_catalog_law(row.source_id);
            if(!entry||entry->group!="citizenship")continue;
            auto& term=sources[row.source_id][row.key];
            term=phaseb::add_checked(term,static_cast<std::int64_t>(
                std::llround(row.value)));
        }
        return sources;
    }();
    constexpr auto primary_key="country_acceptance_primary_culture_add";
    const auto source=cultural_sources.find(p2_catalog_canonical_choice(*law));
    // A missing authored source is Unsupported, never a fabricated score 0.
    if(source==cultural_sources.end()||!source->second.contains(primary_key))
        return std::nullopt;
    const auto culture_modifier=[&](std::string_view key)->std::int64_t {
        const auto term=source->second.find(key);
        return term==source->second.end()?0:term->second;
    };
    const auto national=p2_national_identities_.find(country);
    if(national==p2_national_identities_.end())return std::nullopt;
    const auto& pop=people_.demographic(
        people_.slice(slice).key.demographic);
    const auto culture=p2_cultures_.find(pop.culture);
    if(culture==p2_cultures_.end())return std::nullopt;
    const auto& evidence=culture->second;
    const auto source_modifier=[&](const std::string& selected,
                                    const std::string& key)->std::int64_t {
        if(auto effect=politics::catalogue().modifier(selected,key))
            if(!effect->boolean)return effect->value_ppm/1'000'000;
        return 0;
    };
    constexpr const char* heritage_exact="country_acceptance_shared_heritage_trait_add";
    constexpr const char* heritage_family="country_acceptance_shared_heritage_trait_group_add";
    constexpr const char* heritage_neither="country_acceptance_no_shared_heritage_trait_add";
    const auto racialized_subjecthood=[&](const char* key) {
        return p2_active_national_enactment_points(
            country,"amendment_racialized_subjecthood",key)/1'000'000;
    };
    const auto exact_heritage_change=racialized_subjecthood(heritage_exact);
    const auto family_heritage_change=racialized_subjecthood(heritage_family);
    const auto unrelated_heritage_change=phaseb::add_checked(
        racialized_subjecthood(heritage_neither),
        p2_active_national_enactment_points(
            country,"amendment_homestead_acts",heritage_neither)/1'000'000);
    const auto score_for=[&](const P2CultureTraits& primary)->std::int64_t {
        if(evidence.culture==primary.culture)
            return culture_modifier(primary_key);
        const auto shared=[](const auto& a,const auto& b) {
            for(const auto& x:a)if(b.contains(x))return true;
            return false;
        };
        auto points=std::int64_t{0};
        const auto match=[&](bool direct,bool related,
                             const std::string& exact,
                             const std::string& family,
                             const std::string& neither) {
            return culture_modifier(direct?exact:
                                   related?family:neither);
        };
        // Compare all traits against one primary at a time. An exact
        // heritage match cannot also receive its family/no-match modifier.
        if(shared(evidence.heritages,primary.heritages))
            points+=culture_modifier(heritage_exact)+exact_heritage_change;
        else if(shared(evidence.heritage_groups,primary.heritage_groups))
            points+=culture_modifier(heritage_family)+family_heritage_change;
        else
            points+=culture_modifier(heritage_neither)+unrelated_heritage_change;
        points+=match(shared(evidence.languages,primary.languages),
                      shared(evidence.language_groups,primary.language_groups),
             "country_acceptance_shared_language_trait_add",
             "country_acceptance_shared_language_trait_group_add",
             "country_acceptance_no_shared_language_trait_add");
        points+=shared(evidence.traditions,primary.traditions)?
              culture_modifier("country_acceptance_shared_tradition_trait_add"):
              culture_modifier("country_acceptance_no_shared_tradition_trait_add");
        if(evidence.homelands.contains(pop.region))
            points+=culture_modifier("country_acceptance_homeland_add");
        return points;
    };
    bool found=false;
    std::int64_t score=0;
    for(const auto& primary:national->second.primary_cultures) {
        const auto it=p2_cultures_.find(primary);
        ensure(it!=p2_cultures_.end(),"P2 missing original primary culture");
        score=found?std::max(score,score_for(it->second)):
                      score_for(it->second);
        found=true;
    }
    if(!found)return std::nullopt;
    // The colonization row is independent of the citizenship homeland
    // points above. Use the same verified POP-region evidence, including
    // when a different country evaluates a migration candidate.
    if(evidence.homelands.contains(pop.region)&&
       p2_selected_law(country,"colonization")==
           std::optional<std::string>("no_colonial_affairs")) {
        score=phaseb::add_checked(score,static_cast<std::int64_t>(std::llround(
            p2_catalog_raw_modifier("law_no_colonial_affairs",
                                    "country_acceptance_homeland_add"))));
    }
    const auto religion_law=p2_selected_law(country,"church_and_state");
    constexpr auto religion_border_amendment="amendment_migration_religious_restrictions";
    const bool active_restrictions=[&] {
        if(!p2_active_amendments_.contains(religion_border_amendment))return false;
        const auto& amendment=p2_amendments_.at(religion_border_amendment);
        const auto& parent=p2_laws_.at(amendment.law_id);
        if(parent.country!=country||parent.tier==P2NormTier::local_ordinance||
           (!parent.scope.empty()&&parent.scope!="national")||
           politics::bare_group(parent.domain)!="migration"||
           !politics::catalogue().has("migration",parent.base_choice)||
           !p2_is_active_base(amendment.law_id))return false;
        const auto* source=p2_catalog_amendment(religion_border_amendment);
        return source&&p2_catalog_amendment_applies(
            *source,politics::bare_law(parent.base_choice))&&
            (source->possible_country_tags.empty()||
             p2_catalog_list_contains(source->possible_country_tags,country));
    }();
    if(religion_law||active_restrictions) {
        const auto listed=national->second.state_religions.contains(pop.religion);
        std::string_view branch;
        if(listed)branch="country_acceptance_state_religion_add";
        else if(const auto it=p2_religions_.find(pop.religion);it!=p2_religions_.end()) {
            bool religious_trait=false,religious_family=false;
            for(const auto& id:national->second.state_religions) {
                const auto& primary=p2_religions_.at(id);
                for(const auto& t:it->second.traits)
                    if(primary.traits.contains(t))religious_trait=true;
                for(const auto& t:it->second.trait_groups)
                    if(primary.trait_groups.contains(t))religious_family=true;
            }
            branch=religious_trait?"country_acceptance_shared_religious_trait_add":
                religious_family?"country_acceptance_shared_religious_trait_group_add":
                                 "country_acceptance_no_shared_religious_trait_add";
        }
        // Unknown faith remains unclassified, and the amendment has no
        // listed-state-faith row. Add at most one non-state branch before
        // the existing final score clamp; no entry rate or border-law veto.
        if(!branch.empty()) {
            if(religion_law)
                score+=static_cast<std::int64_t>(std::llround(p2_catalog_raw_modifier(
                    p2_catalog_canonical_choice(*religion_law),branch)));
            if(active_restrictions&&!listed)
                score+=static_cast<std::int64_t>(std::llround(
                    p2_catalog_raw_modifier(religion_border_amendment,branch)));
        }
    }
    // These canonically named additions target exact verified culture IDs.
    // Use the country evaluating acceptance (including a foreign migration
    // destination), not the current sovereign of the POP's home region.
    const char* amendment_id=nullptr;
    const char* effect_key=nullptr;
    if(evidence.culture=="basque") {
        amendment_id="amendment_reinstated_fueros";
        effect_key="country_basque_cultural_acceptance_add";
    } else if(evidence.culture=="polish") {
        amendment_id="amendment_haiti_polish_naturalization";
        effect_key="country_polish_cultural_acceptance_add";
    }
    if(amendment_id&&p2_active_amendments_.contains(amendment_id)) {
        const auto& adopted=p2_amendments_.at(amendment_id);
        const auto& parent=p2_laws_.at(adopted.law_id);
        if(parent.country==country&&parent.tier!=P2NormTier::local_ordinance&&
           (parent.scope.empty()||parent.scope=="national")&&
           politics::bare_group(parent.domain)=="citizenship"&&
           politics::bare_law(parent.base_choice)==*law&&
           p2_is_active_base(adopted.law_id)) {
            const auto* source=p2_catalog_amendment(amendment_id);
            if(source&&p2_catalog_amendment_applies(*source,parent.base_choice)&&
               (source->possible_country_tags.empty()||
                p2_catalog_list_contains(source->possible_country_tags,country)))
                score=phaseb::add_checked(score,static_cast<std::int64_t>(
                    std::llround(p2_catalog_raw_modifier(amendment_id,effect_key))));
        }
    }
    score=std::clamp<std::int64_t>(score,0,100);
    const auto level=score>=80?P2AcceptanceLevel::full:
                     score>=60?P2AcceptanceLevel::second_class:
                     score>=40?P2AcceptanceLevel::open_prejudice:
                     score>=20?P2AcceptanceLevel::cultural_erasure:
                               P2AcceptanceLevel::violent_hostility;
    return P2AcceptanceView{country,pop.culture,pop.religion,pop.region,
                            static_cast<int>(score),level};
}
inline std::optional<P2AcceptanceLawEffect>
IntegratedWorld::p2_acceptance_effect(
        const std::string& country,Id slice)const {
    const auto view=p2_acceptance(country,slice);
    if(!view)return std::nullopt;
    const auto law=p2_selected_law(country,"citizenship");
    if(!law)return std::nullopt;
    return p2_acceptance_law_effect(*law,view->level);
}
inline bool IntegratedWorld::p2_citizenship_vote_allowed(
        const std::string& country,Id slice)const {
    if(const auto effect=p2_acceptance_effect(country,slice))
        return effect->can_vote;
    // Missing law/trait source remains "not established", not "score 0".
    // The legacy ballot conditions still apply in unconfigured scenarios.
    return true;
}
inline bool IntegratedWorld::p2_migration_permitted(
        Id source,const std::string& destination_region)const {
    if(!people_.has_slice(source)||destination_region.empty())return false;
    const auto& dem=people_.demographic(
        people_.slice(source).key.demographic);
    const auto from=sovereignty_.find(dem.region);
    if(from==sovereignty_.end())return false;
    if(!sovereignty_.contains(destination_region)) {
        // Old Phase C fiscal/population fixtures can explicitly migrate to
        // an unseeded economic region before any national government/laws
        // exist. Preserve this historic contract. Once a real sovereign
        // government exists the destination must be legally registered.
        return !governments_.contains(from->second)&&
               !p2_national_identities_.contains(from->second);
    }
    const auto& home=from->second;
    const auto& target=sovereignty_.at(destination_region);
    const bool domestic=home==target;
    const bool internal_peasant=domestic&&dem.region!=destination_region&&
        people_.economic(people_.slice(source).key.economic).profession=="peasant";
    if(dem.legal_status=="serf"||internal_peasant) {
        const auto land_reform=p2_selected_law(home,"land_reform");
        if(dem.legal_status=="serf"&&land_reform=="serfdom")return false;
        // Ordinary foreign routes are not the distinct mass-migration mechanic.
        if(internal_peasant&&land_reform&&p2_catalog_raw_modifier(
            p2_catalog_canonical_choice(*land_reform),
            "state_peasants_internal_migration_disallowed_bool")!=0.0)
            return false;
    }
    if(domestic)return true; // border controls are international only
    const auto departure=p2_selected_law(home,"migration");
    const auto arrival=p2_selected_law(target,"migration");
    if(departure==std::optional<std::string>("closed_borders")||
       arrival==std::optional<std::string>("closed_borders"))
        return false;
    if(arrival==std::optional<std::string>("migration_controls")) {
        // Unresolved culture/faith evidence is NOT treated as automatically
        // accepted. Fail closed for new controlled international migration.
        const auto view=p2_acceptance(target,source);
        if(!view||view->score<40)return false;
    }
    return true;
}
// Emancipation is an actual Phase B legal-status transition on a single
// staged day, not an abstract POP bonus. Membership/personal identity
// references follow the same people and money/asset ownership is unchanged.
inline void IntegratedWorld::phase_time_enforce_abolition() {
    require_stage({10});
    if(p2_emancipation_due_.empty())return;
    std::vector<Id> affected;
    for(const auto& [slice,pop]:people_.slices()) {
        const auto& dem=people_.demographic(pop.key.demographic);
        const auto it=sovereignty_.find(dem.region);
        if(it==sovereignty_.end()||
           !p2_emancipation_due_.contains(it->second))continue;
        const bool slavery_abolished=
            p2_selected_law(it->second,"slavery")==
                std::optional<std::string>("slavery_banned");
        const bool land_emancipated=dem.legal_status=="serf"&&
            p2_selected_law(it->second,"land_reform")!=
                std::optional<std::string>("serfdom");
        if((slavery_abolished&&
            (dem.legal_status=="slave"||dem.legal_status=="enslaved"))||
           land_emancipated)affected.push_back(slice);
    }
    for(const auto source:affected) {
        if(!people_.has_slice(source))continue;
        const auto row=people_.slice(source);
        const auto dem=people_.demographic(row.key.demographic);
        ensure(assigned(source)==0,
               "P2 emancipation of deployed soldiers requires prior discharge");
        const auto employed=people_.employed(source);
        const auto next=people_.change_identity(
            source,row.count,dem.culture,dem.religion,"free",employed);
        if(next==source)continue;
        // Move actual club/union memberships without creating population.
        std::vector<std::pair<std::string,People>> memberships;
        for(const auto& [key,n]:membership_)
            if(key.first==source&&n>0)memberships.push_back({key.second,n});
        for(const auto& [group,n]:memberships) {
            membership_.erase({source,group});
            auto& target=membership_[{next,group}];
            target=phaseb::add_checked(target,n);
            ensure(target<=people_.slice(next).count,
                   "P2 emancipation duplicated organized membership");
        }
        if(auto it=p3_attitudes_.find(source);it!=p3_attitudes_.end()) {
            auto attitude=it->second;
            attitude.slice=next;
            if(!p3_attitudes_.contains(next))
                p3_attitudes_.emplace(next,std::move(attitude));
            p3_attitudes_.erase(it);
        }
        for(auto& [anchor,actual]:person_anchors_)
            if(actual==source)actual=next;
        if(auto it=anchor_counts_.find(source);it!=anchor_counts_.end()) {
            const auto n=it->second;
            anchor_counts_.erase(it);
            auto& target=anchor_counts_[next];
            target=phaseb::add_checked(target,n);
            ensure(target<=people_.slice(next).count,
                   "P2 named person identity overallocated by abolition");
        }
        events_.push_back("p2-actual-pop-legal-status-emancipated");
    }
    p2_emancipation_due_.clear();
}
} // namespace vic25::phasec
