#pragma once
// A constitutional reform is effective only through the authoritative P2
// legal slot, not by rewriting P1 governance before the law takes effect.
namespace vic25::phasec {
inline void IntegratedWorld::p2_apply_constitutional_choice(
        const P2BaseLaw& law) {
    require_stage({0});
    if(law.tier!=P2NormTier::constitution)return;
    auto current=p1_constitution(law.country);
    bool changed=false;
    // The catalog owns full law IDs; P1 retains its compact native
    // constitutional identities. Legacy unprefixed scenarios still load.
    const auto choice=law.base_choice.substr(
        law.base_choice.rfind("law_",0)==0?4:0);
    if(law.domain=="governance_principles") {
        const std::map<std::string,P1GovernancePrinciple> choices{
            {"chiefdom",P1GovernancePrinciple::chiefdom},
            {"monarchy",P1GovernancePrinciple::monarchy},
            {"theocracy",P1GovernancePrinciple::theocracy},
            {"presidential_republic",P1GovernancePrinciple::presidential_republic},
            {"parliamentary_republic",P1GovernancePrinciple::parliamentary_republic},
            {"council_republic",P1GovernancePrinciple::council_republic},
            {"colonial_administration",
                P1GovernancePrinciple::colonial_administration},
            {"corporate_state",P1GovernancePrinciple::corporate_state},
            {"social_monarchy",P1GovernancePrinciple::social_monarchy}};
        const auto it=choices.find(choice);
        ensure(it!=choices.end(),"P2 unknown constitutional governance choice");
        current.governance=it->second;changed=true;
    } else if(law.domain=="distribution_of_power") {
        const std::map<std::string,P1PowerDistribution> choices{
            {"autocracy",P1PowerDistribution::autocracy},
            {"oligarchy",P1PowerDistribution::oligarchy},
            {"landed_vote",P1PowerDistribution::landed_vote},
            {"wealth_vote",P1PowerDistribution::wealth_vote},
            {"universal_vote",P1PowerDistribution::universal_vote},
            {"single_party",P1PowerDistribution::single_party},
            {"technocracy",P1PowerDistribution::technocracy},
            {"bakufu",P1PowerDistribution::bakufu},
            {"neo_absolutism",P1PowerDistribution::neo_absolutism},
            {"organic_regulation",P1PowerDistribution::organic_regulation},
            {"elder_council",P1PowerDistribution::elder_council},
            {"census_voting",P1PowerDistribution::census_voting},
            {"anarchy",P1PowerDistribution::anarchy},
            {"labour_suffrage",P1PowerDistribution::labour_suffrage},
            {"landed_voting",P1PowerDistribution::landed_vote},
            {"wealth_voting",P1PowerDistribution::wealth_vote},
            {"universal_suffrage",P1PowerDistribution::universal_vote},
            {"single_party_state",P1PowerDistribution::single_party}};
        const auto it=choices.find(choice);
        ensure(it!=choices.end(),"P2 unknown constitutional power choice");
        current.power=it->second;changed=true;
    }
    if(changed) {
        current.legal_basis=law.id;
        ensure(valid_p1_constitution(current),
               "P2 constitutional reform incompatible with actual legislature");
        p1_constitutions_[law.country]=std::move(current);
        events_.push_back("p2-p1-constitutional-form-effective:"+law.id);
    }
    bool legislative_amendment=false;
    auto powers=p1_legislative_powers(law.country);
    for(const auto& clause:law.clauses) {
        switch(clause.kind) {
          case P2ClauseKind::constitutional_veto_rule:
            powers.veto=static_cast<P1VetoRule>(clause.value);
            legislative_amendment=true;break;
          case P2ClauseKind::constitutional_override_ppm:
            powers.override_ppm=static_cast<std::int32_t>(clause.value);
            legislative_amendment=true;break;
          case P2ClauseKind::constitutional_assent_days:
            powers.assent_days=static_cast<std::int32_t>(clause.value);
            legislative_amendment=true;break;
          case P2ClauseKind::constitutional_party_initiative:
            powers.party_initiative=clause.value!=0;
            legislative_amendment=true;break;
          default:break;
        }
    }
    if(legislative_amendment) {
        ensure(powers.override_ppm>=500'000&&
               powers.override_ppm<=1'000'000&&
               powers.assent_days>=1&&powers.assent_days<=365,
               "P2 invalid constitutional legislative power");
        p1_legislative_powers_[law.country]=powers;
        events_.push_back("p2-p1-separation-powers-effective:"+law.id);
    }
}
} // namespace vic25::phasec
