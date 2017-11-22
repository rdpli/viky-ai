/*
 *  Eval javascript snipets
 *  Copyright (c) 2017 Pertimm, by Brice Ruzand
 *  Dev : Novembre 2017
 *  Version 1.0
 */

#include "ogm_nlp.h"
#include "duk_module_node.h"

static og_string NlpJsDukTypeString(duk_int_t type);

static void NlpJsDuketapeErrorHandler(void *udata, const char *msg)
{
  og_nlp_th ctrl_nlp_th = udata;

  NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDuketapeErrorHandler: %s", msg);

  abort();
}

og_status NlpJsInit(og_nlp_th ctrl_nlp_th)
{
  ctrl_nlp_th->js->varibale_values = g_string_chunk_new(0x10);
  g_queue_init(ctrl_nlp_th->js->variable_list);

  ctrl_nlp_th->js->duk_context = duk_create_heap(NULL, NULL, NULL, ctrl_nlp_th, NlpJsDuketapeErrorHandler);
  if (ctrl_nlp_th->js->duk_context == NULL)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsInit: unable to init js context");
    DPcErr;
  }

  duk_context *ctx = ctrl_nlp_th->js->duk_context;

//  // load nodes modules
//  duk_push_object(ctx);
//  duk_push_c_function(ctx, cb_resolve_module, DUK_VARARGS);
//  duk_put_prop_string(ctx, -2, "resolve");
//  duk_push_c_function(ctx, cb_load_module, DUK_VARARGS);
//  duk_put_prop_string(ctx, -2, "load");
//  duk_module_node_peval_main(ctx, "/data/git/voqal.ai/platform/nlp/ship/debug/");
//  duk_module_node_init(ctx);

// stack after init
  ctrl_nlp_th->js->init_stack_idx = duk_get_top(ctx);
  if (ctrl_nlp_th->js->init_stack_idx != DUK_INVALID_INDEX)
  {
    ctrl_nlp_th->js->init_stack_idx = 0;
  }

  DONE;
}

og_status NlpJsReset(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;

  IFE(NlpJsStackWipe(ctrl_nlp_th));

  // We need to call it twice to make sure everything
  duk_gc(ctx, 0);
  duk_gc(ctx, 0);

  DONE;
}

og_status NlpJsFlush(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;

  duk_destroy_heap(ctx);
  ctrl_nlp_th->js->duk_context = NULL;

  g_queue_clear(ctrl_nlp_th->js->variable_list);
  g_string_chunk_free(ctrl_nlp_th->js->varibale_values);

  DONE;
}

og_bool NlpJsStackWipe(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;

  g_queue_clear(ctrl_nlp_th->js->variable_list);
  g_string_chunk_clear(ctrl_nlp_th->js->varibale_values);

  duk_idx_t top = duk_get_top(ctx);
  if (top > 0 && top - ctrl_nlp_th->js->init_stack_idx > 0)
  {
    duk_pop_n(ctx, top - ctrl_nlp_th->js->init_stack_idx);
    return TRUE;
  }

  return FALSE;
}

