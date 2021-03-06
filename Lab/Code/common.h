#ifndef __COMMON_H__
#define __COMMON_H__

typedef enum { false, true } bool;
#define LEN 32
#define MAXARGC 30

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <ctype.h>
#include "tree.h"
#include "symbol_table.h"
#include "semantic.h"
#include "ir.h"
#include "optimize.h"

int isWrong;

#endif
