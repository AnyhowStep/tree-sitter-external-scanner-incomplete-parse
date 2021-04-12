#include <string>
#include <regex>
#include "./character-code.cc"
#include "./buffered-lexer.cc"
#include "./token-kind.cc"

namespace {
    bool tryScanQuotedString (TmpLexer &lexer) {
        TmpLexer tmp(lexer);
        auto quote = tmp.advance();

        //std::string result;

        while (!tmp.isEof(0)) {
            auto ch = tmp.peek(0);
            if (ch == quote) {
                if (tmp.peek(1) == quote) {
                    //Strings can contain the quote char by using the quote char twice
                    tmp.advance();
                    tmp.advance();
                    //result += quote;
                } else {
                    tmp.advance();
                    tmp.markEnd();
                    lexer.index = tmp.index;
                    return true;
                    //return result;
                }
            } else if (ch == CharacterCodes::backslash) {
                tmp.advance();
                //https://dev.mysql.com/doc/refman/5.7/en/string-literals.html
                auto escapedCh = tmp.advance();
                switch (escapedCh)
                {
                    case CharacterCodes::_0:
                        //result += '\0';
                        break;
                    case CharacterCodes::b:
                        //result += '\b';
                        break;
                    case CharacterCodes::t:
                        //result += '\t';
                        break;
                    case CharacterCodes::n:
                        //result += '\n';
                        break;
                    case CharacterCodes::r:
                        //result += '\r';
                        break;
                    case CharacterCodes::singleQuote:
                        //result += '\'';
                        break;
                    case CharacterCodes::doubleQuote:
                        //result += '\"';
                        break;
                    case CharacterCodes::Z:
                        //result += '\x1a';
                        break;

                    default:
                        //result += escapedCh;
                        break;
                }
            } else {
                tmp.advance();
                //result += tmp.advance();
            }
        }

        return false;
    }

    bool lexerResult (TSLexer *lexer, const bool *valid_symbols, TokenType tokenType) {
        //std::cout << "lexerResult: " << tokenType << std::endl;
        if (valid_symbols[tokenType]) {
            lexer->result_symbol = tokenType;
            return true;
        } else {
            return false;
        }
    }

    bool lexerForcedResult (TSLexer *lexer, TokenType tokenType) {
        lexer->result_symbol = tokenType;
        return true;
    }

    bool lexerEofResult (TSLexer *lexer) {
        lexer->result_symbol = TokenType::EndOfFile;
        return true;
    }

    std::regex regex0xHexLiteral(
        "^0x[0-9a-fA-F]+$",
        std::regex_constants::ECMAScript
    );

    bool is0xHexLiteral (std::string const &str) {
        return std::regex_match(str, regex0xHexLiteral);
    }

    std::regex regex0bBitLiteral(
        "^0b[01]+$",
        std::regex_constants::ECMAScript
    );

    bool is0bBitLiteral (std::string const &str) {
        return std::regex_match(str, regex0bBitLiteral);
    }

    bool tryScanTillEndOfMultiLineComment (TmpLexer &lexer) {
        TmpLexer tmp(lexer);
        while (!tmp.isEof(0)) {
            if (
                tmp.peek(0) == CharacterCodes::asterisk &&
                tmp.peek(1) == CharacterCodes::slash
            ) {
                tmp.advance();
                tmp.advance();
                tmp.markEnd();
                lexer.index = tmp.index;
                return true;
            }

            tmp.advance();
        }

        return false;
    }

    bool tryScanString(TmpLexer &lexer, std::string const &str, bool markEnd = true) {
        TmpLexer tmp(lexer);
        //Try to match all characters in the given 'str'
        for (size_t i=0; i<str.size(); ++i) {
            auto c = str[i];
            if (tmp.peek(i) != c) {
                return false;
            }
        }

        for (size_t i=0; i<str.size(); ++i) {
            //Consume the character in 'c'
            tmp.advance();
        }

        if (markEnd) {
            tmp.markEnd();
            lexer.index = tmp.index;
        }
        return true;
    }


