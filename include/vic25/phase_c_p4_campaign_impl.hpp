#pragma once
// C-P4: election-linked party formation and campaigning over real POP/IGs.
// Party affinity remains non-exclusive; official membership is one IG->party.
namespace vic25::phasec {
inline void IntegratedWorld::p4_set_official_affiliation(
        const std::string& country,const std::string& group,
        const std::string& party,std::int64_t today) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P4 affiliation requires opening seed or political stage");
    ensure(today>=0&&p3_groups_.contains({country,group})&&
           p4_parties_.contains(party)&&
           p4_parties_.at(party).country==country&&
           p4_parties_.at(party).active,
           "P4 official membership needs real country, group and party");
    p4_official_affiliations_[{country,group}]=
        P4OfficialAffiliation{country,group,party,today};
    events_.push_back("p4-formal-interest-group-party-membership:"+party);
}
inline bool IntegratedWorld::p4_has_campaign_due(std::int64_t today)const {
    if(today<0||p4_contests_.empty())return false;
    if(today%30==0&&!p3_groups_.empty())return true;
    for(const auto& [id,c]:p4_contests_) {
        (void)id;
        if(!c.active||c.next_poll_day<today||c.next_poll_day-today>180)
            continue;
        if((c.next_poll_day-today)%7==0)return true;
    }
    return false;
}
inline void IntegratedWorld::p4_daily_party_campaigns(std::int64_t today) {
    require_stage({9});
    if(!p4_has_campaign_due(today))return;
    // Non-electoral societies do not spontaneously acquire parties.
    // Existing P4 constituencies provide an actual election context.
    for(const auto& [key,group]:p3_groups_) {
        const auto& country=key.first;
        bool has_contest=false;
        for(const auto& [id,c]:p4_contests_) {
            (void)id;
            if(c.country==country&&c.active){has_contest=true;break;}
        }
        if(!has_contest||p3_group_view(country,key.second).members<=0)continue;
        if(p4_official_affiliations_.contains(key))continue;
        P4PartyType type=P4PartyType::conservative;
        switch(group.type) {
          case P3GroupType::industrialists:type=P4PartyType::free_trade;break;
          case P3GroupType::armed_forces:type=P4PartyType::military;break;
          case P3GroupType::devout:type=P4PartyType::religious;break;
          case P3GroupType::intelligentsia:type=P4PartyType::liberal;break;
          case P3GroupType::petite_bourgeoisie:type=P4PartyType::nationalist;break;
          case P3GroupType::rural_folk:type=P4PartyType::agrarian;break;
          case P3GroupType::trade_unions:type=P4PartyType::social_democratic;break;
          default:break;
        }
        std::string party;
        int best_affinity=-1;
        // Prefer an existing explicitly compatible party before creating one.
        for(const auto& [id,candidate]:p4_parties_) {
            if(candidate.country!=country||!candidate.active||
               !candidate.registered)continue;
            const auto it=p4_affinities_.find({id,key.second});
            if(it!=p4_affinities_.end()&&it->second>best_affinity) {
                best_affinity=it->second;party=id;
            }
        }
        if(best_affinity<500'000&&
           p1_constitution(country).power!=P1PowerDistribution::single_party) {
            const auto id=country+":party:"+std::to_string(
                static_cast<int>(type));
            if(!p4_parties_.contains(id)) {
                p4_register_party(P4Party{id,country,"",100'000,true,true});
                p4_party_identities_[id]=P4PartyIdentity{id,type,today,-1};
            }
            if(p4_parties_.at(id).active)party=id;
        }
        if(party.empty()) {
            for(const auto& [id,candidate]:p4_parties_)
                if(candidate.country==country&&candidate.active&&
                   candidate.registered){party=id;break;}
        }
        if(!party.empty())
            p4_set_official_affiliation(country,key.second,party,today);
    }
    for(const auto& [id,contest]:p4_contests_) {
        if(!contest.active||contest.next_poll_day<today||
           contest.next_poll_day-today>180||
           (contest.next_poll_day-today)%7!=0)continue;
        const auto start=contest.next_poll_day-180;
        for(const auto& [pid,party]:p4_parties_) {
            if(party.country!=contest.country||!party.active||
               !party.registered)continue;
            const auto key=std::make_pair(id,pid);
            auto& c=p4_campaigns_[key];
            if(c.contest_id!=id||c.party_id!=pid||c.start_day!=start)
                c=P4Campaign{id,pid,start,-1,0};
            if(c.last_review_day==today)continue;
            std::int64_t clout=0;
            for(const auto& [ig,official]:p4_official_affiliations_)
                if(official.country==party.country&&official.party==pid)
                    clout+=p3_group_view(ig.first,ig.second).clout_ppm;
            c.momentum_ppm=static_cast<std::int32_t>(
                std::min<std::int64_t>(500'000,
                   c.momentum_ppm+2'000+std::min<std::int64_t>(clout/30,40'000)));
            c.last_review_day=today;
        }
    }
}
} // namespace vic25::phasec
