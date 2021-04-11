#include <tree_sitter/parser.h>
#include <cwctype>
#include <vector>
#include <string>
#include <iostream>
#include "./token-kind.cc"
#include "./character-code.cc"
#include "./buffered-lexer.cc"
#include "./scan-util.cc"

namespace {
    struct Scanner {
        static void skip(TSLexer *lexer) { lexer->advance(lexer, true); }

        static void advance(TSLexer *lexer) { lexer->advance(lexer, false); }

        BufferedLexer bufferedLexer;

        /**
         * If true, the next call to `scan()` should return a custom delimiter token
         */
        bool expectCustomDelimiter = false;
        std::string customDelimiter;

        unsigned serialize (char *buffer) {
            buffer[0] = expectCustomDelimiter ? '1' : '0';
            memcpy(buffer+1, customDelimiter.c_str(), sizeof(char)*customDelimiter.size());

            return sizeof(char)*customDelimiter.size() + 1;
        }

        void deserialize (const char *buffer, unsigned length) {
            if (length == 0) {
                expectCustomDelimiter = false;
                customDelimiter.clear();
            } else {
                expectCustomDelimiter = buffer[0] == '1';
                customDelimiter = std::string(buffer+1, length-1);
            }
        }

        bool tryScanOthers (TSLexer *lexer, const bool *valid_symbols) {
            if (tryScanStringCaseInsensitive(bufferedLexer, "DELIMITER ")) {
                expectCustomDelimiter = true;

                return lexerResult(lexer, valid_symbols, TokenType::DELIMITER_STATEMENT);
            }

            auto tokenType = tryScanIdentifierOrKeywordOrNumberLiteral(bufferedLexer, valid_symbols, customDelimiter);
            if (tokenType < 0) {
                if (bufferedLexer.isEof(0)) {
                    return false;
                }
                bufferedLexer.advance();
                bufferedLexer.markEnd();
                return lexerForcedResult(lexer, TokenType::UnknownToken);
            }

            return lexerResult(lexer, valid_symbols, static_cast<TokenType>(tokenType));
        }

        bool scanDelimiter (TSLexer *lexer, const bool *valid_symbols) {
            //Skip leading spaces
            if (bufferedLexer.peek(0) == CharacterCodes::space) {
                bufferedLexer.advance();

                while (bufferedLexer.peek(0) == CharacterCodes::space) {
                    bufferedLexer.advance();
                }

                bufferedLexer.markEnd();
                return lexerResult(lexer, valid_symbols, TokenType::WhiteSpace);
            }

            if (bufferedLexer.isEof(0)) {
                bufferedLexer.advance();
                bufferedLexer.markEnd();
                return lexerEofResult(lexer);
            }

            if (isLineBreak(bufferedLexer.peek(0))) {
                //Cannot have delimiter of length zero
                bufferedLexer.advance();
                bufferedLexer.markEnd();
                return lexerForcedResult(lexer, TokenType::LineBreak);
            }

            /**
             * Find delimiter.
             *
             * Interesting to note, the following are valid,
             * + `\t$$`
             * + `$$`
             * + `\t`
             * + `\t `
             * + `\t\t`
             * + `\t\t `
             * + `$ $` (there is a space character in the middle)
             *
             * The following are invalid,
             * + `$$\t`
             * + `$$ `
             *
             * So, whitespace can be part of delimiter, but cannot be trailing,
             * unless the entire delimiter is whitespace.
             */
            customDelimiter.clear();
            while (!bufferedLexer.isEof(0) && !isLineBreak(bufferedLexer.peek(0))) {
                customDelimiter += bufferedLexer.advance();
            }

            if (customDelimiter.size() == 1 && customDelimiter[0] == ';') {
                //This is intentional.
                //Semicolon is the same as using the "original" delimiter,
                //Not a custom delimiter.
                customDelimiter.clear();
            }

            expectCustomDelimiter = false;
            bufferedLexer.markEnd();
            return lexerResult(lexer, valid_symbols, TokenType::CustomDelimiter);
        }

