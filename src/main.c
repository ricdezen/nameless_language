#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chunk.h"
#include "debug.h"
#include "vm.h"

/**
 * Console interactive interpreter.
 */
static void repl() {
    char line[1024];
    for (;;) {
        printf("> ");

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        interpret(line);
    }
}

/**
 * @param path Path to the file.
 * @return Pointer to a string with the content of the file. Must be freed.
 */
static char *readFile(const char *path) {
    FILE *file = fopen(path, "rb");
    // File was not found or could not be opened.
    if (file == NULL) {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    // Determine file size.
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);

    // Allocate buffer.
    char *buffer = (char *) malloc(fileSize + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    // Read the file.
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }
    buffer[bytesRead] = '\0';

    // Close file handle and return.
    fclose(file);
    return buffer;
}

/**
 * Run a script from a file.
 *
 * @param path The path to the file.
 */
static void runFile(const char *path) {
    char *source = readFile(path);
    InterpretResult result = interpret(source);
    free(source);

    if (result == INTERPRET_COMPILE_ERROR) exit(65);
    if (result == INTERPRET_RUNTIME_ERROR) exit(70);
}

int main(int argc, const char **argv) {
    initVM();

    if (argc == 1) {
        printf("Repl starting: ...\n");
        repl();
    } else if (argc == 2) {
        runFile(argv[1]);
    } else {
        fprintf(stderr, "Usage: nameless [path]\n");
        exit(64);
    }

    freeVM();
    return 0;
}
