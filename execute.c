/*execute.c*/

/**
  * @brief Executes nuPython program, given as a Program Graph.
  *
  * @note YOUR NAME
  *
  * @note Starter code and partial solution: Prof. Joe Hummel
  * @note Northwestern University
  */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>  // true, false
#include <string.h>
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
    char* literal = element->element_value;

    value = malloc(sizeof(struct RAM_VALUE));
    if (value == NULL) {
      printf("**EXECUTION ERROR: out of memory (line %d)\n", stmt->line);
      *success = false;
      return NULL;
    }

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

      default:
        printf("**EXECUTION ERROR: unexpected element type in get_element_value (line %d)\n", stmt->line);
        *success = false;
        break;
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
      else {
        printf("**EXECUTION ERROR: unexpected value type in execute_function_call (line %d)\n", stmt->line);
        return false;
      }
    }
  }

  return true;
}


//
// execute_binary_expression
//
// Given two values and an operator, performs the operation
// and returns the result.
//
static int execute_binary_expression(struct STMT* stmt, int lhs, int operator_type, int rhs, bool* success)
{
  assert(operator_type != OPERATOR_NO_OP);

  int result = -1;
  
  *success = true;  // assume success, change if not:

  //
  // perform the operation:
  //
  switch (operator_type)
  {
  case OPERATOR_PLUS:
    result = lhs + rhs;
    break;

  case OPERATOR_MINUS:
    result = lhs - rhs;
    break;

  case OPERATOR_ASTERISK:
    result = lhs * rhs;
    break;

  case OPERATOR_POWER:
    result = (int)pow(lhs, rhs);
    break;

  case OPERATOR_MOD:
    if (rhs == 0) {
      printf("**SEMANTIC ERROR: modulus by 0 (line %d)\n", stmt->line);
      *success = false;
    }
    else {
      result = lhs % rhs;
    }
    break;

  case OPERATOR_DIV:
    if (rhs == 0) {
      printf("**SEMANTIC ERROR: division by 0 (line %d)\n", stmt->line);
      *success = false;
    }
    else {
      result = lhs / rhs;
    }
    break;

  default:
    //
    // did we miss something?
    //
    printf("**INTERNAL ERROR: unexpected operator_type (%d) in execute_binary_expr (line %d)\n", operator_type, stmt->line);
    *success = false;
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

  //
  // we only have expressions on the RHS, no function calls:
  //
  assert(assign->rhs->value_type == VALUE_EXPR);

  struct EXPR* expr = assign->rhs->types.expr;

  //
  // we always have a LHS:
  //
  assert(expr->lhs != NULL);

  bool success;

  struct RAM_VALUE* lhs_value = execute_get_value(expr->lhs, stmt, memory, &success);

  if (!success)  // semantic error? If so, return now:
    return false;

  //
  // do we have a binary expression?
  //
  if (expr->isBinaryExpr) {
    //
    // binary expression such as x + y
    //
    assert(expr->operator_type != OPERATOR_NO_OP);  // we must have an operator

    struct RAM_VALUE* rhs_value = execute_get_value(expr->rhs, stmt, memory, &success);

    if (!success)  // semantic error? If so, return now:
      return false;

    //
    // perform the operation:
    //

    //
    // NOTE: currently only supporting integer binary expressions:
    //
    assert(lhs_value->value_type == RAM_TYPE_INT);
    assert(rhs_value->value_type == RAM_TYPE_INT);

    int result = execute_binary_expression(stmt, lhs_value->types.i, expr->operator_type, rhs_value->types.i, &success);

    if (!success)
      return false;

    //
    // write the result into the lhs_value so it can be written below:
    //
    assert(lhs_value->value_type == RAM_TYPE_INT);
    lhs_value->types.i = result;
  }

  //
  // write the lhs_value to memory. If there was a binary expressions, we
  // assume the result was placed into the lhs_value:
  //
  if (assign->isPtrDeref) {
    //
    // we have *ptr = ...
    //
    struct RAM_VALUE* address = ram_read_cell_by_name(memory, var_name);

    if (address == NULL) {
      printf("**SEMANTIC ERROR: name '%s' is not defined (line %d)\n", var_name, stmt->line);
      return false;
    }
    
    if (address->value_type != RAM_TYPE_INT) {
      printf("**SEMANTIC ERROR: invalid address type for pointer assignment (line %d)\n", stmt->line);
      return false; 
    }

    int addr = address->types.i;

    success = ram_write_cell_by_addr(memory, *lhs_value, addr);

    if (!success) {
      printf("**SEMANTIC ERROR: invalid memory address for pointer assignment (line %d)\n", stmt->line);
      return false;
    }

    return true;
  }
  else {
    //
    // normal assignment x = ...
    //
    success = ram_write_cell_by_name(memory, *lhs_value, var_name);

    return success;
  }
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
  struct STMT* stmt = program;

  //
  // traverse through the program statements:
  //
  while (stmt != NULL) {

    if (stmt->stmt_type == STMT_ASSIGNMENT) {

      bool success = execute_assignment(stmt, memory);

      if (!success)
        return;

      stmt = stmt->types.assignment->next_stmt;  // advance
    }
    else if (stmt->stmt_type == STMT_FUNCTION_CALL) {

      bool success = execute_function_call(stmt, memory);

      if (!success)
        return;

      stmt = stmt->types.function_call->next_stmt;
    }
    else {
      assert(stmt->stmt_type == STMT_PASS);

      //
      // nothing to do!
      //

      stmt = stmt->types.pass->next_stmt;
    }
  }//while

  //
  // done:
  //
  return;
}