og_status NlpJsEval(og_nlp_th ctrl_nlp_th, int js_script_size, og_string js_script, json_t **p_json_anwser)
{
  // ingnore answer
  if (p_json_anwser == NULL)
  {
    DONE;
  }
  else
  {
    *p_json_anwser = NULL;
  }

  NlpLog(DOgNlpTraceJs, "NlpJsEval js to evaluate : '%.*s'", js_script_size, js_script);

  duk_context *ctx = ctrl_nlp_th->js->duk_context;

  // eval securely
  if (duk_peval_lstring(ctx, js_script, js_script_size) != 0)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsEval: duk_peval_lstring eval failed: %s : \"%.*s\"", duk_safe_to_string(ctx, -1),
        js_script_size, js_script);
    for (GList *iter = ctrl_nlp_th->js->variable_list->head; iter; iter = iter->next)
    {
      og_string variable = iter->data;
      NlpThrowErrorTh(ctrl_nlp_th, "NlpJsEval: %s", variable);
    }
    DPcErr;
  }

  // get result
  duk_int_t type = duk_get_type(ctrl_nlp_th->js->duk_context, -1);
  switch (type)
  {
    case DUK_TYPE_NONE:
    case DUK_TYPE_UNDEFINED:
    case DUK_TYPE_NULL:
    {
      *p_json_anwser = json_null();

      NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is %s", NlpJsDukTypeString(type));

      break;
    }

    case DUK_TYPE_BOOLEAN:
    {
      double computed_boolean = duk_get_boolean(ctx, -1);
      *p_json_anwser = json_boolean(computed_boolean);

      NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is a boolean : %s", ((computed_boolean) ? "TRUE" : "FALSE"));

      break;
    }

    case DUK_TYPE_NUMBER:
    {
      double computed_number = duk_get_number(ctx, -1);

      // check if it is an integer
      if ((json_int_t)((computed_number * 100) / 100) == computed_number)
      {
        json_int_t int_computed_number = computed_number;
        *p_json_anwser = json_integer(computed_number);

        NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is a number (int) : %" JSON_INTEGER_FORMAT,
            int_computed_number);
      }
      else
      {
        *p_json_anwser = json_real(computed_number);

        NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is a number (double): %g", computed_number);
      }

      break;
    }

    case DUK_TYPE_STRING:
    {
      og_string computed_string = duk_get_string(ctx, -1);
      *p_json_anwser = json_string(computed_string);

      NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is a string : '%s'", computed_string);
      break;
    }

    case DUK_TYPE_OBJECT:
    {
      // object, array, function
      og_string computed_json_string = duk_json_encode(ctx, -1);
      int computed_json_string_size = strlen(computed_json_string);

      // pure string value are surrounded by backquote "\"toto\""
      if (computed_json_string_size >= 2 && computed_json_string[0] == '"'
          && computed_json_string[computed_json_string_size - 1] == '"')
      {
        *p_json_anwser = json_stringn(computed_json_string + 1, computed_json_string_size - 2);
      }
      else
      {

        json_error_t error[1];
        *p_json_anwser = json_loadb(computed_json_string, computed_json_string_size, 0, error);
        if (*p_json_anwser == NULL)
        {
          NlpThrowErrorTh(ctrl_nlp_th,
              "NlpJsEval : computed json object contains error in ligne %d and column %d , %s , %s \n'%s'", error->line,
              error->column, error->source, error->text, computed_json_string);
          DPcErr;
        }

      }

      NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is an object/array : '%s'", computed_json_string);

      break;
    }

    default:
    {
      NlpThrowErrorTh(ctrl_nlp_th, "NlpJsEval: unsupported answer duktype : %s", NlpJsDukTypeString(type));
      DPcErr;
    }

  }

  duk_pop(ctx);

  DONE;
}

og_status NlpJsAddVariable(og_nlp_th ctrl_nlp_th, og_string variable_name, og_string variable_eval)
{
  int variable_name_size = strlen(variable_name);
  if (variable_name_size == 0)
  {
    CONT;
  }

  int variable_eval_size = strlen(variable_eval);
  if (variable_eval_size == 0)
  {
    variable_eval = "null";
    variable_eval_size = 4;
  }

  og_string var_template = "var %s = %s;";
  int var_template_size = strlen(var_template);
  int var_command_size = variable_eval_size + variable_name_size + var_template_size;

  og_char_buffer var_command[var_command_size];
  snprintf(var_command, var_command_size, var_template, variable_name, variable_eval);

  NlpLog(DOgNlpTraceJs, "Sending variable to duktape: %s", var_command);

  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (duk_peval_lstring(ctx, var_command, strlen(var_command)) != 0)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsAddVariable: duk_peval_lstring eval failed: %s : '%s'",
        duk_safe_to_string(ctx, -1), var_command);
    DPcErr;
  }

  // keep variable value for better error message
  char *new_var = g_string_chunk_insert(ctrl_nlp_th->js->varibale_values, var_command);
  g_queue_push_tail(ctrl_nlp_th->js->variable_list, new_var);

  DONE;
}

og_status NlpJsAddVariableJson(og_nlp_th ctrl_nlp_th, og_string variable_name, json_t *variable_value)
{
  og_bool truncated = FALSE;
  og_char_buffer variable_eval_buffer[DOgMlogMaxMessageSize / 2];

  IFE(NlpJsonToBuffer(variable_value, variable_eval_buffer, DOgMlogMaxMessageSize / 2, &truncated, JSON_ENCODE_ANY));
  if (truncated)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsAddVariableJson: truncated : %s", variable_eval_buffer);
    DPcErr;
  }

  return NlpJsAddVariable(ctrl_nlp_th, variable_name, variable_eval_buffer);
}

static og_string NlpJsDukTypeString(duk_int_t type)
{
  switch (type)
  {
    case DUK_TYPE_NONE:
      return "none";
    case DUK_TYPE_UNDEFINED:
      return "undefined";
    case DUK_TYPE_NULL:
      return "null";
    case DUK_TYPE_BOOLEAN:
      return "boolean";
    case DUK_TYPE_NUMBER:
      return "number";
    case DUK_TYPE_STRING:
      return "string";
    case DUK_TYPE_OBJECT:
      return "object";
    case DUK_TYPE_BUFFER:
      return "buffer";
    case DUK_TYPE_POINTER:
      return "pointer";
    case DUK_TYPE_LIGHTFUNC:
      return "lightfunc";
  }

  return "nil";
}

