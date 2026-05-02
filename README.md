# Python Interpreter in C

A compact interpreter/runtime for a Python subset implemented in C. This project parses a subset of Python syntax into an intermediate program graph, executes that graph against a custom RAM model, and reports semantic/runtime errors with source line information.

This repo is a good example of systems-oriented application engineering: manual memory management, typed runtime values, expression evaluation, control-flow execution, and interpreter-style state management in plain C.

## Technical Specifications

- Recursive-descent parsing pipeline that converts tokenized Python-subset source into an executable program graph.
- Custom intermediate representation for statements, expressions, function calls, branching, and loops.
- Runtime memory model that stores typed values (`int`, `real`, `string`, `boolean`, `none`, and pointer-style addresses).
- Expression evaluator supporting arithmetic, comparisons, mixed numeric operations, string concatenation, and relational string comparisons.
- Control-flow execution for `if` / `elif` / `else`, `while`, `pass`, assignment, dereferenced assignment, and function-call statements.
- Built-in function support for `print`, `input`, `int`, and `float`.
- Semantic error handling for undefined names, invalid operand types, failed conversions, divide-by-zero, and invalid pointer/address writes.

## Architecture

The interpreter is organized as a small execution pipeline:

1. The scanner and parser validate source input and produce a token stream.
2. The token stream is lowered into a program graph IR defined in [programgraph.h](programgraph.h).
3. The execution engine in [execute.c](execute.c) walks that graph and evaluates statements against the RAM abstraction in [ram.h](ram.h).
4. The entrypoint in [main.c](main.c) ties parsing, graph construction, execution, and memory cleanup together.

## Runtime Features

- Typed value handling with explicit runtime tags
- Dynamic string duplication and ownership management
- Expression dispatch by operand type
- Boolean-style condition evaluation for branches and loops
- Pointer-like memory addressing through the RAM layer
- Error-first execution model that stops on semantic failures

## Build And Run

This snapshot builds the executable from `main.c` and `execute.c` and links against included support objects:

- `ram.o`
- `nupython.o`

Build:

```sh
make
```

Run a script:

```sh
./nupython test-expr.py
```

Run interactively:

```sh
./nupython
```

## Example Programs

- [test-expr.py](test-expr.py): expression evaluation
- [test-if.py](test-if.py): conditional control flow
- [test-while.py](test-while.py): loop execution
- [test-funct.py](test-funct.py): function-call behavior

## Systems-level programming

This project focuses on the runtime machinery that makes a language work: parsing input, representing code as executable structures, evaluating expressions, managing program state, and enforcing runtime semantics.
