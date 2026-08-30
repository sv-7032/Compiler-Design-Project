/*
 * OPERATOR PRECEDENCE PARSER
 * ---------------------------
 * Grammar handled (implicitly, via precedence relations): simple arithmetic
 * expressions built from  id  + - * / ^  ( )  and end marker $
 *
 * The parser does NOT build a parse tree explicitly here — it just performs
 * the shift/reduce steps and prints the stack/input at every step, which is
 * exactly what most college lab exercises + exams ask for.
 *
 * Overall idea (recap):
 *   - We keep a STACK (of terminals) and an INPUT buffer.
 *   - We compare the topmost terminal on the stack with the next input
 *     symbol using a PRECEDENCE TABLE.
 *   - '<'  (yields precedence)  -> SHIFT
 *   - '>'  (takes precedence)   -> REDUCE (pop until we find a '<' below,
 *                                   that popped segment is the "handle")
 *   - '='  (equal precedence)   -> SHIFT (used for matching brackets)
 *   - blank/undefined           -> ERROR
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAX 100

/* Stack to hold terminals (characters) during parsing */
char stack[MAX];
int top = -1;

/* The list of terminal symbols we support, in a fixed order.
 * This fixed order is what lets us index into the 2D precedence table. */
char symbols[] = {'+', '-', '*', '/', '^', '(', ')', 'i', '$'};
#define NUM_SYMBOLS 9

/*
 * PRECEDENCE TABLE
 * Rows = stack top symbol, Columns = incoming input symbol
 * '<' = stack symbol yields precedence to input symbol -> SHIFT
 * '>' = stack symbol takes precedence over input symbol -> REDUCE
 * '=' = equal precedence (only meaningful for '(' vs ')')   -> SHIFT
 * ' ' = blank = invalid combination = ERROR
 *
 * Order of rows/cols:  +  -  *  /  ^  (  )  i  $
 */
char table[NUM_SYMBOLS][NUM_SYMBOLS] = {
    /*        +    -    *    /    ^    (    )    i    $   */
    /* + */ {'>', '>', '<', '<', '<', '<', '>', '<', '>'},
    /* - */ {'>', '>', '<', '<', '<', '<', '>', '<', '>'},
    /* * */ {'>', '>', '>', '>', '<', '<', '>', '<', '>'},
    /* / */ {'>', '>', '>', '>', '<', '<', '>', '<', '>'},
    /* ^ */ {'>', '>', '>', '>', '<', '<', '>', '<', '>'},
    /* ( */ {'<', '<', '<', '<', '<', '<', '=', '<', ' '},
    /* ) */ {'>', '>', '>', '>', '>', ' ', '>', ' ', '>'},
    /* i */ {'>', '>', '>', '>', '>', ' ', '>', ' ', '>'},
    /* $ */ {'<', '<', '<', '<', '<', '<', ' ', '<', '='}
};

/* ----------------------------------------------------------------------
 * FUNCTION: getIndex
 * PURPOSE : Given a terminal character, find its row/column index in the
 *           precedence table (i.e. map a symbol -> array position).
 *           Every identifier ('a', 'b', 'x123', etc.) is treated as the
 *           single terminal class 'i' for table lookup purposes.
 * -------------------------------------------------------------------- */
int getIndex(char symbol) {
    /* 'E' is our non-terminal marker, not an identifier - it should never
     * be looked up in the precedence table directly (callers must skip
     * over it using peekTerminal() below). */
    if (symbol != 'E' && isalpha(symbol)) symbol = 'i';  /* identifiers -> 'i' */
    for (int i = 0; i < NUM_SYMBOLS; i++) {
        if (symbols[i] == symbol) return i;
    }
    return -1;   /* symbol not found -> caller must treat as error */
}

/* ----------------------------------------------------------------------
 * FUNCTION: getPrecedence
 * PURPOSE : Look up the precedence relation between the terminal currently
 *           on top of the stack and the next terminal in the input.
 *           This is the heart of the "which action do I take?" decision.
 * RETURNS : '<' , '>' , '=' , or ' ' (blank/error)
 * -------------------------------------------------------------------- */
char getPrecedence(char stackTop, char input) {
    int row = getIndex(stackTop);
    int col = getIndex(input);
    if (row == -1 || col == -1) return ' ';   /* unknown symbol -> error */
    return table[row][col];
}

/* ----------------------------------------------------------------------
 * FUNCTION: push / pop / peek
 * PURPOSE : Basic stack operations used to maintain the parsing stack.
 * -------------------------------------------------------------------- */
void push(char c) {
    if (top >= MAX - 1) {
        printf("Stack overflow!\n");
        return;
    }
    stack[++top] = c;
}

char pop() {
    if (top < 0) {
        printf("Stack underflow!\n");
        return '\0';
    }
    return stack[top--];
}

