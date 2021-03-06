#include "common.h"

//#define DEBUG

// declaration
struct InterCodeNode* translate_ExtDefList(struct Node* root);
struct InterCodeNode* translate_ExtDef(struct Node* root);
struct InterCodeNode* translate_ExtDecList(struct Node* root);
struct InterCodeNode* translate_FunDec(struct Node* root);
struct InterCodeNode* translate_VarList(struct Node* root);
struct InterCodeNode* translate_ParamDec(struct Node* root);

struct InterCodeNode* translate_Exp(struct Node* root, Operand place);
struct InterCodeNode* translate_Stmt(struct Node* root);
struct InterCodeNode* translate_Cond(struct Node* root, Operand label_true, Operand label_false);
struct InterCodeNode* translate_Args(struct Node* root, struct OperandNode* arg_list);
struct InterCodeNode* translate_Compst(struct Node* root);
struct InterCodeNode* translate_DefList(struct Node* root);
struct InterCodeNode* translate_StmtList(struct Node* root);
struct InterCodeNode* translate_Def(struct Node* root);
struct InterCodeNode* translate_DecList(struct Node* root);
struct InterCodeNode* translate_Dec(struct Node* root);
struct InterCodeNode* translate_VarDec(struct Node* root);

// constant defination
Operand constant0 = NULL;
Operand constant1 = NULL;

// function
Operand newOperand(OperandKind kind, int val) {
	Operand operand = (Operand)malloc(sizeof(struct Operand_));
	operand->kind = kind;
	operand->u.value = val;

	return operand;// return the pointer
}

struct InterCodeNode* newInterCodeNode(OperationKind kind, Operand op1, Operand op2, Operand op3, char* name, int size) {
	struct InterCodeNode* node = (struct InterCodeNode*)malloc(sizeof(struct InterCodeNode));
	node->code.kind = kind;
	
	if(kind == ASSIGN) {
		node->code.u.assign.left = op1;
		node->code.u.assign.right = op2;
	}
	else if(kind == ADD || kind == SUB || kind == MUL || kind == DIVIDE) {
		node->code.u.binop.result = op1;
		node->code.u.binop.op1 = op2;
		node->code.u.binop.op2 = op3;
	}
	else if(kind == LABELOP || kind == GOTO || kind == RETURNOP || kind == READ || kind == WRITE || kind == ARG || kind == PARAM) {
		node->code.u.sigop.op = op1;
	}
	else if(kind == IFOP) {
		node->code.u.ifop.op1 = op1;
		node->code.u.ifop.op2 = op2;
		node->code.u.ifop.label = op3;
		node->code.u.ifop.relop = name;
	}
	else if(kind == FUNCTION) {
		node->code.u.funop.name = name;
	}
	else if(kind == CALL) {
		node->code.u.callop.result = op1;
		node->code.u.callop.name = name;
	}
	else if(kind == DEC) {
		node->code.u.decop.op = op1;
		node->code.u.decop.size = size;
	}

	//node->valid = false;
	node->prev = NULL;
	node->next = NULL;

	return node;
}

Operand new_temp() {
	static int temp_no = 1;
	Operand operand = (Operand)malloc(sizeof(struct Operand_));
	operand->kind = TEMP;
	operand->u.var_no = temp_no;
	temp_no++;
	return operand;
}

Operand new_label() {
	static int label_no = 1;
	Operand operand = (Operand)malloc(sizeof(struct Operand_));
	operand->kind = LABEL;
	operand->u.var_no = label_no;
	label_no++;
	return operand;
}

struct InterCodeNode* concat(int number, ...) {
	va_list p;
	va_start(p, number);

	struct InterCodeNode* head = NULL;
	struct InterCodeNode* tail = NULL;
	int i = 0;
	for(;i<number;i++) {
		struct InterCodeNode* temp = va_arg(p, struct InterCodeNode*);
		if(temp!=NULL) {
			if(head == NULL) {
				head = temp;
				tail = head;
				while(tail->next != NULL) tail = tail->next;
			}
			else {
				tail->next = temp;
				temp->prev = tail;

				tail = temp;
				while(tail->next != NULL) tail = tail->next;
			}
		}
	}

