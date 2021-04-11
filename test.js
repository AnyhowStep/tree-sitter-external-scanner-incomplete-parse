const tape = require("tape");
const { parse } = require("./parse");

tape(__filename, t => {
    /**
     * This is a good example of why you should not use "A" as a DELIMITER
     */
    const sql = `DELIMITER A
CREATE SCHEMA D;`;
    const tree = parse(sql);

    console.log("=== begin rootNode ==")
    /**
     * Expected something like:
     *  (ERROR
     *      (SourceFile
     *          (DelimiterStatement (DELIMITER) (CustomDelimiter))
     *      )
     *      (Identifier) (CustomDelimiter) (Identifier) (CustomDelimiter) (Identifier) (SemiColon)
     *  )
     * 
     * With everything parsed.
     * 
     * Or:
     *  (SourceFile
     *      (DelimiterStatement (DELIMITER) (CustomDelimiter))
     *      (ERROR
     *          (Identifier) (CustomDelimiter) (Identifier) (CustomDelimiter) (Identifier) (SemiColon)
     *      )
     *  )
     * -----
     *  
     * Actual:
     *  (SourceFile
     *      (DelimiterStatement (DELIMITER) (CustomDelimiter))
     *  )
     * 
     * With only part of the string parsed.
     */
    console.log(tree.rootNode.toString())
    console.log("=== end rootNode ==")

    const statements = tree.rootNode.statementNodes;

    t.deepEqual(
        statements.length,
        1
    );

    t.deepEqual(
        tree.rootNode.endPosition,
        {
            row : 1,
            //Should actually be column 16, I think.
            column : 0,
        }
    );

    t.deepEqual(
        tree.rootNode.hasError(),
        //Should actually be true.
        false
    );

    t.end();
});
