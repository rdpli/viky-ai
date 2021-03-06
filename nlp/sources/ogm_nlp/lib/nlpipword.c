/*
 *  handling packages word automaton
 *  Copyright (c) 2017 Pertimm, by Patrick Constant
 *  Dev : October 2017
 *  Version 1.0
 */
#include "ogm_nlp.h"

og_status NlpInputPartWordInit(og_nlp_th ctrl_nlp_th, package_t package)
{
  struct og_aut_param aut_param[1];
  memset(aut_param, 0, sizeof(struct og_aut_param));
  aut_param->herr = ctrl_nlp_th->herr;
  aut_param->hmutex = ctrl_nlp_th->hmutex;
  aut_param->loginfo.trace = DOgAutTraceMinimal;
  aut_param->loginfo.where = ctrl_nlp_th->loginfo->where;
  aut_param->state_number = 0x10;
  sprintf(aut_param->name, "package_ha_word");
  package->ha_word = OgAutInit(aut_param);
  IFn(package->ha_word) DPcErr;
  package->nb_words = 0;
  DONE;
}

og_status NlpInputPartWordFlush(package_t package)
{
  IFE(OgAutFlush(package->ha_word));
  DONE;
}

og_status NlpInputPartWordAdd(og_nlp_th ctrl_nlp_th, package_t package, og_string string_word, int length_string_word,
    int Iinput_part)
{
  unsigned char buffer[DPcAutMaxBufferSize];
  int ibuffer = 0;
  memcpy(buffer, string_word, length_string_word);
  ibuffer = length_string_word;
  buffer[ibuffer++] = '\1';

  unsigned char *p = buffer + ibuffer;
  OggNout(Iinput_part, &p);

  int length = p - buffer;
  IFE(OgAutAdd(package->ha_word, length, buffer));

  package->nb_words++;

  DONE;
}

og_status NlpInputPartWordLog(og_nlp_th ctrl_nlp_th, package_t package)
{
  unsigned char out[DPcAutMaxBufferSize + 9];
  oindex states[DPcAutMaxBufferSize + 9];
  int retour, nstate0, nstate1, iout;

  OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "Words for package '%s' '%s':", package->slug, package->id);

  if ((retour = OgAufScanf(package->ha_word, 0, "", &iout, out, &nstate0, &nstate1, states)))
  {
    do
    {
      IFE(retour);
      int sep = -1;
      for (int i = 0; i < iout; i++)
      {
        if (out[i] == '\1')
        {
          sep = i;
          break;
        }
      }
      if (sep < 0)
      {
        NlpThrowErrorTh(ctrl_nlp_th, "NlpInputPartWordLog: error in ha_word");
        DPcErr;
      }

      int Iinput_part;
      unsigned char *p = out + sep + 1;
      IFE(DOgPnin4(ctrl_nlp_th->herr,&p,&Iinput_part));
      OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "  %.*s : %d", sep, out, Iinput_part);
    }
    while ((retour = OgAufScann(package->ha_word, &iout, out, nstate0, &nstate1, states)));
  }

  DONE;
}

og_status NlpNumberInputPartLog(og_nlp_th ctrl_nlp_th, package_t package)
{
  OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "Number input parts for package '%s' '%s':", package->slug, package->id);
  struct number_input_part *number_input_part_all = OgHeapGetCell(package->hnumber_input_part, 0);
  int number_input_part_used = OgHeapGetCellsUsed(package->hnumber_input_part);

  for (int i = 0; i < number_input_part_used; i++)
  {
    struct number_input_part *number_input_part = number_input_part_all+i;
    OgMsg(ctrl_nlp_th->hmsg, "", DOgMsgDestInLog, "%4d", number_input_part->Iinput_part);
  }

  DONE;
}