    bool tryScanStringCaseInsensitive(TmpLexer &lexer, std::string const &str) {
        TmpLexer tmp(lexer);
        //Try to match all characters in the given 'str'
        for (size_t i=0; i<str.size(); ++i) {
            auto c = str[i];
            if (toupper(tmp.peek(i)) != toupper(c)) {
                return false;
            }
        }

        for (size_t i=0; i<str.size(); ++i) {
            //Consume the character in 'c'
            tmp.advance();
        }

        tmp.markEnd();
        lexer.index = tmp.index;
        return true;
    }

    /**
     * Unquoted identifiers can be interrupted by custom delimiter.
     * If returned length is zero, there is no unquoted identifier.
     */
    std::string tryScanUnquotedIdentifier (TmpLexer &lexer, std::string const &customDelimiter) {
        TmpLexer tmp(lexer);
        std::string result;

        while (!tmp.isEof(0)) {
            if (customDelimiter.size() > 0) {
                if (result.size() > 0) {
                    tmp.markEnd();
                    lexer.index = tmp.index;
                }
                if (tryScanString(tmp, customDelimiter, /* markEnd */false)) {
                    //Interrupted by custom delimiter
                    return result;
                }
            }

            auto ch = tmp.peek(0);
            if (isUnquotedIdentifierCharacter(ch)) {
                result += tmp.advance();
            } else {
                if (result.size() > 0) {
                    tmp.markEnd();
                    lexer.index = tmp.index;
                }
                return result;
            }
        }

        if (result.size() > 0) {
            tmp.markEnd();
            lexer.index = tmp.index;
        }
        return result;
    }

    bool tryScanQuotedIdentifier (TmpLexer &lexer) {
        TmpLexer tmp(lexer);
        auto quote = tmp.advance();

        while (!tmp.isEof(0)) {
            auto ch = tmp.peek(0);
            if (ch == quote) {
                if (tmp.peek(1) == quote) {
                    //Identifiers can contain the quote char by using the quote char twice
                    tmp.advance();
                    tmp.advance();
                } else {
                    tmp.advance();
                    tmp.markEnd();
                    lexer.index = tmp.index;
                    return true;
                }
            } else {
                tmp.advance();
            }
        }

        return false;
    }

    std::regex regexDigitE(
        "^\\d+[eE]$",
        std::regex_constants::ECMAScript
    );

    bool isDigitE (std::string const &str) {
        return std::regex_match(str, regexDigitE);
    }

    std::regex regexDigitEDigit(
        "^\\d+[eE]\\d+",
        std::regex_constants::ECMAScript
    );

    bool isDigitEDigit (std::string const &str) {
        return std::regex_match(str, regexDigitEDigit);
    }

    bool tryScanDigitEDigit (TmpLexer &lexer) {
        TmpLexer tmp(lexer);
        //Digit
        if (!isDigit(tmp.peek(0))) {
            return false;
        }
        tmp.advance();

        while (isDigit(tmp.peek(0))) {
            tmp.advance();
        }

        //E
        auto chE = tmp.peek(0);
        if (chE != CharacterCodes::e && chE != CharacterCodes::E) {
            return false;
        }
        tmp.advance();

        //Digit
        if (!isDigit(tmp.peek(0))) {
            return false;
        }
        tmp.advance();

        while (isDigit(tmp.peek(0))) {
            tmp.advance();
        }

        tmp.markEnd();
        lexer.index = tmp.index;
        return true;
    }

    bool tryScanNumberFractionalPart (TmpLexer &lexer) {
        TmpLexer tmp(lexer);
        if (tmp.peek(0) != CharacterCodes::dot) {
            return false;
        }
        tmp.advance();

        while (isDigit(tmp.peek(0))) {
            tmp.advance();
        }

        tmp.markEnd();
        lexer.index = tmp.index;
        return true;
    }

