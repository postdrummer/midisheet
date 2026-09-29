#include "FormulaEngine.h"
#include <cctype>
#include <cmath>
#include <sstream>

namespace arp {

FormulaEngine::FormulaEngine() {
    // Initialize default variables
    variables_["STEP"] = 0;
    variables_["NOTE"] = 60;
    variables_["VELOCITY"] = 100;
    variables_["LENGTH"] = 100;
    variables_["CHANNEL"] = 1;
    variables_["PREV"] = 0;
    variables_["RANDOM"] = 0.5;
    variables_["TRUE"] = 1;
    variables_["FALSE"] = 0;
}

void FormulaEngine::setCell(const juce::String& cell, double value) {
    cells_[cell.toUpperCase()] = value;
}

double FormulaEngine::getCell(const juce::String& cell) const {
    auto it = cells_.find(cell.toUpperCase());
    if (it != cells_.end()) return it->second;
    return 0.0;
}

void FormulaEngine::setVariable(const juce::String& name, double value) {
    variables_[name.toUpperCase()] = value;
}

void FormulaEngine::clear() {
    cells_.clear();
}

bool FormulaEngine::isCellReference(const juce::String& str) {
    if (str.isEmpty()) return false;
    int i = 0;
    // Letters
    while (i < str.length() && std::isalpha(str[i])) i++;
    if (i == 0) return false;
    // Digits
    int digitStart = i;
    while (i < str.length() && std::isdigit(str[i])) i++;
    if (i == digitStart) return false;
    return i == str.length();
}

bool FormulaEngine::cellToRowCol(const juce::String& cell, int& row, int& col) {
    if (!isCellReference(cell)) return false;
    int i = 0;
    col = 0;
    while (i < cell.length() && std::isalpha(cell[i])) {
        col = col * 26 + (std::toupper(cell[i]) - 'A' + 1);
        i++;
    }
    col--; // 0-indexed
    row = cell.substring(i).getIntValue() - 1;
    return true;
}

juce::String FormulaEngine::rowColToCell(int row, int col) {
    juce::String result;
    col++; // 1-indexed
    while (col > 0) {
        int rem = (col - 1) % 26;
        result = juce::String::charToString(static_cast<juce::juce_wchar>('A' + rem)) + result;
        col = (col - 1) / 26;
    }
    return result + juce::String(row + 1);
}

double FormulaEngine::evaluate(const juce::String& formula) {
    lastError.clear();
    if (formula.isEmpty()) return 0.0;

    juce::String f = formula.trim();
    if (f.startsWith("=")) f = f.substring(1);

    auto tokens = tokenize(f);
    if (tokens.empty()) {
        lastError = "Empty formula";
        return 0.0;
    }

    size_t pos = 0;
    double result = parseExpression(tokens, pos);

    if (pos < tokens.size() && tokens[pos].type != TokenType::End) {
        lastError = "Unexpected token: " + tokens[pos].text;
        return 0.0;
    }

    return result;
}

std::vector<FormulaEngine::Token> FormulaEngine::tokenize(const juce::String& formula) {
    std::vector<Token> tokens;
    int i = 0;
    const int len = formula.length();

    while (i < len) {
        juce::juce_wchar c = formula[i];

        // Skip whitespace
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }

        // Number
        if (std::isdigit(c) || (c == '.' && i + 1 < len && std::isdigit(formula[i + 1]))) {
            int start = i;
            while (i < len && (std::isdigit(formula[i]) || formula[i] == '.')) i++;
            // Scientific notation
            if (i < len && (formula[i] == 'e' || formula[i] == 'E')) {
                i++;
                if (i < len && (formula[i] == '+' || formula[i] == '-')) i++;
                while (i < len && std::isdigit(formula[i])) i++;
            }
            Token t;
            t.type = TokenType::Number;
            t.text = formula.substring(start, i - start);
            t.value = t.text.getDoubleValue();
            tokens.push_back(t);
            continue;
        }

        // String literal
        if (c == '"') {
            i++;
            int start = i;
            while (i < len && formula[i] != '"') i++;
            Token t;
            t.type = TokenType::String;
            t.text = formula.substring(start, i - start);
            tokens.push_back(t);
            if (i < len) i++; // skip closing quote
            continue;
        }

        // Identifier (cell ref, variable, function, boolean)
        if (std::isalpha(c) || c == '_') {
            int start = i;
            while (i < len && (std::isalnum(formula[i]) || formula[i] == '_')) i++;
            juce::String ident = formula.substring(start, i - start);
            juce::String upper = ident.toUpperCase();

            Token t;
            t.text = ident;

            if (upper == "TRUE" || upper == "FALSE") {
                t.type = TokenType::Boolean;
                t.value = (upper == "TRUE") ? 1.0 : 0.0;
            } else if (isCellReference(ident)) {
                t.type = TokenType::CellRef;
            } else if (variables_.find(upper) != variables_.end()) {
                t.type = TokenType::Variable;
            } else {
                // Check if it's a function (followed by '(')
                int j = i;
                while (j < len && (formula[j] == ' ' || formula[j] == '\t')) j++;
                if (j < len && formula[j] == '(') {
                    t.type = TokenType::Function;
                } else {
                    t.type = TokenType::Variable;
                }
            }
            tokens.push_back(t);
            continue;
        }

        // Operators and comparisons
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '^') {
            Token t;
            t.type = TokenType::Operator;
            t.text = juce::String::charToString(c);
            tokens.push_back(t);
            i++;
            continue;
        }