char peekTerminal() {
    /* Returns the topmost TERMINAL on the stack, skipping over any
     * non-terminal 'E' markers on the way down. This matters because we
     * DO push 'E' onto the stack after every reduce (to stand in for
     * "some expression already parsed"), but the precedence table is only
     * defined between pairs of terminals - so when comparing with the
     * next input symbol we must look past any 'E' sitting on top. */
    for (int i = top; i >= 0; i--) {
        if (stack[i] != 'E') return stack[i];
    }
    return '$';   /* nothing but E's (or empty stack) -> treat as leading $ */
}

/* ----------------------------------------------------------------------
 * FUNCTION: printStep
 * PURPOSE : Debug/trace helper - prints the current stack contents and
 *           remaining input, so you can see the shift/reduce trace
 *           (this is what most exam answers/lab records expect you to
 *           show step by step).
 * -------------------------------------------------------------------- */
void printStep(const char *input, int ip, const char *action) {
    for (int i = 0; i <= top; i++) printf("%c", stack[i]);
    printf("\t\t%s\t\t%s\n", input + ip, action);
}

/* ----------------------------------------------------------------------
 * FUNCTION: shiftAction
 * PURPOSE : Perform a SHIFT: push the current input symbol onto the stack
 *           and advance the input pointer by one position.
 * -------------------------------------------------------------------- */
int shiftAction(char *input, int ip) {
    push(input[ip]);
    return ip + 1;   /* move input pointer forward */
}

/* ----------------------------------------------------------------------
 * FUNCTION: reduceAction
 * PURPOSE : Perform a REDUCE: pop symbols off the stack until we cross
 *           the "handle" - i.e. keep popping terminals (and the
 *           non-terminals between them) until the relation between the
 *           new stack top and the symbol just popped was '<'.
 *           That whole popped segment is replaced by a single
 *           non-terminal 'E' (representing "some expression"), since we
 *           are not building an explicit parse tree here.
 * -------------------------------------------------------------------- */
void reduceAction() {
    /* We scan down the stack collecting the "handle" (the portion that
     * will be collapsed into a single non-terminal E). Non-terminal 'E'
     * markers already on the stack are popped along for free (they are
     * part of the handle too) but are never used in a precedence
     * comparison, since the table only relates terminals to terminals.
     *
     * lastTerminal remembers the most recent TERMINAL we popped, so we
     * can compare it against the next terminal further down the stack.
     * We stop (leaving that deeper terminal on the stack) as soon as we
     * find a '<' relation - that terminal is where the handle begins. */
    char lastTerminal = '\0';

    while (top >= 0) {
        char topSym = stack[top];

        if (topSym == 'E') {
            pop();               /* non-terminal: part of handle, just pop */
            continue;
        }

        if (lastTerminal == '\0') {
            /* first terminal encountered in this reduce - always part
             * of the handle */
            lastTerminal = topSym;
            pop();
            continue;
        }

        char rel = getPrecedence(topSym, lastTerminal);
        if (rel == '<') {
            /* topSym yields precedence to lastTerminal -> topSym is BELOW
             * the handle, so we stop here without popping it */
            break;
        }

        /* still inside the handle - keep collecting */
        lastTerminal = topSym;
        pop();
    }

    /* Replace the entire reduced handle with a single non-terminal marker */
    push('E');
}

/* ----------------------------------------------------------------------
 * FUNCTION: parseExpression
 * PURPOSE : The main driver loop implementing the operator precedence
 *           parsing algorithm end-to-end:
 *             1. Initialize stack with '$'
 *             2. Repeat:
 *                - compare stack-top terminal with current input symbol
 *                - '<' or '='  -> SHIFT
 *                - '>'         -> REDUCE
 *                - otherwise   -> ERROR
 *             3. Stop when stack is "$ E" and input is just "$"
 *                (this means ACCEPT)
 * -------------------------------------------------------------------- */
void parseExpression(char *input) {
    int ip = 0;                 /* input pointer */
    push('$');                  /* bottom-of-stack marker */

    printf("STACK\t\tINPUT\t\tACTION\n");

    while (1) {
        char stackTop = peekTerminal();
        char currentInput = input[ip];

        /* ACCEPT condition: stack is $E and remaining input is only $ */
        if (stackTop == '$' && currentInput == '$' && top == 1 && stack[0]=='$' && stack[1]=='E') {
            printStep(input, ip, "ACCEPT");
            printf("\nInput successfully parsed!\n");
            return;
        }

        char relation = getPrecedence(stackTop, currentInput);

        if (relation == '<' || relation == '=') {
            ip = shiftAction(input, ip);
            printStep(input, ip, "Shift");
        }
        else if (relation == '>') {
            reduceAction();
            printStep(input, ip, "Reduce");
        }
        else {
            printf("\nParsing error: no relation between '%c' and '%c'\n",
                   stackTop, currentInput);
            return;
        }
    }
}

/* ----------------------------------------------------------------------
 * MAIN
 * PURPOSE : Take an expression from the user, append the end marker '$',
 *           and kick off parsing.
 * -------------------------------------------------------------------- */
int main() {
    char input[MAX];

    printf("Enter expression (identifiers as single letters, e.g. i+i*i): ");
    scanf("%s", input);
    strcat(input, "$");   /* append end-of-input marker */

    parseExpression(input);

    return 0;
}