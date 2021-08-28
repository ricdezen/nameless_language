# Nameless Language

I am currently reading a very nice book called [Crafting Interpreters](https://craftinginterpreters.com/). In this
repository is the code for my version of *clox*, which is going to initially be just the one in the book but commented
by me as I go.

I already read the whole book. I found it very interesting. I am planning to try to implement a full-featured language.
It will take quite some time, but I have a list of features in mind. Nothing special, I just want to learn things.

Reached chapter 22

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

- Look into [flexible array members](https://en.wikipedia.org/wiki/Flexible_array_member) to see if you can further
  optimize the string structure.

- We always copy the string when allocating the objects. It could be interesting to add support for strings that don't
  own their character array, but instead point to some non-freeable location. Maybe like the constant table.

- ~~Add other values as keys in the hash table.~~ I'll change this to: add has to any object. I do not really dig NaN
  boxing, so I would have those 4 spare bytes inside the Value structure.

- The compiler adds a global variable’s name to the constant table as a string every time an identifier is encountered.
  It creates a new constant each time, even if that variable name is already in a previous slot in the constant table.
  That’s wasteful in cases where the same variable is referenced multiple times by the same function. That, in turn,
  increases the odds of filling up the constant table and running out of slots since we allow only 256 constants in a
  single chunk.

- Looking up a global variable by name in a hash table each time it is used is pretty slow, even with a good hash table.
  Can you come up with a more efficient way to store and access global variables without changing the semantics?

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

- UTF-8 for strings. *Have*. *Fun*.

- There are various TODO in the code, which do not represent new features, mostly technical stuff that can probably be
  changed locally with no trouble.