	va_end(p);

	return head;
}

void arg_concat(Operand t, struct OperandNode* arg_list) {
	struct OperandNode* temp = (struct OperandNode*)malloc(sizeof(struct OperandNode));
	temp->op = t;

	// connect
	struct OperandNode* first = arg_list;
	struct OperandNode* second = arg_list->next;

	first->next = temp;
	temp->next = second;
}

Operand clean_temp(struct InterCodeNode* p) {
	int count = 0;
	struct InterCodeNode* temp = p;
	for(;temp!=NULL;temp = temp->next,count++);

	if(count == 1 && p->code.kind == ASSIGN) {
		return p->code.u.assign.right;
	}
	return NULL;//不能换
}

int countStructSize(FieldList p) {
	int size = 0;
	for(;p!=NULL;p = p->tail) {
		if(p->type->kind == _BASIC_) size += 4;
		else if(p->type->kind == _ARRAY_) {
			printf("cannot translate array.\n");
			exit(0);
		}
		else if(p->type->kind == _STRUCTURE_) size += countStructSize(p->type->u.structure);
	}
	return size;
}

int get_id_list(struct Node* exp1, char* id_list[], int size) {
	if(exp1 == NULL)return size;

	if(exp1->rule == Exp__ID) {
		id_list[size] = exp1->child->lexeme;
		size ++;
	}
	else if(exp1->rule == Exp__Exp_DOT_ID) {
		struct Node* e = exp1->child;
		struct Node* id = e->nextSibling->nextSibling;

		size = get_id_list(e, id_list, size);

		id_list[size] = id->lexeme;
		size++;
	}
	return size;
}

int getOffset(char* id_list[], int size) {
	if(size == 0)return 0;

	struct Symbol* sym = lookupVariable(id_list[0]);
	FieldList s = sym->type->u.structure;

	int offset = 0;
	int i =1;
	for(;i<size;i++) {
		for(;s!=NULL;s=s->tail) {
			if(strcmp(s->name, id_list[i])==0)break;
			else if(s->type->kind == _BASIC_)offset += 4;
			else if(s->type->kind ==_ARRAY_){
				printf("cannot translate array\n");
				exit(0);
			}
			else if(s->type->kind == _STRUCTURE_) offset += countStructSize(s->type->u.structure);
		}
		s = s->type->u.structure;
	}
	return offset;
}

/*
 * translate functions
 */
struct InterCodeNode* translate_Exp(struct Node* root, Operand place) {
	if(root == NULL)return NULL;

