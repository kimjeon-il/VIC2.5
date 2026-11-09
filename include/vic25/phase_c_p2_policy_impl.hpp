#pragma once
// C-P2 supplementary law/ideology catalog and voluntary withdrawal.
// The catalog supplies political preference and legal templates only:
// no duplicate POP, parliamentary seat, or effective law state.
namespace vic25::phasec {
inline void IntegratedWorld::p2_seed_interest_law_stance(
        const std::string& country,const std::string& group,
        const std::string& domain,const std::string& choice,int rank) {
    ensure(stage_==-1&&commit_id_==0&&
           p3_groups_.contains({country,group})&&
           !domain.empty()&&!choice.empty()&&rank>=-2&&rank<=2,
           "P2 explicit ideology rank must attach to a national IG");
    p2_law_stances_[{country,group,domain,choice}]=rank;
}
inline void IntegratedWorld::p2_seed_law_template(P2BaseLaw law) {
    ensure(stage_==-1&&commit_id_==0,
           "P2 legal catalog must seed before simulation");
    p2_validate_base(law);
    p2_validate_constitution_choice(law);
    const auto key=std::make_tuple(law.country,law.domain,law.base_choice);
    ensure(!p2_law_templates_.contains(key),
           "P2 duplicate national legal effect template");
    p2_law_templates_.emplace(key,std::move(law));
}
inline bool IntegratedWorld::p2_has_law_template(
        const std::string& country,const std::string& domain,
        const std::string& choice)const {
    return p2_law_templates_.contains({country,domain,choice});
}
inline void IntegratedWorld::p2_cancel_bill(
        const std::string& bill_id,const std::string& requester,
        std::int64_t today) {
    require_stage({9});
    auto& b=p2_bills_.at(bill_id);
    ensure(today>=b.introduced_day&&
           (b.status==P2BillStatus::submitted||
            b.status==P2BillStatus::approved),
           "P2 only an unfinished bill can be withdrawn");
    const auto sponsor=p2_sponsors_.find(bill_id);
    const bool authorized=requester==governments_.at(b.country).prime_minister||
        (sponsor!=p2_sponsors_.end()&&
         (requester==sponsor->second.party||
          (!sponsor->second.person.empty()&&
           requester==sponsor->second.person)));
    ensure(authorized,"P2 withdrawal requires a legal original sponsor");
    const auto a=p2_assents_.find(bill_id);
    ensure(a==p2_assents_.end()||
           a->second.status==P2AssentStatus::pending,
           "P2 an active veto must be handled by the legislature");
    if(a!=p2_assents_.end()) {
        auto& entry=a->second;
        const auto it=p2_assent_due_.find(entry.decision_deadline);
        if(it!=p2_assent_due_.end()){
            it->second.erase(bill_id);
            if(it->second.empty())p2_assent_due_.erase(it);
        }
        entry.status=P2AssentStatus::expired;
    }
    const auto e=p2_enactments_.find(bill_id);
    if(e!=p2_enactments_.end()) {
        const auto it=p2_review_due_.find(e->second.next_review_day);
        if(it!=p2_review_due_.end()){
            it->second.erase(bill_id);
            if(it->second.empty())p2_review_due_.erase(it);
        }
        e->second.next_review_day=-1;
        p2_retry_after_[e->second.law_key]=today+730;
    }
    p2_negotiation_watch_.erase(bill_id);
    b.status=P2BillStatus::rejected;
    if(b.action==P2BillAction::enact_or_replace_base) {
        // Reneging on a promised reform radicalizes existing affected
        // movements; it never creates supporters, POPs or armed units.
        for(auto& [id,m]:p3_movements_)
            if(m.active&&m.country==b.country&&
               m.issue_domain==b.proposed_base.domain&&
               m.desired_choice==b.proposed_base.base_choice) {
                m.radical_ppm=std::min(1'000'000,m.radical_ppm+150'000);
                events_.push_back("p2-abandoned-reform-radicalized:"+id);
            }
    }
    events_.push_back("p2-legislative-proposal-withdrawn:"+bill_id);
}
} // namespace vic25::phasec
