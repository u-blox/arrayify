/* Copyright (c) 2019 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

// Things to help with parsing filenames.
#define DIR_SEPARATORS "\\/"
#define EXT_SEPARATOR "."
#define OUTPUT_FILE_EXTENSION "array"
#define LINE_LENGTH 80
#define PREFIX "const char %s[] = "
#define PREFIX_LENGTH 16 // The length not including the %s formatter or terminator
#define POSTFIX "\"\n" // Closing quote and newline
#define POSTFIX_LENGTH 2
#define ENDFIX ";\n\n// End of file\n"
#define ENDFIX_LENGTH 18

// Returns true if the given character must be escaped for inclusion in C code, else false.
static bool escapeRequired(char character)
{
    bool escaped = false;

    switch (character) {
        case 0x07: // Bell
            escaped = true;
            break;
        case 0x08: // Backspace
            escaped = true;
            break;
        case 0x1b: // Escape
            escaped = true;
            break;
        case 0x0c: // page break
            escaped = true;
            break;
        case 0x0a: // newline
            escaped = true;
            break;
        case 0x0d: // carriage return
            escaped = true;
            break;
        case 0x09: // tab
            escaped = true;
            break;
        case 0x0b: // vertical tab
            escaped = true;
            break;
        case 0x5c: // backslash
            escaped = true;
            break;
        case 0x27: // single quote
            escaped = true;
            break;
        case 0x22: // double quote
            escaped = true;
            break;
        case 0x3F: // question mark
            escaped = true;
            break;
        default:
            break;
    }

    return escaped;
}

// Return the escaped character for a given character
static char escapedChar(char character)
{
    char cCharacter = character;

    switch (character) {
        case 0x07: // Bell
            cCharacter = 'a';
            break;
        case 0x08: // Backspace
            cCharacter = 'b';
            break;
        case 0x1b: // Escape
            cCharacter = 'e';
            break;
        case 0x0c: // page break
            cCharacter = 'f';
            break;
        case 0x0a: // newline
            cCharacter = 'n';
            break;
        case 0x0d: // carriage return
            cCharacter = 'r';
            break;
        case 0x09: // tab
            cCharacter = 't';
            break;
        case 0x0b: // vertical tab
            cCharacter = 'v';
            break;
        case 0x5c: // backslash
            cCharacter = '\\';
            break;
        case 0x27: // single quote
            cCharacter = '\'';
            break;
        case 0x22: // double quote
            cCharacter = '\"';
            break;
        case 0x3F: // question mark
            cCharacter = '?';
            break;
        default:
            break;
    }

    return cCharacter;
}

// Print the usage text
static void printUsage(char *pExeName) {
    printf("\n%s: take a text file and create from it a C const char array which can be compiled into code. Usage:\n", pExeName);
    printf("    %s input_file <-n name> <-l line_length> <-o output_file> <-b>\n", pExeName);
    printf("where:\n");
    printf("    input_file is the input text file,\n");
    printf("    -n optionally specifies the name for the array (if not specified input_file, without file extension, will be used),\n");
    printf("    -l optionally specifies the length of each line in the output file (%d by default),\n", LINE_LENGTH);
    printf("    -o optionally specifies the output file (if not specified the output file is input_file with extension %s%s);\n", EXT_SEPARATOR, OUTPUT_FILE_EXTENSION);
    printf("       if the output file exists it will be overwritten,\n");
    printf("    -b bare; if this command-line switch is specified no topping/tailing comment lines will be added to the output.\n");
    printf("For example:\n");
    printf("    %s input.txt -n fred -l 120 -o output.blah -b\n\n", pExeName);
}

// Parse the input file and write to the output file
static int parse(FILE *pInputFile, FILE *pOutputFile, char *pInputFileName, char *pExeFileName, bool bare, char *pName, int lineLength)
{
    char inputBuffer[120];
    int bytesRead;
    int linesWritten = 0;
    bool writeLine = false;
    int addEscaped = 0;
    char *pIn;
    char *pOutputBuffer = (char *) malloc (lineLength + 1); // +1 for newline
    char *pOut = pOutputBuffer;
    int prefixLength = PREFIX_LENGTH + strlen(pName);
    char *pPrefix = (char *) malloc (prefixLength + 1); // +1 for terminator, which sprintf() adds

    if ((pOutputBuffer != NULL) && (pPrefix != NULL)) {
        if (!bare) {
            // Write the header on its own line directly to the output file
            fprintf(pOutputFile, "/* This file was created from input file %s by %s */\n\n", pInputFileName, pExeFileName);
        }
        // Create the prefix
        sprintf(pPrefix, PREFIX, pName);
        // Read text from the input file until we get no more
        while ((bytesRead = fread(inputBuffer, 1, sizeof(inputBuffer), pInputFile)) > 0) {
            pIn = inputBuffer;
            // Process the input buffer
            while (pIn < inputBuffer + bytesRead) {
                // Assemble the output buffer for each input character.
                // If we're in the prefix region, add it or a blank (if we're previously 
                // added the prefix)
                if (pOut - pOutputBuffer < prefixLength) {
                    if (pPrefix != NULL) {
                        *pOut = *(pPrefix + (pOut - pOutputBuffer));
                    } else {
                        *pOut = ' ';
                    }
                    pOut++;
                // If we're at the end of the prefix put in the starting quote
                // and free the prefix buffer in case it's not already freed
                } else if (pOut - pOutputBuffer == prefixLength) {
                    free (pPrefix);
                    pPrefix = NULL;
                    *pOut = '"';
                    pOut++;
                // If there's an escaped charcter to do, write it now
                } else if (addEscaped == 2) {
                    *pOut = '\\';
                    pOut++;
                    addEscaped--;
                } else if (addEscaped == 1) {
                    *pOut = escapedChar(*pIn);
                    pOut++;
                    pIn++;
                    addEscaped--;
                } else {
                    // Process an actual character.
                    // Check whether the current character needs escaping
                    if (escapeRequired(*pIn)) {
                        addEscaped = 2;
                        // If we're already too close to the end of the line to add
                        // this character plus its escape character, then write the
                        // line now
                        if (pOut - pOutputBuffer > lineLength - POSTFIX_LENGTH - 2) {
                            writeLine = true;
                        }
                    } else {
                        *pOut = *pIn;
                        pOut++;
                        pIn++;
                    }
                }
                // If we're now at the line length, write the line and reset parameters
                if ((pOut - pOutputBuffer >= lineLength - POSTFIX_LENGTH) || writeLine) {
                    // Add the post-fix, write the line and reset parameters
                    memcpy(pOut, POSTFIX, POSTFIX_LENGTH);
                    pOut += POSTFIX_LENGTH;
                    if (fwrite(pOutputBuffer, pOut - pOutputBuffer, 1, pOutputFile) == 1) {
                        linesWritten++;
                    }
                    pOut = pOutputBuffer;
                    writeLine = false;
                }
            }
        }
        // Write any characters that might be left in the output buffer at the end
        if (pOut - pOutputBuffer > 0) {
            memcpy(pOut, POSTFIX, POSTFIX_LENGTH);
            pOut += POSTFIX_LENGTH;
            if (fwrite(pOutputBuffer, pOut - pOutputBuffer, 1, pOutputFile) == 1) {
                linesWritten++;
            }
        }
        // If we're done, seek back over the postfix and write the endfix
        if (bare) {
            fseek(pOutputFile, -POSTFIX_LENGTH, SEEK_CUR);
            fwrite(";\n", 2, 1, pOutputFile);
        } else {
            fseek(pOutputFile, -POSTFIX_LENGTH, SEEK_CUR);
            fwrite(ENDFIX, ENDFIX_LENGTH, 1, pOutputFile);
        }
        // Tidy up
        free (pPrefix); // Just in case
        free (pOutputBuffer);
    }

    return linesWritten;
}