	switch(root->rule) {
		case Exp__LP_Exp_RP: {
			return translate_Exp(root->child->nextSibling, place);
		}
		case Exp__INT:{
			if(place == NULL) return NULL;

			struct Node* intNode = root->child;
			int value = intNode->value;
			Operand right = newOperand(CONSTANT, value);

			struct InterCodeNode* code = newInterCodeNode(ASSIGN, place, right, NULL, NULL,0);
			
			return code;
		}
		case Exp__ID:{
			if(place == NULL) return NULL;

			struct Node* id = root->child;
			struct Symbol* sym = lookupVariable(id->lexeme);
			
			Operand right = NULL;
			if(sym->type->kind == _STRUCTURE_&&sym->ispointer == false)right = newOperand(ADDRESS, sym->var_no);
			else right = newOperand(VARIABLE, sym->var_no);

			return newInterCodeNode(ASSIGN, place, right, NULL, NULL,0);
		}
		case Exp__Exp_ASSIGNOP_Exp:{
			struct Node* exp1 = root->child;
			struct Node* exp2 = exp1->nextSibling->nextSibling;

			struct InterCodeNode* code = NULL;
			if(exp1->rule == Exp__ID) {
				struct Node* id = exp1->child;
				// variable
				struct Symbol* sym = lookupVariable(id->lexeme);
				Operand var = newOperand(VARIABLE, sym->var_no);

				// code1
				//Operand t1 = new_temp();
				struct InterCodeNode* code1 = translate_Exp(exp2, var);
			
				// code2
				//struct InterCodeNode* c1 = newInterCodeNode(ASSIGN, var, v1, NULL, NULL, 0);
				struct InterCodeNode* code2 = NULL;
				if(place != NULL)code2 = newInterCodeNode(ASSIGN, place, var, NULL, NULL,0);
				//struct InterCodeNode* code2 = concat(2, c1, c2);

				code = concat(2, code1, code2);
			}
			else if(exp1->rule == Exp__Exp_DOT_ID) {
				char* id_list[100];
				int size = get_id_list(exp1, id_list, 0);
				int offset = getOffset(id_list,size);

	            struct Symbol* sym = lookupVariable(id_list[0]);
	            int var_number = sym->var_no;

				Operand address = NULL;
				if(sym->ispointer)address = newOperand(VARIABLE, var_number);
				else address = newOperand(ADDRESS, var_number);
				
				struct InterCodeNode* code1 = NULL;
				Operand temp = new_temp();
				if(offset == 0){
					code1 = newInterCodeNode(ASSIGN, temp, address, NULL, NULL, 0);
				}
				else {
					Operand cst = newOperand(CONSTANT, offset);
					code1 = newInterCodeNode(ADD, temp, address, cst, NULL, 0);
				}

				Operand operand = newOperand(TPOINTER, temp->u.var_no);

				Operand t1 = new_temp();
				struct InterCodeNode* code2 = translate_Exp(exp2, t1);
				//OPTIMIZATION
				Operand v1 = clean_temp(code2);
				if(v1 == NULL) v1 = t1;
				else code2 = NULL;

				struct InterCodeNode* code3 = newInterCodeNode(ASSIGN, operand, v1, NULL, NULL, 0);
				
				struct InterCodeNode* code4 = NULL;
				if(place != NULL)code4 = newInterCodeNode(ASSIGN, place, operand, NULL, NULL, 0);
				code = concat(4,code1, code2, code3, code4);
			}
			else {
				printf("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.\n");
				exit(0);
			}
			return code;
		}
		case Exp__Exp_PLUS_Exp:
		case Exp__Exp_MINUS_Exp:
		case Exp__Exp_STAR_Exp:
		case Exp__Exp_DIV_Exp:{
			if(place == NULL)return NULL;

			struct Node* exp1 = root->child;
			struct Node* exp2 = exp1->nextSibling->nextSibling;

			Operand t1 = new_temp();
			Operand t2 = new_temp();
			struct InterCodeNode* code1 = translate_Exp(exp1, t1);
			struct InterCodeNode* code2 = translate_Exp(exp2, t2);
			
			// OPTIMIZATION
			Operand v1 = clean_temp(code1);
			Operand v2 = clean_temp(code2);

			if(v1 == NULL) v1 = t1;
			else code1 = NULL;

			if(v2 == NULL) v2 = t2;
			else code2 = NULL;

			struct InterCodeNode* code3 = NULL;
			if(root->rule == Exp__Exp_PLUS_Exp)code3 = newInterCodeNode(ADD, place, v1, v2, NULL,0);
			else if(root->rule == Exp__Exp_MINUS_Exp)code3 = newInterCodeNode(SUB, place, v1, v2, NULL,0);
			else if(root->rule == Exp__Exp_STAR_Exp)code3 = newInterCodeNode(MUL, place, v1, v2, NULL,0);

			else code3 = newInterCodeNode(DIVIDE, place, v1, v2, NULL,0);
#ifdef DEBUG
	//printf("!!!!!!!exp binop\n");
	//outputIR(code1);
	//outputIR(code2);
#endif
			return concat(3, code1, code2, code3);
		}
		case Exp__MINUS_Exp:{
			if(place == NULL) return NULL;
			struct Node* exp1 = root->child->nextSibling;

			Operand t1 = new_temp();
			struct InterCodeNode* code1 = translate_Exp(exp1, t1);	

			// OPTIMIZATION
			Operand v1 = clean_temp(code1);
			if(v1 == NULL) v1 = t1;
			else code1 = NULL;

			struct InterCodeNode* code2 = newInterCodeNode(SUB, place, constant0, v1, NULL,0);

			return concat(2, code1, code2);
		}
		case Exp__Exp_RELOP_Exp:
		case Exp__NOT_Exp:
		case Exp__Exp_AND_Exp:
		case Exp__Exp_OR_Exp:{
			if(place == NULL)return NULL;

			Operand label1 = new_label();
			Operand label2 = new_label();

			struct InterCodeNode* code0 = newInterCodeNode(ASSIGN, place, constant0, NULL, NULL,0);
			struct InterCodeNode* code1 = translate_Cond(root, label1, label2);
			struct InterCodeNode* c1 = newInterCodeNode(LABELOP, label1, NULL, NULL, NULL,0);
			
			struct InterCodeNode* c2 = newInterCodeNode(ASSIGN, place, constant1, NULL, NULL,0);
		
			struct InterCodeNode* code2 = concat(2, c1, c2);
			struct InterCodeNode* code3 = newInterCodeNode(LABELOP, label2, NULL, NULL, NULL,0);

			return concat(4, code0 ,code1, code2, code3);
		}
		case Exp__ID_LP_RP:{
			if(place == NULL)place = new_temp();

			struct Node* id = root->child;
			char* function = id->lexeme;

			struct InterCodeNode* c1 = newInterCodeNode(READ, place, NULL, NULL, NULL,0);
			struct InterCodeNode* c2 = newInterCodeNode(CALL, place, NULL, NULL, function,0);

			if(strcmp(function, "read")==0) return c1;
			else return c2;
		}
		case Exp__ID_LP_Args_RP: {
			if(place == NULL)place = new_temp();

			struct Node* id = root->child;
			struct Node* args = id->nextSibling->nextSibling;

			char* function = id->lexeme;

			struct OperandNode* arg_list = (struct OperandNode*)malloc(sizeof(struct OperandNode));
			arg_list->op = NULL;
			arg_list->next = NULL;
			
			struct InterCodeNode* code1 = translate_Args(args, arg_list);

			// write
			struct InterCodeNode* c1 = newInterCodeNode(WRITE, arg_list->next->op, NULL, NULL, NULL,0);
			if(strcmp(function, "write")==0) return concat(2, code1, c1);
			
			// other functions	
			struct InterCodeNode* code2 = NULL;
			struct OperandNode* cur = arg_list->next;
			for(;cur != NULL; cur = cur->next) {
				struct InterCodeNode* c = newInterCodeNode(ARG, cur->op, NULL, NULL, NULL, 0);
				code2 = concat(2,code2, c);
			}
			struct InterCodeNode* c2 = newInterCodeNode(CALL, place, NULL, NULL, function,0);
			return concat(3, code1, code2, c2);
		}
		case Exp__Exp_LB_Exp_RB: {
			printf("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.\n");
			exit(0);
		}
		case Exp__Exp_DOT_ID: {
			if(place == NULL) return NULL;

			char* id_list[100];
			int size = get_id_list(root, id_list, 0);
			int offset = getOffset(id_list,size);
			
			struct Symbol* sym = lookupVariable(id_list[0]);
			int var_number = sym->var_no;
			
			struct InterCodeNode* code = NULL;
			if(offset == 0){
				Operand op = NULL;
				if(sym->ispointer) op = newOperand(VPOINTER, var_number);
				else op = newOperand(VARIABLE, var_number);
				code = newInterCodeNode(ASSIGN, place, op, NULL, NULL, 0);
			}
			else {
				Operand temp = new_temp();
				Operand var = NULL; 
				if(sym->ispointer)var = newOperand(VARIABLE, var_number);
				else var = newOperand(ADDRESS, var_number);
				Operand cst = newOperand(CONSTANT, offset);
				struct InterCodeNode* code1 = newInterCodeNode(ADD, temp, var, cst, NULL, 0);

				Operand op = newOperand(TPOINTER, temp->u.var_no);
				struct InterCodeNode* code2 = newInterCodeNode(ASSIGN, place, op, NULL, NULL, 0);
				code = concat(2, code1, code2);
			}
			
			return code;
		}
		default: {
			printf("UNK EXP!\n");
			exit(0);
		}
	}
}

