#include "object.h"

#include <sstream>
#include <string_view>

#include "statement.h"

using namespace std;

namespace Runtime {

void ClassInstance::Print(std::ostream& os) {
    if (HasMethod("__str__", 0)) {
        auto res = Call("__str__", {});
        res->Print(os);
    }
    else os << this;
}

bool ClassInstance::HasMethod(const std::string& method_name, size_t argument_count) const {
    const Method* method = cls.GetMethod(method_name);
    return method && method->formal_params.size() == argument_count;
}

const Closure& ClassInstance::Fields() const {
    return fields;
}

Closure& ClassInstance::Fields() {
    return fields;
}

ClassInstance::ClassInstance(const Class& cls) : cls(cls) {
}

ObjectHolder ClassInstance::Call(const std::string& method_name, const std::vector<ObjectHolder>& actual_args) {
    if (!HasMethod(method_name, actual_args.size())) throw runtime_error("no such method");
    Closure closure;
    closure["self"] = ObjectHolder::Share(*this);
    const Method* method = cls.GetMethod(method_name);
    for (size_t i = 0; i < actual_args.size(); ++i) {
        closure[method->formal_params[i]] = actual_args[i];
    }
    return method->body->Execute(closure);
}

Class::Class(std::string name, std::vector<Method> methods, const Class* parent)
    : name(std::move(name)), parent(parent) {
    for (auto& method : methods) {
        this->methods[method.name] = std::move(method);
    }
}

const Method* Class::GetMethod(const std::string& name) const {
    if (methods.count(name)) {
        return &methods.at(name);
    }
    if (parent) {
        return parent->GetMethod(name);
    }
    return nullptr;
}

void Class::Print(ostream& os) {
    os << GetName();
}

const std::string& Class::GetName() const {
    return name;
}

void Bool::Print(std::ostream& os) {
    os << (GetValue() ? "True" : "False");
}

} /* namespace Runtime */
