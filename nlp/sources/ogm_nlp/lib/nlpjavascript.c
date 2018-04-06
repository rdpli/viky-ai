/*
 *  Eval javascript snipets
 *  Copyright (c) 2017 Pertimm, by Brice Ruzand
 *  Dev : Novembre 2017
 *  Version 1.0
 */

#include "ogm_nlp.h"
#include "duk_module_node.h"
#include "duk_console.h"
#include <iconv.h>

static og_string NlpJsDukTypeString(duk_int_t type);
static duk_ret_t NlpJsInitLoadModule(duk_context *ctx);
static duk_ret_t NlpJsInitResolvModule(duk_context *ctx);
static duk_ret_t push_file_as_string(duk_context *ctx, og_string filename);
static og_status NlpJsLoadLibMoment(og_nlp_th ctrl_nlp_th);
static og_status NlpJsInitBetterErrorMessage(og_nlp_th ctrl_nlp_th);
static og_status NlpJsDukCESU8toUTF8(og_nlp_th ctrl_nlp_th, og_string cesu, int cesu_length, og_string *utf8);

static void NlpJsDuketapeErrorHandler(void *udata, const char *msg)
{
  og_nlp_th ctrl_nlp_th = udata;

  NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDuketapeErrorHandler: %s", msg);

  abort();
}

og_status NlpJsInit(og_nlp_th ctrl_nlp_th)
{
  og_char_buffer heap_name[DPcPathSize];
  snprintf(heap_name, DPcPathSize, "%s_heap_js_variables", ctrl_nlp_th->name);
  ctrl_nlp_th->js->variables = OgHeapInit(ctrl_nlp_th->hmsg, heap_name, sizeof(unsigned char), 0xFF);

  ctrl_nlp_th->js->duk_context = duk_create_heap(NULL, NULL, NULL, ctrl_nlp_th, NlpJsDuketapeErrorHandler);
  if (ctrl_nlp_th->js->duk_context == NULL)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsInit: unable to init js context");
    DPcErr;
  }

  duk_context *ctx = ctrl_nlp_th->js->duk_context;

  // push og_nlp_th pointer as hidden og_nlp_th variable
  duk_push_pointer(ctx, ctrl_nlp_th);
  if (!duk_put_global_string(ctx, DUK_HIDDEN_SYMBOL("og_nlp_th")))
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsInit: duk_put_global_string(og_nlp_th) failed");
    DPcErr;
  }

  // init console
  duk_console_init(ctx, 0);

  // load nodes modules
  duk_push_object(ctx);
  duk_push_c_function(ctx, NlpJsInitResolvModule, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "resolve");
  duk_push_c_function(ctx, NlpJsInitLoadModule, DUK_VARARGS);
  duk_put_prop_string(ctx, -2, "load");
  duk_module_node_init(ctx);

  IFE(NlpJsInitBetterErrorMessage(ctrl_nlp_th));

  // load and init libs
  IFE(NlpJsLoadLibMoment(ctrl_nlp_th));

  // stack after init
  ctrl_nlp_th->js->init_stack_idx = duk_get_top(ctx);
  if (ctrl_nlp_th->js->init_stack_idx != DUK_INVALID_INDEX)
  {
    ctrl_nlp_th->js->init_stack_idx = 0;
  }

  ctrl_nlp_th->js->reset_counter = 0;
  DONE;
}

og_status NlpJsRequestSetup(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    CONT;
  }

  // set now for the whole request
  IFE(NlpJsSetNow(ctrl_nlp_th));

  // stack after request setup
  ctrl_nlp_th->js->request_stack_idx = duk_get_top(ctx);
  if (ctrl_nlp_th->js->request_stack_idx != DUK_INVALID_INDEX)
  {
    ctrl_nlp_th->js->request_stack_idx = ctrl_nlp_th->js->init_stack_idx;
  }

  DONE;
}