    bool tryScanNumberExponent2 (TmpLexer &lexer) {
        TmpLexer tmp(lexer);
        /**
         * Optional +/- prefix for exponent
         */
        auto chPrefix = tmp.peek(0);
        if (chPrefix == CharacterCodes::plus || chPrefix == CharacterCodes::minus) {
            tmp.advance();
        }

        auto chFirstDigit = tmp.peek(0);
        if (!isDigit(chFirstDigit)) {
            return false;
        }
        tmp.advance();

        while (isDigit(tmp.peek(0))) {
            tmp.advance();
        }

        tmp.markEnd();
        lexer.index = tmp.index;
        return true;
    }

    bool tryScanNumberExponent (TmpLexer &lexer) {
        TmpLexer tmp(lexer);
        auto chE = tmp.peek(0);
        if (chE != CharacterCodes::e && chE != CharacterCodes::E) {
            return false;
        }
        tmp.advance();

        return tryScanNumberExponent2(tmp);
    }

    int tryGetKeywordTokenType (std::string const &str) {
        auto it = keyword2TokenType.find(str);
        if (it == keyword2TokenType.end()) {
            return -1;
        } else {
            return it->second;
        }
    }

    int tryScanIdentifierOrKeywordOrNumberLiteral (TmpLexer &lexer, const bool *valid_symbols, std::string const &customDelimiter) {
        TmpLexer tmp(lexer);
        if (!isUnquotedIdentifierCharacter(tmp.peek(0))) {
            return -1;
        }

        if (tryScanDigitEDigit(tmp)) {
            return TokenType::RealLiteral;
        }

        /**
         * Examples:
         * + `123`
         * + `0`
         * + `0e` (May be followed by +123; e.g. 0e+123)
         * + `0E`
         * + `0e0`
         * + `0E0`
         */
        auto str = tryScanUnquotedIdentifier(tmp, customDelimiter);
        //std::cout << "tryScanIdentifierOrKeywordOrNumberLiteral: " << str << std::endl;

        if (str.size() == 0) {
            //No unquoted identifier.
            //We already checked peek(0) is unquoted identifier character.
            //So, we were interrupted by custom delimiter.
            if (tryScanString(tmp, customDelimiter)) {
                return TokenType::CustomDelimiter;
            } else {
                //I don't know what this is, this should never happen.
                //This only happens if `customDelimiter.size() == 0`.
                //But, if this is the case, then we have a bug in our code.
                return -1;
            }
        }

        if (isAllDigit(str)) {
            /**
             * + 123
             * + 123.
             * + 123.e10
             * + 123.e-10
             * + 123.123
             * + 123.123e123
             * + 123.123e-123
             */
            if (tryScanNumberFractionalPart(tmp)) {
                if (tryScanNumberExponent(tmp)) {
                    return TokenType::RealLiteral;
                } else {
                    return TokenType::DecimalLiteral;
                }
            } else {
                /**
                 * This integer literal might be too large if positive.
                 * This integer literal might be too small, if negative.
                 *
                 * If too large or too small, it's actually a decimal literal.
                 */
                return TokenType::IntegerLiteral;
            }
        }

        if (isDigitE(str)) {
            if (tryScanNumberExponent2(tmp)) {
                return TokenType::RealLiteral;
            } else {
                return TokenType::Identifier;
            }
        }

        if (isDigitEDigit(str)) {
            return TokenType::RealLiteral;
        }

        auto keywordTokenType = tryGetKeywordTokenType(str);
        if (keywordTokenType < 0) {
            return TokenType::Identifier;
        }

        if (
            valid_symbols[TokenType::Identifier] &&
            keywordTokenType > TokenType::START_OF_NON_RESERVED_KEYWORD &&
            keywordTokenType < TokenType::END_OF_NON_RESERVED_KEYWORD
        ) {
            return TokenType::Identifier;
        }

        return keywordTokenType;
    }
}
