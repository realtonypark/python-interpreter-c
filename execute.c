/*execute.c*/

/**
  * @brief Executes nuPython program, given as a Program Graph.
  *
  * @note Tony Park
  */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>  // true, false
#include <string.h>
#include <ctype.h>    // isspace
#include <assert.h>
#include <math.h>

#include "programgraph.h"
#include "ram.h"
#include "execute.h"


//
// Private functions:
//

//
// dupString
//
// Duplicates the given string and returns a pointer
// to the copy.
//
// NOTE: this function allocates memory for the copy,
// the caller takes ownership of the copy and must
// eventually free that memory.
//
static char* dupString(char* s)
{
  assert(s != NULL);

  //
  // be sure to include extra location for null terminator:
  //
  char* copy = (char*)malloc(sizeof(char) * (strlen(s) + 1));
  if (copy == NULL) {
    printf("**EXECUTION ERROR: out of memory (dupString)");
    return NULL;
  }

  strcpy(copy, s);

  return copy;
}


//
// execute_get_value
//
// Given a unary expr, returns the value that it represents.
// This could be the result of a literal (e.g. "123"), or it
// could be the value from memory for an identifier (e.g. "x").
//
// Note that this function can fail --- success or failure is
// returned via the success parameter, which is a pointer to
// where True or False is "returned" to denote success/failure.
//
// Why would it fail? If the identifier does not exist in
// memory. This is a semantic error, and an error message is
// output before returning.
//
// NOTE: this function allocates memory for the value that
// is returned. The caller takes ownership of the copy and
// must eventually free this memory via ram_free_value().
//
static struct RAM_VALUE* execute_get_value(
  struct UNARY_EXPR* unary,
  struct STMT* stmt,
  struct RAM* memory,
  bool* success)
{
  struct RAM_VALUE* value = NULL;

  //
  // we only have simple elements so far (no unary operators):
  //
  assert(unary->expr_type == UNARY_ELEMENT);

  struct ELEMENT* element = unary->element;

  *success = true;  // assume we'll be successful

  if (element->element_type == ELEMENT_IDENTIFIER) {
    //
    // identifier => variable
    //
    assert(element->element_type == ELEMENT_IDENTIFIER);

    char* var_name = element->element_value;

    value = ram_read_cell_by_name(memory, var_name);

    if (value == NULL) {
      printf("**SEMANTIC ERROR: name '%s' is not defined (line %d)\n", var_name, stmt->line);
      *success = false;
      return NULL;
    }
  }
  else {
    //
    // one of the literal types, we need to allocate memory:
    //
    value = malloc(sizeof(struct RAM_VALUE));
    if (value == NULL) {
      printf("**EXECUTION ERROR: out of memory (line %d)\n", stmt->line);
      *success = false;
      return NULL;
    }

    char* literal = element->element_value;

    switch (element->element_type) {
      case ELEMENT_INT_LITERAL:
        value->value_type = RAM_TYPE_INT;
        value->types.i = atoi(literal);
        break;

      case ELEMENT_REAL_LITERAL:
        value->value_type = RAM_TYPE_REAL;
        value->types.d = atof(literal);
        break;

      case ELEMENT_STR_LITERAL:
        value->value_type = RAM_TYPE_STR;
        value->types.s = dupString(literal);
        break;

      case ELEMENT_TRUE:
        value->value_type = RAM_TYPE_BOOLEAN;
        value->types.i = 1;
        break;

      case ELEMENT_FALSE:
        value->value_type = RAM_TYPE_BOOLEAN;
        value->types.i = 0;
        break;

      case ELEMENT_NONE:
        value->value_type = RAM_TYPE_NONE;
        break;

      default:
        printf("**EXECUTION ERROR: unexpected element type in get_element_value (line %d)\n", stmt->line);
        *success = false;
        free(value); // Avoid memory leak on error
        return NULL;
    }
  }//else

  return value;
}


