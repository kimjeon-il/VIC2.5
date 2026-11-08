#pragma once
// Data-driven VIC2.5 content rule loading. A bounded, integer-only JSON
// grammar is intentional: effects remain typed and there is no script VM.
#include "vic25/content.hpp"
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <initializer_list>
#include <limits>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace vic25::content::json {
struct Value {
    using Array=std::vector<Value>;
    using Object=std::map<std::string,Value>;
    std::variant<std::nullptr_t,bool,std::int64_t,std::string,Array,Object> data;
    template<class T>explicit Value(T value):data(std::move(value)){}
    [[nodiscard]] const Object& object()const {
        require(std::holds_alternative<Object>(data),"CONTENT_JSON_EXPECT_OBJECT");
        return std::get<Object>(data);
    }
    [[nodiscard]] const Array& array()const {
        require(std::holds_alternative<Array>(data),"CONTENT_JSON_EXPECT_ARRAY");
        return std::get<Array>(data);
    }
    [[nodiscard]] std::string string()const {
        require(std::holds_alternative<std::string>(data),
                "CONTENT_JSON_EXPECT_STRING");
        return std::get<std::string>(data);
    }
    [[nodiscard]] std::int64_t integer()const {
        require(std::holds_alternative<std::int64_t>(data),
                "CONTENT_JSON_EXPECT_INTEGER");
        return std::get<std::int64_t>(data);
    }
    [[nodiscard]] bool boolean()const {
        require(std::holds_alternative<bool>(data),"CONTENT_JSON_EXPECT_BOOLEAN");
        return std::get<bool>(data);
    }
};
inline const Value* maybe(const Value::Object& o,const std::string& field) {
    const auto it=o.find(field);
    return it==o.end()?nullptr:&it->second;
}
inline const Value& must(const Value::Object& o,const std::string& field) {
    const auto it=o.find(field);
    require(it!=o.end(),"CONTENT_JSON_MISSING_REQUIRED_FIELD");
    return it->second;
}
inline std::string str(const Value::Object& o,const std::string& k,
                       std::string fallback={}) {
    if(const auto* p=maybe(o,k))return p->string();
    return fallback;
}
inline std::int64_t num(const Value::Object& o,const std::string& k,
                       std::int64_t fallback=0) {
    if(const auto* p=maybe(o,k))return p->integer();
    return fallback;
}
inline bool boolean(const Value::Object& o,const std::string& k,bool fallback=false) {
    if(const auto* p=maybe(o,k))return p->boolean();
    return fallback;
}
inline void keys(const Value::Object& obj,
                 std::initializer_list<std::string_view> allowed) {
    for(const auto& [key,val]:obj) {
        (void)val;
        bool found=false;
        for(const auto candidate:allowed)
            if(candidate==key)found=true;
        require(found,"CONTENT_JSON_UNKNOWN_FIELD");
    }
}
inline std::uint64_t positive_id(const Value& v) {
    const auto id=v.integer();
    require(id>0,"CONTENT_JSON_INVALID_PROVINCE_ID");
    return static_cast<std::uint64_t>(id);
}
class Parser {
    std::string_view source_;
    std::size_t position_{};
    void whitespace() {
        while(position_<source_.size()) {
            const auto c=source_[position_];
            if(c==' '||c=='\n'||c=='\r'||c=='\t')++position_;
            else break;
        }
    }
    char take() {
        require(position_<source_.size(),"CONTENT_JSON_UNEXPECTED_EOF");
        return source_[position_++];
    }
    bool eat(char expected) {
        whitespace();
        if(position_<source_.size()&&source_[position_]==expected) {
            ++position_;
            return true;
        }
        return false;
    }
    void expect(char expected) {
        whitespace();
        require(take()==expected,"CONTENT_JSON_EXPECTED_DELIMITER");
    }
    static void utf8(std::string& result,std::uint32_t code) {
        require(code<=0x10ffff&&!(code>=0xd800&&code<=0xdfff),
                "CONTENT_JSON_INVALID_UNICODE");
        if(code<0x80)result.push_back(static_cast<char>(code));
        else if(code<0x800) {
            result.push_back(static_cast<char>(0xc0|(code>>6)));
            result.push_back(static_cast<char>(0x80|(code&0x3f)));
        } else if(code<0x10000) {
            result.push_back(static_cast<char>(0xe0|(code>>12)));
            result.push_back(static_cast<char>(0x80|((code>>6)&0x3f)));
            result.push_back(static_cast<char>(0x80|(code&0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0|(code>>18)));
            result.push_back(static_cast<char>(0x80|((code>>12)&0x3f)));
            result.push_back(static_cast<char>(0x80|((code>>6)&0x3f)));
            result.push_back(static_cast<char>(0x80|(code&0x3f)));
        }
    }
    std::uint16_t hex_escape() {
        require(position_+4<=source_.size(),"CONTENT_JSON_BAD_UNICODE_ESCAPE");
        std::uint16_t value=0;
        for(int i=0;i<4;++i) {
            const auto c=take();
            value=static_cast<std::uint16_t>(value<<4);
            if(c>='0'&&c<='9')value|=static_cast<std::uint16_t>(c-'0');
            else if(c>='a'&&c<='f')value|=static_cast<std::uint16_t>(10+c-'a');
            else if(c>='A'&&c<='F')value|=static_cast<std::uint16_t>(10+c-'A');
            else throw std::invalid_argument("CONTENT_JSON_BAD_HEX_ESCAPE");
        }
        return value;
    }
    std::string parse_string() {
        expect('"');
        std::string out;
        while(true) {
            const auto c=take();
            if(c=='"')break;
            if(c=='\\') {
                const auto escape=take();
                switch(escape) {
                    case '"':out+='"';break;
                    case '\\':out+='\\';break;
                    case '/':out+='/';break;
                    case 'b':out+='\b';break;
                    case 'f':out+='\f';break;
                    case 'n':out+='\n';break;
                    case 'r':out+='\r';break;
                    case 't':out+='\t';break;
                    case 'u': {
                        auto cp=static_cast<std::uint32_t>(hex_escape());
                        if(cp>=0xd800&&cp<=0xdbff) {
                            require(take()=='\\'&&take()=='u',
                                    "CONTENT_JSON_MISSING_SURROGATE");
                            const auto lo=hex_escape();
                            require(lo>=0xdc00&&lo<=0xdfff,
                                    "CONTENT_JSON_INVALID_SURROGATE");
                            cp=0x10000+(cp-0xd800)*0x400+(lo-0xdc00);
                        }
                        utf8(out,cp);
                        break;
                    }
                    default:throw std::invalid_argument("CONTENT_JSON_BAD_ESCAPE");
                }
            } else {
                require(static_cast<unsigned char>(c)>=0x20,
                        "CONTENT_JSON_CONTROL_IN_STRING");
                out+=c;
            }
            require(out.size()<=16'384,"CONTENT_JSON_STRING_TOO_LONG");
        }
        return out;
    }
    Value parse_value(int depth) {
        require(depth<=32,"CONTENT_JSON_NESTING_LIMIT");
        whitespace();
        require(position_<source_.size(),"CONTENT_JSON_UNEXPECTED_EOF");
        if(eat('{')) {
            Value::Object o;
            if(!eat('}')) {
                do {
                    whitespace();
                    require(position_<source_.size()&&source_[position_]=='"',
                            "CONTENT_JSON_EXPECTED_KEY");
                    const auto key=parse_string();
                    expect(':');
                    require(o.emplace(key,parse_value(depth+1)).second,
                            "CONTENT_JSON_DUPLICATE_KEY");
                    require(o.size()<=30'000,"CONTENT_JSON_OBJECT_LIMIT");
                    if(eat('}'))break;
                    expect(',');
                } while(true);
            }
            return Value{std::move(o)};
        }
        if(eat('[')) {
            Value::Array a;
            if(!eat(']')) {
                do {
                    a.push_back(parse_value(depth+1));
                    require(a.size()<=30'000,"CONTENT_JSON_ARRAY_LIMIT");
                    if(eat(']'))break;
                    expect(',');
                } while(true);
            }
            return Value{std::move(a)};
        }
        if(source_[position_]=='"')return Value{parse_string()};
        for(const auto& [literal,truth]:std::initializer_list<
                std::pair<std::string_view,bool>>{{"true",true},{"false",false}}) {
            if(source_.substr(position_,literal.size())==literal) {
                position_+=literal.size();
                return Value{truth};
            }
        }
        if(source_.substr(position_,4)=="null") {
            position_+=4;return Value{nullptr};
        }
        const auto start=position_;
        if(source_[position_]=='-')++position_;
        require(position_<source_.size()&&source_[position_]>='0'&&
                source_[position_]<='9',"CONTENT_JSON_EXPECT_INTEGER");
        if(source_[position_]=='0')++position_;
        else while(position_<source_.size()&&source_[position_]>='0'&&
                   source_[position_]<='9')++position_;
        const auto token=source_.substr(start,position_-start);
        std::int64_t value{};
        const auto from=token.data(),end=token.data()+token.size();
        const auto converted=std::from_chars(from,end,value);
        require(converted.ec==std::errc{}&&converted.ptr==end,
                "CONTENT_JSON_INTEGER_OVERFLOW");
        return Value{value};
    }
public:
    explicit Parser(std::string_view source):source_(source) {
        require(!source.empty()&&source.size()<=4'000'000,
                "CONTENT_JSON_SOURCE_SIZE");
    }
    [[nodiscard]] Value parse() {
        auto result=parse_value(0);
        whitespace();
        require(position_==source_.size(),"CONTENT_JSON_TRAILING_DATA");
        return result;
    }
};
inline Value parse(std::string_view input) {
    return Parser{input}.parse();
}
inline Condition condition(const Value& v,int depth=0) {
    require(depth<=16,"CONTENT_CONDITION_DEPTH");
    if(std::holds_alternative<bool>(v.data))
        return {v.boolean()?Test::yes:Test::no,"",0,{}};
    const auto& o=v.object();
    if(o.empty())return {};
    require(o.size()==1,"CONTENT_CONDITION_SINGLE_OPERATOR");
    const auto& [key,arg]=*o.begin();
    if(key=="all"||key=="any"||key=="none"||key=="not") {
        Condition result;
        result.kind=key=="all"?Test::all:key=="any"?Test::any:Test::none;
        if(key=="not")
            result.children.push_back(condition(arg,depth+1));
        else for(const auto& c:arg.array())
            result.children.push_back(condition(c,depth+1));
        Registry::validate_condition(result);
        return result;
    }
    if(key=="flag")return Condition::flag(arg.string());
    for(const auto& [name,kind]:std::initializer_list<
            std::pair<std::string_view,Test>>{
            {"counter_ge",Test::counter_ge},{"counter_le",Test::counter_le},
            {"domain_ge",Test::domain_ge},{"domain_le",Test::domain_le}}) {
        if(key==name) {
            const auto& data=arg.object();
            keys(data,{"name","value"});
            return {kind,must(data,"name").string(),
                    must(data,"value").integer(),{}};
        }
    }
    throw std::invalid_argument("CONTENT_UNKNOWN_CONDITION_TYPE");
}
inline External external(const Value& v) {
    const auto& o=v.object();
    keys(o,{"kind","target_country","admin_name","head","prime","admin_id",
            "provinces","seats","quorum"});
    External result;
    const auto kind=must(o,"kind").string();
    if(kind=="independence")result.kind=ExternalKind::independence;
    else if(kind=="transfer")result.kind=ExternalKind::transfer;
    else if(kind=="claim")result.kind=ExternalKind::claim;
    else if(kind=="colonize")result.kind=ExternalKind::colonize;
    else throw std::invalid_argument("CONTENT_UNKNOWN_EXTERNAL_KIND");
    result.target_country=str(o,"target_country");
    result.admin_name=str(o,"admin_name");
    result.head=str(o,"head");result.prime=str(o,"prime");
    if(const auto* p=maybe(o,"admin_id"))result.admin_id=positive_id(*p);
    result.seats=num(o,"seats");result.quorum=num(o,"quorum");
    if(const auto* p=maybe(o,"provinces"))
        for(const auto& province:p->array())
            result.provinces.push_back(positive_id(province));
    return result;
}
inline Effect effect(const Value& v) {
    const auto& o=v.object();
    keys(o,{"kind","id","amount","delay","external"});
    Effect e;
    const auto kind=must(o,"kind").string();
    if(kind=="set_flag")e.kind=EffectKind::set_flag;
    else if(kind=="clear_flag")e.kind=EffectKind::clear_flag;
    else if(kind=="add_counter")e.kind=EffectKind::add_counter;
    else if(kind=="schedule_event")e.kind=EffectKind::schedule_event;
    else if(kind=="open_journal")e.kind=EffectKind::open_journal;
    else if(kind=="stop_journal")e.kind=EffectKind::stop_journal;
    else if(kind=="external")e.kind=EffectKind::external;
    else throw std::invalid_argument("CONTENT_UNKNOWN_EFFECT_TYPE");
    e.id=str(o,"id");e.amount=num(o,"amount");e.delay=num(o,"delay");
    if(e.kind==EffectKind::external)
        e.external=external(must(o,"external"));
    else require(!maybe(o,"external"),"CONTENT_UNEXPECTED_EXTERNAL_PAYLOAD");
    return e;
}
inline std::vector<Effect> effects(const Value::Object& o) {
    std::vector<Effect> result;
    if(auto p=maybe(o,"effects"))for(const auto& effect_value:p->array())
        result.push_back(effect(effect_value));
    return result;
}
inline Condition cond(const Value::Object& o,const std::string& key) {
    if(auto p=maybe(o,key))return condition(*p);
    return {};
}
inline Option option(const Value& v) {
    const auto& o=v.object();
    keys(o,{"id","available","effects","ai_weight"});
    Option result;
    result.id=must(o,"id").string();
    result.available=cond(o,"available");
    result.effects=effects(o);
    result.ai_weight=num(o,"ai_weight",1);
    return result;
}
inline Event event(const Value& v) {
    const auto& o=v.object();
    keys(o,{"id","on_action","trigger","options","once_scope","once_world",
            "fixed_day","cooldown_days","choice_timeout","default_option"});
    Event e;
    e.id=must(o,"id").string();
    e.on_action=str(o,"on_action");
    e.trigger=cond(o,"trigger");
    e.once_scope=boolean(o,"once_scope");
    e.once_world=boolean(o,"once_world");
    e.fixed_day=num(o,"fixed_day",-1);
    e.cooldown_days=num(o,"cooldown_days");
    e.choice_timeout=num(o,"choice_timeout",7);
    e.default_option=str(o,"default_option");
    for(const auto& option_value:must(o,"options").array())
        e.options.push_back(option(option_value));
    return e;
}
inline Decision decision(const Value& v) {
    const auto& o=v.object();
    keys(o,{"id","visible","available","effects","once_scope","once_world",
            "cooldown_days","ai_weight"});
    Decision result;
    result.id=must(o,"id").string();
    result.visible=cond(o,"visible");
    result.available=cond(o,"available");
    result.effects=effects(o);
    result.once_scope=boolean(o,"once_scope",true);
    result.once_world=boolean(o,"once_world");
    result.cooldown_days=num(o,"cooldown_days");
    result.ai_weight=num(o,"ai_weight");
    return result;
}
inline Outcome outcome(const Value& v) {
    const auto& o=v.object();
    keys(o,{"id","trigger","effects","priority","on_timeout"});
    Outcome result;
    result.id=must(o,"id").string();
    result.trigger=cond(o,"trigger");
    result.effects=effects(o);
    const auto priority=num(o,"priority");
    require(priority>=std::numeric_limits<int>::min()&&
            priority<=std::numeric_limits<int>::max(),
            "CONTENT_JSON_PRIORITY_OVERFLOW");
    result.priority=static_cast<int>(priority);
    result.on_timeout=boolean(o,"on_timeout");
    return result;
}
inline Journal journal(const Value& v) {
    const auto& o=v.object();
    keys(o,{"id","group","activation","progress_metric","progress_target",
            "timeout_days","automatic","world","outcomes"});
    Journal result;
    result.id=must(o,"id").string();
    result.group=str(o,"group");
    result.activation=cond(o,"activation");
    result.progress_metric=str(o,"progress_metric");
    result.progress_target=num(o,"progress_target",100);
    result.timeout_days=num(o,"timeout_days",-1);
    result.automatic=boolean(o,"automatic");
    result.world=boolean(o,"world");
    for(const auto& outcome_value:must(o,"outcomes").array())
        result.outcomes.push_back(outcome(outcome_value));
    return result;
}
inline Formation formation(const Value& v) {
    const auto& o=v.object();
    keys(o,{"id","target_country","eligibility","required_provinces","needed_ppm",
            "grand_unification","external"});
    Formation result;
    result.id=must(o,"id").string();
    result.target_country=must(o,"target_country").string();
    result.eligibility=cond(o,"eligibility");
    for(const auto& p:must(o,"required_provinces").array())
        result.required_provinces.push_back(positive_id(p));
    result.needed_ppm=num(o,"needed_ppm",1'000'000);
    result.grand_unification=boolean(o,"grand_unification");
    result.external=external(must(o,"external"));
    return result;
}
inline Registry load(std::string_view contents) {
    const auto parsed=parse(contents);
    const auto& root=parsed.object();
    keys(root,{"ruleset","events","decisions","journals","formations"});
    Registry registry(must(root,"ruleset").string());
    if(const auto* p=maybe(root,"events"))
        for(const auto& item:p->array())registry.add(event(item));
    if(const auto* p=maybe(root,"decisions"))
        for(const auto& item:p->array())registry.add(decision(item));
    if(const auto* p=maybe(root,"journals"))
        for(const auto& item:p->array())registry.add(journal(item));
    if(const auto* p=maybe(root,"formations"))
        for(const auto& item:p->array())registry.add(formation(item));
    return registry;
}
inline Registry load_file(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    require(static_cast<bool>(input),"CONTENT_JSON_RULESET_NOT_FOUND");
    input.seekg(0,std::ios::end);
    const auto size=input.tellg();
    require(size>0&&size<=4'000'000,"CONTENT_JSON_SOURCE_SIZE");
    input.seekg(0);
    std::string text(static_cast<std::size_t>(size),'\0');
    input.read(text.data(),static_cast<std::streamsize>(text.size()));
    require(static_cast<bool>(input),"CONTENT_JSON_RULESET_TRUNCATED");
    return load(text);
}
} // namespace vic25::content::json
