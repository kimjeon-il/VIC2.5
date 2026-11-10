#pragma once
// Included at the end of phase_c.hpp, after IntegratedWorld is complete.
// P1 integrates real C/P3/P4 entities without introducing parallel ledgers.
namespace vic25::phasec {

inline void IntegratedWorld::p1_check_coalition(const P1Coalition& coalition) const {
    const auto unique_names=[](const std::vector<std::string>& ids) {
        std::set<std::string> seen;
        for(const auto& id:ids)
            if(id.empty()||!seen.insert(id).second)return false;
        return true;
    };
    ensure(unique_names(coalition.ruling_groups)&&
           unique_names(coalition.ruling_parties),
           "P1 duplicate or unnamed coalition participant");
    for(const auto& group:coalition.ruling_groups)
        ensure(interest_groups_.contains(group),
               "P1 coalition references unknown P3 interest group");
}

inline int IntegratedWorld::p1_percent(People part,People total) {
    ensure(total>0&&part>=0&&part<=total,"P1 invalid support denominator");
    int lo=0,hi=100;
    while(lo<hi) {
        const int mid=lo+(hi-lo+1)/2;
        if(compare_nonnegative_ratios(
            static_cast<std::uint64_t>(part),
            static_cast<std::uint64_t>(total),
            static_cast<std::uint64_t>(mid),100)>=0)
            lo=mid;
        else hi=mid-1;
    }
    return lo;
}

inline void IntegratedWorld::seed_p1_constitution(
        const std::string& country,P1Constitution constitution) {
    ensure(stage_==-1&&commit_id_==0&&governments_.contains(country)&&
           !p1_constitutions_.contains(country)&&
           valid_p1_constitution(constitution),
           "P1 invalid constitutional opening state");
    const bool without_parliament=
        constitution.legislature==P1LegislatureKind::none;
    p1_constitutions_.emplace(country,std::move(constitution));
    p1_coalitions_.emplace(country,P1Coalition{});
    if(without_parliament) {
        // A legacy seed was required to carry one nominal chamber. Once
        // explicitly configured as a no-parliament state, discard phantom
        // seats/quorum; P4 cannot use them to authorize a government.
        auto& government=governments_.at(country);
        government.chamber_seats=0;
        government.quorum=0;
        ++government.revision;
    }
}

inline const P1Constitution& IntegratedWorld::p1_constitution(
        const std::string& country)const {
    return p1_constitutions_.at(country);
}

inline const P1Coalition& IntegratedWorld::p1_coalition(
        const std::string& country)const {
    return p1_coalitions_.at(country);
}

inline P1AuthorityView IntegratedWorld::p1_authority(
        const std::string& country)const {
    const auto& constitution=p1_constitution(country);
    const auto formation=p1_formation_rule(constitution);
    const bool confidence=formation!=P1FormationRule::head_appointment;
    return {formation,constitution.legislature!=P1LegislatureKind::none,
            confidence,!confidence,
            !confidence&&(constitution.power==P1PowerDistribution::autocracy||
                         constitution.power==P1PowerDistribution::bakufu||
                         constitution.power==P1PowerDistribution::neo_absolutism||
                         constitution.legislature==P1LegislatureKind::none)};
}

inline void IntegratedWorld::seed_p1_coalition(
        const std::string& country,P1Coalition coalition) {
    ensure(stage_==-1&&commit_id_==0&&p1_constitutions_.contains(country),
           "P1 coalition requires constitutional country");
    p1_check_coalition(coalition);
    p1_coalitions_[country]=std::move(coalition);
}

inline P1LegitimacyView IntegratedWorld::p1_legitimacy(
        const std::string& country)const {
    const auto& constitution=p1_constitution(country);
    const auto& coalition=p1_coalition(country);
    P1LegitimacyView result;
    result.caretaker=coalition.caretaker;
    People affiliated=0,aligned=0;
    for(const auto& [membership,n]:membership_) {
        const auto& [slice,group]=membership;
        const auto& demo=people_.demographic(
            people_.slice(slice).key.demographic);
        const auto sovereign=sovereignty_.find(demo.region);
        if(sovereign==sovereignty_.end()||sovereign->second!=country)
            continue;
        affiliated=phaseb::add_checked(affiliated,n);
        if(std::find(coalition.ruling_groups.begin(),coalition.ruling_groups.end(),
                     group)!=coalition.ruling_groups.end())
            aligned=phaseb::add_checked(aligned,n);
    }
    if(affiliated>0) {
        result.observed_groups=true;
        result.group_support=p1_percent(aligned,affiliated);
    }
    // Once P3 national group profiles exist, government support follows
    // actual derived clout rather than counting every membership equally.
    bool has_p3_groups=false;
    std::int64_t governing_clout=0;
    for(const auto& [key,profile]:p3_groups_) {
        (void)profile;
        if(key.first!=country)continue;
        has_p3_groups=true;
        if(std::find(coalition.ruling_groups.begin(),
                     coalition.ruling_groups.end(),key.second)!=
                     coalition.ruling_groups.end())
            governing_clout+=p3_group_view(country,key.second).clout_ppm;
    }
    if(has_p3_groups&&affiliated>0) {
        result.observed_groups=true;
        result.group_support=static_cast<int>(
            std::min<std::int64_t>(100,governing_clout/10'000));
    }
    const auto election=country_elections_.find(country);
    if(election!=country_elections_.end()&&
       constitution.legislature!=P1LegislatureKind::none) {
        People seats=0;
        for(const auto& party:coalition.ruling_parties) {
            const auto it=election->second.seats.find(party);
            if(it!=election->second.seats.end())
                seats=phaseb::add_checked(seats,it->second);
        }
        result.observed_election=true;
        result.electoral_support=p1_percent(seats,election->second.total_seats);
        if(p1_authority(country).assembly_confidence_required&&
           (seats<governments_.at(country).quorum||
            p4_parliament_dissolved(country)))
            result.legal_compatibility=coalition.caretaker?50:0;
    } else if(p1_authority(country).assembly_confidence_required) {
        // Unknown legislature is not a fictional majority.
        result.legal_compatibility=50;
    }
    // Pending P3 ideology is not guessed; unknown support components carry
    // a visible neutral prior, not a separate generated political-power stock.
    // Legitimacy modifiers contribute only to an observed government and
    // its actual IG/electoral support. The baseline remains VIC2.5's compact
    // legitimacy formula; law effects do not invent seats or party votes.
    const auto legal=[&](std::string_view key)->double {
        return p2_catalog_modifier_value(country,key);
    };
    const double clout_adjust=legal(
        "country_legitimacy_govt_total_clout_add");
    const double vote_adjust=legal(
        "country_legitimacy_govt_total_votes_add");
    if(result.observed_groups)
        result.group_support=std::clamp(
            static_cast<int>(result.group_support+
                clout_adjust*result.group_support/200.0),0,100);
    if(result.observed_election)
        result.electoral_support=std::clamp(
            static_cast<int>(result.electoral_support+
                vote_adjust*result.electoral_support/200.0),0,100);
    const int institutional=(50*result.group_support+
                             30*result.electoral_support+
                             20*result.legal_compatibility)/100;
    const double head_legal=legal("country_legitimacy_headofstate_add");
    const double base_legal=legal("country_legitimacy_base_add");
    // Incoherence is observed only for ruling groups with an explicit
    // policy stance and a real active law. Unknown ideologies are neutral,
    // not guessed into an extra legitimacy penalty.
    int contradictory_groups=0;
    for(const auto& group:coalition.ruling_groups) {
        const auto it=p3_groups_.find({country,group});
        if(it==p3_groups_.end()||it->second.issue_domain.empty())continue;
        const auto preferred=p1_group_legal_preference(it->second);
        if(preferred.empty())continue;
        for(const auto& [slot,id]:p2_active_base_) {
            (void)slot;
            const auto& law=p2_laws_.at(id);
            if(law.country!=country||
               law.domain!=it->second.issue_domain)continue;
            const auto preferred_id=p2_catalog_law(preferred)?
                p2_catalog_canonical_choice(preferred):preferred;
            const auto active_id=p2_catalog_law(law.base_choice)?
                p2_catalog_canonical_choice(law.base_choice):law.base_choice;
            if(preferred_id!=active_id)++contradictory_groups;
            break;
        }
    }
    const auto included=static_cast<int>(coalition.ruling_groups.size());
    const auto allowed=std::max(1,static_cast<int>(
        2+legal("country_legitimacy_govt_size_add")));
    const int excess=std::max(0,included-allowed);
    const double incoherence_factor=std::clamp(
        1.0+legal("country_legitimacy_ideological_incoherence_mult"),
        0.0,4.0);
    const double penalty=static_cast<double>(
        5*contradictory_groups+3*excess)*incoherence_factor;
    result.score=std::clamp(
        static_cast<int>(institutional+base_legal+
                         head_legal*0.25-penalty),0,100);
    return result;
}

inline void IntegratedWorld::seed_p1_institution(
        const std::string& country,const std::string& institution_id,
        P1InstitutionKind kind,std::uint8_t level) {
    ensure(stage_==-1&&commit_id_==0&&
           p1_constitutions_.contains(country)&&
           public_institutions_.contains(institution_id)&&level<=5&&
           static_cast<unsigned>(kind)<=
               static_cast<unsigned>(P1InstitutionKind::labor_inspection)&&
           !p1_institutions_.contains(institution_id),
           "P1 institution must reference an actual service");
    const auto& service=public_institutions_.at(institution_id);
    const auto sovereign=sovereignty_.find(service.region);
    ensure(sovereign!=sovereignty_.end()&&sovereign->second==country,
           "P1 institution crosses sovereign jurisdiction");
    p1_institutions_.emplace(institution_id,
        P1Institution{country,institution_id,kind,level});
}

inline P1InstitutionView IntegratedWorld::p1_institution_capacity(
        const std::string& institution_id)const {
    const auto& link=p1_institutions_.at(institution_id);
    const auto& service=public_institutions_.at(institution_id);
    const People staff=employed_by(service.operator_id);
    const auto law_level=p2_rule(link.country,institution_id,
         P2ClauseKind::public_institution_level);
    const auto level=law_level?
        static_cast<std::uint8_t>(*law_level):link.level;
    const People slots=(service.monthly_slots/5)*level+
                       ((service.monthly_slots%5)*level)/5;
    const People staffed=std::min(slots,
        positive_product(staff,service.patients_per_worker));
    const bool funded=cash(service.operator_id)>0||
        (has_country_treasury(link.country)&&
         cash(country_treasury(link.country))>0);
    // Irregular conflict can interrupt a real paid institution's reach;
    // it neither destroys equipment nor creates fictional policing staff.
    const People staffed_and_funded=funded?staffed:0;
    const People accessible=p4_scaled(
        staffed_and_funded,
        1'000'000-p5_guerrilla_disruption_ppm(service.region),1'000'000);
    const auto decree=p1_region_decree_bonus_ppm(
        link.country,service.region,P1DecreeKind::public_service_reach);
    const auto improved=std::min(staffed_and_funded,
        phaseb::add_checked(accessible,
            p4_scaled(accessible,decree,1'000'000)));
    return {link.country,institution_id,link.kind,level,
            staff,slots,staffed,improved,funded};
}

inline bool IntegratedWorld::has_due_p1_government(std::int64_t day)const {
    return p1_pending_.contains(day);
}

inline void IntegratedWorld::schedule_p1_cabinet_daily(
        const std::string& country,const std::string& incumbent_head,
        std::string successor_prime,P1Coalition coalition,
        std::int64_t effective_day,const std::string& legal_basis) {
    require_stage({9});
    ensure(p1_constitutions_.contains(country)&&effective_day>=0&&
           !legal_basis.empty()&&!successor_prime.empty()&&
           governments_.at(country).head_of_state==incumbent_head,
           "P1 invalid cabinet formation");
    const auto existing=p1_pending_[effective_day].find(country);
    ensure(existing==p1_pending_[effective_day].end()||
           !existing->second.replace_prime,
           "P1 duplicate same-day cabinet transition");
    p1_check_coalition(coalition);
    const auto authority=p1_authority(country);
    if(authority.assembly_confidence_required) {
        ensure(!p4_parliament_dissolved(country),
               "P1 cannot form a parliamentary government from dissolved seats");
        const auto election=country_elections_.find(country);
        ensure(election!=country_elections_.end()&&
               coalition_seats(coalition.ruling_parties,election->second)>=
                   governments_.at(country).quorum,
               "P1 parliamentary cabinet lacks recorded confidence");
    }
    auto& change=p1_pending_[effective_day][country];
    change.country=country;
    change.new_prime=std::move(successor_prime);
    change.coalition=std::move(coalition);
    if(change.legal_basis.empty())change.legal_basis=legal_basis;
    change.replace_prime=true;
    events_.push_back("p1-daily-cabinet-authorized:"+country);
}

inline void IntegratedWorld::schedule_p1_head_succession_daily(
        const std::string& country,const std::string& incumbent,
        std::string successor,std::int64_t effective_day,
        const std::string& legal_basis) {
    require_stage({9});
    const auto& order=p1_constitution(country);
    ensure((order.governance==P1GovernancePrinciple::monarchy||
            order.governance==P1GovernancePrinciple::social_monarchy||
            order.governance==P1GovernancePrinciple::chiefdom||
            order.governance==P1GovernancePrinciple::theocracy)&&
           governments_.at(country).head_of_state==incumbent&&
           !successor.empty()&&successor!=incumbent&&
           effective_day>=0&&!legal_basis.empty()&&
           !p1_pending_[effective_day].contains(country),
           "P1 head succession needs eligible state office");
    p1_pending_[effective_day].emplace(country,P1GovernmentChange{
        country,std::move(successor),"",p1_coalition(country),legal_basis,
        true,false});
    events_.push_back("p1-daily-head-succession-authorized:"+country);
}

inline void IntegratedWorld::phase_time_apply_p1_government(std::int64_t today) {
    require_stage({0});
    const auto it=p1_pending_.find(today);
    if(it==p1_pending_.end())return;
    for(const auto& [country,change]:it->second) {
        auto& government=governments_.at(country);
        if(change.replace_head)government.head_of_state=change.new_head;
        if(change.replace_prime) {
            government.prime_minister=change.new_prime;
            p1_coalitions_.at(country)=change.coalition;
        }
        ++government.revision;
        events_.push_back("p1-daily-government-effective:"+country);
    }
    p1_pending_.erase(it);
}

} // namespace vic25::phasec