//
// execute_function_call
//
// Executes a function call statement, returning true if
// successful and false if not (an error message will be
// output before false is returned, so the caller doesn't
// need to output anything).
//
// Examples: print()
//           print(x)
//           print(123)
//
static bool execute_function_call(struct STMT* stmt, struct RAM* memory)
{
  struct STMT_FUNCTION_CALL* call = stmt->types.function_call;

  //
  // for now we are assuming it's a call to print:
  //
  char* function_name = call->function_name;

  if (strcmp(function_name, "print") != 0) {
    printf("**SEMANTIC ERROR: unknown function (line %d)\n", stmt->line);
    return false;
  }

  if (call->parameter == NULL)
    printf("\n");
  else {
    //
    // we have a parameter, which type of parameter?
    // Note that a parameter is a simple element, i.e.
    // identifier or literal (or True, False, None):
    //
    char* element_value = call->parameter->element_value;

    if (call->parameter->element_type == ELEMENT_STR_LITERAL) {
      printf("%s\n", element_value);
    }
    else if (call->parameter->element_type == ELEMENT_INT_LITERAL) {
      char* literal = element_value;
      int i = atoi(literal);
      printf("%d\n", i);
    }
    else if (call->parameter->element_type == ELEMENT_REAL_LITERAL) {
      char* literal = element_value;
      double d = atof(literal);
      printf("%lf\n", d);
    }
    else if (call->parameter->element_type == ELEMENT_TRUE) {
      printf("True\n");
    }
    else if (call->parameter->element_type == ELEMENT_FALSE) {
      printf("False\n");
    }
    else if (call->parameter->element_type == ELEMENT_NONE) {
      printf("None\n");
    }
    else {
      //
      // we have an identifer => variable
      //
      assert(call->parameter->element_type == ELEMENT_IDENTIFIER);

      char* var_name = element_value;
      struct RAM_VALUE* value = ram_read_cell_by_name(memory, var_name);

      if (value == NULL) {
        printf("**SEMANTIC ERROR: name '%s' is not defined (line %d)\n", var_name, stmt->line);
        return false;
      }

      if (value->value_type == RAM_TYPE_INT)
        printf("%d\n", value->types.i);
      else if (value->value_type == RAM_TYPE_REAL)
        printf("%lf\n", value->types.d);
      else if (value->value_type == RAM_TYPE_STR)
        printf("%s\n", value->types.s);
      else if (value->value_type == RAM_TYPE_BOOLEAN) {
        if (value->types.i == 1) printf("True\n");
        else printf("False\n");
      }
      else if (value->value_type == RAM_TYPE_NONE)
        printf("None\n");
      else {
        printf("**EXECUTION ERROR: unexpected value type in execute_function_call (line %d)\n", stmt->line);
        ram_free_value(value);
        return false;
      }
      ram_free_value(value);
    }
  }

  return true;
}


//
// execute_binary_int
//
// Helper function to perform binary operations on two integer values.
//
// @param stmt         pointer to the current statement (for error reporting)
// @param left         left operand value
// @param operator_type operator type (e.g. OPERATOR_PLUS)
// @param right        right operand value
// @param result       pointer to store the result RAM_VALUE
// @param success      pointer to success flag
//
static void execute_binary_int(struct STMT* stmt, int left, int operator_type, int right, struct RAM_VALUE* result, bool* success)
{
    result->value_type = RAM_TYPE_INT;
    switch (operator_type) {
      case OPERATOR_PLUS: result->types.i = left + right; break;
      case OPERATOR_MINUS: result->types.i = left - right; break;
      case OPERATOR_ASTERISK: result->types.i = left * right; break;
      case OPERATOR_POWER: result->types.i = (int)pow(left, right); break;
      case OPERATOR_MOD:
        if (right == 0) { printf("**SEMANTIC ERROR: modulus by 0 (line %d)\n", stmt->line); *success = false; }
        else result->types.i = left % right;
        break;
      case OPERATOR_DIV:
        if (right == 0) { printf("**SEMANTIC ERROR: division by 0 (line %d)\n", stmt->line); *success = false; }
        else result->types.i = left / right;
        break;
      case OPERATOR_EQUAL: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left == right); break;
      case OPERATOR_NOT_EQUAL: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left != right); break;
      case OPERATOR_LT: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left < right); break;
      case OPERATOR_LTE: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left <= right); break;
      case OPERATOR_GT: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left > right); break;
      case OPERATOR_GTE: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left >= right); break;
      default:
         printf("**SEMANTIC ERROR: invalid operand types (line %d)\n", stmt->line);
         *success = false;
         break;
    }
}