        bool scan(TSLexer *lexer, const bool *valid_symbols) {
            bufferedLexer.setLexer(lexer);
            bufferedLexer.markEnd();

            if (expectCustomDelimiter) {
                return scanDelimiter(lexer, valid_symbols);
            }

            if (customDelimiter.size() > 0) {
                if (tryScanString(bufferedLexer, customDelimiter)) {
                    return lexerResult(lexer, valid_symbols, TokenType::CustomDelimiter);
                }
            }

            if (isWhiteSpace(bufferedLexer.peek(0))) {
                bufferedLexer.advance();

                while (isWhiteSpace(bufferedLexer.peek(0))) {
                    bufferedLexer.advance();
                }

                bufferedLexer.markEnd();
                return lexerResult(lexer, valid_symbols, TokenType::WhiteSpace);
            }

            char ch = bufferedLexer.peek(0);

            if (ch == CharacterCodes::carriageReturn) {
                if (bufferedLexer.peek(1) == CharacterCodes::lineFeed) {
                    bufferedLexer.advance();
                    bufferedLexer.advance();

                    //\r\n
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::LineBreak);
                }

                bufferedLexer.advance();

                //\r
                bufferedLexer.markEnd();
                return lexerResult(lexer, valid_symbols, TokenType::LineBreak);
            }

            if (ch == CharacterCodes::lineFeed) {
                bufferedLexer.advance();

                //\n
                bufferedLexer.markEnd();
                return lexerResult(lexer, valid_symbols, TokenType::LineBreak);
            }

            //https://dev.mysql.com/doc/refman/5.7/en/hexadecimal-literals.html
            if (ch == CharacterCodes::x || ch == CharacterCodes::X) {
                if (bufferedLexer.peek(1) == CharacterCodes::singleQuote) {
                    bufferedLexer.advance();
                    if (tryScanQuotedString(bufferedLexer)) {
                        return lexerResult(lexer, valid_symbols, TokenType::HexLiteral);
                    } else {
                        bufferedLexer.markEnd();
                        return lexerEofResult(lexer);
                    }
                } else {
                    return tryScanOthers(lexer, valid_symbols);
                }
            }

            if (ch == CharacterCodes::_0) {
                if (bufferedLexer.peek(1) == CharacterCodes::x) {
                    //String length should never be empty, we confirmed existence of characters 0x
                    //And 0x... does not match custom delimiter (already tried to match custom delimiter above)
                    auto str = tryScanUnquotedIdentifier(bufferedLexer, customDelimiter);
                    if (is0xHexLiteral(str)) {
                        return lexerResult(lexer, valid_symbols, TokenType::HexLiteral);
                    } else {
                        return lexerResult(lexer, valid_symbols, TokenType::Identifier);
                    }
                } else {
                    return tryScanOthers(lexer, valid_symbols);
                }
            }

            //https://dev.mysql.com/doc/refman/5.7/en/bit-value-literals.html
            if (ch == CharacterCodes::b || ch == CharacterCodes::B) {
                if (bufferedLexer.peek(1) == CharacterCodes::singleQuote) {
                    bufferedLexer.advance();
                    if (tryScanQuotedString(bufferedLexer)) {
                        return lexerResult(lexer, valid_symbols, TokenType::BitLiteral);
                    } else {
                        bufferedLexer.markEnd();
                        return lexerEofResult(lexer);
                    }
                } else {
                    return tryScanOthers(lexer, valid_symbols);
                }
            }

            if (ch == CharacterCodes::_0) {
                if (bufferedLexer.peek(1) == CharacterCodes::b) {
                    //String length should never be empty, we confirmed existence of characters 0b
                    //And 0b... does not match custom delimiter (already tried to match custom delimiter above)
                    auto str = tryScanUnquotedIdentifier(bufferedLexer, customDelimiter);
                    if (is0bBitLiteral(str)) {
                        return lexerResult(lexer, valid_symbols, TokenType::BitLiteral);
                    } else {
                        return lexerResult(lexer, valid_symbols, TokenType::Identifier);
                    }
                } else {
                    return tryScanOthers(lexer, valid_symbols);
                }
            }

