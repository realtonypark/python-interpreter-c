# Python Interpreter in C

What does it actually mean to "run" a Python program? Not the abstract answer — the mechanical one. CPython, the reference implementation, is a C program. So is nuPython, a minimal Python interpreter I built from scratch. The difference is scale and completeness. The fundamentals are identical.

The entire interpreter is orchestrated in four lines:

```c
struct TokenQueue* tokens = parser_parse(input);
struct STMT*       program = programgraph_build(tokens);
struct RAM*        memory  = ram_init();
execute(program, memory);
```

Scan characters into tokens. Build a graph from those tokens. Allocate memory. Execute the graph against that memory. Then destroy everything in reverse. Every language runtime — CPython, V8, the JVM — follows this same pipeline. What nuPython makes explicit is what each stage actually is in memory: a struct, a linked list, a union, a heap-allocated array.

<br>
<b>Stage 1: Characters to Tokens</b>

The scanner reads a raw stream of bytes. Its job is to recognize that `while` is not five characters — it's `nuPy_KEYW_WHILE`. This is called tokenization. A **token** is the smallest unit of meaning in a language: a keyword, a literal, an operator, a name.

Every token is represented by this struct:

```c
struct Token {
  int id;    // which token (nuPy_KEYW_WHILE, nuPy_INT_LITERAL, etc.)
  int line;  // source line (1-based)
  int col;   // source column (1-based)
};
```

Three integers. Twelve bytes. The `id` encodes what the token _is_ — one of 81 distinct types ranging from `nuPy_PLUS` to `nuPy_KEYW_PASS`. The `line` and `col` fields encode _where_ it appeared. These survive all the way to runtime: when the interpreter prints `**SEMANTIC ERROR: name 'x' is not defined (line 7)`, that `7` is `token.line`.

The token struct doesn't store a string value — `nuPy_PLUS` is always `+`, there's nothing to store. Only identifiers and literals carry a string payload, and that lives in the token queue node.

<br>
<b>The Token Queue</b>

Tokens are collected into a **TokenQueue**: a singly-linked list where each node holds the token metadata, an optional `char* value` (the identifier's name, the literal's text), and a `next` pointer.

Two operations make parsing possible without committing prematurely. `peekToken()` inspects the front of the queue without consuming it. `peek2Token()` looks one further ahead. This two-token lookahead is necessary because the grammar is ambiguous at one token: `*x` at the start of a line could be a pointer dereference assignment or the beginning of an expression, and you can't tell until the second character. `duplicate()` copies the entire queue for speculation — the parser can attempt a parse on the copy, abandon it on failure, and retry on the original. Classic backtracking without modifying the primary stream.

<br>
<b>Stage 2: Tokens to Program Graph</b>

The parser validates syntax and hands a token queue to `programgraph_build()`, which produces the **program graph** — the structure the execution engine will traverse. This is roughly what other systems call an Abstract Syntax Tree (AST), except statements form a linked list rather than a true tree.

The central design pattern is the **discriminated union** (also called a tagged union). Every statement in nuPython is represented by a single `STMT` struct:

```c
struct STMT {
  int stmt_type;  // enum: ASSIGNMENT, FUNCTION_CALL, IF_THEN_ELSE, WHILE_LOOP, PASS
  int line;
  union {
    struct STMT_ASSIGNMENT*    assignment;
    struct STMT_FUNCTION_CALL* function_call;
    struct STMT_IF_THEN_ELSE*  if_then_else;
    struct STMT_WHILE_LOOP*    while_loop;
    struct STMT_PASS*          pass;
  } types;
};
```

The `stmt_type` integer is the _tag_. Without it, a caller holding a `struct STMT*` has no safe way to know which union member is valid — reading the wrong branch is undefined behavior. The tag is what makes runtime dispatch safe. When the execution engine switches on `stmt_type`, it knows exactly which pointer to dereference.

If/elif/else chains are the most interesting structure. Consider:

```python
if x < 0:
    y = -1
elif x == 0:
    y = 0
else:
    y = 1
```

The `STMT_IF_THEN_ELSE` struct has a `true_path` (statements to run if the condition is true), a `false_path` (what to run otherwise), and a `next_stmt` (where execution resumes after the entire block). The `elif` isn't a special case — it's just another `STMT_IF_THEN_ELSE` living at the `false_path` of the one above it. The chain is recursive. The execution engine handles arbitrary `elif` depth without any special logic: it follows `false_path` until a branch matches or all paths are exhausted.

The `next_stmt` pointer deserves attention. It's the **stop-node** mechanism. When `execute_until(program, memory, stop_node)` is called to run an `if` body, it receives the outermost `next_stmt` as the stop. The recursive call stops there — it doesn't bleed into whatever statement follows the entire if block. No explicit return-address stack. The program graph's pointer structure _is_ the control flow graph.

<br>
<b>Stage 3: RAM — A Custom Memory Abstraction</b>

