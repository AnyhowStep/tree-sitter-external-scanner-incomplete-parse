#include <string>
#include <regex>
#include "./character-code.cc"
#include "./buffered-lexer.cc"
#include "./token-kind.cc"

namespace {
    bool tryScanQuotedString (BufferedLexer &bufferedLexer) {
        auto quote = bufferedLexer.advance();

        //std::string result;

        while (!bufferedLexer.isEof(0)) {
            auto ch = bufferedLexer.peek(0);
            if (ch == quote) {
                if (bufferedLexer.peek(1) == quote) {
                    //Strings can contain the quote char by using the quote char twice
                    bufferedLexer.advance();
                    bufferedLexer.advance();
                    //result += quote;
                } else {
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return true;
                    //return result;
                }
            } else if (ch == CharacterCodes::backslash) {
                bufferedLexer.advance();
                //https://dev.mysql.com/doc/refman/5.7/en/string-literals.html
                auto escapedCh = bufferedLexer.advance();
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
                bufferedLexer.advance();
                //result += bufferedLexer.advance();
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

    bool tryScanTillEndOfMultiLineComment (BufferedLexer &bufferedLexer) {
        while (!bufferedLexer.isEof(0)) {
            if (
                bufferedLexer.peek(0) == CharacterCodes::asterisk &&
                bufferedLexer.peek(1) == CharacterCodes::slash
            ) {
                bufferedLexer.advance();
                bufferedLexer.advance();
                bufferedLexer.markEnd();
                return true;
            }

            bufferedLexer.advance();
        }

        return false;
    }

    bool tryScanString(BufferedLexer &bufferedLexer, std::string const &str, bool markEnd = true) {
        //Try to match all characters in the given 'str'
        for (size_t i=0; i<str.size(); ++i) {
            auto c = str[i];
            if (bufferedLexer.peek(i) != c) {
                return false;
            }
        }

        for (size_t i=0; i<str.size(); ++i) {
            //Consume the character in 'c'
            bufferedLexer.advance();
        }

        if (markEnd) {
            bufferedLexer.markEnd();
        }
        return true;
    }


    bool tryScanStringCaseInsensitive(BufferedLexer &bufferedLexer, std::string const &str) {
        //Try to match all characters in the given 'str'
        for (size_t i=0; i<str.size(); ++i) {
            auto c = str[i];
            if (toupper(bufferedLexer.peek(i)) != toupper(c)) {
                return false;
            }
        }

        for (size_t i=0; i<str.size(); ++i) {
            //Consume the character in 'c'
            bufferedLexer.advance();
        }

        bufferedLexer.markEnd();
        return true;
    }

    /**
     * Unquoted identifiers can be interrupted by custom delimiter.
     * If returned length is zero, there is no unquoted identifier.
     */
    std::string tryScanUnquotedIdentifier (BufferedLexer &bufferedLexer, std::string const &customDelimiter) {
        std::string result;

        while (!bufferedLexer.isEof(0)) {
            if (customDelimiter.size() > 0) {
                if (result.size() > 0) {
                    bufferedLexer.markEnd();
                }
                if (tryScanString(bufferedLexer, customDelimiter, /* markEnd */false)) {
                    //Interrupted by custom delimiter
                    return result;
                }
            }

            auto ch = bufferedLexer.peek(0);
            if (isUnquotedIdentifierCharacter(ch)) {
                result += bufferedLexer.advance();
            } else {
                if (result.size() > 0) {
                    bufferedLexer.markEnd();
                }
                return result;
            }
        }

        if (result.size() > 0) {
            bufferedLexer.markEnd();
        }
        return result;
    }

    bool tryScanQuotedIdentifier (BufferedLexer &bufferedLexer) {
        auto quote = bufferedLexer.advance();

        while (!bufferedLexer.isEof(0)) {
            auto ch = bufferedLexer.peek(0);
            if (ch == quote) {
                if (bufferedLexer.peek(1) == quote) {
                    //Identifiers can contain the quote char by using the quote char twice
                    bufferedLexer.advance();
                    bufferedLexer.advance();
                } else {
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return true;
                }
            } else {
                bufferedLexer.advance();
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

    bool tryScanDigitEDigit (BufferedLexer &bufferedLexer) {
        //Digit
        if (!isDigit(bufferedLexer.peek(0))) {
            return false;
        }
        bufferedLexer.advance();

        while (isDigit(bufferedLexer.peek(0))) {
            bufferedLexer.advance();
        }

        //E
        auto chE = bufferedLexer.peek(0);
        if (chE != CharacterCodes::e && chE != CharacterCodes::E) {
            return false;
        }
        bufferedLexer.advance();

        //Digit
        if (!isDigit(bufferedLexer.peek(0))) {
            return false;
        }
        bufferedLexer.advance();

        while (isDigit(bufferedLexer.peek(0))) {
            bufferedLexer.advance();
        }

        bufferedLexer.markEnd();
        return true;
    }

    bool tryScanNumberFractionalPart (BufferedLexer &bufferedLexer) {
        if (bufferedLexer.peek(0) != CharacterCodes::dot) {
            return false;
        }
        bufferedLexer.advance();

        while (isDigit(bufferedLexer.peek(0))) {
            bufferedLexer.advance();
        }

        bufferedLexer.markEnd();
        return true;
    }

    bool tryScanNumberExponent2 (BufferedLexer &bufferedLexer) {
        /**
         * Optional +/- prefix for exponent
         */
        auto chPrefix = bufferedLexer.peek(0);
        if (chPrefix == CharacterCodes::plus || chPrefix == CharacterCodes::minus) {
            bufferedLexer.advance();
        }

        auto chFirstDigit = bufferedLexer.peek(0);
        if (!isDigit(chFirstDigit)) {
            return false;
        }
        bufferedLexer.advance();

        while (isDigit(bufferedLexer.peek(0))) {
            bufferedLexer.advance();
        }

        bufferedLexer.markEnd();
        return true;
    }

    bool tryScanNumberExponent (BufferedLexer &bufferedLexer) {
        auto chE = bufferedLexer.peek(0);
        if (chE != CharacterCodes::e && chE != CharacterCodes::E) {
            return false;
        }
        bufferedLexer.advance();

        return tryScanNumberExponent2(bufferedLexer);
    }

    int tryGetKeywordTokenType (std::string const &str) {
        auto it = keyword2TokenType.find(str);
        if (it == keyword2TokenType.end()) {
            return -1;
        } else {
            return it->second;
        }
    }

    int tryScanIdentifierOrKeywordOrNumberLiteral (BufferedLexer &bufferedLexer, const bool *valid_symbols, std::string const &customDelimiter) {
        if (!isUnquotedIdentifierCharacter(bufferedLexer.peek(0))) {
            //std::cout << "tryScanIdentifierOrKeywordOrNumberLiteral: -FAIL- " << bufferedLexer.peek(0) << std::endl;
            return -1;
        }

        if (tryScanDigitEDigit(bufferedLexer)) {
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
        auto str = tryScanUnquotedIdentifier(bufferedLexer, customDelimiter);
        //std::cout << "tryScanIdentifierOrKeywordOrNumberLiteral: " << str << std::endl;

        if (str.size() == 0) {
            //No unquoted identifier.
            //We already checked peek(0) is unquoted identifier character.
            //So, we were interrupted by custom delimiter.
            if (tryScanString(bufferedLexer, customDelimiter)) {
                return TokenType::CustomDelimiter;
            } else {
                //I don't know what this is, this should never happen
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
            if (tryScanNumberFractionalPart(bufferedLexer)) {
                if (tryScanNumberExponent(bufferedLexer)) {
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
            if (tryScanNumberExponent2(bufferedLexer)) {
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
