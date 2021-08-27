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

typedef void (*ParseFn)();

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

// Template to avoid errors later.
static void expression();

// Template to avoid errors later.
static ParseRule *getRule(TokenType type);

// Template to avoid errors later.
static void parsePrecedence(Precedence precedence);

/**
 * Parse a binary expression. Calls on the rule table to determine how to parse the terms.
 */
static void binary() {

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
 */
static void literal() {

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
 */
static void grouping() {

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
 */
static void number() {

#ifdef DEBUG_PRINT_PARSE_STACK
    PRINT_TABS();
    printToken(&parser.previous);
#endif

    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

/**
 * Compile an unary operator.
 */
static void unary() {

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
        [TOKEN_IDENTIFIER]    = {NULL, NULL, PRECEDENCE_NONE},
        [TOKEN_STRING]        = {NULL, NULL, PRECEDENCE_NONE},
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

    // Actually parse the expression as suited.
    // The produced value will be added to the stack by the interpreter.
    // This concludes our business with the prefix of the expression.
    prefixRule();

    // Parse upwards if the next token is an infix with higher priority.
    // If it is not, I am done with this expression.
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }

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
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    endCompiler();

    return !parser.hadError;
}