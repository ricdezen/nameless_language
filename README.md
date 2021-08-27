# Nameless Language

I am currently reading a very nice book called [Crafting Interpreters](https://craftinginterpreters.com/). In this
repository is the code for my version of *clox*, which is going to initially be just the one in the book but commented
by me as I go.

I already read the whole book. I found it very interesting. I am planning to try to implement a full-featured language.
It will take quite some time, but I have a list of features in mind. Nothing special, I just want to learn things.

Reached chapter 19

**TO-DO**

Book exercises:

- Add support for comment blocks. Does it make sense to allow nesting? ~~I would say yes. My comment may have some code
  snippet which could be further commented.~~ That is not my business, unless I explicitly support markdown for my
  docstring equivalent. It makes more sense not to allow nesting. C does not, I believe Java did? Mmh.

- Remove semicolons (except for loop, I guess).
    - Step 1: Just remove the terminator. End of line means end of instruction.
    - Step 2: Allow multi-line instructions. An example can be to ignore line breaks after tokens that cannot end an
      expression.
      ```bash
      # This is allowed.
      object.foo(
          ...
      )
      
      # This is not (should it?)
      # Don't be a Javascript.
      # Maybe a \ or ... is fine.
      object.builder()
          .param_one(1)
          .param_two(2)
      ```

- Implement ternary operator `condition ? then : else`. Mind precedence and associativity.

- Add error productions to handle each binary operator appearing without a left-hand operand. In other words, detect a
  binary operator appearing at the beginning of an expression. Report that as an error, but also parse and discard a
  right-hand operand with the appropriate precedence.

- Implement bitwise and (`&`), or (`|`) and xor (`^`). Mind precedence (C's is not really cool, check note in book).

- Extend string concatenation to implicitly convert the other operand to a string and concatenate.

- Add error handling for division by 0.

- Add back support in the REPL for expressions (expression statements basically, which do nothing in the code).

- Add support for `break` and `continue`. It is a syntax error to use them outside loops.

- Add support for lambdas.

- Add static members to classes.

- Implement [run-length encoding](https://en.wikipedia.org/wiki/Run-length_encoding) of line numbers.

- Implement another (or another two) instruction to load more than 255 constants.

- Implement a stack on a dynamic array for the value stack.

My own:

- Change the virtual machine from a global to something that can be created and passed around. This implies modifying
  every method that references `vm` to take a `VM`-type argument.

- Same for the Scanner.

- Same for the Parser (and the concept of "current chunk" can probably be added to the parser).

- Change Strings to behave more like other languages. Look into UTF-8 and escape sequences.

- After fixing strings, add non-escape (raw strings) and string interpolation (Python's format strings).

- I assume adding aliases for logical operators would be nice? Or at least uniform the style: I have `and`, `or` and `!`
  . So either turn `!` into `not`, turn `and`/`or` into `&&`/`||`, or keep all 6.

- Some operators such as `!=`, `>=`, `<=` are syntactic sugar. Implement them with a dedicated instruction.

- Other famous operators: `>>`, `<<`, `**`, `++`, `--`, `//`, `+=`, `-=`, `*=`, `/=`, `[]`, `()`.

- Adjust falseness. In order to add operator overloading, the best idea is probably to do what Python did (special
  methods).

- There are various TODO in the code, which do not represent features, mostly stuff that needs to be fixed.