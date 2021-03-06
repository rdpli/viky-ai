/*
 *  Handling request input parts
 *  Copyright (c) 2017 Pertimm, by Patrick Constant
 *  Dev : October 2017
 *  Version 1.0
 */
#include "ogm_nlp.h"

static int NlpRequestPositionCmp(gconstpointer ptr_request_position1, gconstpointer ptr_request_position2,
    gpointer user_data);

og_status NlpRequestPositionAdd(og_nlp_th ctrl_nlp_th, int start, int length, size_t *pIrequest_position)
{
  struct request_position *request_position = OgHeapNewCell(ctrl_nlp_th->hrequest_position, pIrequest_position);
  IFn(request_position) DPcErr;
  IF(*pIrequest_position) DPcErr;
  request_position->start = start;
  request_position->length = length;
  DONE;
}

og_status NlpRequestPositionSort(og_nlp_th ctrl_nlp_th, int request_position_start, int request_positions_nb)
{
  struct request_position *request_position = OgHeapGetCell(ctrl_nlp_th->hrequest_position, request_position_start);
  IFN(request_position) DPcErr;
  g_qsort_with_data(request_position, request_positions_nb, sizeof(struct request_position), NlpRequestPositionCmp,
  NULL);

  DONE;
}

og_bool NlpRequestPositionSame(og_nlp_th ctrl_nlp_th, struct request_expression_access_cache *cache,
    int request_position_start1, int request_positions_nb1, int request_position_start2, int request_positions_nb2)
{
  if (request_positions_nb1 != request_positions_nb2) return FALSE;

  struct request_position *request_position1 = cache->request_positions + request_position_start1;
  IFN(request_position1) DPcErr;
  struct request_position *request_position2 = cache->request_positions + request_position_start2;
  IFN(request_position2) DPcErr;

  if (memcmp(request_position1, request_position2, request_positions_nb1 * sizeof(struct request_position))) return FALSE;
  return TRUE;
}

/*
 * Checks if if position2 is included in position 1
 */
og_bool NlpRequestPositionIncluded(og_nlp_th ctrl_nlp_th, struct request_position *request_positions,
    int request_position_start1, int request_positions_nb1, int request_position_start2, int request_positions_nb2)
{
  int start_position1 = request_positions[request_position_start1].start;
  int start_position2 = request_positions[request_position_start2].start;

  int request_position_end1 = request_position_start1 + request_positions_nb1 - 1;
  int request_position_end2 = request_position_start2 + request_positions_nb2 - 1;

  int end_position1 = request_positions[request_position_end1].start + request_positions[request_position_end1].length;
  int end_position2 = request_positions[request_position_end2].start + request_positions[request_position_end2].length;

  if (start_position1 <= start_position2 && end_position2 <= end_position1) return TRUE;
  return (FALSE);
}


og_bool NlpRequestPositionOverlap(og_nlp_th ctrl_nlp_th, int request_position_start, int request_positions_nb)
{
  struct request_position *request_position = OgHeapGetCell(ctrl_nlp_th->hrequest_position, request_position_start);
  IFN(request_position) DPcErr;
  for (int i = 0; i + 1 < request_positions_nb; i++)
  {
    int starti = request_position[i].start;
    int endi = request_position[i].start+request_position[i].length;
    int startip1 = request_position[i+1].start;
    int endip1 = request_position[i+1].start+request_position[i+1].length;
    if (starti==startip1 && endi==endip1) return TRUE;
    // because positions are sorted we know that start[i] < start[i+1]
    // so an overlap means that start[i+1] < end[i]
    // start[i]    end[i]
    //        start[i+1]   end[i+1]
    if (startip1 < endi) return TRUE;

  }
  return FALSE;
}

static int NlpRequestPositionCmp(gconstpointer ptr_request_position1, gconstpointer ptr_request_position2,
    gpointer user_data)
{
  struct request_position *request_position1 = (struct request_position *) ptr_request_position1;
  struct request_position *request_position2 = (struct request_position *) ptr_request_position2;

  if (request_position1->start != request_position2->start)
  {
    return (request_position1->start - request_position2->start);
  }
  // Just to make sure it is
  return request_position1->length - request_position2->length;
}