            switch (ch) {
                case CharacterCodes::openBrace:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::OpenBrace);
                case CharacterCodes::closeBrace:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::CloseBrace);
                case CharacterCodes::openParen:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::OpenParentheses);
                case CharacterCodes::closeParen:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::CloseParentheses);
                case CharacterCodes::caret:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Caret);
                case CharacterCodes::asterisk:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Asterisk);
                case CharacterCodes::minus:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Minus);
                case CharacterCodes::plus:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Plus);
                case CharacterCodes::comma:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Comma);
                case CharacterCodes::bar:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Bar);
                case CharacterCodes::equals:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Equal);
                case CharacterCodes::semicolon:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::SemiColon);
                case CharacterCodes::dot:
                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Dot);
                case CharacterCodes::lessThan:
                    //<
                    //<<
                    //<>
                    //<=
                    //<=>
                    switch (bufferedLexer.peek(1))
                    {
                        case CharacterCodes::lessThan:
                            bufferedLexer.advance();
                            bufferedLexer.advance();
                            bufferedLexer.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::LessLess);
                        case CharacterCodes::greaterThan:
                            bufferedLexer.advance();
                            bufferedLexer.advance();
                            bufferedLexer.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::LessGreater);
                        case CharacterCodes::equals:
                            switch (bufferedLexer.peek(2))
                            {
                                case CharacterCodes::greaterThan:
                                    bufferedLexer.advance();
                                    bufferedLexer.advance();
                                    bufferedLexer.advance();
                                    bufferedLexer.markEnd();
                                    return lexerResult(lexer, valid_symbols, TokenType::LessEqualGreater);

                                default:
                                    bufferedLexer.advance();
                                    bufferedLexer.advance();
                                    bufferedLexer.markEnd();
                                    return lexerResult(lexer, valid_symbols, TokenType::LessEqual);
                            }

                        default:
                            bufferedLexer.advance();
                            bufferedLexer.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::Less);
                    }
                    break;
                case CharacterCodes::greaterThan:
                    //>
                    //>>
                    //>=
                    switch (bufferedLexer.peek(1))
                    {
                        case CharacterCodes::greaterThan:
                            bufferedLexer.advance();
                            bufferedLexer.advance();
                            bufferedLexer.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::GreaterGreater);

                        case CharacterCodes::equals:
                            bufferedLexer.advance();
                            bufferedLexer.advance();
                            bufferedLexer.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::GreaterEqual);

                        default:
                            bufferedLexer.advance();
                            bufferedLexer.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::Greater);
                    }
                    break;
                case CharacterCodes::singleQuote:
                    if (tryScanQuotedString(bufferedLexer)) {
                        return lexerResult(lexer, valid_symbols, TokenType::StringLiteral);
                    } else {
                        bufferedLexer.markEnd();
                        return lexerEofResult(lexer);
                    }
                case CharacterCodes::slash:
                    if (bufferedLexer.peek(1) == CharacterCodes::asterisk) {
                        if (bufferedLexer.peek(2) == CharacterCodes::exclamation) {
                            bufferedLexer.advance();
                            bufferedLexer.advance();
                            bufferedLexer.advance();
                            if (tryScanTillEndOfMultiLineComment(bufferedLexer)) {
                                return lexerResult(lexer, valid_symbols, TokenType::ExecutionComment);
                            } else {
                                bufferedLexer.markEnd();
                                return lexerEofResult(lexer);
                            }
                        } else {
                            bufferedLexer.advance();
                            bufferedLexer.advance();
                            if (tryScanTillEndOfMultiLineComment(bufferedLexer)) {
                                return lexerResult(lexer, valid_symbols, TokenType::MultiLineComment);
                            } else {
                                bufferedLexer.markEnd();
                                return lexerEofResult(lexer);
                            }
                        }
                    } else {
                        bufferedLexer.advance();
                        bufferedLexer.markEnd();
                        return lexerResult(lexer, valid_symbols, TokenType::Slash);
                    }
                case CharacterCodes::colon:
                    if (bufferedLexer.peek(1) == CharacterCodes::equals) {
                        bufferedLexer.advance();
                        bufferedLexer.advance();
                        bufferedLexer.markEnd();
                        return lexerResult(lexer, valid_symbols, TokenType::ColonEqual);
                    }

                    bufferedLexer.advance();
                    bufferedLexer.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Colon);

                case CharacterCodes::at:
                    if (bufferedLexer.peek(1) == CharacterCodes::at) {
                        bufferedLexer.advance();
                        bufferedLexer.advance();
                        bufferedLexer.markEnd();
                        if (tryScanStringCaseInsensitive(bufferedLexer, "GLOBAL.")) {
                            return lexerResult(lexer, valid_symbols, TokenType::AtAtGlobalDot);
                        }

                        if (tryScanStringCaseInsensitive(bufferedLexer, "SESSION.")) {
                            return lexerResult(lexer, valid_symbols, TokenType::AtAtSessionDot);
                        }

                        return lexerResult(lexer, valid_symbols, TokenType::AtAt);
                    } else if (
                        bufferedLexer.peek(1) == CharacterCodes::doubleQuote ||
                        bufferedLexer.peek(1) == CharacterCodes::backtick ||
                        bufferedLexer.peek(1) == CharacterCodes::singleQuote
                    ) {
                        bufferedLexer.advance();
                        if (tryScanQuotedIdentifier(bufferedLexer)) {
                            return lexerResult(lexer, valid_symbols, TokenType::UserVariableIdentifier);
                        } else {
                            bufferedLexer.markEnd();
                            return lexerEofResult(lexer);
                        }
                    } else if (
                        isUnquotedIdentifierCharacter(bufferedLexer.peek(1))
                    ) {
                        bufferedLexer.advance();
                        bufferedLexer.markEnd();
                        //This may be empty.
                        auto str = tryScanUnquotedIdentifier(bufferedLexer, customDelimiter);
                        if (str.size() == 0) {
                            /**
                             * @todo Investigate why MySQL allows this,
                             * ```sql
                             *  SELECT @;
                             * ```
                             *
                             * @todo Investigate why MySQL allows this,
                             * ```sql
                             *  CREATE DEFINER=root @ FUNCTION FOO () RETURNS BOOLEAN RETURN TRUE;
                             * ```
                             */
                            return lexerResult(lexer, valid_symbols, TokenType::UserVariableIdentifier);
                        } else {
                            return lexerResult(lexer, valid_symbols, TokenType::UserVariableIdentifier);
                        }
                    } else {
                        /**
                         * @todo Investigate why MySQL allows this,
                         * ```sql
                         *  SELECT @;
                         * ```
                         *
                         * @todo Investigate why MySQL allows this,
                         * ```sql
                         *  CREATE DEFINER=root @ FUNCTION FOO () RETURNS BOOLEAN RETURN TRUE;
                         * ```
                         */
                        bufferedLexer.advance();
                        bufferedLexer.markEnd();
                        return lexerResult(lexer, valid_symbols, TokenType::UserVariableIdentifier);
                    }
                case CharacterCodes::doubleQuote:
                case CharacterCodes::backtick:
                    if (tryScanQuotedIdentifier(bufferedLexer)) {
                        return lexerResult(lexer, valid_symbols, TokenType::Identifier);
                    } else {
                        bufferedLexer.markEnd();
                        return lexerEofResult(lexer);
                    }
                default:
                    break;
            }

            return tryScanOthers(lexer, valid_symbols);
        }
    };

}