struct InterCodeNode* translate_Stmt(struct Node* root) {
	if(root == NULL)return NULL;
	switch(root->rule) {
		case Stmt__Exp_SEMI:{
			return translate_Exp(root->child, NULL);
		}
		case Stmt__Compst: {
			return translate_Compst(root->child);
		}
		case Stmt__RETURN_Exp_SEMI: {
			struct Node* exp = root->child->nextSibling;

			Operand t1 = new_temp();
			struct InterCodeNode* code1 = translate_Exp(exp, t1);

			//OPTIMIZATION
			Operand v1 = clean_temp(code1);
			if(v1 == NULL) v1 = t1;
			else code1 = NULL;

			struct InterCodeNode* code2 = newInterCodeNode(RETURNOP, v1, NULL, NULL ,NULL,0);
			return concat(2, code1, code2);
		}
		case Stmt__IF_LP_Exp_RP_Stmt: {
			struct Node* exp = root->child->nextSibling->nextSibling;
			struct Node* stmt1 = exp->nextSibling->nextSibling;

			Operand label1 = new_label();
			Operand label2 = new_label();
			struct InterCodeNode* code1 = translate_Cond(exp, label1, label2);
			struct InterCodeNode* code2 = translate_Stmt(stmt1);
			struct InterCodeNode* c1 = newInterCodeNode(LABELOP, label1, NULL, NULL, NULL,0);
			struct InterCodeNode* c2 = newInterCodeNode(LABELOP, label2, NULL, NULL, NULL,0);

			return concat(4, code1, c1, code2, c2);
		}
		case Stmt__IF_LP_Exp_RP_Stmt_ELSE_Stmt: {
			struct Node* exp = root->child->nextSibling->nextSibling;
			struct Node* stmt1 = exp->nextSibling->nextSibling;
			struct Node* stmt2 = stmt1->nextSibling->nextSibling;

			Operand label1 = new_label();
			Operand label2 = new_label();
			Operand label3 = new_label();

			struct InterCodeNode* code1 = translate_Cond(exp, label1, label2);
			struct InterCodeNode* code2 = translate_Stmt(stmt1);
			struct InterCodeNode* code3 = translate_Stmt(stmt2);

			struct InterCodeNode* c1 = newInterCodeNode(LABELOP, label1, NULL, NULL, NULL,0);
			struct InterCodeNode* c2 = newInterCodeNode(GOTO, label3, NULL, NULL, NULL,0);
			struct InterCodeNode* c3 = newInterCodeNode(LABELOP, label2, NULL, NULL, NULL,0);
			struct InterCodeNode* c4 = newInterCodeNode(LABELOP, label3, NULL, NULL, NULL,0);

			struct InterCodeNode* code = concat(7, code1, c1, code2, c2, c3, code3, c4);
#ifdef DEBUG
	//printf("---if else:\n");
	//outputIR(code);
	//outputIR(c1);
	//outputIR(code2);
	//outputIR(c2);
	//outputIR(c3);
	//outputIR(code3);
	//exit(0);
#endif
			return code;
			//return NULL;
		}
		case Stmt__WHILE_LP_Exp_RP_Stmt: {
			struct Node* exp = root->child->nextSibling->nextSibling;
			struct Node* stmt1 = exp->nextSibling->nextSibling;

			Operand label1 = new_label();
			Operand label2 = new_label();
			Operand label3 = new_label();

			struct InterCodeNode* code1 = translate_Cond(exp, label2, label3);
			struct InterCodeNode* code2 = translate_Stmt(stmt1);
			
			struct InterCodeNode* c1 = newInterCodeNode(LABELOP, label1, NULL, NULL, NULL,0);
			struct InterCodeNode* c2 = newInterCodeNode(LABELOP, label2, NULL, NULL, NULL,0);
			struct InterCodeNode* c3 = newInterCodeNode(GOTO, label1, NULL, NULL, NULL,0);
			struct InterCodeNode* c4 = newInterCodeNode(LABELOP, label3, NULL, NULL, NULL,0);
			struct InterCodeNode* code = concat(6 ,c1, code1, c2, code2, c3, c4);
			return code;
			}
		default: {
					 printf("STMT UNK!\n");
					 exit(0);
				 }
	}
}

