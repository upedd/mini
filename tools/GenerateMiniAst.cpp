#include <algorithm>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <vector>
#include <ranges>

void trim(std::string& str)
{
    str.erase(str.find_last_not_of(' ')+1);
    str.erase(0, str.find_first_not_of(' '));
}

std::string to_lower(const std::string_view str) {
    std::string copy;
    for (const auto c : str) {
        copy += std::tolower(c);
    }
    return copy;
}

void define_visitor(std::ofstream & out, std::string_view base_name, const std::vector<std::pair<std::string, std::string>>& types_data) {
    //out << "    template<typename T>\n";
    out << "    class Visitor {\n";
    out << "    public:\n";
    out << "        virtual ~Visitor() = default;\n";
    for (auto& [type_name, _] : types_data) {
        out << "        virtual std::any visit" << type_name << base_name << "(" << type_name << "* " << to_lower(base_name) << ") = 0;\n";
    }
    out << "    };\n";
}


void define_type(std::ofstream & out, std::string_view base_name, const std::string & class_name, std::string_view fields_list) {
    out << "class " << base_name << "::" << class_name << " : public " << base_name << " {\n";
    out << "public:\n";
    out << "    " << class_name << "(" << fields_list << ") : ";
    std::vector<std::string> fields;
    for (auto field : std::views::split(fields_list, std::string_view(", "))) {
        fields.emplace_back(std::string_view(field));
    }
    for (auto& field : fields) {
        auto name = field.substr(field.find(' ') + 1);
        auto type = field.substr(0, field.find(' '));
        out << name << "(std::move(" << name << "))";
        if (field != fields.back()) {
            out << ", ";
        }
    }

    out << " {}\n";
    for (auto& field: fields) {
        out << "    " << field << "; \n";
    }

    out << '\n';
    out << "    std::any accept(Visitor* visitor) override {\n";
    out << "        return visitor->visit" << class_name << base_name << "(this);" << '\n';
    out << "    }\n";
    out << "};\n";
}

void define_ast(std::string_view output_directory, std::string_view base_name, std::vector<std::string> types, std::string_view additional_includes = "") {
    auto path = std::filesystem::path(output_directory).append(base_name).concat(".h");
    std::cout << path << '\n';
    std::ofstream out(path);
    out << "#ifndef " << base_name << "_H\n";
    out << "#define " << base_name << "_H\n";
    out << additional_includes << '\n';
    //out << "#include \"../Token.h\"\n";
    out << "#include <any>\n";
    out << "#include <memory>\n";
    out << '\n';
    out << "class " << base_name << " {\n";
    out << "public:\n";
    std::vector<std::pair<std::string, std::string>> types_data;
    for (auto& type : types) {
        auto pos = type.find(":");
        std::string class_name = type.substr(0, pos);
        std::string fields = type.substr(pos + 1);
        trim(class_name); trim(fields);
        types_data.emplace_back(class_name, fields);
        out << "    class " << class_name << ";\n";
    }
    define_visitor(out, base_name, types_data);
    //out << "    template<typename T>\n";
    out << "    virtual std::any accept(Visitor* visitor) = 0;\n";
    out << "    virtual ~" << base_name << "() = default;\n";
    out << "};\n";
    for (auto& [class_name, fields] : types_data) {
        define_type(out, base_name, class_name, fields);
    }
    out << "#endif";
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: generate_mini_ast <output directory>\n";
        return 64;
    }
    std::string output_directory = argv[1];

    define_ast(output_directory, "Expr", {
        "Assign : Token name, std::shared_ptr<Expr> value",
        "Binary : std::shared_ptr<Expr> left, Token op, std::shared_ptr<Expr> right",
        "Call : std::shared_ptr<Expr> callee, Token paren, std::vector<std::shared_ptr<Expr>> arguments",
        "Grouping : std::shared_ptr<Expr> expression",
        "Literal : std::any value",
        "Logical : std::shared_ptr<Expr> left, Token op, std::shared_ptr<Expr> right",
        "Unary : Token op, std::shared_ptr<Expr> right",
        "Variable : Token name",
    }, "#include \"../Token.h\"");

    define_ast(output_directory, "Stmt", {
        "Block : std::vector<std::unique_ptr<Stmt>> statements",
        "Expression : std::unique_ptr<Expr> expression",
        "Function : Token name, std::vector<Token> params, std::vector<std::unique_ptr<Stmt>> body",
        "If : std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> then_branch, std::unique_ptr<Stmt> else_branch",
        "Print : std::unique_ptr<Expr> expression",
        "Var : Token name, std::unique_ptr<Expr> initializer",
        "While : std::unique_ptr<Expr> condition, std::unique_ptr<Stmt> body"
    }, "#include \"../Token.h\"\n#include \"Expr.h\"");
}