extern "C" {

    void *tree_sitter_YOUR_LANGUAGE_NAME_external_scanner_create() {
        //std::cout << "create" << std::endl;
        auto result = new Scanner();
        //std::cout << "create2" << std::endl;
        return result;
    }

    void tree_sitter_YOUR_LANGUAGE_NAME_external_scanner_destroy(void *payload) {
        //std::cout << "destroy" << std::endl;
        Scanner *scanner = static_cast<Scanner *>(payload);
        delete scanner;
    }

    bool tree_sitter_YOUR_LANGUAGE_NAME_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
        //std::cout << "scan" << std::endl;
        //std::cout << valid_symbols[TokenType::DATABASE] << std::endl;
        Scanner *scanner = static_cast<Scanner *>(payload);
        return scanner->scan(lexer, valid_symbols);
    }

    unsigned tree_sitter_YOUR_LANGUAGE_NAME_external_scanner_serialize(void *payload, char *buffer) {
        //std::cout << "serialize" << std::endl;
        Scanner *scanner = static_cast<Scanner *>(payload);
        return scanner->serialize(buffer);
    }

    void tree_sitter_YOUR_LANGUAGE_NAME_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
        //std::cout << "deserialize" << std::endl;
        //std::cout << length << std::endl;
        Scanner *scanner = static_cast<Scanner *>(payload);
        scanner->deserialize(buffer, length);
    }

}