static og_status NlpJsInitBetterErrorMessage(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;

  // https://github.com/rotaready/moment-range#node--npm
  og_string extends_error = ""   // keep format
          "Error.prototype.toString = function () {\n"
          "  var line = (this || {}).lineNumber;\n"
          "  return this.name + ': ' + this.message + ' (at line ' + this.lineNumber + ')';\n"
          "};\n"
          "";

  if (duk_peval_string(ctx, extends_error) != 0)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "%s", duk_safe_to_string(ctx, -1));
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsInit: NlpJsInitBetterErrorMessage failed : \n%s", extends_error);
    DPcErr;
  }
  else
  {
    NlpLog(DOgNlpTraceJs, "NlpJsInit: NlpJsInitBetterErrorMessage done.");
  }

  DONE;
}

static og_status NlpJsLoadLibMoment(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;

  // https://github.com/rotaready/moment-range#node--npm
  og_string require = ""   // keep format
          "const moment = require('moment');\n"// moment
          "var moment_range = require('moment-range');\n"// moment-range
          "moment_range.extendMoment(moment);\n"// extends
          "moment_range = undefined;\n"// remove unused variables
          "";

  if (duk_peval_string(ctx, require) != 0)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "%s", duk_safe_to_string(ctx, -1));
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsInit: loading libs 'moment' failed : \n%s", require);
    DPcErr;
  }
  else
  {
    NlpLog(DOgNlpTraceJs, "NlpJsInit: loading libs 'moment' done.");
  }

  DONE;
}

static duk_ret_t NlpJsInitResolvModule(duk_context *ctx)
{
  og_string module_id = duk_require_string(ctx, 0);
  og_string parent_id = duk_require_string(ctx, 1);

  // get og_nlp_th pointer
  duk_get_global_string(ctx, DUK_HIDDEN_SYMBOL("og_nlp_th"));
  og_nlp_th ctrl_nlp_th = duk_get_pointer(ctx, -1);
  if (ctrl_nlp_th == NULL)
  {
    return duk_type_error(ctx, "NlpJsInitResolvModule: duk_get_global_string(og_nlp_th) failed");
  }

  // push back file name
  if (ctrl_nlp_th->ctrl_nlp->WorkingDirectory[0])
  {
    duk_push_sprintf(ctx, "%s/node_modules/%s.js", ctrl_nlp_th->ctrl_nlp->WorkingDirectory, module_id);
  }
  else
  {
    duk_push_sprintf(ctx, "node_modules/%s.js", module_id);
  }

  NlpLog(DOgNlpTraceJs, "NlpJsInitResolvModule: resolve_cb: id:'%s', parent-id:'%s', resolve-to:'%s'", module_id,
      parent_id, duk_get_string(ctx, -1));

  return 1;
}

static duk_ret_t push_file_as_string(duk_context *ctx, og_string filename)
{
  size_t full_file_content_size = 0;
  char *full_file_content = NULL;

  GError *error = NULL;
  og_bool file_loaded = g_file_get_contents(filename, &full_file_content, &full_file_content_size, &error);
  if (!file_loaded)
  {
    og_string error_msg = "";
    if (error != NULL && error->message != NULL)
    {
      error_msg = error->message;
    }

    return duk_type_error(ctx, "push_file_as_string: impossible load file '%s' : %s", filename, error_msg);
  }

  // load javascript file
  duk_push_lstring(ctx, full_file_content, full_file_content_size);

  // free allocated buffer
  g_free(full_file_content);

  return 1;
}

static duk_ret_t NlpJsInitLoadModule(duk_context *ctx)
{
  og_string module_id = duk_require_string(ctx, 0);
  duk_get_prop_string(ctx, 2, "filename");
  og_string filename = duk_require_string(ctx, -1);

  // get og_nlp_th pointer
  duk_get_global_string(ctx, DUK_HIDDEN_SYMBOL("og_nlp_th"));
  og_nlp_th ctrl_nlp_th = duk_get_pointer(ctx, -1);
  if (ctrl_nlp_th == NULL)
  {
    return duk_type_error(ctx, "NlpJsInitLoadModule: duk_get_global_string(og_nlp_th) failed");
  }

  NlpLog(DOgNlpTraceJs, "NlpJsInitLoadModule: load_cb: id:'%s', filename:'%s'", module_id, filename);

  if (!push_file_as_string(ctx, filename))
  {
    return duk_type_error(ctx, "NlpJsInitLoadModule: impossible read file '%s'", filename);
  }

  return 1;
}