//
// execute_binary_real
//
// Helper function to perform binary operations on two real values.
//
// @param stmt         pointer to the current statement (for error reporting)
// @param left         left operand value
// @param operator_type operator type (e.g. OPERATOR_PLUS)
// @param right        right operand value
// @param result       pointer to store the result RAM_VALUE
// @param success      pointer to success flag
//
static void execute_binary_real(struct STMT* stmt, double left, int operator_type, double right, struct RAM_VALUE* result, bool* success)
{
    result->value_type = RAM_TYPE_REAL;
    switch (operator_type) {
      case OPERATOR_PLUS: result->types.d = left + right; break;
      case OPERATOR_MINUS: result->types.d = left - right; break;
      case OPERATOR_ASTERISK: result->types.d = left * right; break;
      case OPERATOR_POWER: result->types.d = pow(left, right); break;
      case OPERATOR_MOD:
        if (right == 0.0) { printf("**SEMANTIC ERROR: modulus by 0 (line %d)\n", stmt->line); *success = false; }
        else result->types.d = fmod(left, right);
        break;
      case OPERATOR_DIV:
        if (right == 0.0) { printf("**SEMANTIC ERROR: division by 0 (line %d)\n", stmt->line); *success = false; }
        else result->types.d = left / right;
        break;
      case OPERATOR_EQUAL: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left == right); break;
      case OPERATOR_NOT_EQUAL: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left != right); break;
      case OPERATOR_LT: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left < right); break;
      case OPERATOR_LTE: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left <= right); break;
      case OPERATOR_GT: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left > right); break;
      case OPERATOR_GTE: result->value_type = RAM_TYPE_BOOLEAN; result->types.i = (left >= right); break;
      default:
         printf("**SEMANTIC ERROR: invalid operand types (line %d)\n", stmt->line);
         *success = false;
         break;
    }
}

//
// execute_binary_str
//
// Helper function to perform binary operations on two string values.
// Only concatenation (+) and relational operators are supported.
//
// @param stmt         pointer to the current statement (for error reporting)
// @param left         left operand value
// @param operator_type operator type (e.g. OPERATOR_PLUS)
// @param right        right operand value
// @param result       pointer to store the result RAM_VALUE
// @param success      pointer to success flag
//
static void execute_binary_str(struct STMT* stmt, char* left, int operator_type, char* right, struct RAM_VALUE* result, bool* success)
{
     if (operator_type == OPERATOR_PLUS) {
        result->value_type = RAM_TYPE_STR;
        result->types.s = malloc(strlen(left) + strlen(right) + 1);
        if (result->types.s == NULL) {
            printf("**EXECUTION ERROR: out of memory (string concat)\n");
            *success = false;
        } else {
            strcpy(result->types.s, left);
            strcat(result->types.s, right);
        }
     } else if (operator_type == OPERATOR_EQUAL || operator_type == OPERATOR_NOT_EQUAL ||
                operator_type == OPERATOR_LT || operator_type == OPERATOR_LTE ||
                operator_type == OPERATOR_GT || operator_type == OPERATOR_GTE) {
        int cmp = strcmp(left, right);
        result->value_type = RAM_TYPE_BOOLEAN;
        switch(operator_type) {
            case OPERATOR_EQUAL: result->types.i = (cmp == 0); break;
            case OPERATOR_NOT_EQUAL: result->types.i = (cmp != 0); break;
            case OPERATOR_LT: result->types.i = (cmp < 0); break;
            case OPERATOR_LTE: result->types.i = (cmp <= 0); break;
            case OPERATOR_GT: result->types.i = (cmp > 0); break;
            case OPERATOR_GTE: result->types.i = (cmp >= 0); break;
        }
     } else {
        printf("**SEMANTIC ERROR: invalid operand types (line %d)\n", stmt->line);
        *success = false;
     }
}


//
// execute_binary_expression
//
// Given two values and an operator, performs the operation
// and returns the result.
//
static struct RAM_VALUE* execute_binary_expression(struct STMT* stmt, struct RAM_VALUE* lhs, int operator_type, struct RAM_VALUE* rhs, bool* success)
{
  assert(operator_type != OPERATOR_NO_OP);

