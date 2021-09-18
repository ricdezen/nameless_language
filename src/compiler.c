#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "memory.h"

#ifdef DEBUG_PRINT_CODE

#include "debug.h"

static int TABS = 0;

static void PRINT_TABS() {
    for (int i = 0; i < TABS; i++)
        printf(" ");
}

#endif

/**
 * Similar to the Scanner, but handles syntax instead of grammar.
 */
typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

/**
 * Precedences for the operators.
 */
typedef enum {
    PRECEDENCE_NONE,
    PRECEDENCE_ASSIGNMENT,  // =
    PRECEDENCE_OR,          // or
    PRECEDENCE_AND,         // and
    PRECEDENCE_EQUALITY,    // == !=
    PRECEDENCE_COMPARISON,  // < > <= >=
    PRECEDENCE_TERM,        // + -
    PRECEDENCE_FACTOR,      // * /
    PRECEDENCE_UNARY,       // ! -
    PRECEDENCE_CALL,        // . ()
    PRECEDENCE_PRIMARY
} Precedence;

/**
 * Just a typedef to build the parse table down below.
 *
 * @param canAssign Whether what was parsed til now is a valid assignment target.
 */
typedef void (*ParseFn)(bool canAssign);

/**
 * Parsing rule. Tells how to compile a prefix operation that begins with such token, how to compile an infix expression
 * whose left operand is followed by a token of that type, and the precedence of an infix expression using the token as
 * an operator.
 */
typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

/**
 * Represents a local variable.
 */
typedef struct {
    Token name;         // The identifier for the variable.
    int depth;          // The scope depth of the variable.
    bool isCaptured;    // Whether this variable is an upvalue somewhere.
} Local;

/**
 * Structure representing an Upvalue.
 */
typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

/**
 * Type of function.
 */
typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

/**
 * Structure representing a Compiler. It keeps track of the local variables, in order to simulate the stack and
 * properly place them in the code. In order to compile a function, each compiler keeps track of it. To compile a script
 * we consider it as implicitly included inside a "main" Function.
 */
typedef struct Compiler {
    struct Compiler *enclosing; // The enclosing Compiler.
    ObjFunction *function;      // The function (or script) being compiled.
    FunctionType type;          // The type of function being compiled (fun or script).
    Local locals[UINT8_COUNT];  // Local variables stack.
    int localCount;             // How many local variables are there.
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;             // The depth of the scope, for local variable scope.
} Compiler;

// Few templates to avoid errors ---

static void statement();

static void expression();

static void declaration();

static ParseRule *getRule(TokenType type);

static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token *name);

// ---

Parser parser;
Compiler *current = NULL;

static Chunk *currentChunk() {
    return &current->function->chunk;
}

/**
 * Print error information.
 *
 * @param token The token that triggered the error.
 * @param message The message explaining the error.
 */
static void errorAt(Token *token, const char *message) {
    if (parser.panicMode)
        return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

/**
 * Just throws an error with last token.
 */
static void error(const char *message) {
    errorAt(&parser.previous, message);
}

/**
 * Same as `error`, with current instead of previous token.
 */
static void errorAtCurrent(const char *message) {
    errorAt(&parser.current, message);
}

/**
 * Scan the next token, report any error.
 */
static void advance() {
    parser.previous = parser.current;

    for (;;) {
        // Scan next Token.
        parser.current = scanToken();

        // Valid token parsed, done.
        if (parser.current.type != TOKEN_ERROR)
            break;

        // Invalid token -> throw error.
        // This is a grammatical error.
        errorAtCurrent(parser.current.start);
    }
}

/**
 * Same as advance, but ensures the next token is of a certain type, and throws an error if it is not.
 */
static void consume(TokenType type, const char *message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    // This is a syntax error.
    errorAtCurrent(message);
}

/**
 * @param type A token type.
 * @return Whether the current token has the given type.
 */
static bool check(TokenType type) {
    return parser.current.type == type;
}

/**
 * @param type A token type.
 * @return if the current token has the given type, advances and returns true, returns false otherwise.
 */
static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

/**
 * Put a byte in a chunk after having parsed some token.
 *
 * @param byte The byte to put in the chunk.
 */
static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

/**
 * Emit two bytes for when we have opcode + param.
 *
 * @param byte1 The instruction.
 * @param byte2 Its parameter.
 */
static void emitTwoBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

/**
 * Jump backwards towards the start of a loop.
 *
 * @param loopStart The start of the loop.
 */
static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->size - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

/**
 * Emit a jump instruction and leave two bytes for the operand.
 *
 * @param instruction The instruction to use for the jump.
 * @return The index in the chunk where the jump argument begins.
 */
static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->size - 2;
}

