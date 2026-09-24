// File: VcdWriter.cpp
#include "VcdWriter.h"
#include <cctype>
#include <map>
#include <stdexcept>

namespace
{
    /// Scope tree built from dotted names.
    struct Scope
    {
        std::map<std::string, Scope> children;
        std::vector<std::pair<std::string, std::string>> vars; ///< (code, name)
    };

    /// VCD names cannot contain whitespace.
    std::string sanitize(std::string s)
    {
        if (s.empty()) return "_";
        for (char& c : s)
            if (std::isspace(static_cast<unsigned char>(c))) c = '_';
        return s;
    }

    void emit(std::ostream& out, const Scope& scope)
    {
        for (const auto& v : scope.vars)
            out << "$var wire 1 " << v.first << ' ' << v.second << " $end\n";
        for (const auto& child : scope.children)
        {
            out << "$scope module " << child.first << " $end\n";
            emit(out, child.second);
            out << "$upscope $end\n";
        }
    }
} // namespace

VcdWriter::VcdWriter(std::ostream& out, std::string timescale, std::string top_scope)
    : out_(out),
      timescale_(std::move(timescale)),
      top_(sanitize(std::move(top_scope)))
{
}

VcdWriter::~VcdWriter()
{
    detach();
}

std::string VcdWriter::codeFor_(std::size_t index)
{
    // Base-94 over the printable characters '!'..'~'.
    std::string code;
    do
    {
        code.push_back(static_cast<char>('!' + index % 94));
        index /= 94;
    } while (index != 0);
    return code;
}

char VcdWriter::vcdChar_(LogicValue v)
{
    switch (v)
    {
        case LogicValue::LOW:   return '0';
        case LogicValue::HIGH:  return '1';
        case LogicValue::HIGHZ: return 'z';
        default:                return 'x';
    }
}

void VcdWriter::addNet(const Net& net)
{
    if (header_written_)
        throw std::logic_error("VcdWriter: nets must be added before the header");
    if (index_.count(&net)) return;

    index_.emplace(&net, vars_.size());
    vars_.push_back(Var{&net, net.id(), codeFor_(vars_.size())});
}

void VcdWriter::writeHeader(uint64_t time)
{
    if (header_written_)
        throw std::logic_error("VcdWriter: header already written");
    header_written_ = true;

    out_ << "$version logicsim $end\n"
         << "$timescale " << timescale_ << " $end\n";

    Scope root;
    for (const Var& v : vars_)
    {
        // "a.b.c" -> scopes a, b and signal c.
        Scope* scope = &root;
        const std::string& name = v.net->getName();
        std::size_t start = 0;
        for (std::size_t dot; (dot = name.find('.', start)) != std::string::npos; start = dot + 1)
            scope = &scope->children[sanitize(name.substr(start, dot - start))];
        scope->vars.emplace_back(v.code, sanitize(name.substr(start)));
    }

    out_ << "$scope module " << top_ << " $end\n";
    emit(out_, root);
    out_ << "$upscope $end\n"
         << "$enddefinitions $end\n";

    writeTime_(time);
    out_ << "$dumpvars\n";
    for (Var& v : vars_)
    {
        v.last = vcdChar_(v.net->getValue());
        out_ << v.last << v.code << '\n';
    }
    out_ << "$end\n";
}

void VcdWriter::writeTime_(uint64_t time)
{
    if (time_written_ && time == last_time_) return;
    out_ << '#' << time << '\n';
    time_written_ = true;
    last_time_ = time;
}

void VcdWriter::change(const Net& net, LogicValue value, uint64_t time)
{
    if (!header_written_ || (time_written_ && time < last_time_)) return;

    const auto it = index_.find(&net);
    if (it == index_.end()) return;
    Var& v = vars_[it->second];
    const char c = vcdChar_(value);
    if (v.net_id != net.id() || c == v.last) return;

    writeTime_(time);
    v.last = c;
    out_ << c << v.code << '\n';
}

void VcdWriter::attach(Simulator& sim)
{
    std::vector<const Net*> all;
    for (const Net* n : sim.nets()) all.push_back(n);
    attach(sim, all);
}

void VcdWriter::attach(Simulator& sim, const std::vector<const Net*>& nets)
{
    if (sim_)
        throw std::logic_error("VcdWriter: already attached");

    // Initialise first so the dumped starting values are the real ones.
    sim.initialize();
    for (const Net* n : nets)
        if (n) addNet(*n);
    writeHeader(sim.now());

    sim_ = &sim;
    listener_ = sim.addChangeListener(
        [this](const Net& net, LogicValue, uint64_t time)
        {
            change(net, net.getValue(), time);
        });
}

void VcdWriter::detach()
{
    if (!sim_) return;
    sim_->removeChangeListener(listener_);
    sim_ = nullptr;
    out_.flush();
}