struct InterCodeNode* translate_Cond(struct Node* root, Operand label_true, Operand label_false) {
	if(root == NULL)return NULL;
	switch(root->rule) {
		case Exp__INT:{ // optimization
			struct Node* intNode = root->child;
			
			if(intNode->value == 1) {
				return newInterCodeNode(GOTO, label_true, NULL, NULL, NULL,0);
			}
			else return newInterCodeNode(GOTO, label_false, NULL, NULL, NULL, 0);
		}
		case Exp__Exp_RELOP_Exp:{
			struct Node* exp1 = root->child;
			struct Node* relop = exp1->nextSibling;
			struct Node* exp2 = relop->nextSibling;

			Operand t1 = new_temp();
			Operand t2 = new_temp();
			struct InterCodeNode* code1 = translate_Exp(exp1, t1);
			struct InterCodeNode* code2 = translate_Exp(exp2, t2);

			//OPTIMIZATION
			Operand v1 = clean_temp(code1);
			Operand v2 = clean_temp(code2);

			if(v1 == NULL) v1 = t1;
			else code1 = NULL;

			if(v2 == NULL) v2 = t2;
			else code2 = NULL;
#ifdef DEBUG
	//printf("~~~exp_relop_exp\n");
	//outputIR(code1);
	//outputIR(code2);
	//exit(0);
#endif
			char* op = relop->lexeme;
			struct InterCodeNode* code3 = newInterCodeNode(IFOP, v1, v2, label_true, op,0);
			struct InterCodeNode* code4 = newInterCodeNode(GOTO, label_false, NULL, NULL ,NULL,0);

			struct InterCodeNode* code = concat(4, code1, code2, code3, code4);

#ifdef DEBUG
	//printf("--------\n");
	//outputIR(code);
	//exit(0);
#endif
			return code;
		}
		case Exp__NOT_Exp:{
			return translate_Cond(root->child->nextSibling, label_false, label_true);
		}
		case Exp__Exp_AND_Exp:{
			struct Node* exp1 = root->child;
			struct Node* exp2 = exp1->nextSibling->nextSibling;

			Operand label1 = new_label();
			struct InterCodeNode* code1 = translate_Cond(exp1, label1, label_false);
			struct InterCodeNode* code2 = translate_Cond(exp2, label_true, label_false);
			struct InterCodeNode* c = newInterCodeNode(LABELOP, label1, NULL, NULL, NULL,0);
			return concat(3, code1, c, code2);
		}
		case Exp__Exp_OR_Exp:{
			struct Node* exp1 = root->child;
			struct Node* exp2 = exp1->nextSibling->nextSibling;

			Operand label1 = new_label();
			struct InterCodeNode* code1 = translate_Cond(exp1, label_true, label1);
			struct InterCodeNode* code2 = translate_Cond(exp2, label_true, label_false);
			struct InterCodeNode* c = newInterCodeNode(LABELOP, label1, NULL, NULL, NULL,0);
			return concat(3, code1, c, code2);
		}
		default: {
					Operand t1 = new_temp();
					struct InterCodeNode* code1 = translate_Exp(root, t1);

					// OPTIMIZATION
					Operand v1 = clean_temp(code1);
					if(v1 == NULL) v1 = t1;
					else code1 = NULL;

					struct InterCodeNode* code2 = newInterCodeNode(IFOP, v1, constant0, label_true, "!=",0);
					struct InterCodeNode* code3 = newInterCodeNode(GOTO, label_false, NULL, NULL, NULL,0);

					return concat(3, code1, code2, code3);
				 }
	}
}