// Entry point
int main(int argc, char* argv[])
{
    int retValue = -1;
    bool success = false;
    int x = 0;
    int lineLength = LINE_LENGTH;
    bool bare = false;
    char *pExeName = NULL;
    char *pInputFileName = NULL;
    FILE *pInputFile = NULL;
    char *pOutputFileName = NULL;
    bool outputFileNameMalloced = false;
    char *pVariableName = NULL;
    FILE *pOutputFile = NULL;
    char *pDefaultName = NULL;
    char *pMallocedName = NULL;
    char *pTmp;
    struct stat st = { 0 };

    // Find the exe name in the first argument
    pTmp = strtok(argv[x], DIR_SEPARATORS);
    while (pTmp != NULL) {
        pExeName = pTmp;
        pTmp = strtok(NULL, DIR_SEPARATORS);
    }
    if (pExeName != NULL) {
        // Remove the extension
        pTmp = strtok(pExeName, EXT_SEPARATOR);
        if (pTmp != NULL) {
            pExeName = pTmp;
        }
    }
    x++;

    // Look for all the command line parameters
    while (x < argc) {
        // Test for input filename
        if (x == 1) {
            pInputFileName = argv[x];
        // Test for variable name option
        } else if (strcmp(argv[x], "-n") == 0) {
            x++;
            if (x < argc) {
                pVariableName = argv[x];
            }
        // Test for line length option
        } else if (strcmp(argv[x], "-l") == 0) {
            x++;
            if (x < argc) {
                lineLength = atoi(argv[x]);
            }
        // Test for bare option
        } else if (strcmp(argv[x], "-b") == 0) {
            bare = true;
        // Test for output file option
        } else if (strcmp(argv[x], "-o") == 0) {
            x++;
            if (x < argc) {
                pOutputFileName = argv[x];
            }
        }
        x++;
    }

    // Validate the command-line parameters and create
    // defaults for those unspecified
    if (pInputFileName != NULL) {
        success = true;
        // Open the input file
        pInputFile = fopen (pInputFileName, "r");
        if (pInputFile == NULL) {
            success = false;
            printf("Cannot open input file %s (%s).\n", pInputFileName, strerror(errno));
        } else {
            // Now copy the file name, lopping off the extension and any path
            pMallocedName = (char *) malloc (strlen(pInputFileName) + 1);
            if (pMallocedName != NULL) {
                strcpy(pMallocedName, pInputFileName);
                pDefaultName = pMallocedName;
                pTmp = strtok(pMallocedName, DIR_SEPARATORS);
                while (pTmp != NULL) {
                    pDefaultName = pTmp;
                    pTmp = strtok(NULL, DIR_SEPARATORS);
                }
                pDefaultName = strtok(pDefaultName, EXT_SEPARATOR);
                if (pVariableName == NULL) {
                    // No name specified, so set it to the input
                    // filename without paths and extension
                    pVariableName = pDefaultName;
                }
                // Check the line length: it must be at least the
                // amount of space required to print the prefix (which
                // includes the variable name) and "\x"\n, where x
                // is at least one character from the input, which
                // [may be] escaped
                if ((lineLength < 0) || (lineLength < PREFIX_LENGTH + strlen(pVariableName) + 5)) {
                    printf("Using line length %d as %d is less than the minimum required to print something.\n", PREFIX_LENGTH + strlen(pVariableName) + 5, lineLength);
                    lineLength = PREFIX_LENGTH + strlen(pVariableName) + 5;
                }
                if (pOutputFileName == NULL) {
                    // No output file specified, so set it to the input
                    // filename without path and with the default extension
                    pOutputFileName = (char *) malloc (strlen(pDefaultName) + sizeof(OUTPUT_FILE_EXTENSION) - 1 + sizeof(EXT_SEPARATOR) - 1 + 1);
                    if (pOutputFileName != NULL) {
                        outputFileNameMalloced = true;
                        strcpy(pOutputFileName, pDefaultName);
                        strcat(pOutputFileName, EXT_SEPARATOR);
                        strcat(pOutputFileName, OUTPUT_FILE_EXTENSION);
                    } else {
                        success = false;
                        printf("Cannot allocate memory for output file name.\n");
                    }
                }
            } else {
                success = false;
                printf("Cannot allocate memory for name.\n");
            }
            // Open the output file
            if (pOutputFileName != NULL) {
                pOutputFile = fopen(pOutputFileName, "w");
                if (pOutputFile == NULL) {
                    success = false;
                    printf("Cannot open output file %s (%s).\n", pOutputFileName, strerror(errno));
                }
            }
        }
        if (success) {
            printf("Arrifying file \"%s\", naming array \"%s\", using %d character lines and writing output to \"%s\"%s\n",
                   pInputFileName, pVariableName, lineLength, pOutputFileName, bare ? " bare." : ".\n");
            x = parse(pInputFile, pOutputFile, pInputFileName, pExeName, bare, pVariableName, lineLength);
            printf("Done: %d line(s) written to file.\n", x);
        } else {
            printUsage(pExeName);
        }
    } else {
        printUsage(pExeName);
    }

    if (success) {
        retValue = 0;
    }

    // Clean up
    if (pInputFile != NULL) {
        fclose(pInputFile);
    }
    if (pOutputFile != NULL) {
        fclose(pOutputFile);
    }
    if (pMallocedName != NULL) {
        free(pMallocedName);
    }
    if (outputFileNameMalloced) {
        free(pOutputFileName);
    }

    return retValue;
}