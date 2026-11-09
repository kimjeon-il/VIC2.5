#pragma once
// A parliament can originate a bill independently of the cabinet. A member
// sponsor must be an already elected P4 person, not a newly minted politician.
namespace vic25::phasec {
inline bool IntegratedWorld::p2_sponsor_authorized(const P2Bill& b)const {
    const auto it=p2_sponsors_.find(b.id);
    if(it==p2_sponsors_.end())
        return governments_.contains(b.country)&&
               b.proposer==governments_.at(b.country).prime_minister;
    const auto& sponsor=it->second;
    if(sponsor.country!=b.country||sponsor.bill_id!=b.id||
       b.proposer!=sponsor.party||
       !p1_constitutions_.contains(b.country)||
       !p1_legislative_checks(b.country).parliamentary_initiative||
       p1_constitution(b.country).legislature==P1LegislatureKind::none||
       p4_parliament_dissolved(b.country)||
       !p4_parties_.contains(sponsor.party)||
       p4_parties_.at(sponsor.party).country!=b.country||
       !p4_parties_.at(sponsor.party).active||
       !country_elections_.contains(b.country))return false;
    const auto& chamber=country_elections_.at(b.country);
    if(!chamber.seats.contains(sponsor.party)||
       chamber.seats.at(sponsor.party)<=0)return false;
    if(sponsor.kind==P2SponsorKind::parliamentary_party)
        return sponsor.person.empty();
    if(sponsor.kind!=P2SponsorKind::elected_member||
       sponsor.person.empty()||
       !person_anchors_.contains(sponsor.person)||
       !p1_character_available(sponsor.person))
        return false;
    for(const auto& [id,c]:p4_contests_)
        if(c.country==b.country&&c.office==P4Office::legislature) {
            const auto record=p4_person_terms_.find({id,sponsor.person});
            if(record!=p4_person_terms_.end()&&
               record->second.incumbent&&
               record->second.party_id==sponsor.party)
                return true;
        }
    return false;
}
inline void IntegratedWorld::p2_submit_parliamentary_bill(
        P2Bill bill,const std::string& party,
        const std::string& person,std::int64_t today) {
    require_stage({9});
    ensure(!bill.id.empty()&&!party.empty()&&today>=0&&
           !p2_sponsors_.contains(bill.id),
           "P2 invalid independent parliamentary initiative");
    bill.proposer=party;
    p2_sponsors_[bill.id]=P2Sponsor{bill.id,bill.country,party,person,
          person.empty()?P2SponsorKind::parliamentary_party:
                         P2SponsorKind::elected_member,today};
    ensure(p2_sponsor_authorized(bill),
           "P2 proposed bill has no actual elected legislator or party mandate");
    // All existing P2 legal slots, cooldown, concessions, enactment checks
    // remain authoritative; only the initial proposal right differs.
    p2_submit_bill(std::move(bill),today);
    events_.push_back("p2-independent-legislative-initiative:"+party);
}
} // namespace vic25::phasec
