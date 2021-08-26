# Nameless Language

I am currently reading a very nice book called [Crafting Interpreters](https://craftinginterpreters.com/). In this
repository is the code for my version of Lox, which is just the one in the book but commented by me as I go.

I will take inspiration from the book's exercises to further develop the language, until it becomes worthy of a name.

Reached 15

**TO-DO**: Mainly the exercises at the end of each chapter. My own ideas will come later.

- Add support for comment blocks. Does it make sense to allow nesting? I would say yes. My comment may have some code
  snippet which could be further commented.

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

- Handle error handling for division by 0.

- Add back support in the REPL for expressions (expression statements basically, which do nothing in the code).

- Add support for `break` and `continue`. It is a syntax error to use them outside loops.

- Add support for lambdas.

- Add static members to classes.

- Implement [run-length encoding](https://en.wikipedia.org/wiki/Run-length_encoding) of line numbers.

- Implement another (or another two) instruction to load more than 255 constants.