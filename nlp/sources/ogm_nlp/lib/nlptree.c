/*
 *  Handling parsing tree from a request expression
 *  Copyright (c) 2017 Pertimm, by Patrick Constant
 *  Dev : October 2017
 *  Version 1.0
 */
#include "ogm_nlp.h"

static og_status NlpInterpretTreeAttachAnyRecursive(og_nlp_th ctrl_nlp_th, struct request_expression *root_expression,
    struct request_expression *request_expression, int offset);

static og_status NlpInterpretTreeLogRecursive(og_nlp_th ctrl_nlp_th, struct request_expression *root_request_expression,
    struct request_expression *request_expression, int offset);
static og_status NlpRequestInputPartWordLog(og_nlp_th ctrl_nlp_th, struct request_input_part *request_input_part,
    int offset);

og_status NlpInterpretTreeAttachAny(og_nlp_th ctrl_nlp_th, struct request_expression *request_expression)
{

  if (ctrl_nlp_th->loginfo->trace & DOgNlpTraceMatch)
  {
    NlpLog(DOgNlpTraceMatch, "\nNlpInterpretTreeAttachAny: starting1 with expression:");
    NlpInterpretTreeLog(ctrl_nlp_th, request_expression, 0);
  }

  IFE(NlpRequestAnysAdd(ctrl_nlp_th, request_expression));
  if (request_expression->request_any_start < 0) DONE;

  IFE(NlpSetSuperExpression(ctrl_nlp_th, request_expression));

  struct request_any *request_anys = OgHeapGetCell(ctrl_nlp_th->hrequest_any, request_expression->request_any_start);
  int loop = 0;
  do
  {
    request_expression->nb_anys_attached = 0;
    IFE(NlpInterpretTreeAttachAnyRecursive(ctrl_nlp_th, request_expression, request_expression, 0));
    for (int i = 0; i < request_expression->request_anys_nb; i++)
    {
      struct request_any *request_any = request_anys + i;
      if (request_any->queue_request_expression->length > 0) request_any->is_attached = TRUE;
    }
    NlpLog(DOgNlpTraceMatch, "NlpInterpretTreeAttachAny: loop=%d nb_anys_attached=%d", loop,
        request_expression->nb_anys_attached);
    loop++;
    if (request_expression->nb_anys_attached <= 0) break;
    IFE(NlpGetNbAnysAttached(ctrl_nlp_th, request_expression));
    if (request_expression->nb_anys_attached >= request_expression->nb_anys) break;
  }
  while (request_expression->nb_anys_attached > 0);

  IFE(NlpGetNbAnysAttached(ctrl_nlp_th, request_expression));
  if (ctrl_nlp_th->loginfo->trace & DOgNlpTraceMatch)
  {
    NlpLog(DOgNlpTraceMatch, "NlpInterpretTreeAttachAny: nb_anys=%d nb_anys_attached=%d:", request_expression->nb_anys,
        request_expression->nb_anys_attached);
    IFE(NlpRequestExpressionAnysLog(ctrl_nlp_th, request_expression));
  }

  DONE;
}

static og_status NlpInterpretTreeAttachAnyRecursive(og_nlp_th ctrl_nlp_th, struct request_expression *root_expression,
    struct request_expression *request_expression, int offset)
{
  if (request_expression->sorted_flat_list->length > 0)
  {

    for (GList *iter = request_expression->sorted_flat_list->head; iter; iter = iter->next)
    {
      int Irequest_expression = GPOINTER_TO_INT(iter->data);
      struct request_expression *sub_request_expression = OgHeapGetCell(ctrl_nlp_th->hrequest_expression,
          Irequest_expression);
      IFE(NlpInterpretTreeAttachAnyRecursive(ctrl_nlp_th, root_expression, sub_request_expression, offset + 2));
    }

  }
  else
  {

    for (int i = 0; i < request_expression->orips_nb; i++)
    {
      struct request_input_part *request_input_part = NlpGetRequestInputPart(ctrl_nlp_th, request_expression, i);
      IFN(request_input_part) DPcErr;

      if (request_input_part->type == nlp_input_part_type_Word) ;
      else if (request_input_part->type == nlp_input_part_type_Interpretation)
      {
        struct request_expression *sub_expression = OgHeapGetCell(ctrl_nlp_th->hrequest_expression,
            request_input_part->Irequest_expression);
        IFN(sub_expression) DPcErr;
        IFE(NlpInterpretTreeAttachAnyRecursive(ctrl_nlp_th, root_expression, sub_expression, offset + 2));
      }
    }

  }

  if (request_expression->expression->any_input_part_position >= 0)
  {
    IFE(NlpRequestAnyAddClosest(ctrl_nlp_th, root_expression, request_expression));
  }

  DONE;
}

