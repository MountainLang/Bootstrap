#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"
#include "validator.h"
#include "transpile.h"



void prepare_file(FILE *target)
{
	fprintf(target, "#include <stdlib.h>\n"
	                "#include <stdint.h>\n"
	                "#include <stdbool.h>\n"
	                "#include <stdio.h>\n"
	                "#include <assert.h>\n\n\n\n");
}


char *type_to_c(struct TYPE_DATA *type) //TODO: Make this not suck
{
	if(strcmp(type->name, "u8") == 0)
		return strdup("char");

	if(strcmp(type->name, "i32") == 0)
		return strdup("int");

	if(strcmp(type->name, "Bool") == 0)
		return strdup("bool");

	if(strcmp(type->name, "Void") == 0)
		return strdup("void");

	if(strcmp(type->name, "Ptr") == 0)
	{
		char *child_transpiled = type_to_c(type->child);
		int child_transpiled_len = strlen(child_transpiled);
		char *output = malloc(child_transpiled_len+2);

		strcpy(output, child_transpiled);
		output[child_transpiled_len] = '*';
		output[child_transpiled_len + 1] = '\0';

		free(child_transpiled);
		return output;
	}

	printf("Invalid type to transpile '%s'\n", type->name);
	exit(EXIT_FAILURE);
}


void transpile_expression(FILE *target, struct NODE *node) //TODO support non-numerical values
{
	if(node->node_type == AST_EXPRESSION)
	{
		transpile_expression(target, node->first_child);
		return;
	}

	if(node->node_type == AST_NAME)
	{
		transpile_expression(target, node->first_child);
		return;
	}

	fprintf(target, "(");

	if(node->node_type == AST_NEGATE)
	{
		fprintf(target,"-");
		transpile_expression(target, node->first_child);
	}
	else if(node->node_type == AST_OP)
	{
		transpile_expression(target, node->first_child);
		switch(node->op_type)
		{
			case OP_EQUALS:
				fprintf(target, " = ");
				break;

			case OP_TEST_EQUAL:
				fprintf(target, " == ");
				break;

			case OP_TEST_NOT_EQUAL:
				fprintf(target, " != ");
				break;

			case OP_TEST_GREATER:
				fprintf(target, " > ");
				break;

			case OP_TEST_GREATER_EQUAL:
				fprintf(target, " >= ");
				break;

			case OP_TEST_LESS:
				fprintf(target, " < ");
				break;

			case OP_TEST_LESS_EQUAL:
				fprintf(target, " <= ");
				break;

			case OP_ADD:
				fprintf(target, " + ");
				break;

			case OP_SUB:
				fprintf(target, " - ");
				break;

			case OP_MUL:
				fprintf(target, " * ");
				break;

			case OP_DIV:
				fprintf(target, " / ");
				break;

			default:
				printf("INTERNAL ERROR: We don't know how to transpile this op\n");
				exit(EXIT_FAILURE);
		}
		transpile_expression(target, node->first_child->next);
	}
	else if(node->node_type == AST_UNOP)
	{
		switch(node->unop_type)
		{
			case UNOP_INVERT:
				fprintf(target, "!");
				break;

			case UNOP_ADDRESS_OF:
				fprintf(target, "&");
				break;

			case UNOP_DEREFERENCE:
				fprintf(target, "*");
				break;

			default:
				printf("INTERNAL ERROR: We don't know how to transpile this unop\n");
				exit(EXIT_FAILURE);
		}

		transpile_expression(target, node->first_child);
	}
	else if(node->node_type == AST_GET)
	{
		fprintf(target, "symbol_%i", node->index);
	}
	else if(node->node_type == AST_CALL)
	{
		fprintf(target, "symbol_%i(", node->index);

		struct NODE *arg = node->first_child;
		while(arg != NULL)
		{
			transpile_expression(target, arg);
			if(arg->next != NULL)
				fprintf(target, ", ");

			arg = arg->next;
		}

		fprintf(target, ")");
	}
	else if(node->node_type == AST_LITERAL)
	{
		switch(node->literal_type)
		{
			case LITERAL_U8:
				fprintf(target, "%i", (int)(node->literal_string[0]));
				break;

			case LITERAL_I32:
				fprintf(target, "%s", node->literal_string);
				break;

			case LITERAL_BOOL:
				fprintf(target, "%s", node->literal_string);
				break;
		}
	}
	else
	{
		printf("INTERNAL ERROR: We don't know how to transpile this ast node\n");
		exit(EXIT_FAILURE);
	}

	fprintf(target, ")");
}


void prototype_globals(FILE *target, struct NODE *root)
{
	assert(root->node_type == AST_BLOCK || root->node_type == AST_MODULE);

	struct NODE *node = root->first_child;
	while(node != NULL)
	{
		if(node->node_type == AST_LET)
		{
			char *type_str = type_to_c(node->type);
			fprintf(target, "%s symbol_%i;\n", type_str, node->index);
			free(type_str);
		}

		node = node->next;
	}

	fprintf(target, "\n\n\n");
}


void transpile_global_sets(FILE *target, struct NODE *root)
{
	assert(root->node_type == AST_BLOCK || root->node_type == AST_MODULE);

	struct NODE *node = root->first_child;
	while(node != NULL)
	{
		if(node->node_type == AST_LET && count_node_children(node) == 1)
		{
			fprintf(target, "symbol_%i = ", node->index);
			transpile_expression(target, node->first_child);
			fprintf(target, ";\n");
		}

		node = node->next;
	}
}


