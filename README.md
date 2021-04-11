### Introduction

This repo demonstrates tree-sitter not parsing an entire string when it encounters unexpected tokens.

Problems:
+ tree-sitter is not parsing the entire string
+ The resulting parse tree reports no errors (Instead of saying something like, "Unexpected token Identifier")

Notes:
+ An external scanner is used (see [src/scanner.cc](src/scanner.cc))
+ See [grammar.js](grammar.js) for the grammar
+ See [test.js](test.js), which contains the repro, and some comments
+ See [test.sql](test.sql), which contains the input file for `tree-sitter parse`

-----

### Instructions

```
npm install
npm run build
npm run test
npm run parse
```

-----

`npm run parse` will output,
```
(SourceFile [0, 0] - [1, 0]
  statement: (DelimiterStatement [0, 0] - [0, 11]
    delimiterStart: (DELIMITER_STATEMENT [0, 0] - [0, 10])
    customDelimiter: (CustomDelimiter [0, 10] - [0, 11]))
  (LineBreak [0, 11] - [1, 0]))
```

However, it should encounter (and parse) some identifiers, custom delimiters, and a semicolon after the final line break.
It does not.
