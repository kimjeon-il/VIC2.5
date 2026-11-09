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
inline P2EnactmentOdds IntegratedWorld::p2_enactment_odds(
        const std::string& bill_id)const {
    const auto& bill=p2_bills_.at(bill_id);
    const auto& enactment=p2_enactments_.at(bill_id);
    const auto& domain=bill.action==P2BillAction::enact_or_replace_base?
        bill.proposed_base.domain:
        p2_laws_.at(bill.action==P2BillAction::attach_amendment?
            bill.proposed_amendment.law_id:
            bill.action==P2BillAction::repeal_base?
            bill.target_id:p2_amendments_.at(bill.target_id).law_id).domain;
    std::string current_choice,proposed_choice;
    for(const auto& [slot,id]:p2_active_base_){
        (void)slot;
        const auto& law=p2_laws_.at(id);
        if(law.country==bill.country&&law.domain==domain) {
            current_choice=law.base_choice;break;
        }
    }
    if(bill.action==P2BillAction::enact_or_replace_base)
        proposed_choice=bill.proposed_base.base_choice;
    std::int64_t success=0,stall=0;
    for(const auto& [key,g]:p3_groups_){
        if(key.first!=bill.country||g.issue_domain!=domain)continue;
        const auto v=p3_group_view(key.first,key.second);
        const auto& coalition=p1_coalitions_.at(bill.country);
        const bool in_government=std::find(
            coalition.ruling_groups.begin(),coalition.ruling_groups.end(),
            key.second)!=coalition.ruling_groups.end();
        const auto preference=p1_group_legal_preference(g);
        const auto rank=[&](const std::string& choice) {
            if(choice.empty())return 0;
            const auto i=p2_law_stances_.find(
                {bill.country,key.second,domain,choice});
            if(i!=p2_law_stances_.end())return i->second;
            return choice==preference?2:0;
        };
        int difference=proposed_choice.empty()?0:
                       rank(proposed_choice)-rank(current_choice);
        if(bill.action==P2BillAction::attach_amendment)
            for(const auto& [id,demand]:p2_group_demands_) {
                (void)id;
                if(demand.country==bill.country&&
                   demand.interest_group==g.id&&
                   demand.amendment.id==bill.proposed_amendment.id)
                    difference=std::max(difference,1);
            }
        if(bill.action==P2BillAction::repeal_base||
           bill.action==P2BillAction::repeal_amendment)
            difference=preference!=current_choice?1:-1;
        if(difference>0&&in_government)
            success+=v.clout_ppm;
        else if(difference<0&&v.clout_ppm>=50'000) {
            // Marginalized IGs have no procedural stall. Disapproving
            // IGs contribute 50%, 100% or 150% by actual P3 approval.
            const int multiplier=v.approval>=55?1:
                                 v.approval<35?3:2;
            stall+=static_cast<std::int64_t>(v.clout_ppm)*multiplier/2;
        }
    }
    for(const auto& [id,m]:p3_movements_){
        if(!m.active||m.country!=bill.country||m.issue_domain!=domain)
            continue;
        const auto v=p3_movement_view(id);
        if(v.activism_ppm<100'000)continue; // passive demand is not an enactment bonus
        const bool endorses=!proposed_choice.empty()?
            m.desired_choice==proposed_choice:
            m.goal==P3MovementGoal::legal_reform;
        if(endorses)success+=v.support_ppm;
        else stall+=v.support_ppm;
    }
    // One ruler endorsement step is five percentage points.
    const auto head=p1_characters_.find(
        governments_.at(bill.country).head_of_state);
    if(head!=p1_characters_.end()&&head->second.active&&
       !head->second.politically_exiled&&
       p1_character_available(head->first)) {
        const auto i=head->second.law_preferences.find(domain);
        if(i!=head->second.law_preferences.end()) {
            if(!proposed_choice.empty()&&i->second==proposed_choice)
                success+=50'000;
            else if(i->second==current_choice)stall+=50'000;
        }
    }
    if(const auto fav=p1_favorites_.find(bill.country);
       fav!=p1_favorites_.end()&&p1_characters_.contains(fav->second)&&
       p1_character_available(fav->second)) {
        const auto& ch=p1_characters_.at(fav->second);
        if(ch.active&&!ch.politically_exiled) {
            const auto i=ch.law_preferences.find(domain);
            if(i!=ch.law_preferences.end()&&
               i->second==proposed_choice)success+=25'000;
        }
    }
    success=std::clamp<std::int64_t>(
        success+enactment.success_bonus_ppm,0,1'000'000);
    stall=std::clamp<std::int64_t>(
        stall+enactment.setbacks*25'000,0,1'000'000);
    if(success+stall>1'000'000){
        const auto total=success+stall;
        success=success*1'000'000/total;
        stall=1'000'000-success;
    }
    const auto advance=std::clamp<std::int64_t>(
        2*success-stall,0,1'000'000-success-stall);
    const auto debate=1'000'000-success-advance-stall;
    return {static_cast<int>(success),static_cast<int>(advance),
            static_cast<int>(debate),static_cast<int>(stall)};
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
