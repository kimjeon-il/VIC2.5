#pragma once
// Governance/Power changes belong to work/politics p2_sync_constitution.
// This small supplement applies only the new legislative-power clauses.
namespace vic25::phasec {
inline void IntegratedWorld::p2_apply_constitutional_choice(
        const P2BaseLaw& law) {
    ensure(stage_==0||(stage_==-1&&commit_id_==0),
           "P2 legislative powers require effective constitutional stage");
    if(law.tier!=P2NormTier::constitution)return;
    bool changed=false;
    auto powers=p1_legislative_powers(law.country);
    for(const auto& clause:law.clauses) {
        switch(clause.kind) {
          case P2ClauseKind::constitutional_veto_rule:
            powers.veto=static_cast<P1VetoRule>(clause.value);
            changed=true;break;
          case P2ClauseKind::constitutional_override_ppm:
            powers.override_ppm=static_cast<std::int32_t>(clause.value);
            changed=true;break;
          case P2ClauseKind::constitutional_assent_days:
            powers.assent_days=static_cast<std::int32_t>(clause.value);
            changed=true;break;
          case P2ClauseKind::constitutional_party_initiative:
            powers.party_initiative=clause.value!=0;
            changed=true;break;
          default:break;
        }
    }
    if(!changed)return;
    ensure(powers.override_ppm>=500'000&&
           powers.override_ppm<=1'000'000&&
           powers.assent_days>=1&&powers.assent_days<=365,
           "P2 invalid effective legislative power rule");
    p1_legislative_powers_[law.country]=powers;
    events_.push_back("p2-constitutional-legislative-authority-effective:"+law.id);
}
} // namespace vic25::phasec
