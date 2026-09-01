#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_STACK 100
#define MAX_STR 20

typedef struct {
    int state;
    char symbol[MAX_STR];
    char val[MAX_STR];
} StackItem;

typedef struct {
    char lhs[MAX_STR];
    char rhs_text[MAX_STR];
    int rhs_len;
} Rule;

typedef struct {
    int state;
    char token[MAX_STR];
    char action[MAX_STR];
} TableEntry;

// Context-Free Grammar Rules
Rule grammar[7] = {
    {"", "", 0},
    {"E", "E + T", 3},  // Rule 1
    {"E", "T", 1},      // Rule 2
    {"T", "T * F", 3},  // Rule 3
    {"T", "F", 1},      // Rule 4
    {"F", "( E )", 3},  // Rule 5
    {"F", "id", 1}      // Rule 6
};

// SLR(1) Parsing Table
TableEntry parseTable[] = {
    {0, "id", "S5"}, {0, "(", "S4"}, {0, "E", "1"}, {0, "T", "2"}, {0, "F", "3"},
    {1, "+", "S6"},  {1, "$", "ACC"},
    {2, "+", "R2"},  {2, "*", "S7"}, {2, ")", "R2"}, {2, "$", "R2"},
    {3, "+", "R4"},  {3, "*", "R4"}, {3, ")", "R4"}, {3, "$", "R4"},
    {4, "id", "S5"}, {4, "(", "S4"}, {4, "E", "8"}, {4, "T", "2"}, {4, "F", "3"},
    {5, "+", "R6"},  {5, "*", "R6"}, {5, ")", "R6"}, {5, "$", "R6"},
    {6, "id", "S5"}, {6, "(", "S4"}, {6, "T", "9"}, {6, "F", "3"},
    {7, "id", "S5"}, {7, "(", "S4"}, {7, "F", "10"},
    {8, "+", "S6"},  {8, ")", "S11"},
    {9, "+", "R1"},  {9, "*", "S7"}, {9, ")", "R1"}, {9, "$", "R1"},
    {10, "+", "R3"}, {10, "*", "R3"}, {10, ")", "R3"}, {10, "$", "R3"},
    {11, "+", "R5"}, {11, "*", "R5"}, {11, ")", "R5"}, {11, "$", "R5"}
};

int tableSize = sizeof(parseTable) / sizeof(parseTable[0]);

const char* getAction(int state, const char* token) {
    for (int i = 0; i < tableSize; i++) {
        if (parseTable[i].state == state && strcmp(parseTable[i].token, token) == 0) {
            return parseTable[i].action;
        }
    }
    return "";
}

void printCFG(void) {
    printf("===========================================\n");
    printf("       CONTEXT-FREE GRAMMAR (CFG)          \n");
    printf("===========================================\n");
    for (int i = 1; i <= 6; i++) {
        printf(" Rule %d: %s -> %s\n", i, grammar[i].lhs, grammar[i].rhs_text);
    }
    printf("===========================================\n\n");
}

int main(void) {
    printCFG();

    // Older version: Hardcoded token list requiring space separation instead of integrated lexer scanner
    char rawTokens[MAX_STACK][MAX_STR] = {"(", "a", "+", "b", ")", "*", "c", "$"};
    char parsedTokens[MAX_STACK][MAX_STR];
    int totalTokens = 8;

    // Convert raw tokens to grammar tokens (mapping alphanumeric to 'id')
    for (int i = 0; i < totalTokens; i++) {
        if (isalnum(rawTokens[i][0]) && strlen(rawTokens[i]) == 1 && 
            rawTokens[i][0] != '+' && rawTokens[i][0] != '*' && 
            rawTokens[i][0] != '(' && rawTokens[i][0] != ')') {
            strcpy(parsedTokens[i], "id");
        } else {
            strcpy(parsedTokens[i], rawTokens[i]);
        }
    }

    StackItem stack[MAX_STACK];
    int top = -1;

    // Push initial State 0
    top++;
    stack[top].state = 0;
    strcpy(stack[top].symbol, "$");
    strcpy(stack[top].val, "");

    int tokenIdx = 0;
    int tempCount = 1;
    int step = 1;

    printf("=== SHIFT-REDUCE PARSING & INTERMEDIATE CODE GENERATION (TAC) ===\n");
    printf("%-5s %-12s %-24s %-20s\n", "Step", "State Stack", "Action", "Generated TAC");
    printf("-----------------------------------------------------------------------\n");

    while (1) {
        int currentState = stack[top].state;
        const char* currentToken = parsedTokens[tokenIdx];

        const char* action = getAction(currentState, currentToken);

        printf("%-5d State %-6d ", step++, currentState);

        if (strlen(action) == 0) {
            printf("ERROR: Syntax Error Detected at token '%s'!\n", rawTokens[tokenIdx]);
            break;
        }

        if (strcmp(action, "ACC") == 0) {
            printf("%-24s %-20s\n", "ACCEPT", "Parsing Completed Successfully!");
            break;
        }

        // --- SHIFT ---
        if (action[0] == 'S') {
            int nextState = atoi(&action[1]);
            char actionText[MAX_STR];
            sprintf(actionText, "Shift %d", nextState);
            printf("%-24s %-20s\n", actionText, "");

            top++;
            stack[top].state = nextState;
            strcpy(stack[top].symbol, currentToken);
            strcpy(stack[top].val, rawTokens[tokenIdx]);

            tokenIdx++;
        }
        // --- REDUCE ---
        else if (action[0] == 'R') {
            int ruleNum = atoi(&action[1]);
            Rule r = grammar[ruleNum];

            StackItem popped[10];
            for (int i = 0; i < r.rhs_len; i++) {
                popped[i] = stack[top];
                top--;
            }

            char generatedTemp[MAX_STR] = "";
            char tacOutput[MAX_STR * 3] = "";

            if (ruleNum == 1) { // E -> E + T
                sprintf(generatedTemp, "t%d", tempCount++);
                sprintf(tacOutput, "%s = %s + %s", generatedTemp, popped[2].val, popped[0].val);
            } else if (ruleNum == 3) { // T -> T * F
                sprintf(generatedTemp, "t%d", tempCount++);
                sprintf(tacOutput, "%s = %s * %s", generatedTemp, popped[2].val, popped[0].val);
            } else if (ruleNum == 5) { // F -> ( E )
                strcpy(generatedTemp, popped[1].val);
            } else { // Unit reductions
                strcpy(generatedTemp, popped[0].val);
            }

            int topState = stack[top].state;
            const char* gotoStr = getAction(topState, r.lhs);

            if (strlen(gotoStr) == 0) {
                printf("ERROR: Invalid GOTO transition for symbol %s in State %d!\n", r.lhs, topState);
                break;
            }

            int gotoState = atoi(gotoStr);

            top++;
            stack[top].state = gotoState;
            strcpy(stack[top].symbol, r.lhs);
            strcpy(stack[top].val, generatedTemp);

            char actionText[MAX_STR * 2];
            sprintf(actionText, "Reduce (R%d: %s->%s)", ruleNum, r.lhs, r.rhs_text);
            printf("%-24s %-20s\n", actionText, tacOutput);
        }
    }

    return 0;
}