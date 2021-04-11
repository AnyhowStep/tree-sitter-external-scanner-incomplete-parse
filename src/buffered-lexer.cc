#ifndef BUFFERED_LEXER_CC
#define BUFFERED_LEXER_CC
#include <tree_sitter/parser.h>
#include <deque>

namespace {
    struct BufferedLexer {
        std::deque<char> buffer;
        TSLexer *lexer;

        void setLexer(TSLexer *lexer) {
            this->buffer.clear();
            this->lexer = lexer;
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

        void markEnd () {
            lexer->mark_end(lexer);
        }
    };
}
#endif