og_status NlpJsReset(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    CONT;
  }

  NlpJsStackRequestWipe(ctrl_nlp_th);

  ctrl_nlp_th->js->reset_counter++;
  if ((ctrl_nlp_th->js->reset_counter % ctrl_nlp_th->ctrl_nlp->env->NlpJSDukGcPeriod) == 0)
  {
    // We need to call it twice to make sure everything
    duk_gc(ctx, 0);
    duk_gc(ctx, 0);
  }

  DONE;
}

og_status NlpJsFlush(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    CONT;
  }

  duk_destroy_heap(ctx);
  ctrl_nlp_th->js->duk_context = NULL;

  OgHeapFlush(ctrl_nlp_th->js->variables);
  ctrl_nlp_th->js->variables = NULL;

  DONE;
}

og_bool NlpJsStackRequestWipe(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    CONT;
  }

  og_bool local_wipped = NlpJsStackLocalWipe(ctrl_nlp_th);

  IFE(OgHeapReset(ctrl_nlp_th->js->variables));

  duk_idx_t top = duk_get_top(ctx);
  if (top > 0 && top - ctrl_nlp_th->js->request_stack_idx > 0)
  {
    duk_pop_n(ctx, top - ctrl_nlp_th->js->request_stack_idx);
    return TRUE;
  }

  return local_wipped;
}

og_bool NlpJsStackLocalWipe(og_nlp_th ctrl_nlp_th)
{
  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    CONT;
  }

  IFE(OgHeapResetWithoutReduce(ctrl_nlp_th->js->variables));

  duk_idx_t top = duk_get_top(ctx);
  if (top > 0 && top - ctrl_nlp_th->js->init_stack_idx > 0)
  {
    duk_pop_n(ctx, top - ctrl_nlp_th->js->init_stack_idx);
    return TRUE;
  }

  return FALSE;
}

