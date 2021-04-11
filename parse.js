const Parser = require('tree-sitter');
const language = require(".");

function parse (input) {
    const parser = new Parser();
    parser.setLanguage(language);

    return parser.parse(input);
}
module.exports = {
    parse,
};
