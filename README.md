# bfm
A simple macro language that generates brainfuck code, supporting // and /**/ style comments, control characters in strings, scoped variables (WIP), control constructs (if, while), and arrays (also WIP).

## syntax
All statements (except for the end keyword) follow this form:

`[keyword] [argument]`

e.g.

`var foo
if foo
print "Hello world!\n"`

All other valid code fragments follow this form:

`[var1] [operation] [var2 || const_expr]`

where the operation order is:
`[dest] [src]`
e.g.
`foo = 15
// foo is now three
foo % 5
foo is now 3`