og_status NlpJsEval(og_nlp_th ctrl_nlp_th, int original_js_script_size, og_string original_js_script,
    json_t **p_json_anwser)
{
  // ignore answer
  if (p_json_anwser == NULL)
  {
    DONE;
  }
  else
  {
    *p_json_anwser = NULL;
  }

  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    NlpLog(DOgNlpTraceJs, "NlpJsEval no javascript ctx initialised");
    CONT;
  }

  // use a localbuffer
  int js_script_size = original_js_script_size;
  og_char_buffer js_script[js_script_size + 1];
  snprintf(js_script, js_script_size + 1, "%.*s", original_js_script_size, original_js_script);

  // trim
  og_string trimed_script = g_strstrip(js_script);
  int trimed_script_length = strlen(trimed_script);

  // 6 is minimal length for (\n\n)\0
  int enhanced_script_size = trimed_script_length + 6;
  og_char_buffer enhanced_script[enhanced_script_size];
  enhanced_script[0] = '\0';

  // starting and ending '{' '}' => surround by '(' ')'
  if (trimed_script[0] == '{' && trimed_script[trimed_script_length - 1] == '}')
  {
    snprintf(enhanced_script, enhanced_script_size, "(\n%s\n)", trimed_script);
  }
  else
  {
    snprintf(enhanced_script, enhanced_script_size, "%s", trimed_script);
  }

  if (ctrl_nlp_th->loginfo->trace & DOgNlpTraceJs)
  {
    // show now
    og_char_buffer now[DPcPathSize];
    now[0] = '\0';
    if (duk_peval_string(ctx, "moment.now_form_request.toJSON()") == 0)
    {
      snprintf(now, DPcPathSize, "// now returned by `moment()` is '%s'\n", duk_safe_to_string(ctx, -1));
    }
    NlpLog(DOgNlpTraceJs, "NlpJsEval js to evaluate : \n// =========\n%s%s\n//=========\n", now, enhanced_script);
  }

  // eval securely
  if (duk_peval_string(ctx, enhanced_script) != 0)
  {
    og_string error = duk_safe_to_string(ctx, -1);

    // show now
    og_char_buffer now[DPcPathSize];
    now[0] = '\0';
    if (duk_peval_string(ctx, "moment.now_form_request.toJSON()") == 0)
    {
      snprintf(now, DPcPathSize, "// now returned by `moment()` is '%s'\n", duk_safe_to_string(ctx, -1));
    }

    IFE(NlpThrowErrorTh(ctrl_nlp_th, "%s", enhanced_script));
    IFE(NlpThrowErrorTh(ctrl_nlp_th, "\n// ====== Eval =======\n%s// JavaScript error : %s\n// ===================\n", now, error));

    og_string variables = OgHeapGetCell(ctrl_nlp_th->js->variables, 0);
    IFE(NlpThrowErrorTh(ctrl_nlp_th, "%s", variables));

    IFE(NlpThrowErrorTh(ctrl_nlp_th, "NlpJsEval: duk_peval_lstring eval failed: %s :\n// ===== Context =====", error));

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
      if ((json_int_t) ((computed_number * 100) / 100) == computed_number)
      {
        json_int_t int_computed_number = computed_number;
        *p_json_anwser = json_integer(computed_number);

        NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is a number (int) : %" JSON_INTEGER_FORMAT,
            int_computed_number);
      }
      else
      {
        *p_json_anwser = json_real(computed_number);

        NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is a number (double): " DOgPrintDouble, computed_number);
      }

      break;
    }

    case DUK_TYPE_STRING:
    {
      duk_size_t icomputed_string = 0;
      og_string computed_string = duk_get_lstring(ctx, -1, &icomputed_string);

      if (g_utf8_validate(computed_string, icomputed_string, NULL))
      {
        NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is a string : '%s'", computed_string);
        *p_json_anwser = json_stringn(computed_string, icomputed_string);
      }
      else
      {
        NlpLog(DOgNlpTraceJs, "NlpJsEval : computed string contains error duk_get_string"
            " return dummy string, try to convert it from CESU-8");

        og_string computed_string_converted = NULL;
        IFE(NlpJsDukCESU8toUTF8(ctrl_nlp_th, computed_string, icomputed_string, &computed_string_converted));
        if (computed_string_converted != NULL)
        {
          *p_json_anwser = json_string(computed_string_converted);

          NlpLog(DOgNlpTraceJs, "NlpJsEval : computed string contains error duk_get_string"
              " return dummy string, try to convert it from CESU-8 successful : %s", computed_string_converted);

          // free converted string
          g_free((gchar *) computed_string_converted);

          if (*p_json_anwser == NULL)
          {
            NlpThrowErrorTh(ctrl_nlp_th, "%s", enhanced_script);
            NlpThrowErrorTh(ctrl_nlp_th, "NlpJsEval : NlpJsDukCESU8toUTF8 failed computed string"
                " contains error duk_get_string return dummy string");
            DPcErr;
          }
        }
        else
        {
          NlpThrowErrorTh(ctrl_nlp_th, "%s", enhanced_script);
          NlpThrowErrorTh(ctrl_nlp_th, "NlpJsEval : NlpJsDukCESU8toUTF8 failed computed string"
              " contains error duk_get_string return dummy string");
          DPcErr;
        }

      }

      break;
    }

    case DUK_TYPE_OBJECT:
    {
      // object, array, function
      og_string computed_json = duk_json_encode(ctx, -1);
      int icomputed_json = strlen(computed_json);

      og_string computed_json_converted = NULL;
      if (!g_utf8_validate(computed_json, icomputed_json, NULL))
      {
        NlpLog(DOgNlpTraceJs, "NlpJsEval : computed json object contains error duk_get_string"
            " return dummy string, try to convert it from CESU-8");

        IFE(NlpJsDukCESU8toUTF8(ctrl_nlp_th, computed_json, icomputed_json, &computed_json_converted));
        if (computed_json_converted != NULL)
        {
          computed_json = computed_json_converted;
          icomputed_json = strlen(computed_json);

          NlpLog(DOgNlpTraceJs, "NlpJsEval : computed json object contains error duk_get_string"
              " return dummy string, try to convert it from CESU-8 successful : %s", computed_json_converted);
        }
        else
        {
          NlpThrowErrorTh(ctrl_nlp_th, "%s", enhanced_script);
          NlpThrowErrorTh(ctrl_nlp_th, "NlpJsEval : NlpJsDukCESU8toUTF8 failed computed json object"
              " contains error duk_get_string return dummy string");
          // free converted string
          g_free((gchar *) computed_json_converted);
          DPcErr;
        }

      }

      // pure string value are surrounded by backquote "\"toto\""
      if (icomputed_json >= 2 && computed_json[0] == '"' && computed_json[icomputed_json - 1] == '"')
      {
        *p_json_anwser = json_stringn(computed_json + 1, icomputed_json - 2);

        // free converted string
        g_free((gchar *) computed_json_converted);
      }
      else
      {

        json_error_t error[1];
        *p_json_anwser = json_loadb(computed_json, icomputed_json, 0, error);
        if (*p_json_anwser == NULL)
        {
          NlpThrowErrorTh(ctrl_nlp_th,
              "NlpJsEval : computed json object contains error in ligne %d and column %d , %s , %s \n'%s'", error->line,
              error->column, error->source, error->text, computed_json);

          // free converted string
          g_free((gchar *) computed_json_converted);
          DPcErr;
        }

        // free converted string
        g_free((gchar *) computed_json_converted);

      }

      NlpLog(DOgNlpTraceJs, "NlpJsEval : computed value is an object/array : '%s'", computed_json);

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

  og_string var_template = "const %s = %s;";
  int var_template_size = strlen(var_template);
  int var_command_size = variable_eval_size + variable_name_size + var_template_size;

  og_char_buffer var_command[var_command_size];
  snprintf(var_command, var_command_size, var_template, variable_name, variable_eval);

  NlpLog(DOgNlpTraceJs, "Sending variable to duktape: %s", var_command);

  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    NlpLog(DOgNlpTraceJs, "NlpJsEval no javascript ctx initialised");
    CONT;
  }

  if (duk_peval_string(ctx, var_command) != 0)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsAddVariable: duk_peval_lstring eval failed: %s : '%s'",
        duk_safe_to_string(ctx, -1), var_command);
    DPcErr;
  }

  // keep variable value for better error message
  IFE(OgHeapAppend(ctrl_nlp_th->js->variables, strlen(var_command), var_command));
  IFE(OgHeapAppend(ctrl_nlp_th->js->variables, 1, "\n"));

  DONE;
}

