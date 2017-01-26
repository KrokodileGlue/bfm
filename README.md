# bfm
A macro language that generates brainfuck code, supporting // and /**/ style comments, control characters in strings, scoped variables, control constructs (if, while), and arrays. The language has many algorithms built in, such as addition, subtraction, multiplication, division, modulus, numeric cell printing, and more.   
You can see the examples for tips on structuring this type of code, but the language itsself is not entirely finished yet.  

## syntax
All statements (except for the end keyword) follow the form `[keyword] [argument]`. There are no rules about whitespace or indentation.

e.g.

    var foo   foo = 1
    if foo
        print "Hello world!\n"
    end

All other valid code fragments follow this form:

`[var1] [operation] [var2 || constant]`

where the operation order is:
`[dest] [src]`
e.g.

    foo = 15
    // foo is now 15
    foo % 4
    //foo is now 3
There is also a system for defining constants with a # token like this:  

    # FIELD_WIDTH 512
Which just means that whenever the language is trying to resolve a righthand identifier it will look it up in the table of constant values defined this way. So `foo = FIELD_WIDTH` would be okay, but

    # print_keyword print
    print_keyword "test"
would not.

Arrays are currently subscripted with a period, and there seem to be some bugs that need working out with them.