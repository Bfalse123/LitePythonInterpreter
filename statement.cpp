#include "statement.h"

#include <iostream>
#include <numeric>
#include <sstream>

#include "object.h"

using namespace std;

namespace Ast {

using Runtime::Closure;

ObjectHolder Assignment::Execute(Closure& closure) {
    return closure[var_name] = right_value->Execute(closure);
}

Assignment::Assignment(std::string var, std::unique_ptr<Statement> rv)
    : var_name(std::move(var)), right_value(std::move(rv)) {
}

VariableValue::VariableValue(std::string var_name) : dotted_ids({var_name}) {
}

VariableValue::VariableValue(std::vector<std::string> dotted_ids)
    : dotted_ids(std::move(dotted_ids)) {
}

ObjectHolder VariableValue::Execute(Closure& closure) {
    const auto& var_name = dotted_ids.front();
    if (!closure.count(var_name)) {
        throw std::runtime_error("unknown variable " + var_name);
    }

    return std::accumulate(
        next(begin(dotted_ids)), end(dotted_ids),
        closure.at(var_name),
        [](ObjectHolder parent, const string& name) { return parent.TryAs<Runtime::ClassInstance>()->Fields().at(name); });
}

unique_ptr<Print> Print::Variable(std::string var) {
    return std::make_unique<Print>(std::make_unique<VariableValue>(std::move(var)));
}

Print::Print(unique_ptr<Statement> argument) {
    args.push_back(std::move(argument));
}

Print::Print(vector<unique_ptr<Statement>> args) : args(std::move(args)) {
}

ObjectHolder Print::Execute(Closure& closure) {
    bool first = true;
    for (auto& arg : args) {
        if (!first) *output << " ";
        first = false;
        if (ObjectHolder obj = arg->Execute(closure)) {
            obj->Print(*output);
        } else {
            *output << "None";
        }
    }
    *output << "\n";
    return ObjectHolder::None();
}

ostream* Print::output = &cout;

void Print::SetOutputStream(ostream& output_stream) {
    output = &output_stream;
}

MethodCall::MethodCall(
    std::unique_ptr<Statement> object, std::string method, std::vector<std::unique_ptr<Statement>> args)
    : object(std::move(object)), method(std::move(method)), args(std::move(args)) {
}

ObjectHolder MethodCall::Execute(Closure& closure) {
    auto* cls_instance = object->Execute(closure).TryAs<Runtime::ClassInstance>();
    if (!cls_instance) throw runtime_error("not class instance");
    std::vector<ObjectHolder> actual_args(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        actual_args[i] = args[i]->Execute(closure);
    }
    return cls_instance->Call(method, actual_args);
}

ObjectHolder Stringify::Execute(Closure& closure) {
    std::ostringstream str;
    argument->Execute(closure)->Print(str);
    return ObjectHolder::Own(Runtime::String(str.str()));
}

ObjectHolder Add::Execute(Closure& closure) {
    using namespace Runtime;
    ObjectHolder obj1 = lhs->Execute(closure);
    ObjectHolder obj2 = rhs->Execute(closure);
    {
        String* v1 = obj1.TryAs<String>();
        String* v2 = obj2.TryAs<String>();
        if (v1 && v2) {
            return ObjectHolder::Own(Runtime::String(v1->GetValue() + v2->GetValue()));
        }
    }
    {
        Number* v1 = obj1.TryAs<Number>();
        Number* v2 = obj2.TryAs<Number>();
        if (v1 && v2) {
            return ObjectHolder::Own(Runtime::Number(v1->GetValue() + v2->GetValue()));
        }
    }
    {
        ClassInstance* v1 = obj1.TryAs<ClassInstance>();
        if (v1 && v1->HasMethod("__add__", 1)) {
            return v1->Call("__add__", {obj2});
        }
    }
    throw std::runtime_error("wrong types");
}

ObjectHolder Sub::Execute(Closure& closure) {
    using namespace Runtime;
    ObjectHolder obj1 = lhs->Execute(closure);
    ObjectHolder obj2 = rhs->Execute(closure);
    {
        Number* v1 = obj1.TryAs<Number>();
        Number* v2 = obj2.TryAs<Number>();
        if (v1 && v2) {
            return ObjectHolder::Own(Runtime::Number(v1->GetValue() - v2->GetValue()));
        }
    }
    ClassInstance* v1 = obj1.TryAs<ClassInstance>();
    if (v1 && v1->HasMethod("__sub__", 1)) {
        return v1->Call("__sub__", {obj2});
    }
    throw std::runtime_error("rwong types");
}

ObjectHolder Mult::Execute(Runtime::Closure& closure) {
    using namespace Runtime;
    ObjectHolder obj1 = lhs->Execute(closure);
    ObjectHolder obj2 = rhs->Execute(closure);
    {
        Number* v1 = obj1.TryAs<Number>();
        Number* v2 = obj2.TryAs<Number>();
        if (v1 && v2) {
            return ObjectHolder::Own(Runtime::Number(v1->GetValue() * v2->GetValue()));
        }
    }
    ClassInstance* v1 = obj1.TryAs<ClassInstance>();
    if (v1 && v1->HasMethod("__mult__", 1)) {
        return v1->Call("__mult__", {obj2});
    }
    throw std::runtime_error("rwong types");
}

ObjectHolder Div::Execute(Runtime::Closure& closure) {
    using namespace Runtime;
    ObjectHolder obj1 = lhs->Execute(closure);
    ObjectHolder obj2 = rhs->Execute(closure);
    {
        Number* v1 = obj1.TryAs<Number>();
        Number* v2 = obj2.TryAs<Number>();
        if (v1 && v2) {
            if (v2->GetValue() == 0) throw std::invalid_argument("division by zero");
            return ObjectHolder::Own(Runtime::Number(v1->GetValue() / v2->GetValue()));
        }
    }
    ClassInstance* v1 = obj1.TryAs<ClassInstance>();
    if (v1 && v1->HasMethod("__div__", 1)) {
        return v1->Call("__div__", {obj2});
    }
    throw std::runtime_error("rwong types");
}

ObjectHolder Compound::Execute(Closure& closure) {
    for (auto& statement : statements) {
        auto ret = statement->Execute(closure);

        bool check_for_return = false || dynamic_cast<Return*>(statement.get()) || dynamic_cast<IfElse*>(statement.get()) || dynamic_cast<Compound*>(statement.get());

        if (check_for_return && ret.Get()) {
            return ret;
        }
    }
    return ObjectHolder::None();
}

ObjectHolder Return::Execute(Closure& closure) {
    return statement->Execute(closure);
}

ClassDefinition::ClassDefinition(ObjectHolder class_)
    : cls(std::move(class_)), class_name(cls.TryAs<Runtime::Class>()->GetName()) {
}

ObjectHolder ClassDefinition::Execute(Runtime::Closure& closure) {
    if (closure.count(class_name)) {
        throw std::runtime_error("multiple definitions");
    }
    closure[class_name] = cls;
    return closure[class_name];
}

FieldAssignment::FieldAssignment(
    VariableValue object, std::string field_name, std::unique_ptr<Statement> rv)
    : object(std::move(object)), field_name(std::move(field_name)), right_value(std::move(rv)) {
}

ObjectHolder FieldAssignment::Execute(Runtime::Closure& closure) {
    ObjectHolder obj = object.Execute(closure);
    ObjectHolder right_obj = right_value->Execute(closure);
    obj.TryAs<Runtime::ClassInstance>()->Fields()[field_name] = right_obj;
    return ObjectHolder::Share(*right_obj);
}

IfElse::IfElse(
    std::unique_ptr<Statement> condition,
    std::unique_ptr<Statement> if_body,
    std::unique_ptr<Statement> else_body)
    : condition(std::move(condition)),
      if_body(std::move(if_body)),
      else_body(std::move(else_body)) {
}

ObjectHolder IfElse::Execute(Runtime::Closure& closure) {
    if (Runtime::IsTrue(condition->Execute(closure))) {
        return if_body->Execute(closure);
    } else if (else_body) {
        return else_body->Execute(closure);
    }
    return ObjectHolder::None();
}

ObjectHolder Or::Execute(Runtime::Closure& closure) {
    return ObjectHolder::Own(Runtime::Bool(
        Runtime::IsTrue(lhs->Execute(closure)) || Runtime::IsTrue(rhs->Execute(closure))));
}

ObjectHolder And::Execute(Runtime::Closure& closure) {
    return ObjectHolder::Own(Runtime::Bool(
        Runtime::IsTrue(lhs->Execute(closure)) && Runtime::IsTrue(rhs->Execute(closure))));
}

ObjectHolder Not::Execute(Runtime::Closure& closure) {
    return ObjectHolder::Own(Runtime::Bool(!Runtime::IsTrue(argument->Execute(closure))));
}

Comparison::Comparison(
    Comparator cmp, unique_ptr<Statement> lhs, unique_ptr<Statement> rhs)
    : comparator(std::move(cmp)), left(std::move(lhs)), right(std::move(rhs)) {
}

ObjectHolder Comparison::Execute(Runtime::Closure& closure) {
    return ObjectHolder::Own(
        Runtime::Bool(comparator(left->Execute(closure), right->Execute(closure))));
}

NewInstance::NewInstance(
    const Runtime::Class& class_, std::vector<std::unique_ptr<Statement>> args)
    : class_(class_), args(std::move(args)) {
}

NewInstance::NewInstance(const Runtime::Class& class_) : NewInstance(class_, {}) {
}

ObjectHolder NewInstance::Execute(Runtime::Closure& closure) {
    Runtime::ClassInstance instance(class_);
    std::vector<ObjectHolder> actual_args(args.size());
    for (size_t i = 0; i < args.size(); ++i) {
        actual_args[i] = args[i]->Execute(closure);
    }
    if (instance.HasMethod("__init__", actual_args.size())) {
        instance.Call("__init__", actual_args);
    }
    return ObjectHolder::Own(std::move(instance));
}

} /* namespace Ast */