#define DOgNlpJsSetNowNowScriptSize DPcPathSize * 2
og_status NlpJsSetNow(og_nlp_th ctrl_nlp_th)
{

  og_string date_now = ctrl_nlp_th->date_now;
  if (date_now == NULL)
  {
    NlpLog(DOgNlpTraceJs, "NlpJsSetNow: current UTC time");
  }
  else
  {
    NlpLog(DOgNlpTraceJs, "NlpJsSetNow: %s", date_now);
  }

  og_string offset_reset = "// ====================================================\n"
      "// Reset now state \n"   //
      "moment.now_form_request = null; \n"
      "moment.updateOffset = function() { }; \n"//
      "moment.now = function() { \n"//
      "  return new Date(); \n"//
      "}; \n"//
      " \n"//
      "// Set now\n";

  og_string offset_setup = ""   //
          "// format moment in ISO with timezone  \n"//
          "moment.fn.toJSON = function() { \n"//
          "   return this.format(); \n"//
          "}; \n"//
          " \n"//
          "// Use now from request \n"//
          "moment.now = function() { \n"//
          "  return moment.now_form_request.toDate(); \n"//
          "}; \n"//
          " \n"//
          "// Adjust utcOffset to now utcOffset, replace setOffsetToParsedOffset() \n"//
          "moment.updateOffset = function(m, keepLocalTime) { \n"//
          "  if (!m.updateOffsetInProgress) { \n"//
          "    m.updateOffsetInProgress = true; \n"//
          "    m.parseZone = function() { \n"//
          "      if (this._tzm != null) { \n"//
          "        //this.utcOffset(this._tzm, false, true); \n"//
          "        this.utcOffset(moment.now_form_request.utcOffset(), false, true); \n"//
          "      } else if (typeof this._i === 'string') { \n"//
          "        this.utcOffset(moment.now_form_request.utcOffset(), true, true); \n"//
          "      } else { \n"//
          "        this.utcOffset(moment.now_form_request.utcOffset(), false, true); \n"//
          "      } \n"//
          "      return this; \n"//
          "    } \n"//
          "    m.parseZone(); \n"//
          "    m.updateOffsetInProgress = false; \n"//
          "  } \n"//
          "}; \n"//
          "// ====================================================\n";

  og_char_buffer now_setup_command[DOgNlpJsSetNowNowScriptSize];
  if (date_now == NULL || date_now[0] == '\0')
  {
    snprintf(now_setup_command, DOgNlpJsSetNowNowScriptSize, "%smoment.now_form_request ="
        " moment.utc();\n\n%s", offset_reset, offset_setup);
  }
  else
  {
    snprintf(now_setup_command, DOgNlpJsSetNowNowScriptSize, "%smoment.now_form_request ="
        " moment.parseZone('%s');\n\n%s", offset_reset, date_now, offset_setup);
  }

  NlpLog(DOgNlpTraceJs, "Sending command to duktape:\n%s", now_setup_command);

  duk_context *ctx = ctrl_nlp_th->js->duk_context;
  if (ctx == NULL)
  {
    NlpLog(DOgNlpTraceJs, "NlpJsSetNow no javascript ctx initialised");
    CONT;
  }

  if (duk_peval_string(ctx, now_setup_command) != 0)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsSetNow: duk_peval_lstring eval failed: %s :\n%s", duk_safe_to_string(ctx, -1),
        now_setup_command);
    DPcErr;
  }

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

