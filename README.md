### Introduction

This repo demonstrates tree-sitter not parsing an entire string when it encounters unexpected tokens.

Problems:
+ tree-sitter is not parsing the entire string
+ The resulting parse tree reports no errors (Instead of saying something like, "Unexpected token Identifier")

Notes:
+ An external scanner is used (see [src/scanner.cc](src/scanner.cc))
+ See [grammar.js](grammar.js) for the grammar
+ See [test.js](test.js), which contains the repro, and some comments

-----

### Instructions

```
npm install
npm run build
npm run test
```

