#ifndef BUFFERED_LEXER_CC
#define BUFFERED_LEXER_CC
#include <tree_sitter/parser.h>
#include <deque>

namespace {

    struct BufferedLexer {
        std::deque<char> buffer;
        TSLexer *lexer;

        void setLexer (TSLexer *lexer) {
            this->buffer.clear();
            this->lexer = lexer;
            markEnd();
        }

        char peek (int offset) {
            while (static_cast<size_t>(offset) > buffer.size()) {
                buffer.push_back(lexer->lookahead);
                lexer->advance(lexer, false);
            }

            if (static_cast<size_t>(offset) == buffer.size()) {
                return lexer->lookahead;
            }

            return buffer[offset];
        }

        bool isEof (int offset) {
            return peek(offset) == 0;
        }

        char advance () {
            if (buffer.size() == 0) {
                auto result = lexer->lookahead;
                lexer->advance(lexer, false);
                return result;
            }

            auto result = buffer.front();
            buffer.pop_front();
            return result;
        }

        void advanceN (int n) {
            for (int i=0; i<n; ++i) {
                advance();
            }
        }

        void markEnd () {
            lexer->mark_end(lexer);
        }
    };
    struct TmpLexer {
        BufferedLexer &lexer;
        int index = 0;

        TmpLexer (BufferedLexer &lexer) : lexer(lexer) {
        }

        TmpLexer (TmpLexer &lexer) : lexer(lexer.lexer), index(lexer.index) {
        }

        char peek (int offset) {
            return lexer.peek(index+offset);
        }

        bool isEof (int offset) {
            return lexer.isEof(index+offset);
        }

        char advance () {
            auto result = peek(0);
            ++index;
            return result;
        }

        void markEnd () {
            lexer.advanceN(index);
            lexer.markEnd();
            index = 0;
        }
    };
}
#endif