struct InterCodeNode* translate_Args(struct Node* root, struct OperandNode* arg_list) {
	if(root == NULL)return NULL;
	switch(root->rule) {
		case Args__Exp: {
			struct Node* exp = root->child;

			Operand t1 = new_temp();
			struct InterCodeNode* code1 = translate_Exp(exp, t1);

			//OPTIMIZATION
			Operand v1 = clean_temp(code1);
			if(v1 == NULL) v1 = t1;
			else code1 = NULL;
			
			arg_concat(v1, arg_list);
			return code1;
		}
		case Args__Exp_COMMA_Args: {
			struct Node* exp = root->child;
			struct Node* args1 = exp->nextSibling->nextSibling;

			Operand t1 = new_temp();
			struct InterCodeNode* code1 = translate_Exp(exp, t1);
			
			//OPTIMIZATION
			Operand v1 = clean_temp(code1);
			if(v1 == NULL) v1 = t1;
			else code1 = NULL;
			
			arg_concat(v1, arg_list);

			struct InterCodeNode* code2 = translate_Args(args1, arg_list);

			return concat(2, code1, code2);
		}
	}
}

struct InterCodeNode* translate_Compst(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* cur = root->child->nextSibling;
	
	struct InterCodeNode* code = NULL;
	if(cur != NULL && strcmp(cur->token, "DefList") == 0) {
		struct InterCodeNode* code1= translate_DefList(cur);
		code = concat(2, code, code1);
		cur = cur->nextSibling;
	}
	if(cur != NULL && strcmp(cur->token, "StmtList") == 0) {
		struct InterCodeNode* code2 = translate_StmtList(cur);
		code = concat(2, code, code2);
	}

#ifdef DEBUG
	//printf("translate_Compst\n");
	//outputIR(code);
#endif
	return code;
}