        // Comparison operators
        if (c == '=' || c == '<' || c == '>') {
            Token t;
            t.type = TokenType::Comparison;
            if (c == '<' && i + 1 < len && formula[i + 1] == '>') {
                t.text = "<>";
                i += 2;
            } else if (c == '<' && i + 1 < len && formula[i + 1] == '=') {
                t.text = "<=";
                i += 2;
            } else if (c == '>' && i + 1 < len && formula[i + 1] == '=') {
                t.text = ">=";
                i += 2;
            } else {
                t.text = juce::String::charToString(c);
                i++;
            }
            tokens.push_back(t);
            continue;
        }

        // Parentheses
        if (c == '(') {
            Token t;
            t.type = TokenType::LParen;
            t.text = "(";
            tokens.push_back(t);
            i++;
            continue;
        }
        if (c == ')') {
            Token t;
            t.type = TokenType::RParen;
            t.text = ")";
            tokens.push_back(t);
            i++;
            continue;
        }

        // Comma
        if (c == ',') {
            Token t;
            t.type = TokenType::Comma;
            t.text = ",";
            tokens.push_back(t);
            i++;
            continue;
        }

        // Unknown character
        lastError = "Unknown character: " + juce::String::charToString(c);
        return {};
    }

    Token endToken;
    endToken.type = TokenType::End;
    endToken.text = "";
    tokens.push_back(endToken);

    return tokens;
}

double FormulaEngine::parseExpression(const std::vector<Token>& tokens, size_t& pos) {
    return parseComparison(tokens, pos);
}

double FormulaEngine::parseComparison(const std::vector<Token>& tokens, size_t& pos) {
    double left = parseAddSub(tokens, pos);

    while (pos < tokens.size() && tokens[pos].type == TokenType::Comparison) {
        juce::String op = tokens[pos].text;
        pos++;
        double right = parseAddSub(tokens, pos);

        if (op == "=") left = (left == right) ? 1.0 : 0.0;
        else if (op == "<>") left = (left != right) ? 1.0 : 0.0;
        else if (op == "<") left = (left < right) ? 1.0 : 0.0;
        else if (op == ">") left = (left > right) ? 1.0 : 0.0;
        else if (op == "<=") left = (left <= right) ? 1.0 : 0.0;
        else if (op == ">=") left = (left >= right) ? 1.0 : 0.0;
    }

    return left;
}

double FormulaEngine::parseAddSub(const std::vector<Token>& tokens, size_t& pos) {
    double left = parseMulDiv(tokens, pos);

    while (pos < tokens.size() && tokens[pos].type == TokenType::Operator &&
           (tokens[pos].text == "+" || tokens[pos].text == "-")) {
        juce::String op = tokens[pos].text;
        pos++;
        double right = parseMulDiv(tokens, pos);
        if (op == "+") left += right;
        else left -= right;
    }

    return left;
}

double FormulaEngine::parseMulDiv(const std::vector<Token>& tokens, size_t& pos) {
    double left = parsePower(tokens, pos);

    while (pos < tokens.size() && tokens[pos].type == TokenType::Operator &&
           (tokens[pos].text == "*" || tokens[pos].text == "/" || tokens[pos].text == "%")) {
        juce::String op = tokens[pos].text;
        pos++;
        double right = parsePower(tokens, pos);
        if (op == "*") left *= right;
        else if (op == "/") left = (right != 0) ? left / right : 0;
        else if (op == "%") left = (right != 0) ? std::fmod(left, right) : 0;
    }

    return left;
}

double FormulaEngine::parsePower(const std::vector<Token>& tokens, size_t& pos) {
    double base = parseUnary(tokens, pos);

    if (pos < tokens.size() && tokens[pos].type == TokenType::Operator && tokens[pos].text == "^") {
        pos++;
        double exp = parsePower(tokens, pos); // right-associative
        base = std::pow(base, exp);
    }

    return base;
}

double FormulaEngine::parseUnary(const std::vector<Token>& tokens, size_t& pos) {
    if (pos < tokens.size() && tokens[pos].type == TokenType::Operator && tokens[pos].text == "-") {
        pos++;
        return -parseUnary(tokens, pos);
    }
    if (pos < tokens.size() && tokens[pos].type == TokenType::Operator && tokens[pos].text == "+") {
        pos++;
        return parseUnary(tokens, pos);
    }
    return parsePrimary(tokens, pos);
}