og_status NlpSetSuperExpression(og_nlp_th ctrl_nlp_th, struct request_expression *request_expression)
{
  if (request_expression->sorted_flat_list->length > 0)
  {

    for (GList *iter = request_expression->sorted_flat_list->head; iter; iter = iter->next)
    {
      int Irequest_expression = GPOINTER_TO_INT(iter->data);
      struct request_expression *sub_request_expression = OgHeapGetCell(ctrl_nlp_th->hrequest_expression,
          Irequest_expression);
      IFE(NlpSetSuperExpression(ctrl_nlp_th, sub_request_expression));
      sub_request_expression->Isuper_request_expression = request_expression->self_index;
    }

  }
  else
  {

    for (int i = 0; i < request_expression->orips_nb; i++)
    {
      struct request_input_part *request_input_part = NlpGetRequestInputPart(ctrl_nlp_th, request_expression, i);
      IFN(request_input_part) DPcErr;

      if (request_input_part->type == nlp_input_part_type_Interpretation)
      {
        struct request_expression *sub_request_expression = OgHeapGetCell(ctrl_nlp_th->hrequest_expression,
            request_input_part->Irequest_expression);
        IFN(sub_request_expression) DPcErr;
        IFE(NlpSetSuperExpression(ctrl_nlp_th, sub_request_expression));
        sub_request_expression->Isuper_request_expression = request_expression->self_index;
      }
    }

  }
  DONE;
}

og_status NlpInterpretTreeLog(og_nlp_th ctrl_nlp_th, struct request_expression *request_expression, int offset)
{
  OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "Tree representation for expression:");
  IFE(NlpInterpretTreeLogRecursive(ctrl_nlp_th, request_expression, request_expression, offset));
  DONE;
}

static og_status NlpInterpretTreeLogRecursive(og_nlp_th ctrl_nlp_th, struct request_expression *root_request_expression,
    struct request_expression *request_expression, int offset)
{
  char string_offset[DPcPathSize];
  memset(string_offset, ' ', offset);
  string_offset[offset] = 0;

  if (request_expression->sorted_flat_list->length > 0)
  {
    OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "%sSorted flat list of expressions:", string_offset);
    for (GList *iter = request_expression->sorted_flat_list->head; iter; iter = iter->next)
    {
      int Irequest_expression = GPOINTER_TO_INT(iter->data);
      struct request_expression *sub_request_expression = OgHeapGetCell(ctrl_nlp_th->hrequest_expression,
          Irequest_expression);
      IFE(NlpInterpretTreeLogRecursive(ctrl_nlp_th, sub_request_expression, sub_request_expression, offset));
    }
  }
  else
  {
    IFE(NlpRequestExpressionLog(ctrl_nlp_th, request_expression, offset));
    for (int i = 0; i < request_expression->orips_nb; i++)
    {
      struct request_input_part *request_input_part = NlpGetRequestInputPart(ctrl_nlp_th, request_expression, i);
      IFN(request_input_part) DPcErr;

      if (request_input_part->type == nlp_input_part_type_Word)
      {
        struct request_word *request_word = request_input_part->request_word;
        og_string string_request_word = OgHeapGetCell(ctrl_nlp_th->hba, request_word->start);
        IFN(string_request_word) DPcErr;
        IFE(NlpRequestInputPartWordLog(ctrl_nlp_th, request_input_part, offset + 2));
      }
      else if (request_input_part->type == nlp_input_part_type_Interpretation)
      {
        struct request_expression *sub_request_expression = OgHeapGetCell(ctrl_nlp_th->hrequest_expression,
            request_input_part->Irequest_expression);
        IFN(sub_request_expression) DPcErr;
        IFE(NlpInterpretTreeLogRecursive(ctrl_nlp_th, root_request_expression, sub_request_expression, offset + 2));
      }
    }

    if (request_expression->expression->any_input_part_position >= 0)
    {
      if (request_expression->Irequest_any >= 0)
      {
        struct request_any *request_any = OgHeapGetCell(ctrl_nlp_th->hrequest_any, request_expression->Irequest_any);
        IFN(request_any) DPcErr;

        char string_any[DPcPathSize];
        NlpRequestAnyString(ctrl_nlp_th, request_any, DPcPathSize, string_any);

        char string_any_position[DPcPathSize];
        NlpRequestAnyPositionString(ctrl_nlp_th, request_any, DPcPathSize, string_any_position);

        char highlight[DPcPathSize];
        NlpRequestAnyStringPretty(ctrl_nlp_th, request_any, DPcPathSize, highlight);

        OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "  %s%2d: '%s' [%s] any: '%s'", string_offset,
            request_expression->level, string_any, string_any_position, highlight);
      }
      else
      {
        OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "  %s%2d: [] any", string_offset, request_expression->level);
      }
    }
  }

  DONE;
}

static og_status NlpRequestInputPartWordLog(og_nlp_th ctrl_nlp_th, struct request_input_part *request_input_part,
    int offset)
{
  IFN(request_input_part) DPcErr;

  char string_offset[DPcPathSize];
  memset(string_offset, ' ', offset);
  string_offset[offset] = 0;

  char string_positions[DPcPathSize];
  NlpRequestPositionString(ctrl_nlp_th, request_input_part->request_position_start,
      request_input_part->request_positions_nb, DPcPathSize, string_positions);

  struct request_word *request_word = request_input_part->request_word;
  og_string string_request_word = OgHeapGetCell(ctrl_nlp_th->hba, request_word->start);
  IFN(string_request_word) DPcErr;

  char number[DPcPathSize];
  number[0] = 0;
  if (request_word->is_number)
  {
    snprintf(number, DPcPathSize, " -> " DOgPrintDouble, request_word->number_value);
  }

  OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "%s%2d:%d [%s] '%.*s'%s", string_offset,
      request_input_part->Ioriginal_request_input_part, request_input_part->level, string_positions, DPcPathSize,
      string_request_word, number);
  DONE;
}