/**
 * Helper function that just outputs a return instruction.
 */
static void emitReturn() {
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

/**
 * Put a constant into the value array of the current chunk and return its index.
 *
 * @param value The value for the constant.
 * @return The index of the new constant.
 */
static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t) constant;
}

/**
 * Emit a constant into the chunk.
 *
 * @param value The value to emit.
 */
static void emitConstant(Value value) {
    uint8_t constant = makeConstant(value);
    emitTwoBytes(OP_CONSTANT, constant);
}

/**
 * Write the actual jump operand. Writes the current instruction index as the jump operand of the jump instruction
 * with the operand beginning at the given offset.
 *
 * @param offset The offset returned by `emitJump`.
 */
static void patchJump(int offset) {
    // How much to jump from the jump instruction, excluding the two byte operand.
    // Assumes we want to jump "here", where we called this function.
    // -2 to adjust for the bytecode for the jump offset itself.
    int jump = currentChunk()->size - offset - 2;

    if (jump > UINT16_MAX)
        error("Too much code to jump over.");

    // Write the jump offset.
    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

/**
 * Compile and with short-circuit capabilities.
 *
 * @param canAssign Unused.
 */
static void and_(bool canAssign) {
    // The left-hand side has already been parsed.
    // Emit short-circuit jump if the left hand is false.
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    // Right-hand side. If the left-hand was true,
    // the result depends only on the right-hand.
    emitByte(OP_POP);
    parsePrecedence(PRECEDENCE_AND);

    patchJump(endJump);
}

/**
 * Compile or with short-circuit capabilities.
 *
 * @param canAssign Unused.
 */
static void or_(bool canAssign) {
    // TODO the book does it like this to show what kind of flexibility we can expect. Better to add a JUMP_IF_TRUE.
    // If false, avoid the unconditional jump that immediately follows and would skip the right-hand side.
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    // Skip right-hand if left-hand is true.
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);

    emitByte(OP_POP);
    parsePrecedence(PRECEDENCE_OR);

    patchJump(endJump);
}

/**
 * Initialize a compiler and assign it to the global member `compiler`.
 *
 * @param compiler The compiler to initialize.
 * @param type The type of function compiled by this compiler.
 */
static void initCompiler(Compiler *compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;

    compiler->function = newFunction();

    current = compiler;
    // This chunk of code has a name if it is not a script.
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    // Take ownership of the first Local. Give it an empty name to avoid user writing on it.
    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    local->name.start = "";
    local->name.length = 0;
}

/**
 * Finish compilation. Last instruction is a return. Returning from top level means the execution terminates (with some
 * exit code).
 *
 * @return The function that was compiled.
 */
static ObjFunction *endCompiler() {
    emitReturn();
    ObjFunction *function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    // Go back to compiling the outer block.
    current = current->enclosing;

    return function;

}

/**
 * Just compare two tokens.
 *
 * @param a First token.
 * @param b Second token.
 * @return true if they contain the same text, false otherwise.
 */
static bool identifiersEqual(Token *a, Token *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

/**
 * Find the first local corresponding to the given identifier.
 *
 * @param compiler The compiler.
 * @param name The identifier.
 * @return The index of the local or -1 if no local with the name exists.
 */
static int resolveLocal(Compiler *compiler, Token *name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

/**
 * Add a reference to an up-value to the given compiler.
 *
 * @param compiler The compiler.
 * @param index The original index of the up-value in the stack.
 * @param isLocal Whether the value is a local variable or not.
 * @return The index where the up-value will be in the closure's up-value array.
 */
static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;

    return compiler->function->upvalueCount++;
}

/**
 * Find the first up-value (local in an enclosing function) corresponding to the given identifier.
 * If the up-value is found, it is tracked in the function's compiler, so to have a reference to it.
 *
 * @param compiler The compiler.
 * @param name The identifier.
 * @return The up-value index for the up-value or -1 if no up-value with the given name was found.
 */
static int resolveUpvalue(Compiler *compiler, Token *name) {
    if (compiler->enclosing == NULL) return -1;

    // If the upvalue is a local in the enclosing function.
    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t) local, true);
    }

    // If the upvalue is an upvalue in the enclosing function.
    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t) upvalue, false);
    }


    return -1;
}

static void beginScope() {
    current->scopeDepth++;
}

/**
 * Pop all the locals from the stack.
 */