struct data_CESU8toUTF8
{
  og_nlp_th ctrl_nlp_th;

  og_string original_cesu_str;
  int original_cesu_str_length;

  og_bool match;
  og_bool error;
};

static gboolean NlpJsDukCESU8toUTF8Callback(const GMatchInfo *match_info, GString *result, gpointer user_data)
{

  struct data_CESU8toUTF8 *data = user_data;
  og_nlp_th ctrl_nlp_th = data->ctrl_nlp_th;

  int start_pos = 0;
  int end_pos = 0;

  og_bool match = g_match_info_fetch_pos(match_info, 0, &start_pos, &end_pos);
  if (match)
  {
    int match_length = (end_pos - start_pos);
    if (match_length <= 0)
    {
      data->error = TRUE;
      NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8Callback: match length is %d", match_length);
      return TRUE;
    }

    if (match_length > data->original_cesu_str_length)
    {
      data->error = TRUE;
      NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8Callback: match length is %d > %d (orginal lentgh)",
          match_length, data->original_cesu_str_length);
      return TRUE;
    }

    // matching buffer must contains 6 bytes
    if (match_length != 6)
    {
      data->error = TRUE;
      NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8Callback: match length not 6 bytes length", match_length);
      return TRUE;
    }

    og_char_buffer buffer[8];
    memcpy(buffer, data->original_cesu_str + start_pos, end_pos - start_pos);

    // create UCS-4 character from CESU-8 encoded surrogate pair
    // http://www.unicode.org/reports/tr26/#definitions

    // 3 bytes CESU-8 to UNICODE high surrogate:
    gunichar high = ((buffer[0] & 0x0F) << 12) + ((buffer[1] & 0x3F) << 6) + (buffer[2] & 0x3F);
    // 3 bytes CESU-8 to UNICODE low surrogate:
    gunichar low = ((buffer[3] & 0x0F) << 12) + ((buffer[4] & 0x3F) << 6) + (buffer[5] & 0x3F);

    gunichar codepoint = ((high - 0xD800) * 0x400) + (low - 0xDC00) + 0x10000;

    // convert it to UTF-8
    og_char_buffer utf8_buffer[8];
    memset(utf8_buffer, 0, sizeof(og_char_buffer) * 8);
    int utf8_buffer_length = g_unichar_to_utf8(codepoint, utf8_buffer);
    utf8_buffer[utf8_buffer_length] = 0;

    // append it
    g_string_append(result, utf8_buffer);

    data->match = TRUE;

  }

  return FALSE;
}

