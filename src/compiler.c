#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

/**
 * Similar to the Scanner, but handles syntax instead of grammar.
 */
typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

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
 * Finish compilation. Last instruction is a return. Returning from top level means the execution terminates (with some
 * exit code).
 */
static void endCompiler() {
    emitReturn();
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
    parser.hadError = false;
    parser.panicMode = false;

    advance();
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
    compilingChunk = chunk;

    return !parser.hadError;
}