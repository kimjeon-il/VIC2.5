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
    if(law.domain=="governance_principles") {
        const std::map<std::string,P1GovernancePrinciple> choices{
            {"chiefdom",P1GovernancePrinciple::chiefdom},
            {"monarchy",P1GovernancePrinciple::monarchy},
            {"theocracy",P1GovernancePrinciple::theocracy},
            {"presidential_republic",P1GovernancePrinciple::presidential_republic},
            {"parliamentary_republic",P1GovernancePrinciple::parliamentary_republic},
            {"council_republic",P1GovernancePrinciple::council_republic}};
        const auto it=choices.find(law.base_choice);
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
            {"technocracy",P1PowerDistribution::technocracy}};
        const auto it=choices.find(law.base_choice);
        ensure(it!=choices.end(),"P2 unknown constitutional power choice");
        current.power=it->second;changed=true;
    }
    if(!changed)return;
    current.legal_basis=law.id;
    ensure(valid_p1_constitution(current),
           "P2 constitutional reform incompatible with actual legislature");
    p1_constitutions_[law.country]=std::move(current);
    events_.push_back("p2-p1-constitutional-form-effective:"+law.id);
}
} // namespace vic25::phasec