struct InterCodeNode* translate_DefList(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* def = root->child;
	struct Node* deflist = def->nextSibling;

	struct InterCodeNode* code1 = translate_Def(def);
	struct InterCodeNode* code2 = translate_DefList(deflist);

	return concat(2, code1, code2);
}

struct InterCodeNode* translate_StmtList(struct Node* root) {
	if(root == NULL)return NULL;

	struct Node* stmt = root->child;
	struct Node* stmtlist = stmt->nextSibling;

	struct InterCodeNode* code1 = translate_Stmt(stmt);

#ifdef DEBUG
	//printf("$$$$stmt:$$$$\n");
	//outputIR(code1);
#endif

	struct InterCodeNode* code2 = translate_StmtList(stmtlist);

	struct InterCodeNode* code = concat(2, code1, code2);

	return code;
}

struct InterCodeNode* translate_Def(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* specifier = root->child;
	struct Node* declist = specifier->nextSibling;

	// specifier ........
	
	struct InterCodeNode* code2 = translate_DecList(declist);
	return code2;
}

struct InterCodeNode* translate_DecList(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* dec = root->child;
	struct InterCodeNode* code = translate_Dec(dec);

#ifdef DEBUG
	//printf("/////declist:\n");
	//outputIR(code);
#endif

	if(root->rule == DecList__Dec_COMMA_DecList) {
		struct Node* declist = dec->nextSibling->nextSibling;
		struct InterCodeNode* code2 = translate_DecList(declist);
		code = concat(2, code, code2);
	}

	return code;
}

struct InterCodeNode* translate_Dec(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* vardec = root->child;
	struct InterCodeNode* code = translate_VarDec(vardec);

	struct Symbol* sym = lookupVariable(vardec->lexeme);
	Operand place = newOperand(VARIABLE, sym->var_no);

	if(sym->type->kind == _STRUCTURE_){
		FieldList structure = sym->type->u.structure;
		int size = countStructSize(structure);
		struct InterCodeNode* code1 = newInterCodeNode(DEC, place, NULL, NULL, NULL, size);
		code = concat(2, code, code1);
	}
	else if(root->rule == Dec__VarDec_ASSIGNOP_Exp) {
		struct Node* exp = vardec->nextSibling->nextSibling;
		struct InterCodeNode* code2 = translate_Exp(exp, place);
		code = concat(2, code, code2);
	}

	return code;
}

struct InterCodeNode* translate_VarDec(struct Node* root) {
	if(root == NULL)return NULL;

	if(root->rule == VarDec__VarDec_LB_INT_RB) {
		printf("Cannot translate: Code contains variables of multi-dimensional array type or parameters of array type.\n");
		//return NULL;
		exit(0);
	}
	
