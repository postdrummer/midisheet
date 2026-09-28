#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <map>
#include <vector>

namespace arp {

/**
 * Excel-like formula engine for the arpeggiator.
 * Supports:
 *  - Cell references: A1, B2, AA10, etc.
 *  - Arithmetic: +, -, *, /, %, ^ (power)
 *  - Functions: MOD, IF, SUM, AVG, MIN, MAX, ABS, ROUND, FLOOR, CEIL
 *  - Variables: STEP, NOTE, VELOCITY, LENGTH, CHANNEL, PREV, RANDOM
 *  - Comparisons: =, <>, <, >, <=, >=
 *  - String literals in double quotes
 *  - Boolean literals: TRUE, FALSE
 */
class FormulaEngine {
public:
    FormulaEngine();
    ~FormulaEngine() = default;

    // Set a cell value (e.g., "A1" -> 60)
    void setCell(const juce::String& cell, double value);

    // Get a cell value
    double getCell(const juce::String& cell) const;

    // Set a variable value (e.g., "STEP" -> 4)
    void setVariable(const juce::String& name, double value);

    // Evaluate a formula string (e.g., "=A1+12" or "=IF(MOD(STEP,4)=0,60,62)")
    double evaluate(const juce::String& formula);

    // Clear all cells
    void clear();

    // Get last error message
    juce::String getLastError() const { return lastError; }

    // Check if a string is a valid cell reference
    static bool isCellReference(const juce::String& str);

    // Convert cell reference to row/col (0-indexed)
    static bool cellToRowCol(const juce::String& cell, int& row, int& col);

    // Convert row/col to cell reference
    static juce::String rowColToCell(int row, int col);

private:
    std::map<juce::String, double> cells_;
    std::map<juce::String, double> variables_;
    juce::String lastError;

    // Token types for the parser
    enum class TokenType {
        Number,
        String,
        CellRef,
        Variable,
        Function,
        Operator,
        LParen,
        RParen,
        Comma,
        Comparison,
        Boolean,
        End
    };

    struct Token {
        TokenType type;
        juce::String text;
        double value = 0.0;
    };

    // Tokenizer
    std::vector<Token> tokenize(const juce::String& formula);

    // Recursive descent parser
    double parseExpression(const std::vector<Token>& tokens, size_t& pos);
    double parseComparison(const std::vector<Token>& tokens, size_t& pos);
    double parseAddSub(const std::vector<Token>& tokens, size_t& pos);
    double parseMulDiv(const std::vector<Token>& tokens, size_t& pos);
    double parsePower(const std::vector<Token>& tokens, size_t& pos);
    double parseUnary(const std::vector<Token>& tokens, size_t& pos);
    double parsePrimary(const std::vector<Token>& tokens, size_t& pos);
    double parseFunction(const std::vector<Token>& tokens, size_t& pos, const juce::String& name);

    // Built-in functions
    double fnMod(double a, double b);
    double fnIf(double condition, double trueVal, double falseVal);
    double fnSum(const std::vector<double>& args);
    double fnAvg(const std::vector<double>& args);
    double fnMin(const std::vector<double>& args);
    double fnMax(const std::vector<double>& args);
    double fnAbs(double a);
    double fnRound(double a, double decimals);
    double fnFloor(double a);
    double fnCeil(double a);
    double fnRandom(double min, double max);
};

} // namespace arp