double FormulaEngine::parsePrimary(const std::vector<Token>& tokens, size_t& pos) {
    if (pos >= tokens.size()) {
        lastError = "Unexpected end of formula";
        return 0;
    }

    const Token& t = tokens[pos];

    if (t.type == TokenType::Number) {
        pos++;
        return t.value;
    }

    if (t.type == TokenType::Boolean) {
        pos++;
        return t.value;
    }

    if (t.type == TokenType::String) {
        pos++;
        return 0.0; // Strings evaluate to 0 in numeric context
    }

    if (t.type == TokenType::CellRef) {
        pos++;
        return getCell(t.text);
    }

    if (t.type == TokenType::Variable) {
        pos++;
        auto it = variables_.find(t.text.toUpperCase());
        if (it != variables_.end()) return it->second;
        return 0;
    }

    if (t.type == TokenType::Function) {
        juce::String name = t.text;
        pos++;
        return parseFunction(tokens, pos, name);
    }

    if (t.type == TokenType::LParen) {
        pos++;
        double result = parseExpression(tokens, pos);
        if (pos < tokens.size() && tokens[pos].type == TokenType::RParen) {
            pos++;
        } else {
            lastError = "Missing closing parenthesis";
        }
        return result;
    }

    lastError = "Unexpected token: " + t.text;
    pos++;
    return 0;
}

double FormulaEngine::parseFunction(const std::vector<Token>& tokens, size_t& pos, const juce::String& name) {
    // Expect opening paren
    if (pos >= tokens.size() || tokens[pos].type != TokenType::LParen) {
        lastError = "Expected '(' after function name: " + name;
        return 0;
    }
    pos++; // skip '('

    juce::String upper = name.toUpperCase();

    // Functions with special parsing needs
    if (upper == "IF") {
        double condition = parseExpression(tokens, pos);
        if (pos < tokens.size() && tokens[pos].type == TokenType::Comma) pos++;
        double trueVal = parseExpression(tokens, pos);
        if (pos < tokens.size() && tokens[pos].type == TokenType::Comma) pos++;
        double falseVal = parseExpression(tokens, pos);
        if (pos < tokens.size() && tokens[pos].type == TokenType::RParen) pos++;
        return (condition != 0) ? trueVal : falseVal;
    }

    // Collect arguments for other functions
    std::vector<double> args;
    if (pos < tokens.size() && tokens[pos].type != TokenType::RParen) {
        args.push_back(parseExpression(tokens, pos));
        while (pos < tokens.size() && tokens[pos].type == TokenType::Comma) {
            pos++;
            args.push_back(parseExpression(tokens, pos));
        }
    }

    if (pos < tokens.size() && tokens[pos].type == TokenType::RParen) {
        pos++;
    } else {
        lastError = "Missing closing parenthesis in function: " + name;
        return 0;
    }

    // Evaluate function
    if (upper == "MOD" && args.size() == 2) return fnMod(args[0], args[1]);
    if (upper == "SUM") return fnSum(args);
    if (upper == "AVG" || upper == "AVERAGE") return fnAvg(args);
    if (upper == "MIN") return fnMin(args);
    if (upper == "MAX") return fnMax(args);
    if (upper == "ABS" && args.size() == 1) return fnAbs(args[0]);
    if (upper == "ROUND" && args.size() >= 1) return fnRound(args[0], args.size() > 1 ? args[1] : 0);
    if (upper == "FLOOR" && args.size() == 1) return fnFloor(args[0]);
    if (upper == "CEIL" && args.size() == 1) return fnCeil(args[0]);
    if (upper == "RANDOM" && args.size() == 2) return fnRandom(args[0], args[1]);
    if (upper == "RANDOM" && args.size() == 0) return fnRandom(0, 1);

    lastError = "Unknown function or wrong argument count: " + name;
    return 0;
}

double FormulaEngine::fnMod(double a, double b) {
    if (b == 0) return 0;
    return std::fmod(a, b);
}

double FormulaEngine::fnIf(double condition, double trueVal, double falseVal) {
    return (condition != 0) ? trueVal : falseVal;
}

double FormulaEngine::fnSum(const std::vector<double>& args) {
    double sum = 0;
    for (double v : args) sum += v;
    return sum;
}

double FormulaEngine::fnAvg(const std::vector<double>& args) {
    if (args.empty()) return 0;
    return fnSum(args) / static_cast<double>(args.size());
}

double FormulaEngine::fnMin(const std::vector<double>& args) {
    if (args.empty()) return 0;
    double m = args[0];
    for (double v : args) if (v < m) m = v;
    return m;
}

double FormulaEngine::fnMax(const std::vector<double>& args) {
    if (args.empty()) return 0;
    double m = args[0];
    for (double v : args) if (v > m) m = v;
    return m;
}

double FormulaEngine::fnAbs(double a) {
    return std::abs(a);
}

double FormulaEngine::fnRound(double a, double decimals) {
    double factor = std::pow(10, decimals);
    return std::round(a * factor) / factor;
}

double FormulaEngine::fnFloor(double a) {
    return std::floor(a);
}

double FormulaEngine::fnCeil(double a) {
    return std::ceil(a);
}

double FormulaEngine::fnRandom(double min, double max) {
    return min + (max - min) * (static_cast<double>(rand()) / RAND_MAX);
}

} // namespace arp