	struct Node* id = root->child;
	root->lexeme = id->lexeme;

	return NULL;
}

struct InterCodeNode* translate_ExtDefList(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* extdef = root->child;
	struct Node* extdeflist = extdef->nextSibling;

	struct InterCodeNode* code1 = translate_ExtDef(extdef);

#ifdef DEBUG
	//printf(",,,,,,,extdeflist:\n");
	//outputIR(code1);
#endif

	struct InterCodeNode* code2 = translate_ExtDefList(extdeflist);

	return concat(2, code1, code2);
}

struct InterCodeNode* translate_ExtDef(struct Node* root) {
	if(root == NULL)return NULL;
	switch(root->rule) {
		case ExtDef__Specifier_ExtDecList_SEMI: {
			struct Node* extdeclist = root->child->nextSibling;

			struct InterCodeNode* code = translate_ExtDecList(extdeclist);
			return code;
		}
		case ExtDef__Specifier_FunDec_Compst: {
			struct Node* fundec = root->child->nextSibling;
			struct Node* compst = fundec->nextSibling;

			struct InterCodeNode* code1 = translate_FunDec(fundec);
			struct InterCodeNode* code2 = translate_Compst(compst);

			return concat(2, code1, code2);
		}
		default: return NULL;
	}
}

struct InterCodeNode* translate_ExtDecList(struct Node* root) {
	if(root == NULL)return NULL;

	struct Node* vardec = root->child;
	struct InterCodeNode* code = translate_VarDec(vardec);

	if(root->rule == ExtDecList__VarDec_COMMA_ExtDecList) {
		struct Node* extdeclist = vardec->nextSibling->nextSibling;
		struct InterCodeNode* code2 = translate_ExtDecList(extdeclist);

		code = concat(2, code, code2);
	}
	return code;
}

struct InterCodeNode* translate_FunDec(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* id = root->child;
	char* function = id->lexeme;

	struct InterCodeNode* code1 = newInterCodeNode(FUNCTION, NULL, NULL, NULL, function,0);

	if(root->rule == FunDec__ID_LP_VarList_RP) {
		struct Node* varlist = id->nextSibling->nextSibling;
		
		struct InterCodeNode* code2 = translate_VarList(varlist);

		code1 = concat(2, code1, code2);
	}
#ifdef DEBUG
	//printf("^^^^^^function name:\n");
	//outputIR(code1);
#endif
	
	return code1;
}

struct InterCodeNode* translate_VarList(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* paramdec = root->child;

	struct InterCodeNode* code = translate_ParamDec(paramdec);

	if(root->rule == VarList__ParamDec_COMMA_VarList) {
		struct Node* varlist = paramdec->nextSibling->nextSibling;
		struct InterCodeNode* code2 = translate_VarList(varlist);

		code = concat(2, code, code2);
	}
#ifdef DEBUG
	//printf("******varlist:\n");
	//outputIR(code);
#endif
	return code;
}

struct InterCodeNode* translate_ParamDec(struct Node* root) {
	if(root == NULL)return NULL;
	struct Node* specifier = root->child;
	struct Node* vardec = specifier->nextSibling;

	struct InterCodeNode* code1 = translate_VarDec(vardec);

	struct Symbol* sym = lookupVariable(vardec->lexeme);
	Operand var = newOperand(VARIABLE, sym->var_no);
	struct InterCodeNode* code2 = newInterCodeNode(PARAM, var, NULL, NULL, NULL,0);

	return concat(2, code1, code2);
}

void generateIR(struct Node* root, char* filename) {
	// preprocessing
	//outputTree(root, 0);
	const_folding(root);
	//printf("----------------------------------\n");
	//outputTree(root, 0);

	constant0 = newOperand(CONSTANT, 0);
	constant1 = newOperand(CONSTANT, 1);

	icHead = translate_ExtDefList(root->child);

	optimize_control();
	remove_extralabel();
	optimize_algebra();

	remove_equation();

#ifdef DEBUG
	//outputIR(icHead);
#endif
	outputIR2File(filename);
}
