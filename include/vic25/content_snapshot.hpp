#pragma once
// Complete opt-in content+native daily checkpoint. Content owns only script
// state; all physical/economic data are in the original daily snapshot.
#include "vic25/content_world.hpp"
#include "vic25/daily_snapshot.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iterator>
#include <type_traits>

namespace vic25::content {
namespace persistence {
using Writer=phasec::persistence_detail::BinaryWriter;
using Reader=phasec::persistence_detail::BinaryReader;
template<class A>void scope(A& a,Scope& s) {
    a(s.kind);a(s.id);
    if constexpr(std::is_same_v<A,Reader>)
        require(!s.kind.empty()&&!s.id.empty(),"CONTENT_BAD_SCOPE");
}
template<class A>void history(A& a,History& h) {
    a(h.day);a(h.kind);a(h.definition);a(h.scope);a(h.outcome);
}
template<class A>void event(A& a,EventInstance& e) {
    a(e.occurrence);a(e.definition);scope(a,e.scope);
    a(e.opened);a(e.expires);
}
template<class A>void journal(A& a,JournalInstance& j) {
    a(j.definition);scope(a,j.scope);a(j.opened);a(j.closed);
    a(j.outcome);
}
template<class A>void request(A& a,Request& r) {
    a(r.kind);a(r.definition);a(r.option);scope(a,r.scope);
    if constexpr(std::is_same_v<A,Reader>)
        require(static_cast<int>(r.kind)<=
                static_cast<int>(RequestKind::ai_consideration),
                "CONTENT_BAD_REQUEST_KIND");
}
template<class A>void length(A& a,std::uint64_t& count,
                              std::uint64_t max_count=2'000'000) {
    a(count);
    require(count<=max_count,"CONTENT_SNAPSHOT_OBJECT_LIMIT");
}
template<class A>void state(A& a,State& st) {
    a(st.ruleset);a(st.initialized);a(st.next_occurrence);
    a(st.counters);a(st.flags);a(st.one_shot_world);
    a(st.one_shot_scope);a(st.last_events);a(st.last_decisions);
    std::uint64_t count=0;
    if constexpr(std::is_same_v<A,Writer>)count=st.scheduled.size();
    length(a,count,500'000);
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& [day,items]:st.scheduled) {
            a(day);std::uint64_t n=items.size();length(a,n,200'000);
            for(const auto& [id,s]:items) {
                auto x=id;auto y=s;a(x);scope(a,y);
            }
        }
    } else {
        for(std::uint64_t i=0;i<count;++i) {
            Day day{};std::uint64_t n{};a(day);length(a,n,200'000);
            require(day>=0&&n>0,"CONTENT_INVALID_SCHEDULE_GROUP");
            std::vector<std::pair<std::string,Scope>> events;
            events.reserve(static_cast<std::size_t>(n));
            for(std::uint64_t j=0;j<n;++j) {
                std::string id;Scope s;a(id);scope(a,s);
                require(!id.empty(),"CONTENT_BAD_SCHEDULED_EVENT");
                events.push_back({std::move(id),std::move(s)});
            }
            require(st.scheduled.emplace(day,std::move(events)).second,
                    "CONTENT_DUPLICATE_SCHEDULE_DAY");
        }
    }
    count=0;
    if constexpr(std::is_same_v<A,Writer>)count=st.pending_choices.size();
    length(a,count);
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& [key,v]:st.pending_choices) {
            auto k=key,copy=v;a(k);event(a,copy);
        }
    } else {
        for(std::uint64_t i=0;i<count;++i) {
            std::string key;EventInstance e;a(key);event(a,e);
            require(!key.empty()&&key==e.occurrence&&
                    st.pending_choices.emplace(key,std::move(e)).second,
                    "CONTENT_DUPLICATE_CHOICE");
        }
    }
    auto journals=[&](auto& entries){
        std::uint64_t n=0;
        if constexpr(std::is_same_v<A,Writer>)n=entries.size();
        length(a,n);
        if constexpr(std::is_same_v<A,Writer>) {
            for(const auto& [key,j]:entries) {
                auto k=key,copy=j;a(k);journal(a,copy);
            }
        } else {
            for(std::uint64_t i=0;i<n;++i) {
                std::string key;JournalInstance j;a(key);journal(a,j);
                require(key==scoped(j.definition,j.scope)&&
                        entries.emplace(key,std::move(j)).second,
                        "CONTENT_DUPLICATE_JOURNAL_INSTANCE");
            }
        }
    };
    journals(st.active_journals);journals(st.finished_journals);
    count=0;
    if constexpr(std::is_same_v<A,Writer>)count=st.requests.size();
    length(a,count,500'000);
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& r:st.requests){auto copy=r;request(a,copy);}
    } else {
        st.requests.reserve(static_cast<std::size_t>(count));
        for(std::uint64_t i=0;i<count;++i) {
            Request r;request(a,r);st.requests.push_back(std::move(r));
        }
    }
    count=0;
    if constexpr(std::is_same_v<A,Writer>)count=st.history.size();
    length(a,count);
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& h:st.history){auto copy=h;history(a,copy);}
    } else {
        st.history.reserve(static_cast<std::size_t>(count));
        for(std::uint64_t i=0;i<count;++i) {
            History h;history(a,h);st.history.push_back(std::move(h));
        }
    }
    count=0;
    if constexpr(std::is_same_v<A,Writer>)count=st.ai_scopes.size();
    length(a,count,100'000);
    if constexpr(std::is_same_v<A,Writer>) {
        for(const auto& s:st.ai_scopes){auto copy=s;scope(a,copy);}
    } else {
        for(std::uint64_t i=0;i<count;++i) {
            Scope s;scope(a,s);
            require(st.ai_scopes.insert(s).second,"CONTENT_DUPLICATE_AI_SCOPE");
        }
    }
    if constexpr(std::is_same_v<A,Reader>) {
        require(!st.ruleset.empty()&&st.initialized,"CONTENT_INVALID_RULESET");
        for(const auto& [key,j]:st.active_journals)
            require(!st.finished_journals.contains(key)&&j.closed<0,
                    "CONTENT_BAD_ACTIVE_JOURNAL");
        for(const auto& [key,j]:st.finished_journals)
            require(j.closed>=j.opened&&!j.outcome.empty(),
                    "CONTENT_BAD_FINISHED_JOURNAL");
    }
}
} // namespace persistence
struct SnapshotCodec {
    static constexpr const char* magic="VIC25_CONTENT_SNAPSHOT_1";
    static constexpr const char* version="vic25-content-save/1.0";
    static std::string encode(const World& world) {
        const auto base=daily::SnapshotCodec::encode(world.native());
        auto content=world.content_state();
        std::ostringstream stream(std::ios::binary|std::ios::out);
        persistence::Writer writer{stream};
        writer(std::string(magic));writer(std::string(version));
        std::uint64_t n=base.size();writer(n);
        stream.write(base.data(),static_cast<std::streamsize>(base.size()));
        if(!stream)throw std::runtime_error("CONTENT_BASE_SNAPSHOT_WRITE");
        persistence::state(writer,content);
        const auto payload=stream.str();
        std::ostringstream tail(std::ios::binary|std::ios::out);
        persistence::Writer checksum{tail};
        checksum(phasec::persistence_detail::checksum(payload));
        return payload+tail.str();
    }
    static World decode(const std::string& bytes,
                        std::shared_ptr<const Registry> rules) {
        require(bytes.size()>=64&&bytes.size()<=900'000'000,
                "CONTENT_INVALID_SNAPSHOT_SIZE");
        const std::string payload=bytes.substr(0,bytes.size()-8);
        std::uint64_t expected{};
        std::istringstream trailer(bytes.substr(bytes.size()-8),
                                   std::ios::binary|std::ios::in);
        persistence::Reader chk{trailer};chk(expected);
        require(expected==phasec::persistence_detail::checksum(payload),
                "CONTENT_SNAPSHOT_CHECKSUM_MISMATCH");
        std::istringstream stream(payload,std::ios::binary|std::ios::in);
        persistence::Reader reader{stream};
        std::string m,v;reader(m);reader(v);
        require(m==magic&&v==version,"CONTENT_SNAPSHOT_VERSION_MISMATCH");
        std::uint64_t length{};reader(length);
        require(length>0&&length<=850'000'000,
                "CONTENT_BASE_SNAPSHOT_INVALID_SIZE");
        std::string native(length,'\0');
        stream.read(native.data(),static_cast<std::streamsize>(length));
        require(static_cast<bool>(stream),"CONTENT_BASE_SNAPSHOT_TRUNCATED");
        State st;persistence::state(reader,st);
        require(stream.peek()==std::char_traits<char>::eof(),
                "CONTENT_SNAPSHOT_TRAILING_BYTES");
        auto base=daily::SnapshotCodec::decode(native);
        return World::restore(std::move(base),std::move(rules),std::move(st));
    }
    static World migrate_legacy_daily(const std::string& legacy,
                           std::shared_ptr<const Registry> rules) {
        return World::adopt(daily::SnapshotCodec::decode(legacy),
                            std::move(rules));
    }
    static void save(const World& world,const std::filesystem::path& path) {
        require(!std::filesystem::exists(path),"CONTENT_REFUSE_OVERWRITE");
        const auto data=encode(world);
        const auto temporary=path.string()+".uncommitted";
        {
            std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);
            require(static_cast<bool>(stream),"CONTENT_SAVE_OPEN_FAILED");
            stream.write(data.data(),static_cast<std::streamsize>(data.size()));
            stream.flush();
            require(static_cast<bool>(stream),"CONTENT_SAVE_WRITE_FAILED");
        }
        std::filesystem::rename(temporary,path);
    }
    static World load(const std::filesystem::path& path,
                      std::shared_ptr<const Registry> rules) {
        std::ifstream stream(path,std::ios::binary);
        require(static_cast<bool>(stream),"CONTENT_SAVE_NOT_FOUND");
        const std::string data{std::istreambuf_iterator<char>{stream},
                               std::istreambuf_iterator<char>{}};
        return decode(data,std::move(rules));
    }
};
} // namespace vic25::content
