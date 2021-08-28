#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

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

// Few templates to avoid errors ---

static void statement();

static void expression();

static void declaration();

static ParseRule *getRule(TokenType type);

static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token *name);

// ---

Parser parser;
Chunk *compilingChunk;

// Just returns the chunk that's currently being compiled.
static Chunk *currentChunk() {
    return compilingChunk;
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
 * Helper function that just outputs a return instruction.
 */
static void emitReturn() {
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
 * Finish compilation. Last instruction is a return. Returning from top level means the execution terminates (with some
 * exit code).
 */
static void endCompiler() {
    emitReturn();

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif

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
    uint8_t arg = identifierConstant(&name);

    // If there's an equal then I am not trying
    // to get the variable's value, but to set it.
    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitTwoBytes(OP_SET_GLOBAL, arg);
    } else {
        emitTwoBytes(OP_GET_GLOBAL, arg);
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
        [TOKEN_LEFT_PAREN]    = {grouping, NULL, PRECEDENCE_NONE},
        [TOKEN_RIGHT_PAREN]   = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_LEFT_BRACE]    = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_RIGHT_BRACE]   = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_COMMA]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_DOT]           = {NULL, NULL, PRECEDENCE_NONE},
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
        [TOKEN_AND]           = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_CLASS]         = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_ELSE]          = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_FALSE]         = {literal, NULL, PRECEDENCE_NONE},
        [TOKEN_FOR]           = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_FUN]           = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_IF]            = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_NIL]           = {literal, NULL, PRECEDENCE_NONE},
        [TOKEN_OR]            = {NULL, NULL, PRECEDENCE_NONE},
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
 * Parse a Variable.
 *
 * @param errorMessage The error in case an identifier is not found.
 * @return
 */
static uint8_t parseVariable(const char *errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

/**
 * Define a global variable.
 *
 * @param global The index of a global variable's name in the constant table.
 */
static void defineVariable(uint8_t global) {
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
 * An expression statement is just an expression followed by a semicolon.
 */
static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
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
 * In place for later.
 */
static void declaration() {

    if (match(TOKEN_VAR)) {
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
    } else {
        expressionStatement();
    }
}

/**
 * Compile source code into a chunk of bytecode.
 *
 * @param source Source code.
 * @param chunk Target chunk.
 * @return true if compilation was successful, false if it was not.
 */
bool compile(const char *source, Chunk *chunk) {
    initScanner(source);
    compilingChunk = chunk;

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();

    return !parser.hadError;
}