  struct RAM_VALUE* result = malloc(sizeof(struct RAM_VALUE));
  if (result == NULL) {
     printf("**EXECUTION ERROR: out of memory (execute_binary_expression)\n");
     *success = false;
     return NULL;
  }

  *success = true;
  result->value_type = RAM_TYPE_NONE;

  if (lhs->value_type == RAM_TYPE_INT && rhs->value_type == RAM_TYPE_INT) {
    execute_binary_int(stmt, lhs->types.i, operator_type, rhs->types.i, result, success);
  }
  else if (lhs->value_type == RAM_TYPE_REAL && rhs->value_type == RAM_TYPE_REAL) {
    execute_binary_real(stmt, lhs->types.d, operator_type, rhs->types.d, result, success);
  }
  else if ((lhs->value_type == RAM_TYPE_INT && rhs->value_type == RAM_TYPE_REAL) ||
           (lhs->value_type == RAM_TYPE_REAL && rhs->value_type == RAM_TYPE_INT)) {
    double left = (lhs->value_type == RAM_TYPE_INT) ? (double)lhs->types.i : lhs->types.d;
    double right = (rhs->value_type == RAM_TYPE_INT) ? (double)rhs->types.i : rhs->types.d;
    execute_binary_real(stmt, left, operator_type, right, result, success);
  }
  else if (lhs->value_type == RAM_TYPE_STR && rhs->value_type == RAM_TYPE_STR) {
    execute_binary_str(stmt, lhs->types.s, operator_type, rhs->types.s, result, success);
  }
  else {
    printf("**SEMANTIC ERROR: invalid operand types (line %d)\n", stmt->line);
    *success = false;
  }

  if (!(*success)) {
      free(result);
      return NULL;
  }
  return result;
}

//
// execute_expr
//
// Executes an expression, returning the value.
//
static struct RAM_VALUE* execute_expr(struct STMT* stmt, struct RAM* memory, struct EXPR* expr, bool* success)
{
    struct RAM_VALUE* lhs_value = execute_get_value(expr->lhs, stmt, memory, success);
    if (!(*success)) return NULL;

    if (expr->isBinaryExpr) {
        struct RAM_VALUE* rhs_value = execute_get_value(expr->rhs, stmt, memory, success);
        if (!(*success)) {
            ram_free_value(lhs_value);
            return NULL;
        }

        struct RAM_VALUE* result = execute_binary_expression(stmt, lhs_value, expr->operator_type, rhs_value, success);

        ram_free_value(lhs_value);
        ram_free_value(rhs_value);

        return result;
    } else {
        return lhs_value;
    }
}

//
// is_valid_zero
//
// Helper to check if a string represents a valid zero (0, 00, -0, 0.0, etc.)
// Used to distinguish between atoi/atof returning 0 for failure vs actual 0.
//
// @param s string to check
// @return true if string represents zero, false otherwise
//
static bool is_valid_zero(char* s)
{
    // Skip whitespace
    while(isspace((unsigned char)*s)) s++;
    // Optional sign
    if (*s == '+' || *s == '-') s++;
    // Must start with 0
    return (*s == '0');
}


//
// execute_builtin_call
//
// Helper to handle built-in function calls (input, int, float)
// that appear on the RHS of an assignment.
//
// @param stmt     pointer to the current statement
// @param memory   pointer to memory
// @param call     pointer to function call struct
// @param success  pointer to success flag
// @return         pointer to RAM_VALUE result (caller must free)
//
static struct RAM_VALUE* execute_builtin_call(struct STMT* stmt, struct RAM* memory, struct FUNCTION_CALL* call, bool* success)
{
    struct RAM_VALUE* result = NULL;
    *success = true;