static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        // Close upvalues, and pop the rest of the locals immediately.
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        current->localCount--;
    }
}

/**
 * Parse a binary expression. Calls on the rule table to determine how to parse the terms.
 *
 * @param canAssign Unused.
 */
static void binary(bool canAssign) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printf("BINARY: ");
    printToken(&parser.previous);
#endif

    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence) (rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:
            emitTwoBytes(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
        case TOKEN_GREATER:
            emitByte(OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:
            emitTwoBytes(OP_LESS, OP_NOT);
            break;
        case TOKEN_LESS:
            emitByte(OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            emitTwoBytes(OP_GREATER, OP_NOT);
            break;
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(OP_DIVIDE);
            break;
        default:
            return; // Unreachable.
    }
}

/**
 * Compile a list of expressions separated by commas.
 * TODO this is good inspiration for tuple/list compilation.
 *
 * @return How many arguments the function was called with, used for arity check.
 */
static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

/**
 * Compile a function call.
 *
 * @param canAssign Unused.
 */
static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitTwoBytes(OP_CALL, argCount);
}

/**
 * Compile get and set expressions.
 *
 * @param canAssign Whether the left-hand side can be assigned to.
 */
static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'.");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitTwoBytes(OP_SET_PROPERTY, name);
    } else {
        emitTwoBytes(OP_GET_PROPERTY, name);
    }
}

/**
 * Just parse a literal. A literal just has a corresponding operation in the vm.
 *
 * @param canAssign Unused.
 */
static void literal(bool canAssign) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printf("LITERAL: ");
    printToken(&parser.previous);
#endif

    switch (parser.previous.type) {
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        default:
            return; // Unreachable.
    }
}

/**
 * Parse a grouping.
 *
 * @param canAssign Unused.
 */
static void grouping(bool canAssign) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printf("GROUPING START (\n");
    TABS += 4;
#endif

    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");

#ifdef DEBUG_PRINT_PARSE_STACK
    TABS -= 4;
    PRINT_TABS();
    printf(") GROUPING END\n");
#endif

}

/**
 * Parse a number.
 *
 * @param canAssign Unused.
 */
static void number(bool canAssign) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printToken(&parser.previous);
#endif

    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

/**
 * Parse a string.
 *
 * @param canAssign Unused.
 */
static void string(bool canAssign) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printToken(&parser.previous);
#endif

    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

/**
 * Apparently this will make more sense later.
 */
static void namedVariable(Token name, bool canAssign) {
    // Determine whether the variable is a global or local one.
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        //
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    // If there's an equal then I am not trying
    // to get the variable's value, but to set it.
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitTwoBytes(setOp, (uint8_t) arg);
    } else {
        emitTwoBytes(getOp, (uint8_t) arg);
    }
}

/**
 * Parse a variable's name.
 *
 * @param canAssign Whether an assignment can be made.
 */
static void variable(bool canAssign) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printToken(&parser.previous);
#endif

    namedVariable(parser.previous, canAssign);
}

/**
 * Compile an unary operator.
 *
 * @param canAssign Unused.
 */
static void unary(bool canAssign) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printf("UNARY: ");
    printToken(&parser.previous);
#endif

    TokenType operatorType = parser.previous.type;

    // Compile the operand.
    parsePrecedence(PRECEDENCE_UNARY);

    // Emit the operator instruction.
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;
        default:
            return; // Unreachable.
    }
}

/**
 * Rules for the various tokens.
 */