og_status NlpRequestPositionDistance(og_nlp_th ctrl_nlp_th, int request_position_start, int request_positions_nb)
{
  struct request_position *request_position = OgHeapGetCell(ctrl_nlp_th->hrequest_position, request_position_start);
  IFN(request_position) DPcErr;
  int distance = 0;
  for (int i = 0; i + 1 < request_positions_nb; i++)
  {
    int interval_distance = request_position[i + 1].start - (request_position[i].start + request_position[i].length);
    distance += interval_distance;
  }
  return distance;
}

og_bool NlpRequestPositionsAreOrdered(og_nlp_th ctrl_nlp_th, int request_position_start1, int request_positions_nb1,
    int request_position_start2, int request_positions_nb2)
{
  struct request_position *request_position1 = OgHeapGetCell(ctrl_nlp_th->hrequest_position,
      request_position_start1 + request_positions_nb1 - 1);
  struct request_position *request_position2 = OgHeapGetCell(ctrl_nlp_th->hrequest_position, request_position_start2);
  if (request_position1->start + request_position1->length <= request_position2->start) return TRUE;
  return FALSE;
}

og_bool NlpRequestPositionsAreGlued(og_nlp_th ctrl_nlp_th, int request_position_start1, int request_positions_nb1,
    int request_position_start2, int request_positions_nb2)
{
  struct request_position *request_positions = OgHeapGetCell(ctrl_nlp_th->hrequest_position, 0);

  struct request_position *request_position1 = request_positions + request_position_start1 + request_positions_nb1 - 1;
  struct request_position *request_position2 = request_positions + request_position_start2;
  int position1 = request_position1->start + request_position1->length;
  int position2 = request_position2->start;

  if (position1 > position2)
  {
    request_position2 = request_positions + request_position_start2 + request_positions_nb2 - 1;
    request_position1 = request_positions + request_position_start1;
    position2 = request_position2->start + request_position2->length;
    position1 = request_position1->start;
  }

  enum nlp_glue_status glue_status = NlpGluedGetStatusForPositions(ctrl_nlp_th, position1, position2);
  switch (glue_status)
  {
    case nlp_glue_status_Loose:
      return FALSE;
    case nlp_glue_status_Glued:
      return TRUE;
    case nlp_glue_status_Stuck:
      return TRUE;
  }
  return FALSE;
}

int NlpRequestPositionString(og_nlp_th ctrl_nlp_th, int request_position_start, int request_positions_nb, int size,
    char *string)
{
  struct request_position *request_position = OgHeapGetCell(ctrl_nlp_th->hrequest_position, request_position_start);
  IFN(request_position) DPcErr;
  int length = 0;
  string[length] = 0;
  for (int i = 0; i < request_positions_nb; i++)
  {
    length = strlen(string);
    snprintf(string + length, size - length, "%s%d:%d", (i ? " " : ""), request_position[i].start,
        request_position[i].length);
  }

  DONE;
}

int NlpRequestPositionStringHighlight(og_nlp_th ctrl_nlp_th, int request_position_start, int request_positions_nb,
    int size, char *string)
{
  struct request_position *request_position = OgHeapGetCell(ctrl_nlp_th->hrequest_position, request_position_start);

  og_string s = ctrl_nlp_th->request_sentence;
  int is = strlen(s);

  int position = 0;

  IFN(request_position) DPcErr;
  int length = 0;
  string[length] = 0;
  for (int i = 0; i < request_positions_nb; i++)
  {
    if (position < request_position[i].start)
    {
      length = strlen(string);
      snprintf(string + length, size - length, "%.*s", request_position[i].start - position, s + position);
      position = request_position[i].start;

    }
    length = strlen(string);
    snprintf(string + length, size - length, "[%.*s]", request_position[i].length, s + request_position[i].start);
    position = request_position[i].start + request_position[i].length;
  }

  if (position < is)
  {
    length = strlen(string);
    snprintf(string + length, size - length, "%.*s", is - position, s + position);
    position = is;
  }

  DONE;
}