    if (strcmp(call->function_name, "input") == 0) {
        if (call->parameter != NULL && call->parameter->element_type == ELEMENT_STR_LITERAL) {
            printf("%s", call->parameter->element_value);
        }

        char line[512];
        if (fgets(line, sizeof(line), stdin) != NULL) {
             line[strcspn(line, "\r\n")] = '\0';
             result = malloc(sizeof(struct RAM_VALUE));
             if (result == NULL) {
                 printf("**EXECUTION ERROR: out of memory (input)\n");
                 *success = false;
                 return NULL;
             }
             result->value_type = RAM_TYPE_STR;
             result->types.s = dupString(line);
        } else {
             result = malloc(sizeof(struct RAM_VALUE));
             if (result == NULL) {
                 printf("**EXECUTION ERROR: out of memory (input)\n");
                 *success = false;
                 return NULL;
             }
             result->value_type = RAM_TYPE_STR;
             result->types.s = dupString("");
        }
    }
    else if (strcmp(call->function_name, "int") == 0) {
       struct UNARY_EXPR tmp_unary;
       tmp_unary.expr_type = UNARY_ELEMENT;
       tmp_unary.element = call->parameter;

       struct RAM_VALUE* val = execute_get_value(&tmp_unary, stmt, memory, success);
       if (!(*success)) return NULL;

       if (val->value_type != RAM_TYPE_STR) {
           printf("**SEMANTIC ERROR: conversion failed for int() (line %d)\n", stmt->line);
           ram_free_value(val);
           *success = false;
           return NULL;
       }

       result = malloc(sizeof(struct RAM_VALUE));
       if (result == NULL) {
           printf("**EXECUTION ERROR: out of memory (int conversion)\n");
           ram_free_value(val);
           *success = false;
           return NULL;
       }
       result->value_type = RAM_TYPE_INT;
       result->types.i = atoi(val->types.s);

       if (result->types.i == 0 && !is_valid_zero(val->types.s)) {
           printf("**SEMANTIC ERROR: conversion failed for int() (line %d)\n", stmt->line);
           free(result);
           ram_free_value(val);
           *success = false;
           return NULL;
       }
       ram_free_value(val);
    }
    else if (strcmp(call->function_name, "float") == 0) {
       struct UNARY_EXPR tmp_unary;
       tmp_unary.expr_type = UNARY_ELEMENT;
       tmp_unary.element = call->parameter;

       struct RAM_VALUE* val = execute_get_value(&tmp_unary, stmt, memory, success);
       if (!(*success)) return NULL;

       if (val->value_type != RAM_TYPE_STR) {
           printf("**SEMANTIC ERROR: conversion failed for float() (line %d)\n", stmt->line);
           ram_free_value(val);
           *success = false;
           return NULL;
       }

       result = malloc(sizeof(struct RAM_VALUE));
       if (result == NULL) {
           printf("**EXECUTION ERROR: out of memory (float conversion)\n");
           ram_free_value(val);
           *success = false;
           return NULL;
       }
       result->value_type = RAM_TYPE_REAL;
       result->types.d = atof(val->types.s);

       if (result->types.d == 0.0 && !is_valid_zero(val->types.s)) {
           printf("**SEMANTIC ERROR: conversion failed for float() (line %d)\n", stmt->line);
           free(result);
           ram_free_value(val);
           *success = false;
           return NULL;
       }
       ram_free_value(val);
    }
    else {
         printf("**SEMANTIC ERROR: unknown function (line %d)\n", stmt->line);
         *success = false;
         return NULL;
    }

    return result;
}


//
// execute_assignment
//
// Executes an assignment statement, returning true if
// successful and false if not (an error message will be
// output before false is returned, so the caller doesn't
// need to output anything).
//
// Examples: x = 123
//           y = x ** 2
//
static bool execute_assignment(struct STMT* stmt, struct RAM* memory)
{
  struct STMT_ASSIGNMENT* assign = stmt->types.assignment;
  char* var_name = assign->var_name;
  bool success = true;
  struct RAM_VALUE* result = NULL;

  if (assign->rhs->value_type == VALUE_EXPR) {
    result = execute_expr(stmt, memory, assign->rhs->types.expr, &success);
  }
  else {
    assert(assign->rhs->value_type == VALUE_FUNCTION_CALL);
    struct FUNCTION_CALL* call = assign->rhs->types.function_call;
    result = execute_builtin_call(stmt, memory, call, &success);
  }

  if (!success || result == NULL) return false;

  if (assign->isPtrDeref) {
    struct RAM_VALUE* address = ram_read_cell_by_name(memory, var_name);

    if (address == NULL) {
      printf("**SEMANTIC ERROR: name '%s' is not defined (line %d)\n", var_name, stmt->line);
      ram_free_value(result);
      return false;
    }

    if (address->value_type != RAM_TYPE_INT) {
      printf("**SEMANTIC ERROR: invalid address type for pointer assignment (line %d)\n", stmt->line);
      ram_free_value(result);
      return false;
    }

    int addr = address->types.i;

    success = ram_write_cell_by_addr(memory, *result, addr);
    ram_free_value(result);

    if (!success) {
      printf("**SEMANTIC ERROR: invalid memory address for pointer assignment (line %d)\n", stmt->line);
      return false;
    }

    return true;
  }
  else {
    success = ram_write_cell_by_name(memory, *result, var_name);
    ram_free_value(result);
    return success;
  }
}