static og_status NlpJsDukCESU8toUTF8(og_nlp_th ctrl_nlp_th, og_string cesu, int cesu_length, og_string *utf8)
{

  // https://github.com/svaarala/duktape-wiki/blob/master/HowtoNonBmpCharacters.md
  // https://github.com/svaarala/duktape/issues/996
  // https://github.com/svaarala/duktape/blob/master/doc/utf8-internal-representation.rst
  // http://fileformats.archiveteam.org/wiki/CESU-8

  // CESU-8 is an inefficient Unicode character encoding related to UTF-8. It is not an accepted standard,
  // but has been documented in the interest of practicality.
  // It's what you get if you take UTF-16 data, reinterpret it as UCS-2, then convert it to UTF-8
  // (while ignoring any rules forbidding the use of code points in the range U+D800 to U+DFFF).
  // A code point thus uses 1, 2, 3, or 6 bytes.
  // It is sometimes used by accident, but may be used deliberately to accommodate systems
  // that don't support 4-byte UTF-8 sequences, or when a close correspondence between UTF-16 and a UTF-8-like
  // encoding is deemed necessary.

  // original string
  struct data_CESU8toUTF8 data[1];
  memset(data, 0, sizeof(struct data_CESU8toUTF8));
  data->ctrl_nlp_th = ctrl_nlp_th;
  data->original_cesu_str = cesu;
  data->original_cesu_str_length = cesu_length;

  // implemented from PHP https://stackoverflow.com/questions/34151138/convert-cesu-8-to-utf-8-with-high-performance

  og_string pattern = "(\\xED[\\xA0-\\xAF][\\x80-\\xBF]\\xED[\\xB0-\\xBF][\\x80-\\xBF])";

  GError *regexp_error = NULL;
  GRegex * regex = g_regex_new(pattern, G_REGEX_RAW, 0, &regexp_error);
  if (!regex || regexp_error)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8: g_regex_new failed: %s\npattern: %s\n", regexp_error->message,
        pattern);
    g_error_free(regexp_error);
    DPcErr;
  }

  gchar *result = g_regex_replace_eval(regex, cesu, cesu_length, 0, 0, NlpJsDukCESU8toUTF8Callback, data,
      &regexp_error);

  // free memmory
  g_regex_unref(regex);

  if (!result || regexp_error)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8: g_regex_replace_eval failed: %s", regexp_error->message);
    g_error_free(regexp_error);
    DPcErr;
  }
  else if (data->error)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8: g_regex_replace_eval NlpJsDukCESU8toUTF8Callback failed");
    DPcErr;
  }
  else if (!data->match)
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8: cannot be converted");
    DPcErr;
  }
  else if (!g_utf8_validate(result, -1, NULL))
  {
    NlpThrowErrorTh(ctrl_nlp_th, "NlpJsDukCESU8toUTF8: result is not valide UTF-8");
    DPcErr;
  }

  *utf8 = result;

  DONE;

}
