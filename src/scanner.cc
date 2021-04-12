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

        bool tryScanOthers (TmpLexer &lexer, const bool *valid_symbols) {
            TmpLexer tmp(lexer);

            if (tryScanStringCaseInsensitive(tmp, "DELIMITER ")) {
                expectCustomDelimiter = true;

                return lexerResult(tmp.lexer.lexer, valid_symbols, TokenType::DELIMITER_STATEMENT);
            }

            auto tokenType = tryScanIdentifierOrKeywordOrNumberLiteral(tmp, valid_symbols, customDelimiter);
            if (tokenType < 0) {
                if (tmp.isEof(0)) {
                    return false;
                }
                tmp.advance();
                tmp.markEnd();
                return lexerForcedResult(tmp.lexer.lexer, TokenType::UnknownToken);
            }

            return lexerResult(tmp.lexer.lexer, valid_symbols, static_cast<TokenType>(tokenType));
        }

        bool scanDelimiter (TmpLexer &lexer, const bool *valid_symbols) {
            TmpLexer tmp(lexer);

            //Skip leading spaces
            if (tmp.peek(0) == CharacterCodes::space) {
                tmp.advance();

                while (tmp.peek(0) == CharacterCodes::space) {
                    tmp.advance();
                }

                tmp.markEnd();
                return lexerResult(tmp.lexer.lexer, valid_symbols, TokenType::WhiteSpace);
            }

            if (tmp.isEof(0)) {
                tmp.advance();
                tmp.markEnd();
                return lexerEofResult(tmp.lexer.lexer);
            }

            if (isLineBreak(tmp.peek(0))) {
                //Cannot have delimiter of length zero
                tmp.advance();
                tmp.markEnd();
                return lexerForcedResult(tmp.lexer.lexer, TokenType::LineBreak);
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
            while (!tmp.isEof(0) && !isLineBreak(tmp.peek(0))) {
                customDelimiter += tmp.advance();
            }

            if (customDelimiter.size() == 1 && customDelimiter[0] == ';') {
                //This is intentional.
                //Semicolon is the same as using the "original" delimiter,
                //Not a custom delimiter.
                customDelimiter.clear();
            }

            expectCustomDelimiter = false;
            tmp.markEnd();
            return lexerResult(tmp.lexer.lexer, valid_symbols, TokenType::CustomDelimiter);
        }

        bool scan(TSLexer *lexer, const bool *valid_symbols) {
            bufferedLexer.setLexer(lexer);

            TmpLexer tmp(bufferedLexer);

            if (expectCustomDelimiter) {
                return scanDelimiter(tmp, valid_symbols);
            }

            if (customDelimiter.size() > 0) {
                if (tryScanString(tmp, customDelimiter)) {
                    return lexerResult(lexer, valid_symbols, TokenType::CustomDelimiter);
                }
            }

            if (isWhiteSpace(tmp.peek(0))) {
                tmp.advance();

                while (isWhiteSpace(tmp.peek(0))) {
                    tmp.advance();
                }

                tmp.markEnd();
                return lexerResult(lexer, valid_symbols, TokenType::WhiteSpace);
            }

            char ch = tmp.peek(0);

            if (ch == CharacterCodes::carriageReturn) {
                if (tmp.peek(1) == CharacterCodes::lineFeed) {
                    tmp.advance();
                    tmp.advance();

                    //\r\n
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::LineBreak);
                }

                tmp.advance();

                //\r
                tmp.markEnd();
                return lexerResult(lexer, valid_symbols, TokenType::LineBreak);
            }

            if (ch == CharacterCodes::lineFeed) {
                tmp.advance();

                //\n
                tmp.markEnd();
                return lexerResult(lexer, valid_symbols, TokenType::LineBreak);
            }

            //https://dev.mysql.com/doc/refman/5.7/en/hexadecimal-literals.html
            if (ch == CharacterCodes::x || ch == CharacterCodes::X) {
                if (tmp.peek(1) == CharacterCodes::singleQuote) {
                    tmp.advance();
                    if (tryScanQuotedString(tmp)) {
                        return lexerResult(lexer, valid_symbols, TokenType::HexLiteral);
                    } else {
                        tmp.markEnd();
                        return lexerEofResult(lexer);
                    }
                } else {
                    return tryScanOthers(tmp, valid_symbols);
                }
            }

            if (ch == CharacterCodes::_0) {
                if (tmp.peek(1) == CharacterCodes::x) {
                    //String length should never be empty, we confirmed existence of characters 0x
                    //And 0x... does not match custom delimiter (already tried to match custom delimiter above)
                    auto str = tryScanUnquotedIdentifier(tmp, customDelimiter);
                    if (is0xHexLiteral(str)) {
                        return lexerResult(lexer, valid_symbols, TokenType::HexLiteral);
                    } else {
                        return lexerResult(lexer, valid_symbols, TokenType::Identifier);
                    }
                } else {
                    return tryScanOthers(tmp, valid_symbols);
                }
            }

            //https://dev.mysql.com/doc/refman/5.7/en/bit-value-literals.html
            if (ch == CharacterCodes::b || ch == CharacterCodes::B) {
                if (tmp.peek(1) == CharacterCodes::singleQuote) {
                    tmp.advance();
                    if (tryScanQuotedString(tmp)) {
                        return lexerResult(lexer, valid_symbols, TokenType::BitLiteral);
                    } else {
                        tmp.markEnd();
                        return lexerEofResult(lexer);
                    }
                } else {
                    return tryScanOthers(tmp, valid_symbols);
                }
            }

            if (ch == CharacterCodes::_0) {
                if (tmp.peek(1) == CharacterCodes::b) {
                    //String length should never be empty, we confirmed existence of characters 0b
                    //And 0b... does not match custom delimiter (already tried to match custom delimiter above)
                    auto str = tryScanUnquotedIdentifier(tmp, customDelimiter);
                    if (is0bBitLiteral(str)) {
                        return lexerResult(lexer, valid_symbols, TokenType::BitLiteral);
                    } else {
                        return lexerResult(lexer, valid_symbols, TokenType::Identifier);
                    }
                } else {
                    return tryScanOthers(tmp, valid_symbols);
                }
            }

            switch (ch) {
                case CharacterCodes::openBrace:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::OpenBrace);
                case CharacterCodes::closeBrace:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::CloseBrace);
                case CharacterCodes::openParen:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::OpenParentheses);
                case CharacterCodes::closeParen:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::CloseParentheses);
                case CharacterCodes::caret:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Caret);
                case CharacterCodes::asterisk:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Asterisk);
                case CharacterCodes::minus:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Minus);
                case CharacterCodes::plus:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Plus);
                case CharacterCodes::comma:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Comma);
                case CharacterCodes::bar:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Bar);
                case CharacterCodes::equals:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Equal);
                case CharacterCodes::semicolon:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::SemiColon);
                case CharacterCodes::dot:
                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Dot);
                case CharacterCodes::lessThan:
                    //<
                    //<<
                    //<>
                    //<=
                    //<=>
                    switch (tmp.peek(1))
                    {
                        case CharacterCodes::lessThan:
                            tmp.advance();
                            tmp.advance();
                            tmp.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::LessLess);
                        case CharacterCodes::greaterThan:
                            tmp.advance();
                            tmp.advance();
                            tmp.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::LessGreater);
                        case CharacterCodes::equals:
                            switch (tmp.peek(2))
                            {
                                case CharacterCodes::greaterThan:
                                    tmp.advance();
                                    tmp.advance();
                                    tmp.advance();
                                    tmp.markEnd();
                                    return lexerResult(lexer, valid_symbols, TokenType::LessEqualGreater);

                                default:
                                    tmp.advance();
                                    tmp.advance();
                                    tmp.markEnd();
                                    return lexerResult(lexer, valid_symbols, TokenType::LessEqual);
                            }

                        default:
                            tmp.advance();
                            tmp.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::Less);
                    }
                    break;
                case CharacterCodes::greaterThan:
                    //>
                    //>>
                    //>=
                    switch (tmp.peek(1))
                    {
                        case CharacterCodes::greaterThan:
                            tmp.advance();
                            tmp.advance();
                            tmp.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::GreaterGreater);

                        case CharacterCodes::equals:
                            tmp.advance();
                            tmp.advance();
                            tmp.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::GreaterEqual);

                        default:
                            tmp.advance();
                            tmp.markEnd();
                            return lexerResult(lexer, valid_symbols, TokenType::Greater);
                    }
                    break;
                case CharacterCodes::singleQuote:
                    if (tryScanQuotedString(tmp)) {
                        return lexerResult(lexer, valid_symbols, TokenType::StringLiteral);
                    } else {
                        tmp.markEnd();
                        return lexerEofResult(lexer);
                    }
                case CharacterCodes::slash:
                    if (tmp.peek(1) == CharacterCodes::asterisk) {
                        if (tmp.peek(2) == CharacterCodes::exclamation) {
                            tmp.advance();
                            tmp.advance();
                            tmp.advance();
                            if (tryScanTillEndOfMultiLineComment(tmp)) {
                                return lexerResult(lexer, valid_symbols, TokenType::ExecutionComment);
                            } else {
                                tmp.markEnd();
                                return lexerEofResult(lexer);
                            }
                        } else {
                            tmp.advance();
                            tmp.advance();
                            if (tryScanTillEndOfMultiLineComment(tmp)) {
                                return lexerResult(lexer, valid_symbols, TokenType::MultiLineComment);
                            } else {
                                tmp.markEnd();
                                return lexerEofResult(lexer);
                            }
                        }
                    } else {
                        tmp.advance();
                        tmp.markEnd();
                        return lexerResult(lexer, valid_symbols, TokenType::Slash);
                    }
                case CharacterCodes::colon:
                    if (tmp.peek(1) == CharacterCodes::equals) {
                        tmp.advance();
                        tmp.advance();
                        tmp.markEnd();
                        return lexerResult(lexer, valid_symbols, TokenType::ColonEqual);
                    }

                    tmp.advance();
                    tmp.markEnd();
                    return lexerResult(lexer, valid_symbols, TokenType::Colon);

                case CharacterCodes::at:
                    if (tmp.peek(1) == CharacterCodes::at) {
                        tmp.advance();
                        tmp.advance();
                        tmp.markEnd();
                        if (tryScanStringCaseInsensitive(tmp, "GLOBAL.")) {
                            return lexerResult(lexer, valid_symbols, TokenType::AtAtGlobalDot);
                        }

                        if (tryScanStringCaseInsensitive(tmp, "SESSION.")) {
                            return lexerResult(lexer, valid_symbols, TokenType::AtAtSessionDot);
                        }

                        return lexerResult(lexer, valid_symbols, TokenType::AtAt);
                    } else if (
                        tmp.peek(1) == CharacterCodes::doubleQuote ||
                        tmp.peek(1) == CharacterCodes::backtick ||
                        tmp.peek(1) == CharacterCodes::singleQuote
                    ) {
                        tmp.advance();
                        if (tryScanQuotedIdentifier(tmp)) {
                            return lexerResult(lexer, valid_symbols, TokenType::UserVariableIdentifier);
                        } else {
                            tmp.markEnd();
                            return lexerEofResult(lexer);
                        }
                    } else if (
                        isUnquotedIdentifierCharacter(tmp.peek(1))
                    ) {
                        tmp.advance();
                        tmp.markEnd();
                        //This may be empty.
                        auto str = tryScanUnquotedIdentifier(tmp, customDelimiter);
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
                        tmp.advance();
                        tmp.markEnd();
                        return lexerResult(lexer, valid_symbols, TokenType::UserVariableIdentifier);
                    }
                case CharacterCodes::doubleQuote:
                case CharacterCodes::backtick:
                    if (tryScanQuotedIdentifier(tmp)) {
                        return lexerResult(lexer, valid_symbols, TokenType::Identifier);
                    } else {
                        tmp.markEnd();
                        return lexerEofResult(lexer);
                    }
                default:
                    break;
            }

            return tryScanOthers(tmp, valid_symbols);
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