//
// execute_until
//
// Helper to execute statements until a stop node is reached.
// Returns true if successful, false if a semantic error occurred.
//
static bool execute_until(struct STMT* program, struct RAM* memory, struct STMT* stop_node)
{
  struct STMT* stmt = program;

  while (stmt != NULL && stmt != stop_node) {

    if (stmt->stmt_type == STMT_ASSIGNMENT) {

      bool success = execute_assignment(stmt, memory);

      if (!success)
        return false;

      stmt = stmt->types.assignment->next_stmt;
    }
    else if (stmt->stmt_type == STMT_FUNCTION_CALL) {

      bool success = execute_function_call(stmt, memory);

      if (!success)
        return false;

      stmt = stmt->types.function_call->next_stmt;
    }
    else if (stmt->stmt_type == STMT_IF_THEN_ELSE) {
        struct STMT_IF_THEN_ELSE* ifstmt = stmt->types.if_then_else;
        bool success;
        struct RAM_VALUE* cond = execute_expr(stmt, memory, ifstmt->condition, &success);

        if (!success) return false;

        bool condition_true = false;
        if (cond->value_type == RAM_TYPE_INT) {
            condition_true = (cond->types.i != 0);
        } else if (cond->value_type == RAM_TYPE_BOOLEAN) {
            condition_true = (cond->types.i != 0);
        } else {
             printf("**SEMANTIC ERROR: invalid condition type (line %d)\n", stmt->line);
             ram_free_value(cond);
             return false;
        }
        ram_free_value(cond);

        if (condition_true) {
            if (ifstmt->true_path != NULL) {
               if (!execute_until(ifstmt->true_path, memory, ifstmt->next_stmt)) return false;
            }
        } else {
            if (ifstmt->false_path != NULL) {
                if (!execute_until(ifstmt->false_path, memory, ifstmt->next_stmt)) return false;
            }
        }

        stmt = ifstmt->next_stmt;
    }
    else if (stmt->stmt_type == STMT_WHILE_LOOP) {
        struct STMT_WHILE_LOOP* loop = stmt->types.while_loop;

        while (true) {
            bool success;
            struct RAM_VALUE* cond = execute_expr(stmt, memory, loop->condition, &success);
            if (!success) return false;

            bool condition_true = false;
            if (cond->value_type == RAM_TYPE_INT || cond->value_type == RAM_TYPE_BOOLEAN) {
                condition_true = (cond->types.i != 0);
            } else {
                 printf("**SEMANTIC ERROR: invalid condition type (line %d)\n", stmt->line);
                 ram_free_value(cond);
                 return false;
            }
            ram_free_value(cond);

            if (!condition_true) break;

            if (loop->loop_body != NULL) {
                if (!execute_until(loop->loop_body, memory, loop->next_stmt)) return false;
            }
        }

        stmt = loop->next_stmt;
    }
    else {
      assert(stmt->stmt_type == STMT_PASS);
      stmt = stmt->types.pass->next_stmt;
    }
  }

  return true;
}


//
// Public functions:
//

/**
  * @brief executes a nuPython program
  *
  * Given a nuPython program graph and a memory,
  * executes the statements in the program graph.
  * If a semantic error occurs (e.g. type error),
  * an error message is output, execution stops,
  * and the function returns.
  *
  * @param program pointer to first stmt in program graph
  * @param memory pointer to memory unit
  * @return void
  */
void execute(struct STMT* program, struct RAM* memory)
{
    (void)execute_until(program, memory, NULL);
}