ParseRule rules[] = {
        [TOKEN_LEFT_PAREN]    = {grouping, call, PRECEDENCE_CALL},
        [TOKEN_RIGHT_PAREN]   = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_LEFT_BRACE]    = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_RIGHT_BRACE]   = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_COMMA]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_DOT]           = {NULL, dot, PRECEDENCE_CALL},
        [TOKEN_MINUS]         = {unary, binary, PRECEDENCE_TERM},
        [TOKEN_PLUS]          = {NULL, binary, PRECEDENCE_TERM},
        [TOKEN_SEMICOLON]     = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_SLASH]         = {NULL, binary, PRECEDENCE_FACTOR},
        [TOKEN_STAR]          = {NULL, binary, PRECEDENCE_FACTOR},
        [TOKEN_BANG]          = {unary, NULL, PRECEDENCE_NONE},
        [TOKEN_BANG_EQUAL]    = {NULL, binary, PRECEDENCE_EQUALITY},
        [TOKEN_EQUAL]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_EQUAL_EQUAL]   = {NULL, binary, PRECEDENCE_EQUALITY},
        [TOKEN_GREATER]       = {NULL, binary, PRECEDENCE_COMPARISON},
        [TOKEN_GREATER_EQUAL] = {NULL, binary, PRECEDENCE_COMPARISON},
        [TOKEN_LESS]          = {NULL, binary, PRECEDENCE_COMPARISON},
        [TOKEN_LESS_EQUAL]    = {NULL, binary, PRECEDENCE_COMPARISON},
        [TOKEN_IDENTIFIER]    = {variable, NULL, PRECEDENCE_NONE},
        [TOKEN_STRING]        = {string, NULL, PRECEDENCE_NONE},
        [TOKEN_NUMBER]        = {number, NULL, PRECEDENCE_NONE},
        [TOKEN_AND]           = {NULL, and_, PRECEDENCE_AND},
        [TOKEN_CLASS]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_ELSE]          = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_FALSE]         = {literal, NULL, PRECEDENCE_NONE},
        [TOKEN_FOR]           = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_FUN]           = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_IF]            = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_NIL]           = {literal, NULL, PRECEDENCE_NONE},
        [TOKEN_OR]            = {NULL, or_, PRECEDENCE_OR},
        [TOKEN_PRINT]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_RETURN]        = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_SUPER]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_THIS]          = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_TRUE]          = {literal, NULL, PRECEDENCE_NONE},
        [TOKEN_VAR]           = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_WHILE]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_ERROR]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_EOF]           = {NULL, NULL, PRECEDENCE_NONE},
};

/**
 * Parses according to precedence.
 *
 * @param precedence The minimum precedence.
 */
static void parsePrecedence(Precedence precedence) {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printf("PRIORITY: %d\n", precedence);
#endif

    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    // Access to a variable should allow a following = only if the context
    // is an assignment. If not so, we would admit `a * b = 1;`.
    bool canAssign = precedence <= PRECEDENCE_ASSIGNMENT;
    // Actually parse the expression as suited.
    // The produced value will be added to the stack by the interpreter.
    // This concludes our business with the prefix of the expression.
    prefixRule(canAssign);

    // Parse upwards if the next token is an infix with higher priority.
    // If it is not, I am done with this expression.
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    // If we got back here and have an equal -> error.
    // If no error happened the equal should have been consumed.
    // This means the left-hand was not a valid assignment target.
    if (canAssign && match(TOKEN_EQUAL))
        error("Invalid assignment target.");

}

/**
 * Get the parsing rule given a type.
 */
static ParseRule *getRule(TokenType type) {
    return &rules[type];
}

/**
 * Compile the expression that follows in the source code.
 */
static void expression() {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printf("EXPRESSION: \n");
    TABS += 4;
#endif

    parsePrecedence(PRECEDENCE_ASSIGNMENT);

#ifdef DEBUG_PRINT_PARSE_STACK
    TABS -= 4;
#endif

}

static void block() {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printf("BLOCK START {\n");
    TABS += 4;
#endif

    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
        declaration();

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");

#ifdef DEBUG_PRINT_PARSE_STACK
    TABS -= 4;
    PRINT_TABS();
    printf("} BLOCK END\n");
#endif

}

/**
 * Declare the name of the variable as a String constant.
 *
 * @param name The token representing the variable's name.
 * @return The index where the constant (the string with the variable's name) is stored.
 */
static uint8_t identifierConstant(Token *name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

/**
 * Keep track of a local variable.
 *
 * @param name The name for the variable.
 */
static void addLocal(Token name) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;          // -1 implies un-initialized state.
    local->isCaptured = false;
}

/**
 * Declare a local variable. Does nothing for globals.
 */
static void declareVariable() {
    if (current->scopeDepth == 0)
        return;

    Token *name = &parser.previous;

    for (int i = current->localCount - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name);
}

/**
 * Parse a Variable.
 *
 * @param errorMessage The error in case an identifier is not found.
 * @return
 */
static uint8_t parseVariable(const char *errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    // You don't look up local variables by name but by index.
    if (current->scopeDepth > 0)
        return 0;

    return identifierConstant(&parser.previous);
}

/**
 * Mark last local variable as initialized. If this is called on a global variable, nothing is done.
 */
