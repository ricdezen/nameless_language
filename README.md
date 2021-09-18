# Nameless Language

I am currently reading a very nice book called [Crafting Interpreters](https://craftinginterpreters.com/). In this
repository is the code for my version of *clox*, which is going to initially be just the one in the book but commented
by me as I go.

I already read the whole book. I found it very interesting. I am planning to try to implement a full-featured language.
It will take quite some time, but I have a list of features in mind. Nothing special, this is not going to be the new
Python, I just want to learn things.

Reached chapter 27

**TO-DO**

Book exercises:

- Add support for comment blocks. Does it make sense to allow nesting? ~~I would say yes. My comment may have some code
  snippet which could be further commented.~~ ~~That is not my business, unless I explicitly support markdown for my
  docstring equivalent. It makes more sense not to allow nesting. C does not, I believe Java did? Mmh.~~ Whatever, I'll
  allow it.

- Remove semicolons (except in `for` loop, I guess).
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

- Idem for other places where the 255 limit is in place (e.g. local variables).

- Implement a stack on a dynamic array for the value stack.

- Look into [flexible array members](https://en.wikipedia.org/wiki/Flexible_array_member) to see if you can further
  optimize the string structure.

- We always copy the string when allocating the objects. It could be interesting to add support for strings that don't
  own their character array, but instead point to some non-freeable location. Maybe like the constant table.

- ~~Add other values as keys in the hash table.~~ I'll change this to: add hash to any object. I do not really dig NaN
  boxing, so I would have those 4 spare bytes inside the Value structure.

- The compiler adds a global variable’s name to the constant table as a string every time an identifier is encountered.
  It creates a new constant each time, even if that variable name is already in a previous slot in the constant table.
  That’s wasteful in cases where the same variable is referenced multiple times by the same function. That, in turn,
  increases the odds of filling up the constant table and running out of slots since we allow only 256 constants in a
  single chunk.

- Looking up a global variable by name in a hash table each time it is used is pretty slow, even with a good hash table.
  Can you come up with a more efficient way to store and access global variables without changing the semantics?

- Our simple local array makes it easy to calculate the stack slot of each local variable. But it means that when the
  compiler resolves a reference to a variable, we have to do a linear scan through the array. Come up with something
  more efficient. Do you think the additional complexity is worth it?

- Implement a switch statement. For this I think it would be easier for me to avoid fall-through. Another option would
  be to have both "match" (no fall-through) and "switch" (with fall-through).

- Reading and writing the ip field is one of the most frequent operations inside the bytecode loop. Right now, we access
  it through a pointer to the current CallFrame. That requires a pointer indirection which may force the CPU to bypass
  the cache and hit main memory. That can be a real performance sink. Ideally, we’d keep the ip in a native CPU
  register. C does not let us require that without dropping into inline assembly, but we can structure the code to
  encourage the compiler to make that optimization. If we store the ip directly in a C local variable and mark it
  register, there’s a good chance the C compiler will accede to our polite request. This does mean we need to be careful
  to load and store the local ip back into the correct CallFrame when starting and ending function calls. Implement this
  optimization. Write a couple of benchmarks and see how it affects the performance. Do you think the extra code
  complexity is worth it?

- Add arity checking to native function calls.

- Add possibility to report runtime errors to function calls.

- Only wrap functions in closures if they do capture a value **OR** make closure the standard implementation of a
  function, since the main point is to avoid the pointer indirection.

- Consider whether a loop should create a new variable at each iteration or not.

- Consider challenges 1, 2 and 3 of chapter 26 about GC optimization.

My own:

- Implement Stack, List etc. separately from the rest of the structures. Take care to make two versions of the List, one
  avoids being managed by garbage collection so that it can be used during garbage collection itself and for the value
  stack.

- To avoid having so many pointer indirections it could be useful to have a set of cached fields in the VM structure,
  such as call frames, constant tables etc., which get changed only on frame switch.

- Change the virtual machine from a global to something that can be created and passed around. This implies modifying
  every method that references `vm` to take a `VM`-type argument. Before doing this, consider whether it makes sense or
  not. The main advantage is being able to run multi-threaded operations using multiple vms. I still can, by updating
  the value of the vm variable, but then It would need to be a pointer, and the difference with passing such pointer
  around is minimal.

- Scanner, Parser and Compiler can probably be subject to some of this treatment too, although their implementation
  changed since I wrote this.

- Change Strings to behave more like other languages. Look into UTF-8 and escape sequences.

- After fixing strings, add non-escape (raw strings) and string interpolation (Python's format strings).

- I assume adding aliases for logical operators would be nice? Or at least uniform the style: I have `and`, `or` and `!`
  . So either turn `!` into `not`, turn `and`/`or` into `&&`/`||`, or keep all 6.

- What about other logical operators? Such as xor, nand, implication etc.?

- Some operators such as `!=`, `>=`, `<=` are syntactic sugar. Implement them with a dedicated instruction.

- Other common operators: `>>`, `<<`, `**`, `++`, `--`, `//`, `+=`, `-=`, `*=`, `/=`, `[]`, `()`.

- Add custom falseness. In order to add operator overloading in general, the best idea is probably to do what Python
  did (special methods), although Ruby uses the operators themselves as method names, which may be confusing in case
  of `()` overloading.

- UTF-8 for strings. *Have*. *Fun*.

- Increase max control flow jump distance. Probably a set of three instructions will slightly improve speed, like for
  constants. My main assumption is that it is going to be roughly the same speed for any number of opcodes below 255.

- I suppose the call frames array in the VM should be dynamic. This implies the stack has to be dynamic first. Is a hard
  limit to call frames really necessary? Python has it, but I don't know if it is done in order to keep the stack as a
  VM member, for speed, or some other reason.

- The stack for gray objects in garbage collection is never reset, it can only increase in size.

- There are various TODO in the code, which do not represent new features, mostly technical stuff that can probably be
  changed locally with no trouble.