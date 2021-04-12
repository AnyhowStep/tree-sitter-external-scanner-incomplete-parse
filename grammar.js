
const Precedence = {
    identifier : 0,
    real_literal : 1,

}

const {externals} = require("./externals");
module.exports = grammar({
    name: 'YOUR_LANGUAGE_NAME',

    externals,
    extras: $ => [
        $.SingleLineComment,
        $.MultiLineComment,
        $.ExecutionComment,
        $.WhiteSpace,
        $.LineBreak,
    ],
    inline: $ => [
        $.Statement,
        $.Schema,
        $.CharSet,
    ],
    supertypes: $ => [
        $.Statement,
    ],

    rules: {
        SourceFile: $ => seq(
            field("statement", repeat(choice($.LeadingStatement, $.DelimiterStatement))),
            field("statement", choice($.TrailingStatement, $.DelimiterStatement)),
        ),

        Statement: $ => choice(
            //TODO
            $.BinLogStatement,
            $.CreateSchemaStatement,
        ),

        BinLogStatement: $ => seq(
            field("binLogToken", $.BINLOG),
            field("str", $.StringLiteral),
        ),

        CreateSchemaStatement: $ => seq(
            field("createToken", $.CREATE),
            field("schemaToken", $.Schema),
            field("ifNotExists", optional($.IfNotExists)),
            field("identifier", $.Identifier),
            field("createSchemaOptionList", optional($.CreateSchemaOptionList)),
        ),

        CreateSchemaOptionList: $ => field("item", repeat1(choice(
            $.DefaultCharacterSet,
            $.DefaultCollate
        ))),

        DefaultCharacterSet: $ => seq(
            field("defaultToken", optional($.DEFAULT)),
            field("characterSetToken", $.CharSet),
            field("equalToken", optional($.Equal)),
            field("characterSetName", $.CharacterSetNameOrDefault),
        ),

        DefaultCollate: $ => seq(
            field("defaultToken", optional($.DEFAULT)),
            field("collateToken", $.COLLATE),
            field("equalToken", optional($.Equal)),
            field("collationName", $.CollationNameOrDefault),
        ),

        CharacterSetNameOrDefault: $ => choice(
            $.DEFAULT,
            $.BINARY,
            $.Identifier,
            $.StringLiteral,
        ),

        CollationNameOrDefault: $ => choice(
            $.DEFAULT,
            $.Identifier,
            $.StringLiteral,
        ),

        CharSet: $ => choice(
            seq($.CHARACTER, $.SET),
            $.CHARSET,
        ),

        Schema: $ => choice(
            $.DATABASE,
            $.SCHEMA,
        ),

        IfNotExists: $ => seq(
            field("ifToken", $.IF),
            field("notToken", $.NOT),
            field("existsToken", $.EXISTS),
        ),

        LeadingStatement: $ => seq(
            field("statement", $.Statement),
            choice(
                seq(
                    field("semiColonToken", $.SemiColon),
                    field("customDelimiter", optional($.CustomDelimiter)),
                ),
                field("customDelimiter", $.CustomDelimiter),
            ),
        ),

        TrailingStatement: $ => seq(
            field("statement", $.Statement),
            field("semiColonToken", optional($.SemiColon)),
            field("customDelimiter", optional($.CustomDelimiter)),
        ),

        /**
         * A client-only statement
         */
        DelimiterStatement: $ => seq(
            field("delimiterStart", $.DELIMITER_STATEMENT),
            field("customDelimiter", $.CustomDelimiter),
        ),

        /**
         * This is never used.
         */
        //dummy_rule: $ => /\w+/,
    },
});