static void markInitialized() {
    if (current->scopeDepth == 0)
        return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

/**
 * Define a global variable. Does nothing for locals.
 *
 * @param global The index of a global variable's name in the constant table.
 */
static void defineVariable(uint8_t global) {
    // Don't do anything if the variable is local.
    // This means the value at the top of the stack IS the local variable.
    if (current->scopeDepth > 0) {
        // Set the variable as initialized.
        markInitialized();
        return;
    }

    emitTwoBytes(OP_DEFINE_GLOBAL, global);
}

/**
 * Parse a variable's declaration.
 */
static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

/**
 * Parse a function.
 *
 * @param type The type of function (script or function).
 */
static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    // Parameters.
    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            // TODO move this 255 to a constant.
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters.");
            }
            uint8_t constant = parseVariable("Expect parameter name.");
            defineVariable(constant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");

    // The body of the function.
    block();

    ObjFunction *function = endCompiler();
    emitTwoBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    // Push set of bytes as operands to OP_CLOSURE.
    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

/**
 * Compile a Class.
 */
static void classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable();

    emitTwoBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant);

    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body.");
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
}

/**
 * Parse a function's declaration and assign it to a variable.
 */
static void funDeclaration() {
    uint8_t global = parseVariable("Expect function name.");
    // The function is already initialized, because
    // we can reference it in its body for recursion.
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

/**
 * An expression statement is just an expression followed by a semicolon.
 */
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

/**
 * Parse if statement.
 */
static void ifStatement() {
    // Parse the condition.
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();   // The condition.
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    // Emit instruction to jump over the then branch.
    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);

    // Compile the statement for the then branch.
    statement();

    // Emit instruction to jump over the else branch.
    int elseJump = emitJump(OP_JUMP);

    // Put the offset for jumping over the else.
    patchJump(thenJump);
    emitByte(OP_POP);

    // If there is an else, compile the statement for the else branch.
    if (match(TOKEN_ELSE))
        statement();

    // Put the offset for jumping over the else.
    patchJump(elseJump);
}

/**
 * Compile a print statement.
 */
static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

/**
 * Compile a return statement.
 */
static void returnStatement() {
    // Self-explanatory.
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        // Empty return.
        emitReturn();
    } else {
        // Return something.
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

/**
 * Compile a while statement.
 */
static void whileStatement() {
    // Position of the condition for the backwards jump.
    int loopStart = currentChunk()->size;

    // Condition compilation.
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    // Exit the loop if the condition is false.
    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);   // Pop condition.

    // Body of the loop.
    statement();

    // Go back to the condition.
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);   // Pop condition.
}

/**
 * Compile a for statement.
 */
static void forStatement() {
    // Enclose in a scope because any initializer
    // must be visible in the loop but not outside.
    beginScope();

    // Initializer.
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer.
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    // Each iteration starts at the condition.
    int loopStart = currentChunk()->size;

    // Condition.
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false.
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        // Ignore value of condition (jump only peeks).
        emitByte(OP_POP);
    }

    // Increment statement.
    if (!match(TOKEN_RIGHT_PAREN)) {
        // Don't increment at the first iteration so jump over it the first time.
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->size;
        // Ignore result of expression, we only care about its side effect.
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

        // After incrementing, loop back at the condition.
        emitLoop(loopStart);
        // Jump at the body after the condition.
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    // This is just the body, after which we will jump back at the increment
    // or at the condition if there is no increment.
    statement();
    emitLoop(loopStart);

    // If there is no condition, I need no jump.
    if (exitJump != -1) {
        patchJump(exitJump);
        // Ignore value of condition after jump.
        emitByte(OP_POP);
    }

    endScope();
}

/**
 * Reach the next synchronization point.
 * Currently, this is just the end of the statement.
 */
static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        // Reached statement end, can resume parsing safely.
        if (parser.previous.type == TOKEN_SEMICOLON)
            return;
        // Maybe we never find the end of the previous one, but
        // some tokens clearly indicate a new one must have begun.
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;

            default:; // Do nothing.
        }

        advance();
    }
}

/**
 * Compile a declaration: class, function etc.
 */
static void declaration() {

    if (match(TOKEN_CLASS)) {
        classDeclaration();
    } else if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }

    // If an error has been detected, reach a synchronization point.
    if (parser.panicMode)
        synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

/**
 * Compile source code into a chunk of bytecode.
 *
 * @param source Source code.
 * @return the compiled function if compilation was successful, NULL otherwise if it was not.
 */
ObjFunction *compile(const char *source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction *function = endCompiler();
    return parser.hadError ? NULL : function;
}

/**
 * The only object the compiler needs to hang onto is the function it is compiling.
 */
void markCompilerRoots() {
    Compiler *compiler = current;
    while (compiler != NULL) {
        markObject((Obj *) compiler->function);
        compiler = compiler->enclosing;
    }
}