When Python executes `x = 5`, something has to store the integer 5 and associate it with the name `"x"`. CPython uses a reference-counted object heap. nuPython uses `struct RAM` — a flat, explicitly-managed variable store.

Every value is typed at runtime:

```c
struct RAM_VALUE {
  int value_type;  // INT, REAL, STR, PTR, BOOLEAN, NONE
  union {
    int    i;  // INT, PTR, BOOLEAN
    double d;  // REAL
    char*  s;  // STR
  } types;
};
```

The union means each cell costs `max(sizeof(int), sizeof(double), sizeof(char*))` bytes of payload — 8 bytes on a 64-bit machine — not the sum of all three. Type is stored as a 4-byte tag. The whole value is 16 bytes with padding.

The RAM struct holds an array of these cells plus a map from variable names to cell indices:

```c
struct RAM {
  struct RAM_VALUE* cells;  // array of typed values
  struct RAM_MAP*   map;    // varname → cell index
  int size;
  int capacity;
};
```

There are two read functions: `ram_read_cell_by_name()` does an O(n) scan of `map` to find the index, then O(1) into `cells`. `ram_read_cell_by_addr()` skips the name lookup entirely — O(1) direct. This dual interface is what makes pointer semantics work.

The **pointer** in nuPython is literally an integer array index. `p = &x` stores the cell index of `x` as a `RAM_TYPE_PTR` integer in `p`. Later, `*p = 10` reads that integer from `p` and calls `ram_write_cell_by_addr()` with it. There's no actual C pointer dereference happening — it's two RAM operations: a name lookup returning an integer, then an index-based write.

```python
x = 5
p = &x
*p = 99
print(x)   # prints 99
```

String ownership is explicit throughout. Every `ram_write_cell_by_name()` call duplicates the string — the RAM owns its copy. Every `ram_read_cell_by_name()` call also duplicates the string — the caller owns that copy and must call `ram_free_value()` when done. Conservative, but correct. No use-after-free from a stale pointer into RAM's internal storage.

<br>
<b>Stage 4: Execution</b>

The execution engine is a dispatch loop:

```c
while (stmt != NULL && stmt != stop_node) {
  switch (stmt->stmt_type) {
    case STMT_ASSIGNMENT:    execute_assignment(...);    break;
    case STMT_FUNCTION_CALL: execute_function_call(...); break;
    case STMT_IF_THEN_ELSE:  /* evaluate, recurse */    break;
    case STMT_WHILE_LOOP:    /* evaluate, recurse */    break;
    case STMT_PASS:          /* no-op */                break;
  }
  stmt = get_next_stmt(stmt);
}
```

Expression evaluation is where types collide. The binary operator handler inspects both operands' `value_type` fields and routes to a type-specific function: `execute_binary_int`, `execute_binary_real`, or `execute_binary_str`. Mixed int and real operands are upcasted to `double` before the real handler runs — the same implicit promotion Python applies, made explicit here because C has no implicit promotion for user-defined types.

Division by zero is caught before the operator executes:

```c
if (right == 0) {
  printf("**SEMANTIC ERROR: division by zero (line %d)\n", stmt->line);
  *success = false;
  return;
}
```

No signal handler. No `setjmp`/`longjmp`. No exception. The error propagates up through every caller via `*success = false`. This is the Go-style error-first pattern: explicit, predictable, and zero magic.

String concatenation mallocs a new buffer: `malloc(strlen(left) + strlen(right) + 1)`. The `+1` is the null terminator — a classic C pitfall when forgotten. The result is caller-owned and eventually lands in RAM, where it gets duplicated again.

The four builtins — `print()`, `input()`, `int()`, `float()` — are dispatched by function name string comparison. `input()` always returns `RAM_TYPE_STR`, exactly like Python's `input()`. `int()` uses `atoi()` with a validity check: `atoi` returns 0 both on success with input `"0"` and on failure with non-numeric input, so the interpreter checks whether the input string is actually `"0"` to distinguish them.

```python
x = int(input("Enter a number: "))
y = x * x
print(y)
```

<br>
<b>What Was Left Out (and What That Teaches You)</b>

nuPython supports integer, float, string, boolean, and None types. Assignments, while loops, if/elif/else, and four builtins. No `def`, no `for`, no lists, no dicts, no exceptions.

Each omission is a lesson. No garbage collector means every `malloc` has a visible paired `free`. The ownership contract is explicit — you can trace exactly who allocated and who frees every byte. CPython's reference counting hides this; nuPython forces you to see it. No exception mechanism means every error is a returned `NULL` or `false`. The discipline required to propagate errors correctly through ten call frames is exactly what makes robust C code hard. No operator precedence means `a * b + c` requires `tmp = a * b; result = tmp + c` — which surfaces the fact that precedence in a real parser isn't magic, it's grammar rule hierarchy. No `def` means no call stack, no local scopes, no return mechanism — each of which is a significant addition on its own.

nuPython is about 2,000 lines of C. CPython is 400,000. The delta is features. The fundamentals are the same.
