#pragma once
// P4 person-bound elective mandates. Uses C's real named POP anchors;
// no invented politicians or parallel population ledger.
namespace vic25::phasec {

inline void IntegratedWorld::p4_nominate(
        const std::string& contest_id,const std::string& party,
        std::vector<std::string> persons) {
    ensure((stage_==-1&&commit_id_==0)||stage_==9,
           "P4 candidate registration needs political stage");
    const auto& contest=p4_contests_.at(contest_id);
    const auto& organization=p4_parties_.at(party);
    ensure(organization.country==contest.country&&
           organization.active&&organization.registered&&
           !persons.empty()&&
           persons.size()<=static_cast<std::size_t>(contest.seats),
           "P4 nominations must belong to registered party and real seats");
    std::set<std::string> unique;
    for(const auto& person:persons) {
        ensure(!person.empty()&&unique.insert(person).second&&
               person_anchors_.contains(person),
               "P4 nominee must be one unique real named POP");
        const auto id=person_anchors_.at(person);
        ensure(people_.has_slice(id),"P4 nominee missing POP identity");
        const auto& dem=people_.demographic(people_.slice(id).key.demographic);
        const auto sovereign=sovereignty_.find(dem.region);
        ensure(sovereign!=sovereignty_.end()&&
               sovereign->second==contest.country&&
               (contest.office!=P4Office::local&&
                contest.office!=P4Office::local_head||
                dem.region==contest.jurisdiction)&&
               dem.citizenship=="citizen",
               "P4 nomination lacks citizenship or office jurisdiction");
        for(const auto& [key,list]:p4_nominations_)
            if(key.first==contest_id&&key.second!=party)
                ensure(std::find(list.begin(),list.end(),person)==list.end(),
                       "P4 person cannot run on multiple party lists");
    }
    p4_nominations_[{contest_id,party}]=std::move(persons);
    events_.push_back("p4-named-person-nominations:"+contest_id);
}

inline void IntegratedWorld::p4_record_elected_persons(
        const P4Result& result) {
    const auto& contest=p4_contests_.at(result.contest_id);
    std::map<std::string,std::string> winners;
    for(const auto& [party,seats]:result.seats) {
        if(seats<=0)continue;
        const auto nominations=p4_nominations_.find({result.contest_id,party});
        if(nominations==p4_nominations_.end())continue; // legacy party-only list
        ensure(seats<=static_cast<People>(nominations->second.size()),
               "P4 winning party lacks real named candidates for awarded seats");
        for(People index=0;index<seats;++index) {
            const auto& person=nominations->second.at(static_cast<std::size_t>(index));
            ensure(p2_candidate_eligible(result.contest_id,person),
                   "P2 consecutive or lifetime office term limit exceeded");
            ensure(winners.emplace(person,party).second,
                   "P4 individual may occupy only one seat in a contest");
            for(const auto& [key,record]:p4_person_terms_)
                ensure(key.first==result.contest_id||
                       key.second!=person||!record.incumbent,
                       "P4 double elected office requires an explicit dual-mandate law");
        }
    }
    // End the prior personal mandates in this elected institution only;
    // historical term counters remain and returning candidates are tracked.
    for(auto& [key,record]:p4_person_terms_) {
        if(key.first!=result.contest_id)continue;
        if(!winners.contains(key.second))record.consecutive_terms=0;
        record.incumbent=false;
    }
    for(const auto& [person,party]:winners) {
        auto& record=p4_person_terms_[{result.contest_id,person}];
        if(record.total_terms==0){
            record.contest_id=result.contest_id;record.person_id=person;
        }
        const bool consecutive=record.consecutive_terms>0;
        ensure(record.total_terms<std::numeric_limits<std::int32_t>::max()&&
               record.consecutive_terms<std::numeric_limits<std::int32_t>::max(),
               "P4 elected mandate counter overflow");
        ++record.total_terms;
        record.consecutive_terms=consecutive?record.consecutive_terms+1:1;
        record.incumbent=true;
        record.party_id=party;
        record.last_elected_day=result.day;
    }
    if(!winners.empty())
        events_.push_back("p4-real-POP-personal-term-certified:"+contest.id);
}

} // namespace vic25::phasec