void transpile_function_signature(FILE *target, struct NODE *node)
{
	char *type_str = type_to_c(node->type);
	fprintf(target, "%s symbol_%i(", type_str, node->index);
	free(type_str);

	struct ARG_DATA *arg = node->first_arg;
	while(arg != NULL)
	{
		type_str = type_to_c(arg->type);
		fprintf(target, "%s symbol_%i", type_str, arg->index);
		free(type_str);

		if(arg->next != NULL)
			fprintf(target, ", ");

		arg = arg->next;
	}
	fprintf(target, ")");
}


void prototype_functions(FILE *target)
{
	struct FUNC_PROTOTYPE *prototype = first_func_prototype;
	while(prototype != NULL)
	{
		transpile_function_signature(target, prototype->func);
		fprintf(target, ";\n");

		prototype = prototype->next;
	}

	fprintf(target, "\n\n\n");
}


void transpile_functions(FILE *target)
{
	struct FUNC_PROTOTYPE *prototype = first_func_prototype;
	while(prototype != NULL)
	{
		transpile_function_signature(target, prototype->func);
		transpile_block(target, prototype->func->first_child, 0);
		fprintf(target, "\n");

		prototype = prototype->next;
	}

	fprintf(target, "\n\n\n");
}


void transpile_tests(FILE *target, struct NODE *module)
{
	struct NODE *node = module->first_child;
	while(node != NULL)
	{
		if(node->node_type == AST_TEST)
		{
			fprintf(target, "bool test_%i()\n", node->index);
			transpile_block(target, node->first_child, 0);
		}

		node = node->next;
	}
}


void transpile_test_calls(FILE *target)
{
	fprintf(target, "\n\n");

	int test_count = 0;

	struct NODE *module = first_module;
	while(module != NULL)
	{
		struct NODE *node = module->first_child;
		while(node != NULL)
		{
			if(node->node_type == AST_TEST)
			{
				test_count++;

				fprintf(target, "printf(\"\\nRunning test '%s'\\n\");\n"
				                "if(!test_%i())\n{\n"
				                "printf(\"Test failed\\n\");\n"
				                "exit(EXIT_FAILURE);\n}\n"
				                "printf(\"Test succeeded\\n\\n\");\n",
				        node->name, node->index);
			}

			node = node->next;
		}

		module = module->next;
	}

	fprintf(target, "printf(\"\\nAll %i tests succeeded\\n\");\n", test_count);
}


void transpile_block(FILE *target, struct NODE *node, int level) //This is in no way efficient...
{
	assert(level >= 0);
	assert(node->node_type == AST_BLOCK);
	node = node->first_child;

	fprintf(target, "{\n");

	//"foreach" node
	while(node != NULL)
	{
		switch(node->node_type)
		{
			case AST_BLOCK:
				transpile_block(target, node, level + 1);
				break;

			case AST_EXPRESSION:
				fprintf(target, "(void) ");
				transpile_expression(target, node);
				fprintf(target, ";\n");
				break;

			case AST_LET:
				assert(node->index >= 0);

				char *type_str = type_to_c(node->type);
				fprintf(target, "%s symbol_%i", type_str, node->index);
				free(type_str);

				if(count_node_children(node) == 1)
				{
					fprintf(target, " = ");
					transpile_expression(target, node->first_child);
				}

				fprintf(target, ";\n");
				break;

			case AST_IF:
				assert(count_node_children(node) == 2);
				fprintf(target, "if");
				transpile_expression(target, node->first_child);
				fprintf(target, "\n");
				transpile_block(target, node->last_child, level + 1);
				break;

			case AST_ELIF:
				assert(count_node_children(node) == 2);
				fprintf(target, "else if");
				transpile_expression(target, node->first_child);
				fprintf(target, "\n");
				transpile_block(target, node->last_child, level + 1);
				break;

			case AST_ELSE:
				assert(count_node_children(node) == 1);
				fprintf(target, "else\n");
				transpile_block(target, node->last_child, level + 1);
				break;

			case AST_WHILE:
				assert(count_node_children(node) == 2);
				fprintf(target, "while");
				transpile_expression(target, node->first_child);
				fprintf(target, "\n");
				transpile_block(target, node->last_child, level + 1);
				break;

			case AST_BREAK:
				assert(count_node_children(node) == 0);
				fprintf(target, "break;\n");
				break;

			case AST_CONTINUE:
				assert(count_node_children(node) == 0);
				fprintf(target, "continue;\n");
				break;

			case AST_RETURN:
				if(count_node_children(node) == 0)
					fprintf(target, "return;\n");
				else
				{
					assert(count_node_children(node) == 1);

					fprintf(target, "return ");
					transpile_expression(target, node->first_child);
					fprintf(target, ";\n");
				}

				break;

			case AST_FUNC:
				break; //This is transpiled elsewhere

			case AST_STRUCT:
				break; //This is transpiled elsewhere

			default:
				printf("INTERNAL ERROR: We don't know how to transpile this statement\n");
				exit(EXIT_FAILURE);
		}

		node = node->next;
	}

	fprintf(target, "}\n");
}
