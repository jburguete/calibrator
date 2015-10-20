/*
Calibrator: a software to make calibrations of empirical parameters.

AUTHORS: Javier Burguete and Borja Latorre.

Copyright 2012-2015, AUTHORS.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

	1. Redistributions of source code must retain the above copyright notice,
		this list of conditions and the following disclaimer.

	2. Redistributions in binary form must reproduce the above copyright notice,
		this list of conditions and the following disclaimer in the
		documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY AUTHORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL AUTHORS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

/**
 * \file calibrator.c
 * \brief Source file of the calibrator.
 * \authors Javier Burguete and Borja Latorre.
 * \copyright Copyright 2012-2015, all rights reserved.
 */
#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <locale.h>
#include <gsl/gsl_rng.h>
#include <libxml/parser.h>
#include <libintl.h>
#include <glib.h>
#ifdef G_OS_WIN32
#include <windows.h>
#elif (!__BSD_VISIBLE)
#include <alloca.h>
#endif
#if HAVE_MPI
#include <mpi.h>
#endif
#include "genetic/genetic.h"
#include "calibrator.h"
#if HAVE_GTK
#include <gio/gio.h>
#include <gtk/gtk.h>
#include "interface.h"
#endif

/**
 * \def DEBUG
 * \brief Macro to debug.
 */
#define DEBUG 0

/**
 * \var ntasks
 * \brief Number of tasks.
 * \var nthreads
 * \brief Number of threads.
 * \var mutex
 * \brief Mutex struct.
 * \var void (*calibrate_step)()
 * \brief Pointer to the function to perform a calibration algorithm step.
 * \var input
 * \brief Input struct to define the input file to calibrator.
 * \var calibrate
 * \brief Calibration data.
 * \var template
 * \brief Array of xmlChar strings with template labels.
 * \var logo
 * \brief Logo pixmap.
 */
int ntasks;
unsigned int nthreads;
#if GLIB_MINOR_VERSION >= 32
GMutex mutex[1];
#else
GMutex *mutex;
#endif
void (*calibrate_step) ();
Input input[1];
Calibrate calibrate[1];
const xmlChar *template[MAX_NINPUTS] = {
  XML_TEMPLATE1, XML_TEMPLATE2, XML_TEMPLATE3, XML_TEMPLATE4,
  XML_TEMPLATE5, XML_TEMPLATE6, XML_TEMPLATE7, XML_TEMPLATE8
};

const char *logo[] = {
  "32 32 3 1",
  " 	c None",
  ".	c #0000FF",
  "+	c #FF0000",
  "                                ",
  "                                ",
  "                                ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .     +++     .     ",
  "     .      .    +++++    .     ",
  "     .      .    +++++    .     ",
  "     .      .    +++++    .     ",
  "    +++     .     +++    +++    ",
  "   +++++    .      .    +++++   ",
  "   +++++    .      .    +++++   ",
  "   +++++    .      .    +++++   ",
  "    +++     .      .     +++    ",
  "     .      .      .      .     ",
  "     .     +++     .      .     ",
  "     .    +++++    .      .     ",
  "     .    +++++    .      .     ",
  "     .    +++++    .      .     ",
  "     .     +++     .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "     .      .      .      .     ",
  "                                ",
  "                                ",
  "                                "
};

/*
const char * logo[] = {
"32 32 3 1",
" 	c #FFFFFFFFFFFF",
".	c #00000000FFFF",
"X	c #FFFF00000000",
"                                ",
"                                ",
"                                ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .     XXX     .     ",
"     .      .    XXXXX    .     ",
"     .      .    XXXXX    .     ",
"     .      .    XXXXX    .     ",
"    XXX     .     XXX    XXX    ",
"   XXXXX    .      .    XXXXX   ",
"   XXXXX    .      .    XXXXX   ",
"   XXXXX    .      .    XXXXX   ",
"    XXX     .      .     XXX    ",
"     .      .      .      .     ",
"     .     XXX     .      .     ",
"     .    XXXXX    .      .     ",
"     .    XXXXX    .      .     ",
"     .    XXXXX    .      .     ",
"     .     XXX     .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"     .      .      .      .     ",
"                                ",
"                                ",
"                                "};
*/

#if HAVE_GTK
/**
 * \var window
 * \brief Window struct to define the main interface window.
 */
Window window[1];
#endif

/**
 * \fn void show_message(char *title, char *msg)
 * \brief Function to show a dialog with a message.
 * \param title
 * \brief Title.
 * \param msg
 * \brief Message.
 */
void
show_message (char *title, char *msg)
{
#if HAVE_GTK
  GtkMessageDialog *dlg;

  // Creating the dialog
  dlg = (GtkMessageDialog *) gtk_message_dialog_new
    (window->window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
     "%s", msg);

  // Setting the dialog title
  gtk_window_set_title (GTK_WINDOW (dlg), title);

  // Showing the dialog and waiting response
  gtk_dialog_run (GTK_DIALOG (dlg));

  // Closing and freeing memory
  gtk_widget_destroy (GTK_WIDGET (dlg));

#else
  printf ("%s: %s\n", title, msg);
#endif
}

/**
 * \fn void show_error(char *msg)
 * \brief Function to show a dialog with an error message.
 * \param msg
 * \brief Error message.
 */
void
show_error (char *msg)
{
  show_message (gettext ("ERROR!"), msg);
}

/**
 * \fn int xml_node_get_int(xmlNode *node, const xmlChar *prop, int *error_code)
 * \brief Function to get an integer number of a XML node property.
 * \param node
 * \brief XML node.
 * \param prop
 * \brief XML property.
 * \param error_code
 * \brief Error code.
 * \return Integer number value.
 */
int
xml_node_get_int (xmlNode * node, const xmlChar * prop, int *error_code)
{
  int i = 0;
  xmlChar *buffer;
  buffer = xmlGetProp (node, prop);
  if (!buffer)
    *error_code = 1;
  else
    {
      if (sscanf ((char *) buffer, "%d", &i) != 1)
        *error_code = 2;
      else
        *error_code = 0;
      xmlFree (buffer);
    }
  return i;
}

/**
 * \fn int xml_node_get_uint(xmlNode *node, const xmlChar *prop, \
 *   int *error_code)
 * \brief Function to get an unsigned integer number of a XML node property.
 * \param node
 * \brief XML node.
 * \param prop
 * \brief XML property.
 * \param error_code
 * \brief Error code.
 * \return Unsigned integer number value.
 */
unsigned int
xml_node_get_uint (xmlNode * node, const xmlChar * prop, int *error_code)
{
  unsigned int i = 0;
  xmlChar *buffer;
  buffer = xmlGetProp (node, prop);
  if (!buffer)
    *error_code = 1;
  else
    {
      if (sscanf ((char *) buffer, "%u", &i) != 1)
        *error_code = 2;
      else
        *error_code = 0;
      xmlFree (buffer);
    }
  return i;
}

/**
 * \fn double xml_node_get_float(xmlNode *node, const xmlChar *prop, \
 *   int *error_code)
 * \brief Function to get a floating point number of a XML node property.
 * \param node
 * \brief XML node.
 * \param prop
 * \brief XML property.
 * \param error_code
 * \brief Error code.
 * \return Floating point number value.
 */
double
xml_node_get_float (xmlNode * node, const xmlChar * prop, int *error_code)
{
  double x = 0.;
  xmlChar *buffer;
  buffer = xmlGetProp (node, prop);
  if (!buffer)
    *error_code = 1;
  else
    {
      if (sscanf ((char *) buffer, "%lf", &x) != 1)
        *error_code = 2;
      else
        *error_code = 0;
      xmlFree (buffer);
    }
  return x;
}

/**
 * \fn void xml_node_set_int(xmlNode *node, const xmlChar *prop, int value)
 * \brief Function to set an integer number in a XML node property.
 * \param node
 * \brief XML node.
 * \param prop
 * \brief XML property.
 * \param value
 * \brief Integer number value.
 */
void
xml_node_set_int (xmlNode * node, const xmlChar * prop, int value)
{
  xmlChar buffer[64];
  snprintf ((char *) buffer, 64, "%d", value);
  xmlSetProp (node, prop, buffer);
}

/**
 * \fn void xml_node_set_uint(xmlNode *node, const xmlChar *prop, \
 *   unsigned int value)
 * \brief Function to set an unsigned integer number in a XML node property.
 * \param node
 * \brief XML node.
 * \param prop
 * \brief XML property.
 * \param value
 * \brief Unsigned integer number value.
 */
void
xml_node_set_uint (xmlNode * node, const xmlChar * prop, unsigned int value)
{
  xmlChar buffer[64];
  snprintf ((char *) buffer, 64, "%u", value);
  xmlSetProp (node, prop, buffer);
}

/**
 * \fn void xml_node_set_float(xmlNode *node, const xmlChar *prop, \
 *   double value)
 * \brief Function to set a floating point number in a XML node property.
 * \param node
 * \brief XML node.
 * \param prop
 * \brief XML property.
 * \param value
 * \brief Floating point number value.
 */
void
xml_node_set_float (xmlNode * node, const xmlChar * prop, double value)
{
  xmlChar buffer[64];
  snprintf ((char *) buffer, 64, "%.14lg", value);
  xmlSetProp (node, prop, buffer);
}

/**
 * \fn void input_new()
 * \brief Function to create a new Input struct.
 */
void
input_new ()
{
  unsigned int i;
  input->nvariables = input->nexperiments = input->ninputs = 0;
  input->simulator = input->evaluator = input->directory = input->name = NULL;
  input->experiment = input->label = input->format = NULL;
  input->nsweeps = input->nbits = NULL;
  input->rangemin = input->rangemax = input->rangeminabs = input->rangemaxabs
    = input->weight = NULL;
  for (i = 0; i < MAX_NINPUTS; ++i)
    input->template[i] = NULL;
}

/**
 * \fn int input_open(char *filename)
 * \brief Function to open the input file.
 * \param filename
 * \brief Input data file name.
 * \return 1 on success, 0 on error.
 */
int
input_open (char *filename)
{
  int error_code;
  unsigned int i;
  xmlChar *buffer;
  xmlDoc *doc;
  xmlNode *node, *child;

#if DEBUG
  fprintf (stderr, "input_new: start\n");
#endif

  // Resetting input data
  input_new ();

  // Parsing the input file
  doc = xmlParseFile (filename);
  if (!doc)
    {
      show_error (gettext ("Unable to parse the input file"));
      return 0;
    }

  // Getting the root node
  node = xmlDocGetRootElement (doc);
  if (xmlStrcmp (node->name, XML_CALIBRATE))
    {
      show_error (gettext ("Bad root XML node"));
      return 0;
    }

  // Opening simulator program name
  input->simulator = (char *) xmlGetProp (node, XML_SIMULATOR);
  if (!input->simulator)
    {
      show_error (gettext ("Bad simulator program"));
      return 0;
    }

  // Opening evaluator program name
  input->evaluator = (char *) xmlGetProp (node, XML_EVALUATOR);

  // Opening algorithm
  buffer = xmlGetProp (node, XML_ALGORITHM);
  if (!xmlStrcmp (buffer, XML_MONTE_CARLO))
    {
      input->algorithm = ALGORITHM_MONTE_CARLO;

      // Obtaining simulations number
      input->nsimulations =
        xml_node_get_int (node, XML_NSIMULATIONS, &error_code);
      if (error_code)
        {
          show_error (gettext ("Bad simulations number"));
          return 0;
        }
    }
  else if (!xmlStrcmp (buffer, XML_SWEEP))
    input->algorithm = ALGORITHM_SWEEP;
  else if (!xmlStrcmp (buffer, XML_GENETIC))
    {
      input->algorithm = ALGORITHM_GENETIC;

      // Obtaining population
      if (xmlHasProp (node, XML_NPOPULATION))
        {
          input->nsimulations =
            xml_node_get_uint (node, XML_NPOPULATION, &error_code);
          if (error_code || input->nsimulations < 3)
            {
              show_error (gettext ("Invalid population number"));
              return 0;
            }
        }
      else
        {
          show_error (gettext ("No population number"));
          return 0;
        }

      // Obtaining generations
      if (xmlHasProp (node, XML_NGENERATIONS))
        {
          input->niterations =
            xml_node_get_uint (node, XML_NGENERATIONS, &error_code);
          if (error_code || !input->niterations)
            {
              show_error (gettext ("Invalid generation number"));
              return 0;
            }
        }
      else
        {
          show_error (gettext ("No generation number"));
          return 0;
        }

      // Obtaining mutation probability
      if (xmlHasProp (node, XML_MUTATION))
        {
          input->mutation_ratio =
            xml_node_get_float (node, XML_MUTATION, &error_code);
          if (error_code || input->mutation_ratio < 0.
              || input->mutation_ratio >= 1.)
            {
              show_error (gettext ("Invalid mutation probability"));
              return 0;
            }
        }
      else
        {
          show_error (gettext ("No mutation probability"));
          return 0;
        }

      // Obtaining reproduction probability
      if (xmlHasProp (node, XML_REPRODUCTION))
        {
          input->reproduction_ratio =
            xml_node_get_float (node, XML_REPRODUCTION, &error_code);
          if (error_code || input->reproduction_ratio < 0.
              || input->reproduction_ratio >= 1.0)
            {
              show_error (gettext ("Invalid reproduction probability"));
              return 0;
            }
        }
      else
        {
          show_error (gettext ("No reproduction probability"));
          return 0;
        }

      // Obtaining adaptation probability
      if (xmlHasProp (node, XML_ADAPTATION))
        {
          input->adaptation_ratio =
            xml_node_get_float (node, XML_ADAPTATION, &error_code);
          if (error_code || input->adaptation_ratio < 0.
              || input->adaptation_ratio >= 1.)
            {
              show_error (gettext ("Invalid adaptation probability"));
              return 0;
            }
        }
      else
        {
          show_error (gettext ("No adaptation probability"));
          return 0;
        }

      // Checking survivals
      i = input->mutation_ratio * input->nsimulations;
      i += input->reproduction_ratio * input->nsimulations;
      i += input->adaptation_ratio * input->nsimulations;
      if (i > input->nsimulations - 2)
        {
          show_error (gettext
                      ("No enough survival entities to reproduce the population"));
          return 0;
        }
    }
  else
    {
      show_error (gettext ("Unknown algorithm"));
      return 0;
    }

  if (input->algorithm == ALGORITHM_MONTE_CARLO
      || input->algorithm == ALGORITHM_SWEEP)
    {

      // Obtaining iterations number
      input->niterations =
        xml_node_get_int (node, XML_NITERATIONS, &error_code);
      if (error_code == 1)
        input->niterations = 1;
      else if (error_code)
        {
          show_error (gettext ("Bad iterations number"));
          return 0;
        }

      // Obtaining best number
      if (xmlHasProp (node, XML_NBEST))
        {
          input->nbest = xml_node_get_uint (node, XML_NBEST, &error_code);
          if (error_code || !input->nbest)
            {
              show_error (gettext ("Invalid best number"));
              return 0;
            }
        }
      else
        input->nbest = 1;

      // Obtaining tolerance
      if (xmlHasProp (node, XML_TOLERANCE))
        {
          input->tolerance =
            xml_node_get_float (node, XML_TOLERANCE, &error_code);
          if (error_code || input->tolerance < 0.)
            {
              show_error (gettext ("Invalid tolerance"));
              return 0;
            }
        }
      else
        input->tolerance = 0.;
    }

  // Reading the experimental data
  for (child = node->children; child; child = child->next)
    {
      if (xmlStrcmp (child->name, XML_EXPERIMENT))
        break;
#if DEBUG
      fprintf (stderr, "input_new: nexperiments=%u\n", input->nexperiments);
#endif
      if (xmlHasProp (child, XML_NAME))
        {
          input->experiment =
            g_realloc (input->experiment,
                       (1 + input->nexperiments) * sizeof (char *));
          input->experiment[input->nexperiments] =
            (char *) xmlGetProp (child, XML_NAME);
        }
      else
        {
          show_error (gettext ("No experiment file name"));
          return 0;
        }
#if DEBUG
      fprintf (stderr, "input_new: experiment=%s\n",
               input->experiment[input->nexperiments]);
#endif
      input->weight =
        g_realloc (input->weight, (1 + input->nexperiments) * sizeof (double));
      if (xmlHasProp (child, XML_WEIGHT))
        input->weight[input->nexperiments] =
          xml_node_get_float (child, XML_WEIGHT, &error_code);
      else
        input->weight[input->nexperiments] = 1.;
#if DEBUG
      fprintf (stderr, "input_new: weight=%lg\n",
               input->weight[input->nexperiments]);
#endif
      if (!input->nexperiments)
        input->ninputs = 0;
#if DEBUG
      fprintf (stderr, "input_new: template[0]\n");
#endif
      if (xmlHasProp (child, XML_TEMPLATE1))
        {
          input->template[0] =
            g_realloc (input->template[0],
                       (1 + input->nexperiments) * sizeof (char *));
          input->template[0][input->nexperiments] =
            (char *) xmlGetProp (child, template[0]);
#if DEBUG
          fprintf (stderr, "input_new: experiment=%u template1=%s\n",
                   input->nexperiments,
                   input->template[0][input->nexperiments]);
#endif
          if (!input->nexperiments)
            ++input->ninputs;
#if DEBUG
          fprintf (stderr, "input_new: ninputs=%u\n", input->ninputs);
#endif
        }
      else
        {
          show_error (gettext ("No experiment template"));
          return 0;
        }
      for (i = 1; i < MAX_NINPUTS; ++i)
        {
#if DEBUG
          fprintf (stderr, "input_new: template%u\n", i + 1);
#endif
          if (xmlHasProp (child, template[i]))
            {
              if (input->nexperiments && input->ninputs < 2)
                {
                  printf ("Experiment %u: bad templates number\n",
                          input->nexperiments + 1);
                  return 0;
                }
              input->template[i] =
                g_realloc (input->template[i],
                           (1 + input->nexperiments) * sizeof (char *));
              input->template[i][input->nexperiments] =
                (char *) xmlGetProp (child, template[i]);
#if DEBUG
              fprintf (stderr, "input_new: experiment=%u template%u=%s\n",
                       input->nexperiments, i + 1,
                       input->template[i][input->nexperiments]);
#endif
              if (!input->nexperiments)
                ++input->ninputs;
#if DEBUG
              fprintf (stderr, "input_new: ninputs=%u\n", input->ninputs);
#endif
            }
          else if (input->nexperiments && input->ninputs > 1)
            {
              printf ("No experiment %u template%u\n",
                      input->nexperiments + 1, i + 1);
              return 0;
            }
          else
            break;
        }
      ++input->nexperiments;
#if DEBUG
      fprintf (stderr, "input_new: nexperiments=%u\n", input->nexperiments);
#endif
    }
  if (!input->nexperiments)
    {
      show_error (gettext ("No calibration experiments"));
      return 0;
    }

  // Reading the variables data
  for (; child; child = child->next)
    {
      if (xmlStrcmp (child->name, XML_VARIABLE))
        {
          show_error (gettext ("Bad XML node"));
          return 0;
        }
      if (xmlHasProp (child, XML_NAME))
        {
          input->label = g_realloc
            (input->label, (1 + input->nvariables) * sizeof (char *));
          input->label[input->nvariables] =
            (char *) xmlGetProp (child, XML_NAME);
        }
      else
        {
          show_error (gettext ("No variable name"));
          return 0;
        }
      if (xmlHasProp (child, XML_MINIMUM))
        {
          input->rangemin = g_realloc
            (input->rangemin, (1 + input->nvariables) * sizeof (double));
          input->rangeminabs = g_realloc
            (input->rangeminabs, (1 + input->nvariables) * sizeof (double));
          input->rangemin[input->nvariables] =
            xml_node_get_float (child, XML_MINIMUM, &error_code);
          if (xmlHasProp (child, XML_ABSOLUTE_MINIMUM))
            {
              input->rangeminabs[input->nvariables] =
                xml_node_get_float (child, XML_ABSOLUTE_MINIMUM, &error_code);
            }
          else
            input->rangeminabs[input->nvariables] = -INFINITY;
        }
      else
        {
          show_error (gettext ("No minimum range"));
          return 0;
        }
      if (xmlHasProp (child, XML_MAXIMUM))
        {
          input->rangemax = g_realloc
            (input->rangemax, (1 + input->nvariables) * sizeof (double));
          input->rangemaxabs = g_realloc
            (input->rangemaxabs, (1 + input->nvariables) * sizeof (double));
          input->rangemax[input->nvariables] =
            xml_node_get_float (child, XML_MAXIMUM, &error_code);
          if (xmlHasProp (child, XML_ABSOLUTE_MAXIMUM))
            input->rangemaxabs[input->nvariables]
              = xml_node_get_float (child, XML_ABSOLUTE_MINIMUM, &error_code);
          else
            input->rangemaxabs[input->nvariables] = INFINITY;
        }
      else
        {
          show_error (gettext ("No maximum range"));
          return 0;
        }
      input->format = g_realloc (input->format,
                                 (1 + input->nvariables) * sizeof (char *));
      if (xmlHasProp (child, XML_FORMAT))
        {
          input->format[input->nvariables] =
            (char *) xmlGetProp (child, XML_FORMAT);
        }
      else
        {
          input->format[input->nvariables] =
            (char *) xmlStrdup (DEFAULT_FORMAT);
        }
      if (input->algorithm == ALGORITHM_SWEEP)
        {
          if (xmlHasProp (child, XML_NSWEEPS))
            {
              input->nsweeps =
                g_realloc (input->nsweeps,
                           (1 + input->nvariables) * sizeof (unsigned int));
              input->nsweeps[input->nvariables] =
                xml_node_get_uint (child, XML_NSWEEPS, &error_code);
            }
          else
            {
              show_error (gettext ("No sweeps number"));
              return 0;
            }
#if DEBUG
          fprintf (stderr, "input_new: nsweeps=%u nsimulations=%u\n",
                   input->nsweeps[input->nvariables], input->nsimulations);
#endif
        }
      if (input->algorithm == ALGORITHM_GENETIC)
        {
          // Obtaining bits representing each variable
          if (xmlHasProp (child, XML_NBITS))
            {
              input->nbits =
                g_realloc (input->nbits,
                           (1 + input->nvariables) * sizeof (unsigned int));
              i = xml_node_get_uint (child, XML_NBITS, &error_code);
              if (error_code || !i)
                {
                  show_error (gettext ("Invalid bit number"));
                  return 0;
                }
              input->nbits[input->nvariables] = i;
            }
          else
            {
              show_error (gettext ("No bits number"));
              return 0;
            }
        }
      ++input->nvariables;
    }
  if (!input->nvariables)
    {
      show_error (gettext ("No calibration variables"));
      return 0;
    }

  // Getting the working directory
  input->directory = g_path_get_dirname (filename);
  input->name = g_path_get_basename (filename);

  // Closing the XML document
  xmlFreeDoc (doc);

#if DEBUG
  fprintf (stderr, "input_new: end\n");
#endif

  return 1;
}

/**
 * \fn void input_free()
 * \brief Function to free the memory of the input file data.
 */
void
input_free ()
{
  unsigned int i, j;
#if DEBUG
  fprintf (stderr, "input_free: start\n");
#endif
  g_free (input->name);
  g_free (input->directory);
  for (i = 0; i < input->nexperiments; ++i)
    {
      xmlFree (input->experiment[i]);
      for (j = 0; j < input->ninputs; ++j)
        xmlFree (input->template[j][i]);
    }
  g_free (input->experiment);
  for (i = 0; i < input->ninputs; ++i)
    g_free (input->template[i]);
  for (i = 0; i < input->nvariables; ++i)
    {
      xmlFree (input->label[i]);
      xmlFree (input->format[i]);
    }
  g_free (input->label);
  g_free (input->format);
  g_free (input->rangemin);
  g_free (input->rangemax);
  g_free (input->rangeminabs);
  g_free (input->rangemaxabs);
  g_free (input->weight);
  g_free (input->nsweeps);
  g_free (input->nbits);
  xmlFree (input->evaluator);
  xmlFree (input->simulator);
  input->nexperiments = input->ninputs = input->nvariables = 0;
#if DEBUG
  fprintf (stderr, "input_free: end\n");
#endif
}

/**
 * \fn void calibrate_input(unsigned int simulation, char *input, \
 *   GMappedFile *template)
 * \brief Function to write the simulation input file.
 * \param simulation
 * \brief Simulation number.
 * \param input
 * \brief Input file name.
 * \param template
 * \brief Template of the input file name.
 */
void
calibrate_input (unsigned int simulation, char *input, GMappedFile * template)
{
  unsigned int i;
  char buffer[32], value[32], *buffer2, *buffer3, *content;
  FILE *file;
  gsize length;
  GRegex *regex;

#if DEBUG
  fprintf (stderr, "calibrate_input: start\n");
#endif

  // Checking the file
  if (!template)
    goto calibrate_input_end;

  // Opening template
  content = g_mapped_file_get_contents (template);
  length = g_mapped_file_get_length (template);
#if DEBUG
  fprintf (stderr, "calibrate_input: length=%lu\ncontent:\n%s", length,
           content);
#endif
  file = fopen (input, "w");

  // Parsing template
  for (i = 0; i < calibrate->nvariables; ++i)
    {
#if DEBUG
      fprintf (stderr, "calibrate_input: variable=%u\n", i);
#endif
      snprintf (buffer, 32, "@variable%u@", i + 1);
      regex = g_regex_new (buffer, 0, 0, NULL);
      if (i == 0)
        {
          buffer2 = g_regex_replace_literal (regex, content, length, 0,
                                             calibrate->label[i], 0, NULL);
#if DEBUG
          fprintf (stderr, "calibrate_input: buffer2\n%s", buffer2);
#endif
        }
      else
        {
          length = strlen (buffer3);
          buffer2 = g_regex_replace_literal (regex, buffer3, length, 0,
                                             calibrate->label[i], 0, NULL);
          g_free (buffer3);
        }
      g_regex_unref (regex);
      length = strlen (buffer2);
      snprintf (buffer, 32, "@value%u@", i + 1);
      regex = g_regex_new (buffer, 0, 0, NULL);
      snprintf (value, 32, calibrate->format[i],
                calibrate->value[simulation * calibrate->nvariables + i]);

#if DEBUG
      fprintf (stderr, "calibrate_input: value=%s\n", value);
#endif
      buffer3 = g_regex_replace_literal (regex, buffer2, length, 0, value,
                                         0, NULL);
      g_free (buffer2);
      g_regex_unref (regex);
    }

  // Saving input file
  fwrite (buffer3, strlen (buffer3), sizeof (char), file);
  g_free (buffer3);
  fclose (file);

calibrate_input_end:
#if DEBUG
  fprintf (stderr, "calibrate_input: end\n");
#endif
  return;
}

/**
 * \fn double calibrate_parse(unsigned int simulation, unsigned int experiment)
 * \brief Function to parse input files, simulating and calculating the \
 *   objective function.
 * \param simulation
 * \brief Simulation number.
 * \param experiment
 * \brief Experiment number.
 * \return Objective function value.
 */
double
calibrate_parse (unsigned int simulation, unsigned int experiment)
{
  unsigned int i;
  double e;
  char buffer[512], input[MAX_NINPUTS][32], output[32], result[32];
  FILE *file_result;

#if DEBUG
  fprintf (stderr, "calibrate_parse: start\n");
  fprintf (stderr, "calibrate_parse: simulation=%u experiment=%u\n", simulation,
           experiment);
#endif

  // Opening input files
  for (i = 0; i < calibrate->ninputs; ++i)
    {
      snprintf (&input[i][0], 32, "input-%u-%u-%u", i, simulation, experiment);
#if DEBUG
      fprintf (stderr, "calibrate_parse: i=%u input=%s\n", i, &input[i][0]);
#endif
      calibrate_input (simulation, &input[i][0],
                       calibrate->file[i][experiment]);
    }
  for (; i < MAX_NINPUTS; ++i)
    strcpy (&input[i][0], "");
#if DEBUG
  fprintf (stderr, "calibrate_parse: parsing end\n");
#endif

  // Performing the simulation
  snprintf (output, 32, "output-%u-%u", simulation, experiment);
  snprintf (buffer, 512, "./%s %s %s %s %s %s %s %s %s %s",
            calibrate->simulator,
            &input[0][0], &input[1][0], &input[2][0], &input[3][0],
            &input[4][0], &input[5][0], &input[6][0], &input[7][0], output);
#if DEBUG
  fprintf (stderr, "calibrate_parse: %s\n", buffer);
#endif
  system (buffer);

  // Checking the objective value function
  if (calibrate->evaluator)
    {
      snprintf (result, 32, "result-%u-%u", simulation, experiment);
      snprintf (buffer, 512, "./%s %s %s %s", calibrate->evaluator, output,
                calibrate->experiment[experiment], result);
#if DEBUG
      fprintf (stderr, "calibrate_parse: %s\n", buffer);
#endif
      system (buffer);
      file_result = fopen (result, "r");
      e = atof (fgets (buffer, 512, file_result));
      fclose (file_result);
    }
  else
    {
      strcpy (result, "");
      file_result = fopen (output, "r");
      e = atof (fgets (buffer, 512, file_result));
      fclose (file_result);
    }

  // Removing files
#if !DEBUG
  for (i = 0; i < calibrate->ninputs; ++i)
    {
      if (calibrate->file[i][0])
        {
          snprintf (buffer, 512, "rm %s", &input[i][0]);
          system (buffer);
        }
    }
  snprintf (buffer, 512, "rm %s %s", output, result);
  system (buffer);
#endif

#if DEBUG
  fprintf (stderr, "calibrate_parse: end\n");
#endif

  // Returning the objective function
  return e * calibrate->weight[experiment];
}

/**
 * \fn void calibrate_best_thread(unsigned int simulation, double value)
 * \brief Function to save the best simulations of a thread.
 * \param simulation
 * \brief Simulation number.
 * \param value
 * \brief Objective function value.
 */
void
calibrate_best_thread (unsigned int simulation, double value)
{
  unsigned int i, j;
  double e;
#if DEBUG
  fprintf (stderr, "calibrate_best_thread: start\n");
#endif
  if (calibrate->nsaveds < calibrate->nbest
      || value < calibrate->error_best[calibrate->nsaveds - 1])
    {
      g_mutex_lock (mutex);
      if (calibrate->nsaveds < calibrate->nbest)
        ++calibrate->nsaveds;
      calibrate->error_best[calibrate->nsaveds - 1] = value;
      calibrate->simulation_best[calibrate->nsaveds - 1] = simulation;
      for (i = calibrate->nsaveds; --i;)
        {
          if (calibrate->error_best[i] < calibrate->error_best[i - 1])
            {
              j = calibrate->simulation_best[i];
              e = calibrate->error_best[i];
              calibrate->simulation_best[i] = calibrate->simulation_best[i - 1];
              calibrate->error_best[i] = calibrate->error_best[i - 1];
              calibrate->simulation_best[i - 1] = j;
              calibrate->error_best[i - 1] = e;
            }
          else
            break;
        }
      g_mutex_unlock (mutex);
    }
#if DEBUG
  fprintf (stderr, "calibrate_best_thread: end\n");
#endif
}

/**
 * \fn void calibrate_best_sequential(unsigned int simulation, double value)
 * \brief Function to save the best simulations.
 * \param simulation
 * \brief Simulation number.
 * \param value
 * \brief Objective function value.
 */
void
calibrate_best_sequential (unsigned int simulation, double value)
{
  unsigned int i, j;
  double e;
#if DEBUG
  fprintf (stderr, "calibrate_best_sequential: start\n");
#endif
  if (calibrate->nsaveds < calibrate->nbest
      || value < calibrate->error_best[calibrate->nsaveds - 1])
    {
      if (calibrate->nsaveds < calibrate->nbest)
        ++calibrate->nsaveds;
      calibrate->error_best[calibrate->nsaveds - 1] = value;
      calibrate->simulation_best[calibrate->nsaveds - 1] = simulation;
      for (i = calibrate->nsaveds; --i;)
        {
          if (calibrate->error_best[i] < calibrate->error_best[i - 1])
            {
              j = calibrate->simulation_best[i];
              e = calibrate->error_best[i];
              calibrate->simulation_best[i] = calibrate->simulation_best[i - 1];
              calibrate->error_best[i] = calibrate->error_best[i - 1];
              calibrate->simulation_best[i - 1] = j;
              calibrate->error_best[i - 1] = e;
            }
          else
            break;
        }
    }
#if DEBUG
  fprintf (stderr, "calibrate_best_sequential: end\n");
#endif
}

/**
 * \fn void* calibrate_thread(ParallelData *data)
 * \brief Function to calibrate on a thread.
 * \param data
 * \brief Function data.
 * \return NULL
 */
void *
calibrate_thread (ParallelData * data)
{
  unsigned int i, j, thread;
  double e;
#if DEBUG
  fprintf (stderr, "calibrate_thread: start\n");
#endif
  thread = data->thread;
#if DEBUG
  fprintf (stderr, "calibrate_thread: thread=%u start=%u end=%u\n", thread,
           calibrate->thread[thread], calibrate->thread[thread + 1]);
#endif
  for (i = calibrate->thread[thread]; i < calibrate->thread[thread + 1]; ++i)
    {
      e = 0.;
      for (j = 0; j < calibrate->nexperiments; ++j)
        e += calibrate_parse (i, j);
      calibrate_best_thread (i, e);
#if DEBUG
      fprintf (stderr, "calibrate_thread: i=%u e=%lg\n", i, e);
#endif
    }
#if DEBUG
  fprintf (stderr, "calibrate_thread: end\n");
#endif
  g_thread_exit (NULL);
  return NULL;
}

/**
 * \fn void calibrate_sequential()
 * \brief Function to calibrate sequentially.
 */
void
calibrate_sequential ()
{
  unsigned int i, j;
  double e;
#if DEBUG
  fprintf (stderr, "calibrate_sequential: start\n");
  fprintf (stderr, "calibrate_sequential: nstart=%u nend=%u\n",
           calibrate->nstart, calibrate->nend);
#endif
  for (i = calibrate->nstart; i < calibrate->nend; ++i)
    {
      e = 0.;
      for (j = 0; j < calibrate->nexperiments; ++j)
        e += calibrate_parse (i, j);
      calibrate_best_sequential (i, e);
#if DEBUG
      fprintf (stderr, "calibrate_sequential: i=%u e=%lg\n", i, e);
#endif
    }
#if DEBUG
  fprintf (stderr, "calibrate_sequential: end\n");
#endif
}

/**
 * \fn void calibrate_merge(unsigned int nsaveds, \
 *   unsigned int *simulation_best, double *error_best)
 * \brief Function to merge the 2 calibration results.
 * \param nsaveds
 * \brief Number of saved results.
 * \param simulation_best
 * \brief Array of best simulation numbers.
 * \param error_best
 * \brief Array of best objective function values.
 */
void
calibrate_merge (unsigned int nsaveds, unsigned int *simulation_best,
                 double *error_best)
{
  unsigned int i, j, k, s[calibrate->nbest];
  double e[calibrate->nbest];
#if DEBUG
  fprintf (stderr, "calibrate_merge: start\n");
#endif
  i = j = k = 0;
  do
    {
      if (i == calibrate->nsaveds)
        {
          s[k] = simulation_best[j];
          e[k] = error_best[j];
          ++j;
          ++k;
          if (j == nsaveds)
            break;
        }
      else if (j == nsaveds)
        {
          s[k] = calibrate->simulation_best[i];
          e[k] = calibrate->error_best[i];
          ++i;
          ++k;
          if (i == calibrate->nsaveds)
            break;
        }
      else if (calibrate->error_best[i] > error_best[j])
        {
          s[k] = simulation_best[j];
          e[k] = error_best[j];
          ++j;
          ++k;
        }
      else
        {
          s[k] = calibrate->simulation_best[i];
          e[k] = calibrate->error_best[i];
          ++i;
          ++k;
        }
    }
  while (k < calibrate->nbest);
  calibrate->nsaveds = k;
  memcpy (calibrate->simulation_best, s, k * sizeof (unsigned int));
  memcpy (calibrate->error_best, e, k * sizeof (double));
#if DEBUG
  fprintf (stderr, "calibrate_merge: end\n");
#endif
}

/**
 * \fn void calibrate_synchronise()
 * \brief Function to synchronise the calibration results of MPI tasks.
 */
#if HAVE_MPI
void
calibrate_synchronise ()
{
  unsigned int i, nsaveds, simulation_best[calibrate->nbest];
  double error_best[calibrate->nbest];
  MPI_Status mpi_stat;
#if DEBUG
  fprintf (stderr, "calibrate_synchronise: start\n");
#endif
  if (calibrate->mpi_rank == 0)
    {
      for (i = 1; i < ntasks; ++i)
        {
          MPI_Recv (&nsaveds, 1, MPI_INT, i, 1, MPI_COMM_WORLD, &mpi_stat);
          MPI_Recv (simulation_best, nsaveds, MPI_INT, i, 1,
                    MPI_COMM_WORLD, &mpi_stat);
          MPI_Recv (error_best, nsaveds, MPI_DOUBLE, i, 1,
                    MPI_COMM_WORLD, &mpi_stat);
          calibrate_merge (nsaveds, simulation_best, error_best);
        }
    }
  else
    {
      MPI_Send (&calibrate->nsaveds, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
      MPI_Send (calibrate->simulation_best, calibrate->nsaveds, MPI_INT, 0, 1,
                MPI_COMM_WORLD);
      MPI_Send (calibrate->error_best, calibrate->nsaveds, MPI_DOUBLE, 0, 1,
                MPI_COMM_WORLD);
    }
#if DEBUG
  fprintf (stderr, "calibrate_synchronise: end\n");
#endif
}
#endif

/**
 * \fn void calibrate_sweep()
 * \brief Function to calibrate with the sweep algorithm.
 */
void
calibrate_sweep ()
{
  unsigned int i, j, k, l;
  double e;
  GThread *thread[nthreads];
  ParallelData data[nthreads];
#if DEBUG
  fprintf (stderr, "calibrate_sweep: start\n");
#endif
  for (i = 0; i < calibrate->nsimulations; ++i)
    {
      k = i;
      for (j = 0; j < calibrate->nvariables; ++j)
        {
          l = k % calibrate->nsweeps[j];
          k /= calibrate->nsweeps[j];
          e = calibrate->rangemin[j];
          if (calibrate->nsweeps[j] > 1)
            e += l * (calibrate->rangemax[j] - calibrate->rangemin[j])
              / (calibrate->nsweeps[j] - 1);
          calibrate->value[i * calibrate->nvariables + j] = e;
        }
    }
  calibrate->nsaveds = 0;
  if (nthreads <= 1)
    calibrate_sequential ();
  else
    {
      for (i = 0; i < nthreads; ++i)
        {
          data[i].thread = i;
#if GLIB_MINOR_VERSION >= 32
          thread[i] =
            g_thread_new (NULL, (void (*)) calibrate_thread, &data[i]);
#else
          thread[i] =
            g_thread_create ((void (*)) calibrate_thread, &data[i], TRUE, NULL);
#endif
        }
      for (i = 0; i < nthreads; ++i)
        g_thread_join (thread[i]);
    }
#if HAVE_MPI
  // Communicating tasks results
  calibrate_synchronise ();
#endif
#if DEBUG
  fprintf (stderr, "calibrate_sweep: end\n");
#endif
}

/**
 * \fn void calibrate_MonteCarlo()
 * \brief Function to calibrate with the Monte-Carlo algorithm.
 */
void
calibrate_MonteCarlo ()
{
  unsigned int i, j;
  GThread *thread[nthreads];
  ParallelData data[nthreads];
#if DEBUG
  fprintf (stderr, "calibrate_MonteCarlo: start\n");
#endif
  for (i = 0; i < calibrate->nsimulations; ++i)
    for (j = 0; j < calibrate->nvariables; ++j)
      calibrate->value[i * calibrate->nvariables + j] =
        calibrate->rangemin[j] + gsl_rng_uniform (calibrate->rng)
        * (calibrate->rangemax[j] - calibrate->rangemin[j]);
  calibrate->nsaveds = 0;
  if (nthreads <= 1)
    calibrate_sequential ();
  else
    {
      for (i = 0; i < nthreads; ++i)
        {
          data[i].thread = i;
#if GLIB_MINOR_VERSION >= 32
          thread[i] =
            g_thread_new (NULL, (void (*)) calibrate_thread, &data[i]);
#else
          thread[i] =
            g_thread_create ((void (*)) calibrate_thread, &data[i], TRUE, NULL);
#endif
        }
      for (i = 0; i < nthreads; ++i)
        g_thread_join (thread[i]);
    }
#if HAVE_MPI
  // Communicating tasks results
  calibrate_synchronise ();
#endif
#if DEBUG
  fprintf (stderr, "calibrate_MonteCarlo: end\n");
#endif
}

/**
 * \fn double calibrate_genetic_objective(Entity *entity)
 * \brief Function to calculate the objective function of an entity.
 * \param entity
 * \brief entity data.
 * \return objective function value.
 */
double
calibrate_genetic_objective (Entity * entity)
{
  unsigned int j;
  double objective;
#if DEBUG
  fprintf (stderr, "calibrate_genetic_objective: start\n");
#endif
  for (j = 0; j < calibrate->nvariables; ++j)
    calibrate->value[entity->id * calibrate->nvariables + j]
      = genetic_get_variable (entity, calibrate->genetic_variable + j);
  for (j = 0, objective = 0.; j < calibrate->nexperiments; ++j)
    objective += calibrate_parse (entity->id, j);
#if DEBUG
  fprintf (stderr, "calibrate_genetic_objective: end\n");
#endif
  return objective;
}

/**
 * \fn void calibrate_genetic()
 * \brief Function to calibrate with the genetic algorithm.
 */
void
calibrate_genetic ()
{
  unsigned int i;
  char buffer[512], *best_genome;
  double best_objective, *best_variable;
#if DEBUG
  fprintf (stderr, "calibrate_genetic: start\n");
  fprintf (stderr, "calibrate_genetic: ntasks=%u nthreads=%u\n", ntasks,
           nthreads);
  fprintf (stderr,
           "calibrate_genetic: nvariables=%u population=%u generations=%u\n",
           calibrate->nvariables, calibrate->nsimulations,
           calibrate->niterations);
  fprintf (stderr,
           "calibrate_genetic: mutation=%lg reproduction=%lg adaptation=%lg\n",
           calibrate->mutation_ratio, calibrate->reproduction_ratio,
           calibrate->adaptation_ratio);
#endif
  genetic_algorithm_default (calibrate->nvariables,
                             calibrate->genetic_variable,
                             calibrate->nsimulations,
                             calibrate->niterations,
                             calibrate->mutation_ratio,
                             calibrate->reproduction_ratio,
                             calibrate->adaptation_ratio,
                             &calibrate_genetic_objective,
                             &best_genome, &best_variable, &best_objective);
#if DEBUG
  fprintf (stderr, "calibrate_genetic: the best\n");
#endif
  printf ("THE BEST IS\n");
  fprintf (calibrate->result, "THE BEST IS\n");
  printf ("error=%le\n", best_objective);
  fprintf (calibrate->result, "error=%le\n", best_objective);
  for (i = 0; i < calibrate->nvariables; ++i)
    {
      snprintf (buffer, 512, "%s=%s\n",
                calibrate->label[i], calibrate->format[i]);
      printf (buffer, best_variable[i]);
      fprintf (calibrate->result, buffer, best_variable[i]);
    }
  fflush (calibrate->result);
  g_free (best_genome);
  g_free (best_variable);
#if DEBUG
  fprintf (stderr, "calibrate_genetic: start\n");
#endif
}

/**
 * \fn void calibrate_print()
 * \brief Function to print the results.
 */
void
calibrate_print ()
{
  unsigned int i;
  char buffer[512];
#if HAVE_MPI
  if (!calibrate->mpi_rank)
    {
#endif
      printf ("THE BEST IS\n");
      fprintf (calibrate->result, "THE BEST IS\n");
      printf ("error=%le\n", calibrate->error_old[0]);
      fprintf (calibrate->result, "error=%le\n", calibrate->error_old[0]);
      for (i = 0; i < calibrate->nvariables; ++i)
        {
          snprintf (buffer, 512, "%s=%s\n",
                    calibrate->label[i], calibrate->format[i]);
          printf (buffer, calibrate->value_old[i]);
          fprintf (calibrate->result, buffer, calibrate->value_old[i]);
        }
      fflush (calibrate->result);
#if HAVE_MPI
    }
#endif
}

/**
 * \fn void calibrate_save_old()
 * \brief Function to save the best results on iterative methods.
 */
void
calibrate_save_old ()
{
  unsigned int i, j;
#if DEBUG
  fprintf (stderr, "calibrate_save_old: start\n");
#endif
  memcpy (calibrate->error_old, calibrate->error_best,
          calibrate->nbest * sizeof (double));
  for (i = 0; i < calibrate->nbest; ++i)
    {
      j = calibrate->simulation_best[i];
      memcpy (calibrate->value_old + i * calibrate->nvariables,
              calibrate->value + j * calibrate->nvariables,
              calibrate->nvariables * sizeof (double));
    }
#if DEBUG
  for (i = 0; i < calibrate->nvariables; ++i)
    fprintf (stderr, "calibrate_save_old: best variable %u=%lg\n",
             i, calibrate->value_old[i]);
  fprintf (stderr, "calibrate_save_old: end\n");
#endif
}

/**
 * \fn void calibrate_merge_old()
 * \brief Function to merge the best results with the previous step best results
 *   on iterative methods.
 */
void
calibrate_merge_old ()
{
  unsigned int i, j, k;
  double v[calibrate->nbest * calibrate->nvariables], e[calibrate->nbest],
    *enew, *eold;
#if DEBUG
  fprintf (stderr, "calibrate_merge_old: start\n");
#endif
  enew = calibrate->error_best;
  eold = calibrate->error_old;
  i = j = k = 0;
  do
    {
      if (*enew < *eold)
        {
          memcpy (v + k * calibrate->nvariables,
                  calibrate->value
                  + calibrate->simulation_best[i] * calibrate->nvariables,
                  calibrate->nvariables * sizeof (double));
          e[k] = *enew;
          ++k;
          ++enew;
          ++i;
        }
      else
        {
          memcpy (v + k * calibrate->nvariables,
                  calibrate->value_old + j * calibrate->nvariables,
                  calibrate->nvariables * sizeof (double));
          e[k] = *eold;
          ++k;
          ++eold;
          ++j;
        }
    }
  while (k < calibrate->nbest);
  memcpy (calibrate->value_old, v, k * calibrate->nvariables * sizeof (double));
  memcpy (calibrate->error_old, e, k * sizeof (double));
#if DEBUG
  fprintf (stderr, "calibrate_merge_old: end\n");
#endif
}

/**
 * \fn void calibrate_refine()
 * \brief Function to refine the search ranges of the variables in iterative
 *   algorithms.
 */
void
calibrate_refine ()
{
  unsigned int i, j;
  double d;
#if HAVE_MPI
  MPI_Status mpi_stat;
#endif
#if DEBUG
  fprintf (stderr, "calibrate_refine: start\n");
#endif
#if HAVE_MPI
  if (!calibrate->mpi_rank)
    {
#endif
      for (j = 0; j < calibrate->nvariables; ++j)
        {
          calibrate->rangemin[j] = calibrate->rangemax[j]
            = calibrate->value_old[j];
        }
      for (i = 0; ++i < calibrate->nbest;)
        {
          for (j = 0; j < calibrate->nvariables; ++j)
            {
              calibrate->rangemin[j] = fmin (calibrate->rangemin[j],
                                             calibrate->value_old[i *
                                                                  calibrate->nvariables
                                                                  + j]);
              calibrate->rangemax[j] =
                fmax (calibrate->rangemax[j],
                      calibrate->value_old[i * calibrate->nvariables + j]);
            }
        }
      for (j = 0; j < calibrate->nvariables; ++j)
        {
          d = 0.5 * calibrate->tolerance
            * (calibrate->rangemax[j] - calibrate->rangemin[j]);
          calibrate->rangemin[j] -= d;
          calibrate->rangemin[j]
            = fmax (calibrate->rangemin[j], calibrate->rangeminabs[j]);
          calibrate->rangemax[j] += d;
          calibrate->rangemax[j]
            = fmin (calibrate->rangemax[j], calibrate->rangemaxabs[j]);
          printf ("%s min=%lg max=%lg\n", calibrate->label[j],
                  calibrate->rangemin[j], calibrate->rangemax[j]);
          fprintf (calibrate->result, "%s min=%lg max=%lg\n",
                   calibrate->label[j], calibrate->rangemin[j],
                   calibrate->rangemax[j]);
        }
#if HAVE_MPI
      for (i = 1; i < ntasks; ++i)
        {
          MPI_Send (calibrate->rangemin, calibrate->nvariables, MPI_DOUBLE, i,
                    1, MPI_COMM_WORLD);
          MPI_Send (calibrate->rangemax, calibrate->nvariables, MPI_DOUBLE, i,
                    1, MPI_COMM_WORLD);
        }
    }
  else
    {
      MPI_Recv (calibrate->rangemin, calibrate->nvariables, MPI_DOUBLE, 0, 1,
                MPI_COMM_WORLD, &mpi_stat);
      MPI_Recv (calibrate->rangemax, calibrate->nvariables, MPI_DOUBLE, 0, 1,
                MPI_COMM_WORLD, &mpi_stat);
    }
#endif
#if DEBUG
  fprintf (stderr, "calibrate_refine: end\n");
#endif
}

/**
 * \fn void calibrate_iterate()
 * \brief Function to iterate the algorithm.
 */
void
calibrate_iterate ()
{
  unsigned int i;
#if DEBUG
  fprintf (stderr, "calibrate_iterate: start\n");
#endif
  calibrate->error_old
    = (double *) g_malloc (calibrate->nbest * sizeof (double));
  calibrate->value_old = (double *)
    g_malloc (calibrate->nbest * calibrate->nvariables * sizeof (double));
  calibrate_step ();
  calibrate_save_old ();
  calibrate_refine ();
  calibrate_print ();
  for (i = 1; i < calibrate->niterations; ++i)
    {
      calibrate_step ();
      calibrate_merge_old ();
      calibrate_refine ();
      calibrate_print ();
    }
  g_free (calibrate->error_old);
  g_free (calibrate->value_old);
#if DEBUG
  fprintf (stderr, "calibrate_iterate: end\n");
#endif
}

/**
 * \fn int calibrate_new(char *filename)
 * \brief Function to open and perform a calibration.
 * \param filename
 * \brief Input data file name.
 * \return 1 on success, 0 on error.
 */
int
calibrate_new (char *filename)
{
  unsigned int i, j, *nbits;

#if DEBUG
  fprintf (stderr, "calibrate_new: start\n");
#endif

  // Parsing the XML data file
  if (!input_open (filename))
    return 0;

  // Obtaining the simulator file
  calibrate->simulator = input->simulator;

  // Obtaining the evaluator file
  calibrate->evaluator = input->evaluator;

  // Reading the algorithm
  calibrate->algorithm = input->algorithm;
  switch (calibrate->algorithm)
    {
    case ALGORITHM_MONTE_CARLO:
      calibrate_step = calibrate_MonteCarlo;
      break;
    case ALGORITHM_SWEEP:
      calibrate_step = calibrate_sweep;
      break;
    default:
      calibrate_step = calibrate_genetic;
      calibrate->mutation_ratio = input->mutation_ratio;
      calibrate->reproduction_ratio = input->reproduction_ratio;
      calibrate->adaptation_ratio = input->adaptation_ratio;
    }
  calibrate->nsimulations = input->nsimulations;
  calibrate->niterations = input->niterations;
  calibrate->nbest = input->nbest;
  calibrate->tolerance = input->tolerance;

  calibrate->simulation_best =
    (unsigned int *) alloca (calibrate->nbest * sizeof (unsigned int));
  calibrate->error_best =
    (double *) alloca (calibrate->nbest * sizeof (double));

  // Reading the experimental data
  calibrate->nexperiments = input->nexperiments;
  calibrate->ninputs = input->ninputs;
  calibrate->experiment = input->experiment;
  calibrate->weight = input->weight;
  for (i = 0; i < input->ninputs; ++i)
    {
      calibrate->template[i] = input->template[i];
      calibrate->file[i] =
        g_malloc (input->nexperiments * sizeof (GMappedFile *));
    }
  for (i = 0; i < input->nexperiments; ++i)
    {
#if DEBUG
      fprintf (stderr, "calibrate_new: i=%u\n", i);
      fprintf (stderr, "calibrate_new: experiment=%s\n",
               calibrate->experiment[i]);
      fprintf (stderr, "calibrate_new: weight=%lg\n", calibrate->weight[i]);
#endif
      for (j = 0; j < input->ninputs; ++j)
        {
#if DEBUG
          fprintf (stderr, "calibrate_new: template%u\n", j + 1);
          fprintf (stderr, "calibrate_new: experiment=%u template%u=%s\n",
                   i, j + 1, calibrate->template[j][i]);
#endif
          calibrate->file[j][i] =
            g_mapped_file_new (input->template[j][i], 0, NULL);
        }
    }

  // Reading the variables data
  calibrate->nvariables = input->nvariables;
  calibrate->label = input->label;
  calibrate->rangemin = input->rangemin;
  calibrate->rangeminabs = input->rangeminabs;
  calibrate->rangemax = input->rangemax;
  calibrate->rangemaxabs = input->rangemaxabs;
  calibrate->format = input->format;
  calibrate->nsweeps = input->nsweeps;
  nbits = input->nbits;
  if (input->algorithm == ALGORITHM_SWEEP)
    calibrate->nsimulations = 1;
  else if (input->algorithm == ALGORITHM_GENETIC)
    for (i = 0; i < input->nvariables; ++i)
      {
        if (calibrate->algorithm == ALGORITHM_SWEEP)
          {
            calibrate->nsimulations *= input->nsweeps[i];
#if DEBUG
            fprintf (stderr, "calibrate_new: nsweeps=%u nsimulations=%u\n",
                     calibrate->nsweeps[i], calibrate->nsimulations);
#endif
          }
      }

  // Allocating values
  calibrate->genetic_variable = NULL;
  if (calibrate->algorithm == ALGORITHM_GENETIC)
    {
      calibrate->genetic_variable = (GeneticVariable *)
        g_malloc (calibrate->nvariables * sizeof (GeneticVariable));
      for (i = 0; i < calibrate->nvariables; ++i)
        {
          calibrate->genetic_variable[i].maximum = calibrate->rangemax[i];
          calibrate->genetic_variable[i].minimum = calibrate->rangemin[i];
          calibrate->genetic_variable[i].nbits = nbits[i];
        }
    }
  calibrate->value = (double *) g_malloc (calibrate->nsimulations *
                                          calibrate->nvariables *
                                          sizeof (double));

  // Calculating simulations to perform on each task
#if HAVE_MPI
  calibrate->nstart = calibrate->mpi_rank * calibrate->nsimulations / ntasks;
  calibrate->nend = (1 + calibrate->mpi_rank) * calibrate->nsimulations
    / ntasks;
#else
  calibrate->nstart = 0;
  calibrate->nend = calibrate->nsimulations;
#endif
#if DEBUG
  fprintf (stderr, "calibrate_new: nstart=%u nend=%u\n", calibrate->nstart,
           calibrate->nend);
#endif

  // Calculating simulations to perform on each thread
  calibrate->thread
    = (unsigned int *) alloca ((1 + nthreads) * sizeof (unsigned int));
  for (i = 0; i <= nthreads; ++i)
    {
      calibrate->thread[i] = calibrate->nstart
        + i * (calibrate->nend - calibrate->nstart) / nthreads;
#if DEBUG
      fprintf (stderr, "calibrate_new: i=%u thread=%u\n", i,
               calibrate->thread[i]);
#endif
    }

  // Opening result file
  calibrate->result = fopen ("result", "w");

  // Performing the algorithm
  switch (calibrate->algorithm)
    {
      // Genetic algorithm
    case ALGORITHM_GENETIC:
      calibrate_genetic ();
      break;

      // Iterative algorithm
    default:
      calibrate_iterate ();
    }

  // Closing result file
  fclose (calibrate->result);

  // Freeing memory
  input_free ();
  for (i = 0; i < calibrate->nexperiments; ++i)
    {
      for (j = 0; j < calibrate->ninputs; ++j)
        g_mapped_file_unref (calibrate->file[j][i]);
    }
  for (i = 0; i < calibrate->ninputs; ++i)
    g_free (calibrate->file[i]);
  g_free (calibrate->value);
  g_free (calibrate->genetic_variable);

#if DEBUG
  fprintf (stderr, "calibrate_new: end\n");
#endif

  return 1;
}

#if HAVE_GTK

/**
 * \fn void input_save(char *filename)
 * \brief Function to save the input file.
 * \param filename
 * \brief Input file name.
 */
void
input_save (char *filename)
{
  unsigned int i, j;
  char *buffer;
  xmlDoc *doc;
  xmlNode *node, *child;
  GFile *file, *file2;

  // Getting the input file directory
  input->directory = g_path_get_dirname (filename);
  file = g_file_new_for_path (input->directory);

  // Opening the input file
  doc = xmlNewDoc ((const xmlChar *) "1.0");

  // Setting root XML node
  node = xmlNewDocNode (doc, 0, XML_CALIBRATE, 0);
  xmlDocSetRootElement (doc, node);

  // Adding properties to the root XML node
  file2 = g_file_new_for_path (input->simulator);
  buffer = g_file_get_relative_path (file, file2);
  g_object_unref (file2);
  xmlSetProp (node, XML_SIMULATOR, (xmlChar *) buffer);
  g_free (buffer);
  file2 = g_file_new_for_path (input->evaluator);
  buffer = g_file_get_relative_path (file, file2);
  g_object_unref (file2);
  if (xmlStrlen ((xmlChar *) buffer))
    xmlSetProp (node, XML_EVALUATOR, (xmlChar *) buffer);
  g_free (buffer);

  // Setting the algorithm
  buffer = (char *) g_malloc (64);
  switch (input->algorithm)
    {
    case ALGORITHM_MONTE_CARLO:
      snprintf (buffer, 64, "%u", input->nsimulations);
      xmlSetProp (node, XML_NSIMULATIONS, (xmlChar *) buffer);
      snprintf (buffer, 64, "%u", input->niterations);
      xmlSetProp (node, XML_NITERATIONS, (xmlChar *) buffer);
      snprintf (buffer, 64, "%.3lg", input->tolerance);
      xmlSetProp (node, XML_TOLERANCE, (xmlChar *) buffer);
      snprintf (buffer, 64, "%u", input->nbest);
      xmlSetProp (node, XML_NBEST, (xmlChar *) buffer);
      break;
    case ALGORITHM_SWEEP:
      xmlSetProp (node, XML_ALGORITHM, XML_SWEEP);
      snprintf (buffer, 64, "%u", input->niterations);
      xmlSetProp (node, XML_NITERATIONS, (xmlChar *) buffer);
      snprintf (buffer, 64, "%.3lg", input->tolerance);
      xmlSetProp (node, XML_TOLERANCE, (xmlChar *) buffer);
      snprintf (buffer, 64, "%u", input->nbest);
      xmlSetProp (node, XML_NBEST, (xmlChar *) buffer);
      break;
    default:
      xmlSetProp (node, XML_ALGORITHM, XML_GENETIC);
      snprintf (buffer, 64, "%u", input->nsimulations);
      xmlSetProp (node, XML_NPOPULATION, (xmlChar *) buffer);
      snprintf (buffer, 64, "%u", input->niterations);
      xmlSetProp (node, XML_NGENERATIONS, (xmlChar *) buffer);
      snprintf (buffer, 64, "%.3lg", input->mutation_ratio);
      xmlSetProp (node, XML_MUTATION, (xmlChar *) buffer);
      snprintf (buffer, 64, "%.3lg", input->reproduction_ratio);
      xmlSetProp (node, XML_REPRODUCTION, (xmlChar *) buffer);
      snprintf (buffer, 64, "%.3lg", input->adaptation_ratio);
      xmlSetProp (node, XML_ADAPTATION, (xmlChar *) buffer);
      break;
    }
  g_free (buffer);

  // Setting the experimental data
  for (i = 0; i < input->nexperiments; ++i)
    {
      child = xmlNewChild (node, 0, XML_EXPERIMENT, 0);
      xmlSetProp (child, XML_NAME, (xmlChar *) input->experiment[i]);
      if (input->weight[i] != 1.)
        xml_node_set_float (child, XML_WEIGHT, input->weight[i]);
      for (j = 0; j < input->ninputs; ++j)
        xmlSetProp (child, template[j], (xmlChar *) input->template[j][i]);
    }

  // Setting the variables data
  for (i = 0; i < input->nvariables; ++i)
    {
      child = xmlNewChild (node, 0, XML_VARIABLE, 0);
      xmlSetProp (child, XML_NAME, (xmlChar *) input->label[i]);
      xml_node_set_float (child, XML_MINIMUM, input->rangemin[i]);
      if (input->rangeminabs[i] != -INFINITY && !isnan (input->rangeminabs[i]))
        xml_node_set_float (child, XML_ABSOLUTE_MINIMUM, input->rangeminabs[i]);
      xml_node_set_float (child, XML_MAXIMUM, input->rangemax[i]);
      if (input->rangemaxabs[i] != INFINITY && !isnan (input->rangemaxabs[i]))
        xml_node_set_float (child, XML_ABSOLUTE_MAXIMUM, input->rangemaxabs[i]);
      if (xmlStrcmp ((xmlChar *) input->format[i], DEFAULT_FORMAT))
        xmlSetProp (child, XML_FORMAT, (xmlChar *) input->format[i]);
      if (input->algorithm == ALGORITHM_SWEEP)
        xml_node_set_uint (child, XML_NSWEEPS, input->nsweeps[i]);
      else if (input->algorithm == ALGORITHM_GENETIC)
        xml_node_set_uint (child, XML_NBITS, input->nbits[i]);
    }

  // Saving the XML file
  xmlSaveFormatFile (filename, doc, 1);

  // Freeing memory
  xmlFreeDoc (doc);
}

/**
 * \fn void window_save()
 * \brief Function to save the input file.
 */
void
window_save ()
{
  char *buffer;
  GtkFileChooserDialog *dlg;

  // Opening the saving dialog
  dlg = (GtkFileChooserDialog *)
    gtk_file_chooser_dialog_new (gettext ("Save file"),
                                 window->window,
                                 GTK_FILE_CHOOSER_ACTION_SAVE,
                                 gettext ("_Cancel"),
                                 GTK_RESPONSE_CANCEL,
                                 gettext ("_OK"), GTK_RESPONSE_OK, NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dlg), TRUE);

  // If OK response then saving
  if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {

      // Adding properties to the root XML node
      input->simulator = gtk_file_chooser_get_filename
        (GTK_FILE_CHOOSER (window->button_simulator));
      input->evaluator = gtk_file_chooser_get_filename
        (GTK_FILE_CHOOSER (window->button_evaluator));

      // Setting the algorithm
      switch (window_get_algorithm ())
        {
        case ALGORITHM_MONTE_CARLO:
          input->algorithm = ALGORITHM_MONTE_CARLO;
          input->nsimulations =
            gtk_spin_button_get_value_as_int (window->entry_simulations);
          input->niterations =
            gtk_spin_button_get_value_as_int (window->entry_iterations);
          input->tolerance =
            gtk_spin_button_get_value (window->entry_tolerance);
          input->nbest = gtk_spin_button_get_value_as_int (window->entry_bests);
          break;
        case ALGORITHM_SWEEP:
          input->algorithm = ALGORITHM_SWEEP;
          input->niterations =
            gtk_spin_button_get_value_as_int (window->entry_iterations);
          input->tolerance =
            gtk_spin_button_get_value (window->entry_tolerance);
          input->nbest = gtk_spin_button_get_value_as_int (window->entry_bests);
          break;
        default:
          input->algorithm = ALGORITHM_GENETIC;
          input->nsimulations
            = gtk_spin_button_get_value_as_int (window->entry_population);
          input->niterations =
            gtk_spin_button_get_value_as_int (window->entry_generations);
          input->mutation_ratio =
            gtk_spin_button_get_value (window->entry_mutation);
          input->reproduction_ratio =
            gtk_spin_button_get_value (window->entry_reproduction);
          input->adaptation_ratio =
            gtk_spin_button_get_value (window->entry_adaptation);
          break;
        }

      // Saving the XML file
      buffer = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg));
      input_save (buffer);

      // Freeing memory
      g_free (buffer);
    }

  // Closing and freeing memory
  gtk_widget_destroy (GTK_WIDGET (dlg));
}

/**
 * \fn void window_run()
 * \brief Function to run a calibration.
 */
void
window_run ()
{
  size_t n;
  char *dir, *program, *line, *msg, *msg2, buffer[1024];
  FILE *file;
  window_save ();
  dir = g_get_current_dir ();
  program = g_build_filename (dir, "calibrator", NULL);
  snprintf
    (buffer, 1024, "cd %s; %s %s", input->directory, program, input->name);
  printf ("%s\n", buffer);
  system (buffer);
  g_free (program);
  g_free (dir);
  program = g_build_filename (input->directory, "result", NULL);
  file = fopen (program, "r");
  g_free (program);
  for (line = msg = NULL, n = 0; getline (&line, &n, file) > 0;
       free (line), g_free (msg), line = NULL, msg = msg2, n = 0)
    if (!msg)
      msg2 = g_strdup (line);
    else
      msg2 = g_strconcat (msg, line, NULL);
  fclose (file);
  show_message (gettext ("Best result"), msg2);
  g_free (msg2);
}

/**
 * \fn void window_help()
 * \brief Function to show a help dialog.
 */
void
window_help ()
{
  gchar *authors[] = {
    "Javier Burguete Tolosa (jburguete@eead.csic.es)",
    "Borja Latorre Garcés (borja.latorre@csic.es)",
    NULL
  };
  gtk_show_about_dialog (window->window,
                         "program_name",
                         "Calibrator",
                         "comments",
                         gettext
                         ("A software to make calibrations of empirical parameters"),
                         "authors", authors,
                         "translator-credits",
                         "Javier Burguete Tolosa (jburguete@eead.csic.es)",
                         "version", "1.1.12", "copyright",
                         "Copyright 2012-2015 Javier Burguete Tolosa",
                         "logo", window->logo,
                         "website-label", gettext ("Website"),
                         "website",
                         "https://github.com/jburguete/calibrator", NULL);
}

/**
 * \fn int window_get_algorithm()
 * \brief Function to get the algorithm number.
 * \return Algorithm number.
 */
int
window_get_algorithm ()
{
  unsigned int i;
  for (i = 0; i < NALGORITHMS; ++i)
    if (gtk_toggle_button_get_active
        (GTK_TOGGLE_BUTTON (window->button_algorithm[i])))
      break;
  return i;
}

/**
 * \fn void window_update()
 * \brief Function to update the main window view.
 */
void
window_update ()
{
  unsigned int i;
  gtk_widget_hide (GTK_WIDGET (window->label_simulations));
  gtk_widget_hide (GTK_WIDGET (window->entry_simulations));
  gtk_widget_hide (GTK_WIDGET (window->label_iterations));
  gtk_widget_hide (GTK_WIDGET (window->entry_iterations));
  gtk_widget_hide (GTK_WIDGET (window->label_tolerance));
  gtk_widget_hide (GTK_WIDGET (window->entry_tolerance));
  gtk_widget_hide (GTK_WIDGET (window->label_bests));
  gtk_widget_hide (GTK_WIDGET (window->entry_bests));
  gtk_widget_hide (GTK_WIDGET (window->label_population));
  gtk_widget_hide (GTK_WIDGET (window->entry_population));
  gtk_widget_hide (GTK_WIDGET (window->label_generations));
  gtk_widget_hide (GTK_WIDGET (window->entry_generations));
  gtk_widget_hide (GTK_WIDGET (window->label_mutation));
  gtk_widget_hide (GTK_WIDGET (window->entry_mutation));
  gtk_widget_hide (GTK_WIDGET (window->label_reproduction));
  gtk_widget_hide (GTK_WIDGET (window->entry_reproduction));
  gtk_widget_hide (GTK_WIDGET (window->label_adaptation));
  gtk_widget_hide (GTK_WIDGET (window->entry_adaptation));
  gtk_widget_hide (GTK_WIDGET (window->label_sweeps));
  gtk_widget_hide (GTK_WIDGET (window->entry_sweeps));
  gtk_widget_hide (GTK_WIDGET (window->label_bits));
  gtk_widget_hide (GTK_WIDGET (window->entry_bits));
  switch (window_get_algorithm ())
    {
    case ALGORITHM_MONTE_CARLO:
      gtk_widget_show (GTK_WIDGET (window->label_simulations));
      gtk_widget_show (GTK_WIDGET (window->entry_simulations));
      gtk_widget_show (GTK_WIDGET (window->label_iterations));
      gtk_widget_show (GTK_WIDGET (window->entry_iterations));
      gtk_widget_show (GTK_WIDGET (window->label_tolerance));
      gtk_widget_show (GTK_WIDGET (window->entry_tolerance));
      gtk_widget_show (GTK_WIDGET (window->label_bests));
      gtk_widget_show (GTK_WIDGET (window->entry_bests));
      break;
    case ALGORITHM_SWEEP:
      gtk_widget_show (GTK_WIDGET (window->label_iterations));
      gtk_widget_show (GTK_WIDGET (window->entry_iterations));
      gtk_widget_show (GTK_WIDGET (window->label_tolerance));
      gtk_widget_show (GTK_WIDGET (window->entry_tolerance));
      gtk_widget_show (GTK_WIDGET (window->label_bests));
      gtk_widget_show (GTK_WIDGET (window->entry_bests));
      gtk_widget_show (GTK_WIDGET (window->label_sweeps));
      gtk_widget_show (GTK_WIDGET (window->entry_sweeps));
      break;
    default:
      gtk_widget_show (GTK_WIDGET (window->label_population));
      gtk_widget_show (GTK_WIDGET (window->entry_population));
      gtk_widget_show (GTK_WIDGET (window->label_generations));
      gtk_widget_show (GTK_WIDGET (window->entry_generations));
      gtk_widget_show (GTK_WIDGET (window->label_mutation));
      gtk_widget_show (GTK_WIDGET (window->entry_mutation));
      gtk_widget_show (GTK_WIDGET (window->label_reproduction));
      gtk_widget_show (GTK_WIDGET (window->entry_reproduction));
      gtk_widget_show (GTK_WIDGET (window->label_adaptation));
      gtk_widget_show (GTK_WIDGET (window->entry_adaptation));
      gtk_widget_show (GTK_WIDGET (window->label_bits));
      gtk_widget_show (GTK_WIDGET (window->entry_bits));
    }
  gtk_widget_set_sensitive
    (GTK_WIDGET (window->button_remove_experiment), input->nexperiments > 1);
  gtk_widget_set_sensitive
    (GTK_WIDGET (window->button_remove_variable), input->nvariables > 1);
  for (i = 0; i < input->ninputs; ++i)
    {
      gtk_widget_show (GTK_WIDGET (window->check_template[i]));
      gtk_widget_show (GTK_WIDGET (window->button_template[i]));
      gtk_widget_set_sensitive (GTK_WIDGET (window->check_template[i]), 0);
      g_signal_handler_block
        (window->check_template[i], window->id_template[i]);
      g_signal_handler_block (window->button_template[i], window->id_input[i]);
      gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON (window->check_template[i]), 1);
      g_signal_handler_unblock
        (window->button_template[i], window->id_input[i]);
      g_signal_handler_unblock
        (window->check_template[i], window->id_template[i]);
    }
  if (i > 0)
    gtk_widget_set_sensitive (GTK_WIDGET (window->check_template[i - 1]), 1);
  if (i < MAX_NINPUTS)
    {
      gtk_widget_show (GTK_WIDGET (window->check_template[i]));
      gtk_widget_show (GTK_WIDGET (window->button_template[i]));
      gtk_widget_set_sensitive (GTK_WIDGET (window->check_template[i]), 1);
      g_signal_handler_block
        (window->check_template[i], window->id_template[i]);
      g_signal_handler_block (window->button_template[i], window->id_input[i]);
      gtk_toggle_button_set_active
        (GTK_TOGGLE_BUTTON (window->check_template[i]), 0);
      g_signal_handler_unblock
        (window->button_template[i], window->id_input[i]);
      g_signal_handler_unblock
        (window->check_template[i], window->id_template[i]);
    }
  while (++i < MAX_NINPUTS)
    {
      gtk_widget_hide (GTK_WIDGET (window->check_template[i]));
      gtk_widget_hide (GTK_WIDGET (window->button_template[i]));
    }
}

/**
 * \fn void window_set_algorithm()
 * \brief Function to avoid memory errors changing the algorithm.
 */
void
window_set_algorithm ()
{
  unsigned int i;
  i = window_get_algorithm ();
  switch (i)
    {
    case ALGORITHM_SWEEP:
      input->nsweeps = (unsigned int *) g_realloc
        (input->nsweeps, input->nvariables * sizeof (unsigned int));
      break;
    case ALGORITHM_GENETIC:
      input->nbits = (unsigned int *) g_realloc
        (input->nbits, input->nvariables * sizeof (unsigned int));
    }
  window_update ();
}

/**
 * \fn void window_set_experiment()
 * \brief Function to set the experiment data in the main window.
 */
void
window_set_experiment ()
{
  unsigned int i, j;
  char *buffer1, *buffer2;
#if DEBUG
  fprintf (stderr, "window_set_experiment: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_experiment));
  gtk_spin_button_set_value (window->entry_weight, input->weight[i]);
  buffer1 = gtk_combo_box_text_get_active_text (window->combo_experiment);
  buffer2 = g_build_filename (input->directory, buffer1, NULL);
  g_free (buffer1);
  g_signal_handler_block
    (window->button_experiment, window->id_experiment_name);
  gtk_file_chooser_set_filename
    (GTK_FILE_CHOOSER (window->button_experiment), buffer2);
  g_signal_handler_unblock
    (window->button_experiment, window->id_experiment_name);
  g_free (buffer2);
  for (j = 0; j < input->ninputs; ++j)
    {
      g_signal_handler_block (window->button_template[j], window->id_input[j]);
      buffer2
        = g_build_filename (input->directory, input->template[j][i], NULL);
      gtk_file_chooser_set_filename
        (GTK_FILE_CHOOSER (window->button_template[j]), buffer2);
      g_free (buffer2);
      g_signal_handler_unblock
        (window->button_template[j], window->id_input[j]);
    }
#if DEBUG
  fprintf (stderr, "window_set_experiment: end\n");
#endif
}

/**
 * \fn void window_remove_experiment()
 * \brief Function to remove an experiment in the main window.
 */
void
window_remove_experiment ()
{
  unsigned int i, j;
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_experiment));
  g_signal_handler_block (window->combo_experiment, window->id_experiment);
  gtk_combo_box_text_remove (window->combo_experiment, i);
  g_signal_handler_unblock (window->combo_experiment, window->id_experiment);
  xmlFree (input->experiment[i]);
  --input->nexperiments;
  for (j = i; j < input->nexperiments; ++j)
    {
      input->experiment[j] = input->experiment[j + 1];
      input->weight[j] = input->weight[j + 1];
    }
  j = input->nexperiments - 1;
  if (i > j)
    i = j;
  for (j = 0; j < input->ninputs; ++j)
    g_signal_handler_block (window->button_template[j], window->id_input[j]);
  g_signal_handler_block
    (window->button_experiment, window->id_experiment_name);
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_experiment), i);
  g_signal_handler_unblock
    (window->button_experiment, window->id_experiment_name);
  for (j = 0; j < input->ninputs; ++j)
    g_signal_handler_unblock (window->button_template[j], window->id_input[j]);
  window_update ();
}

/**
 * \fn void window_add_experiment()
 * \brief Function to add an experiment in the main window.
 */
void
window_add_experiment ()
{
  unsigned int i, j;
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_experiment));
  g_signal_handler_block (window->combo_experiment, window->id_experiment);
  gtk_combo_box_text_insert_text
    (window->combo_experiment, i, input->experiment[i]);
  g_signal_handler_unblock (window->combo_experiment, window->id_experiment);
  input->experiment = (char **) g_realloc
    (input->experiment, (input->nexperiments + 1) * sizeof (char *));
  input->weight = (double *) g_realloc
    (input->weight, (input->nexperiments + 1) * sizeof (double));
  for (j = input->nexperiments - 1; j > i; --j)
    {
      input->experiment[j + 1] = input->experiment[j];
      input->weight[j + 1] = input->weight[j];
    }
  input->experiment[j + 1]
    = (char *) xmlStrdup ((xmlChar *) input->experiment[j]);
  input->weight[j + 1] = input->weight[j];
  ++input->nexperiments;
  for (j = 0; j < input->ninputs; ++j)
    g_signal_handler_block (window->button_template[j], window->id_input[j]);
  g_signal_handler_block
    (window->button_experiment, window->id_experiment_name);
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_experiment), i + 1);
  g_signal_handler_unblock
    (window->button_experiment, window->id_experiment_name);
  for (j = 0; j < input->ninputs; ++j)
    g_signal_handler_unblock (window->button_template[j], window->id_input[j]);
  window_update ();
}

/**
 * \fn void window_name_experiment()
 * \brief Function to set the experiment name in the main window.
 */
void
window_name_experiment ()
{
  unsigned int i;
  char *buffer;
  GFile *file1, *file2;
#if DEBUG
  fprintf (stderr, "window_name_experiment: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_experiment));
  file1
    = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (window->button_experiment));
  file2 = g_file_new_for_path (input->directory);
  buffer = g_file_get_relative_path (file2, file1);
  g_signal_handler_block (window->combo_experiment, window->id_experiment);
  gtk_combo_box_text_remove (window->combo_experiment, i);
  gtk_combo_box_text_insert_text (window->combo_experiment, i, buffer);
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_experiment), i);
  g_signal_handler_unblock (window->combo_experiment, window->id_experiment);
  g_free (buffer);
  g_object_unref (file2);
  g_object_unref (file1);
#if DEBUG
  fprintf (stderr, "window_name_experiment: end\n");
#endif
}

/**
 * \fn void window_weight_experiment()
 * \brief Function to update the experiment weight in the main window.
 */
void
window_weight_experiment ()
{
  unsigned int i;
#if DEBUG
  fprintf (stderr, "window_weight_experiment: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_experiment));
  input->weight[i] = gtk_spin_button_get_value (window->entry_weight);
#if DEBUG
  fprintf (stderr, "window_weight_experiment: end\n");
#endif
}

/**
 * \fn void window_inputs_experiment()
 * \brief Function to update the experiment input templates number in the main
 *   window.
 */
void
window_inputs_experiment ()
{
  unsigned int j;
#if DEBUG
  fprintf (stderr, "window_inputs_experiment: start\n");
#endif
  j = input->ninputs - 1;
  if (j
      && !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                        (window->check_template[j])))
    --input->ninputs;
  if (input->ninputs < MAX_NINPUTS
      && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                       (window->check_template[j])))
    {
      ++input->ninputs;
      for (j = 0; j < input->ninputs; ++j)
        {
          input->template[j] = (char **)
            g_realloc (input->template[j], input->nvariables * sizeof (char *));
        }
    }
  window_update ();
#if DEBUG
  fprintf (stderr, "window_inputs_experiment: end\n");
#endif
}

/**
 * \fn void window_template_experiment(void *data)
 * \brief Function to update the experiment i-th input template in the main
 *   window.
 * \param data
 * \brief Callback data (i-th input template).
 */
void
window_template_experiment (void *data)
{
  unsigned int i, j;
  char *buffer;
  GFile *file1, *file2;
#if DEBUG
  fprintf (stderr, "window_template_experiment: start\n");
#endif
  i = (size_t) data;
  j = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_experiment));
  file1
    = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (window->button_template[i]));
  file2 = g_file_new_for_path (input->directory);
  buffer = g_file_get_relative_path (file2, file1);
  input->template[i][j] = (char *) xmlStrdup ((xmlChar *) buffer);
  g_free (buffer);
  g_object_unref (file2);
  g_object_unref (file1);
#if DEBUG
  fprintf (stderr, "window_template_experiment: end\n");
#endif
}

/**
 * \fn void window_set_variable()
 * \brief Function to set the variable data in the main window.
 */
void
window_set_variable ()
{
  unsigned int i;
  char buffer[64];
#if DEBUG
  fprintf (stderr, "window_set_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  g_signal_handler_block (window->entry_variable, window->id_variable_label);
  gtk_entry_set_text (window->entry_variable, input->label[i]);
  g_signal_handler_unblock (window->entry_variable, window->id_variable_label);
  snprintf (buffer, 64, input->format[i], input->rangemin[i]);
  gtk_entry_set_text (window->entry_min, buffer);
  snprintf (buffer, 64, input->format[i], input->rangemax[i]);
  gtk_entry_set_text (window->entry_max, buffer);
  snprintf (buffer, 64, input->format[i], input->rangeminabs[i]);
  gtk_entry_set_text (window->entry_minabs, buffer);
  snprintf (buffer, 64, input->format[i], input->rangemaxabs[i]);
  gtk_entry_set_text (window->entry_maxabs, buffer);
  gtk_entry_set_text (window->entry_format, input->format[i]);
  switch (input->algorithm)
    {
    case ALGORITHM_SWEEP:
      gtk_spin_button_set_value (window->entry_sweeps,
                                 (gdouble) input->nsweeps[i]);
      break;
    case ALGORITHM_GENETIC:
      gtk_spin_button_set_value (window->entry_bits, (gdouble) input->nbits[i]);
      break;
    }
#if DEBUG
  fprintf (stderr, "window_set_variable: end\n");
#endif
}

/**
 * \fn void window_remove_variable()
 * \brief Function to remove a variable in the main window.
 */
void
window_remove_variable ()
{
  unsigned int i, j;
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  g_signal_handler_block (window->combo_variable, window->id_variable);
  gtk_combo_box_text_remove (window->combo_variable, i);
  g_signal_handler_unblock (window->combo_variable, window->id_variable);
  xmlFree (input->label[i]);
  xmlFree (input->format[i]);
  --input->nvariables;
  for (j = i; j < input->nvariables; ++j)
    {
      input->label[j] = input->label[j + 1];
      input->rangemin[j] = input->rangemin[j + 1];
      input->rangemax[j] = input->rangemax[j + 1];
      input->rangeminabs[j] = input->rangeminabs[j + 1];
      input->rangemaxabs[j] = input->rangemaxabs[j + 1];
      input->format[j] = input->format[j + 1];
      switch (window_get_algorithm ())
        {
        case ALGORITHM_SWEEP:
          input->nsweeps[j] = input->nsweeps[j + 1];
          break;
        case ALGORITHM_GENETIC:
          input->nbits[j] = input->nbits[j + 1];
        }
    }
  j = input->nvariables - 1;
  if (i > j)
    i = j;
  g_signal_handler_block (window->entry_variable, window->id_variable_label);
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_variable), i);
  g_signal_handler_unblock (window->entry_variable, window->id_variable_label);
  window_update ();
}

/**
 * \fn void window_add_variable()
 * \brief Function to add a variable in the main window.
 */
void
window_add_variable ()
{
  unsigned int i, j;
#if DEBUG
  fprintf (stderr, "window_add_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  g_signal_handler_block (window->combo_variable, window->id_variable);
  gtk_combo_box_text_insert_text (window->combo_variable, i, input->label[i]);
  g_signal_handler_unblock (window->combo_variable, window->id_variable);
  input->label = (char **) g_realloc
    (input->label, (input->nvariables + 1) * sizeof (char *));
  input->rangemin = (double *) g_realloc
    (input->rangemin, (input->nvariables + 1) * sizeof (double));
  input->rangemax = (double *) g_realloc
    (input->rangemax, (input->nvariables + 1) * sizeof (double));
  input->rangeminabs = (double *) g_realloc
    (input->rangeminabs, (input->nvariables + 1) * sizeof (double));
  input->rangemaxabs = (double *) g_realloc
    (input->rangemaxabs, (input->nvariables + 1) * sizeof (double));
  input->format = (char **) g_realloc
    (input->format, (input->nvariables + 1) * sizeof (char *));
  for (j = input->nvariables - 1; j > i; --j)
    {
      input->label[j + 1] = input->label[j];
      input->rangemin[j + 1] = input->rangemin[j];
      input->rangemax[j + 1] = input->rangemax[j];
      input->rangeminabs[j + 1] = input->rangeminabs[j];
      input->rangemaxabs[j + 1] = input->rangemaxabs[j];
      input->format[j + 1] = input->format[j];
    }
  input->label[j + 1] = (char *) xmlStrdup ((xmlChar *) input->label[j]);
  input->rangemin[j + 1] = input->rangemin[j];
  input->rangemax[j + 1] = input->rangemax[j];
  input->rangeminabs[j + 1] = input->rangeminabs[j];
  input->rangemaxabs[j + 1] = input->rangemaxabs[j];
  input->format[j + 1] = (char *) xmlStrdup ((xmlChar *) input->format[j]);
  switch (window_get_algorithm ())
    {
    case ALGORITHM_SWEEP:
      input->nsweeps = (unsigned int *) g_realloc
        (input->nsweeps, (input->nvariables + 1) * sizeof (unsigned int));
      for (j = input->nvariables - 1; j > i; --j)
        input->nsweeps[j + 1] = input->nsweeps[j];
      input->nsweeps[j + 1] = input->nsweeps[j];
      break;
    case ALGORITHM_GENETIC:
      input->nbits = (unsigned int *) g_realloc
        (input->nbits, (input->nvariables + 1) * sizeof (unsigned int));
      for (j = input->nvariables - 1; j > i; --j)
        input->nbits[j + 1] = input->nbits[j];
      input->nbits[j + 1] = input->nbits[j];
    }
  ++input->nvariables;
  g_signal_handler_block (window->entry_variable, window->id_variable_label);
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_variable), i + 1);
  g_signal_handler_unblock (window->entry_variable, window->id_variable_label);
  window_update ();
#if DEBUG
  fprintf (stderr, "window_add_variable: end\n");
#endif
}

/**
 * \fn void window_label_variable()
 * \brief Function to set the variable label in the main window.
 */
void
window_label_variable ()
{
  unsigned int i;
  const char *buffer;
#if DEBUG
  fprintf (stderr, "window_label_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  buffer = gtk_entry_get_text (window->entry_variable);
  g_signal_handler_block (window->combo_variable, window->id_variable);
  gtk_combo_box_text_remove (window->combo_variable, i);
  gtk_combo_box_text_insert_text (window->combo_variable, i, buffer);
  gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_variable), i);
  g_signal_handler_unblock (window->combo_variable, window->id_variable);
#if DEBUG
  fprintf (stderr, "window_label_variable: end\n");
#endif
}

/**
 * \fn void window_format_variable()
 * \brief Function to update the variable format in the main window.
 */
void
window_format_variable ()
{
  unsigned int i;
  const char *buffer;
#if DEBUG
  fprintf (stderr, "window_format_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  buffer = gtk_entry_get_text (window->entry_format);
  input->format[i] = (char *) xmlStrdup ((xmlChar *) buffer);
#if DEBUG
  fprintf (stderr, "window_format_variable: end\n");
#endif
}

/**
 * \fn void window_rangemin_variable()
 * \brief Function to update the variable rangemin in the main window.
 */
void
window_rangemin_variable ()
{
  unsigned int i;
  const char *buffer;
#if DEBUG
  fprintf (stderr, "window_rangemin_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  buffer = gtk_entry_get_text (window->entry_min);
  sscanf (buffer, input->format[i], input->rangemin + i);
#if DEBUG
  fprintf (stderr, "window_rangemin_variable: end\n");
#endif
}

/**
 * \fn void window_rangemax_variable()
 * \brief Function to update the variable rangemax in the main window.
 */
void
window_rangemax_variable ()
{
  unsigned int i;
  const char *buffer;
#if DEBUG
  fprintf (stderr, "window_rangemax_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  buffer = gtk_entry_get_text (window->entry_max);
  sscanf (buffer, input->format[i], input->rangemax + i);
#if DEBUG
  fprintf (stderr, "window_rangemax_variable: end\n");
#endif
}

/**
 * \fn void window_rangeminabs_variable()
 * \brief Function to update the variable rangeminabs in the main window.
 */
void
window_rangeminabs_variable ()
{
  unsigned int i;
  const char *buffer;
#if DEBUG
  fprintf (stderr, "window_rangeminabs_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  buffer = gtk_entry_get_text (window->entry_minabs);
  sscanf (buffer, input->format[i], input->rangeminabs + i);
#if DEBUG
  fprintf (stderr, "window_rangeminabs_variable: end\n");
#endif
}

/**
 * \fn void window_rangemaxabs_variable()
 * \brief Function to update the variable rangemaxabs in the main window.
 */
void
window_rangemaxabs_variable ()
{
  unsigned int i;
  const char *buffer;
#if DEBUG
  fprintf (stderr, "window_rangemaxabs_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  buffer = gtk_entry_get_text (window->entry_maxabs);
  sscanf (buffer, input->format[i], input->rangemaxabs + i);
#if DEBUG
  fprintf (stderr, "window_rangemaxabs_variable: end\n");
#endif
}

/**
 * \fn void window_update_variable()
 * \brief Function to update the variable data in the main window.
 */
void
window_update_variable ()
{
  unsigned int i;
#if DEBUG
  fprintf (stderr, "window_update_variable: start\n");
#endif
  i = gtk_combo_box_get_active (GTK_COMBO_BOX (window->combo_variable));
  switch (window_get_algorithm ())
    {
    case ALGORITHM_SWEEP:
      input->nsweeps[i]
        = gtk_spin_button_get_value_as_int (window->entry_sweeps);
      break;
    case ALGORITHM_GENETIC:
      input->nbits[i] = gtk_spin_button_get_value_as_int (window->entry_bits);
    }
#if DEBUG
  fprintf (stderr, "window_update_variable: end\n");
#endif
}

/**
 * \fn void window_open()
 * \brief Function to open the input data.
 */
void
window_open ()
{
  unsigned int i;
  char *buffer;
  GtkFileChooserDialog *dlg;
  dlg = (GtkFileChooserDialog *)
    gtk_file_chooser_dialog_new (gettext ("Open input file"),
                                 window->window,
                                 GTK_FILE_CHOOSER_ACTION_OPEN,
                                 gettext ("_Cancel"),
                                 GTK_RESPONSE_CANCEL,
                                 gettext ("_OK"), GTK_RESPONSE_OK, NULL);
  if (gtk_dialog_run (GTK_DIALOG (dlg)) == GTK_RESPONSE_OK)
    {
      input_free ();
      input_open (gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dlg)));
      buffer = g_build_filename (input->directory, input->simulator, NULL);
      gtk_file_chooser_set_filename (GTK_FILE_CHOOSER
                                     (window->button_simulator), buffer);
      g_free (buffer);
      if (input->evaluator)
        {
          buffer = g_build_filename (input->directory, input->evaluator, NULL);
          gtk_file_chooser_set_filename (GTK_FILE_CHOOSER
                                         (window->button_evaluator), buffer);
          g_free (buffer);
        }
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                    (window->button_algorithm
                                     [input->algorithm]), TRUE);
      switch (input->algorithm)
        {
        case ALGORITHM_MONTE_CARLO:
          gtk_spin_button_set_value (window->entry_simulations,
                                     (gdouble) input->nsimulations);
        case ALGORITHM_SWEEP:
          gtk_spin_button_set_value (window->entry_iterations,
                                     (gdouble) input->niterations);
          gtk_spin_button_set_value (window->entry_bests,
                                     (gdouble) input->nbest);
          gtk_spin_button_set_value (window->entry_tolerance, input->tolerance);
          break;
        default:
          gtk_spin_button_set_value (window->entry_population,
                                     (gdouble) input->nsimulations);
          gtk_spin_button_set_value (window->entry_generations,
                                     (gdouble) input->niterations);
          gtk_spin_button_set_value (window->entry_mutation,
                                     input->mutation_ratio);
          gtk_spin_button_set_value (window->entry_reproduction,
                                     input->reproduction_ratio);
          gtk_spin_button_set_value (window->entry_adaptation,
                                     input->adaptation_ratio);
        }
      g_signal_handler_block (window->combo_experiment, window->id_experiment);
      g_signal_handler_block
        (window->button_experiment, window->id_experiment_name);
      gtk_combo_box_text_remove_all (window->combo_experiment);
      for (i = 0; i < input->nexperiments; ++i)
        gtk_combo_box_text_append_text (window->combo_experiment,
                                        input->experiment[i]);
      g_signal_handler_unblock
        (window->button_experiment, window->id_experiment_name);
      g_signal_handler_unblock
        (window->combo_experiment, window->id_experiment);
      gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_experiment), 0);
      g_signal_handler_block (window->combo_variable, window->id_variable);
      g_signal_handler_block (window->entry_variable,
                              window->id_variable_label);
      gtk_combo_box_text_remove_all (window->combo_variable);
      for (i = 0; i < input->nvariables; ++i)
        gtk_combo_box_text_append_text (window->combo_variable,
                                        input->label[i]);
      g_signal_handler_unblock
        (window->entry_variable, window->id_variable_label);
      g_signal_handler_unblock (window->combo_variable, window->id_variable);
      gtk_combo_box_set_active (GTK_COMBO_BOX (window->combo_variable), 0);
      window_update ();
    }
  gtk_widget_destroy (GTK_WIDGET (dlg));
}

/**
 * \fn void window_new(GtkApplication *application)
 * \brief Function to open the main window.
 * \param application
 * \brief Main GtkApplication.
 */
void
window_new (GtkApplication * application)
{
  unsigned int i;
  char *label_algorithm[NALGORITHMS] = {
    "_Monte-Carlo", gettext ("_Sweep"), gettext ("_Genetic")
  };

  // Creating the window
  window->window = (GtkWindow *) gtk_application_window_new (application);

  // Setting the window title
  gtk_window_set_title (window->window, PROGRAM_INTERFACE);

  // Creating the open button
  window->button_open
    = (GtkButton *) gtk_button_new_with_mnemonic (gettext ("_Open"));
  g_signal_connect (window->button_open, "clicked", window_open, NULL);

  // Creating the save button
  window->button_save
    = (GtkButton *) gtk_button_new_with_mnemonic (gettext ("_Save"));
  g_signal_connect (window->button_save, "clicked", window_save, NULL);

  // Creating the run button
  window->button_run
    = (GtkButton *) gtk_button_new_with_mnemonic (gettext ("_Run"));
  g_signal_connect (window->button_run, "clicked", window_run, NULL);

  // Creating the help button
  window->button_help
    = (GtkButton *) gtk_button_new_with_mnemonic (gettext ("_Help"));
  g_signal_connect (window->button_help, "clicked", window_help, NULL);

  // Creating the exit button
  window->button_exit
    = (GtkButton *) gtk_button_new_with_mnemonic (gettext ("E_xit"));
  g_signal_connect_swapped (window->button_exit, "clicked",
                            (void (*)) gtk_widget_destroy, window->window);

  window->grid_buttons = (GtkGrid *) gtk_grid_new ();
  gtk_grid_attach (window->grid_buttons, GTK_WIDGET (window->button_open),
                   0, 0, 1, 1);
  gtk_grid_attach (window->grid_buttons, GTK_WIDGET (window->button_save),
                   1, 0, 1, 1);
  gtk_grid_attach (window->grid_buttons, GTK_WIDGET (window->button_run),
                   2, 0, 1, 1);
  gtk_grid_attach (window->grid_buttons, GTK_WIDGET (window->button_help),
                   3, 0, 1, 1);
  gtk_grid_attach (window->grid_buttons, GTK_WIDGET (window->button_exit),
                   4, 0, 1, 1);

  // Creating the simulator program label and entry
  window->label_simulator
    = (GtkLabel *) gtk_label_new (gettext ("Simulator program"));
  window->button_simulator =
    (GtkFileChooserButton *)
    gtk_file_chooser_button_new (gettext ("Simulator program"),
                                 GTK_FILE_CHOOSER_ACTION_OPEN);

  // Creating the evaluator program label and entry
  window->label_evaluator
    = (GtkLabel *) gtk_label_new (gettext ("Evaluator program"));
  window->button_evaluator =
    (GtkFileChooserButton *)
    gtk_file_chooser_button_new (gettext ("Evaluator program"),
                                 GTK_FILE_CHOOSER_ACTION_OPEN);

  // Creating the algorithm properties
  window->label_simulations = (GtkLabel *) gtk_label_new
    (gettext ("Simulations number"));
  window->entry_simulations
    = (GtkSpinButton *) gtk_spin_button_new_with_range (1., 1.e12, 1.);
  window->label_iterations =
    (GtkLabel *) gtk_label_new (gettext ("Iterations number"));
  window->entry_iterations =
    (GtkSpinButton *) gtk_spin_button_new_with_range (1., 1.e6, 1.);
  window->label_tolerance = (GtkLabel *) gtk_label_new (gettext ("Tolerance"));
  window->entry_tolerance =
    (GtkSpinButton *) gtk_spin_button_new_with_range (0., 1., 0.001);
  window->label_bests = (GtkLabel *) gtk_label_new (gettext ("Bests number"));
  window->entry_bests =
    (GtkSpinButton *) gtk_spin_button_new_with_range (1., 1.e6, 1.);
  window->label_population =
    (GtkLabel *) gtk_label_new (gettext ("Population number"));
  window->entry_population =
    (GtkSpinButton *) gtk_spin_button_new_with_range (1., 1.e12, 1.);
  window->label_generations =
    (GtkLabel *) gtk_label_new (gettext ("Generations number"));
  window->entry_generations =
    (GtkSpinButton *) gtk_spin_button_new_with_range (1., 1.e6, 1.);
  window->label_mutation =
    (GtkLabel *) gtk_label_new (gettext ("Mutation ratio"));
  window->entry_mutation =
    (GtkSpinButton *) gtk_spin_button_new_with_range (0., 1., 0.001);
  window->label_reproduction =
    (GtkLabel *) gtk_label_new (gettext ("Reproduction ratio"));
  window->entry_reproduction =
    (GtkSpinButton *) gtk_spin_button_new_with_range (0., 1., 0.001);
  window->label_adaptation =
    (GtkLabel *) gtk_label_new (gettext ("Adaptation ratio"));
  window->entry_adaptation =
    (GtkSpinButton *) gtk_spin_button_new_with_range (0., 1., 0.001);

  // Creating the array of algorithms
  window->grid_algorithm = (GtkGrid *) gtk_grid_new ();
  window->button_algorithm[0] = (GtkRadioButton *)
    gtk_radio_button_new_with_mnemonic (NULL, label_algorithm[0]);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->button_algorithm[0]), 0, 0, 1, 1);
  g_signal_connect (window->button_algorithm[0], "clicked",
                    window_set_algorithm, NULL);
  for (i = 0; ++i < NALGORITHMS;)
    {
      window->button_algorithm[i] = (GtkRadioButton *)
        gtk_radio_button_new_with_mnemonic
        (gtk_radio_button_get_group (window->button_algorithm[0]),
         label_algorithm[i]);
      gtk_grid_attach (window->grid_algorithm,
                       GTK_WIDGET (window->button_algorithm[i]), 0, i, 1, 1);
      g_signal_connect (window->button_algorithm[i], "clicked",
                        window_set_algorithm, NULL);
    }
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_simulations), 0,
                   NALGORITHMS, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_simulations), 1,
                   NALGORITHMS, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_iterations), 0,
                   NALGORITHMS + 1, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_iterations), 1,
                   NALGORITHMS + 1, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_tolerance), 0,
                   NALGORITHMS + 2, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_tolerance), 1,
                   NALGORITHMS + 2, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_bests), 0, NALGORITHMS + 3, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_bests), 1, NALGORITHMS + 3, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_population), 0,
                   NALGORITHMS + 4, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_population), 1,
                   NALGORITHMS + 4, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_generations), 0,
                   NALGORITHMS + 5, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_generations), 1,
                   NALGORITHMS + 5, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_mutation), 0,
                   NALGORITHMS + 6, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_mutation), 1,
                   NALGORITHMS + 6, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_reproduction), 0,
                   NALGORITHMS + 7, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_reproduction), 1,
                   NALGORITHMS + 7, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->label_adaptation), 0,
                   NALGORITHMS + 8, 1, 1);
  gtk_grid_attach (window->grid_algorithm,
                   GTK_WIDGET (window->entry_adaptation), 1,
                   NALGORITHMS + 8, 1, 1);
  window->frame_algorithm = (GtkFrame *) gtk_frame_new (gettext ("Algorithm"));
  gtk_container_add (GTK_CONTAINER (window->frame_algorithm),
                     GTK_WIDGET (window->grid_algorithm));

  // Creating the variable widgets
  window->combo_variable = (GtkComboBoxText *) gtk_combo_box_text_new ();
  window->id_variable = g_signal_connect
    (window->combo_variable, "changed", window_set_variable, NULL);
  window->button_add_variable =
    (GtkButton *) gtk_button_new_from_icon_name ("list-add",
                                                 GTK_ICON_SIZE_BUTTON);
  g_signal_connect
    (window->button_add_variable, "clicked", window_add_variable, NULL);
  gtk_widget_set_tooltip_text (GTK_WIDGET
                               (window->button_add_variable),
                               gettext ("Add variable"));
  window->button_remove_variable =
    (GtkButton *) gtk_button_new_from_icon_name ("list-remove",
                                                 GTK_ICON_SIZE_BUTTON);
  g_signal_connect
    (window->button_remove_variable, "clicked", window_remove_variable, NULL);
  gtk_widget_set_tooltip_text (GTK_WIDGET
                               (window->button_remove_variable),
                               gettext ("Remove variable"));
  window->label_variable = (GtkLabel *) gtk_label_new (gettext ("Name"));
  window->entry_variable = (GtkEntry *) gtk_entry_new ();
  window->id_variable_label = g_signal_connect
    (window->entry_variable, "changed", window_label_variable, NULL);
  window->label_min = (GtkLabel *) gtk_label_new (gettext ("Minimum"));
  window->entry_min = (GtkEntry *) gtk_entry_new ();
  g_signal_connect
    (window->entry_min, "changed", window_rangemin_variable, NULL);
  window->label_max = (GtkLabel *) gtk_label_new (gettext ("Maximum"));
  window->entry_max = (GtkEntry *) gtk_entry_new ();
  g_signal_connect
    (window->entry_max, "changed", window_rangemax_variable, NULL);
  window->label_minabs =
    (GtkLabel *) gtk_label_new (gettext ("Absolute minimum"));
  window->entry_minabs = (GtkEntry *) gtk_entry_new ();
  g_signal_connect
    (window->entry_minabs, "changed", window_rangeminabs_variable, NULL);
  window->label_maxabs =
    (GtkLabel *) gtk_label_new (gettext ("Absolute maximum"));
  window->entry_maxabs = (GtkEntry *) gtk_entry_new ();
  g_signal_connect
    (window->entry_maxabs, "changed", window_rangemaxabs_variable, NULL);
  window->label_format = (GtkLabel *) gtk_label_new (gettext ("C-format"));
  window->entry_format = (GtkEntry *) gtk_entry_new ();
  g_signal_connect
    (window->entry_format, "changed", window_format_variable, NULL);
  window->label_sweeps = (GtkLabel *) gtk_label_new (gettext ("Sweeps number"));
  window->entry_sweeps =
    (GtkSpinButton *) gtk_spin_button_new_with_range (1., 1.e12, 1.);
  g_signal_connect
    (window->entry_sweeps, "value-changed", window_update_variable, NULL);
  window->label_bits = (GtkLabel *) gtk_label_new (gettext ("Bits number"));
  window->entry_bits =
    (GtkSpinButton *) gtk_spin_button_new_with_range (1., 64., 1.);
  g_signal_connect
    (window->entry_bits, "value-changed", window_update_variable, NULL);
  window->grid_variable = (GtkGrid *) gtk_grid_new ();
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->combo_variable), 0, 0, 2, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->button_add_variable), 2, 0, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->button_remove_variable), 3, 0, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_variable), 0, 1, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_variable), 1, 1, 3, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_min), 0, 2, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_min), 1, 2, 3, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_max), 0, 3, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_max), 1, 3, 3, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_minabs), 0, 4, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_minabs), 1, 4, 3, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_maxabs), 0, 5, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_maxabs), 1, 5, 3, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_format), 0, 6, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_format), 1, 6, 3, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_sweeps), 0, 7, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_sweeps), 1, 7, 3, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->label_bits), 0, 8, 1, 1);
  gtk_grid_attach (window->grid_variable,
                   GTK_WIDGET (window->entry_bits), 1, 8, 3, 1);
  window->frame_variable = (GtkFrame *) gtk_frame_new (gettext ("Variable"));
  gtk_container_add (GTK_CONTAINER (window->frame_variable),
                     GTK_WIDGET (window->grid_variable));

  // Creating the experiment widgets
  window->combo_experiment = (GtkComboBoxText *) gtk_combo_box_text_new ();
  window->id_experiment = g_signal_connect
    (window->combo_experiment, "changed", window_set_experiment, NULL);
  window->button_add_experiment =
    (GtkButton *) gtk_button_new_from_icon_name ("list-add",
                                                 GTK_ICON_SIZE_BUTTON);
  g_signal_connect
    (window->button_add_experiment, "clicked", window_add_experiment, NULL);
  gtk_widget_set_tooltip_text (GTK_WIDGET (window->button_add_experiment),
                               gettext ("Add experiment"));
  window->button_remove_experiment =
    (GtkButton *) gtk_button_new_from_icon_name ("list-remove",
                                                 GTK_ICON_SIZE_BUTTON);
  g_signal_connect (window->button_remove_experiment, "clicked",
                    window_remove_experiment, NULL);
  gtk_widget_set_tooltip_text (GTK_WIDGET (window->button_remove_experiment),
                               gettext ("Remove experiment"));
  window->label_experiment = (GtkLabel *) gtk_label_new (gettext ("Name"));
  window->button_experiment = (GtkFileChooserButton *)
    gtk_file_chooser_button_new (gettext ("Experimental data file"),
                                 GTK_FILE_CHOOSER_ACTION_OPEN);
  window->id_experiment_name
    = g_signal_connect (window->button_experiment, "selection-changed",
                        window_name_experiment, NULL);
  window->label_weight = (GtkLabel *) gtk_label_new (gettext ("Weight"));
  window->entry_weight =
    (GtkSpinButton *) gtk_spin_button_new_with_range (0., 1., 0.001);
  g_signal_connect
    (window->entry_weight, "value-changed", window_weight_experiment, NULL);
  window->grid_experiment = (GtkGrid *) gtk_grid_new ();
  gtk_grid_attach (window->grid_experiment,
                   GTK_WIDGET (window->combo_experiment), 0, 0, 2, 1);
  gtk_grid_attach (window->grid_experiment,
                   GTK_WIDGET (window->button_add_experiment), 2, 0, 1, 1);
  gtk_grid_attach (window->grid_experiment,
                   GTK_WIDGET (window->button_remove_experiment), 3, 0, 1, 1);
  gtk_grid_attach (window->grid_experiment,
                   GTK_WIDGET (window->label_experiment), 0, 1, 1, 1);
  gtk_grid_attach (window->grid_experiment,
                   GTK_WIDGET (window->button_experiment), 1, 1, 3, 1);
  gtk_grid_attach (window->grid_experiment,
                   GTK_WIDGET (window->label_weight), 0, 2, 1, 1);
  gtk_grid_attach (window->grid_experiment,
                   GTK_WIDGET (window->entry_weight), 1, 2, 3, 1);
  for (i = 0; i < MAX_NINPUTS; ++i)
    {
      window->check_template[i] = (GtkCheckButton *)
        gtk_check_button_new_with_label ((char *) template[i]);
      window->id_template[i]
        = g_signal_connect (window->check_template[i], "toggled",
                            window_inputs_experiment, NULL);
      gtk_grid_attach (window->grid_experiment,
                       GTK_WIDGET (window->check_template[i]), 0, 3 + i, 1, 1);
      window->button_template[i] = (GtkFileChooserButton *)
        gtk_file_chooser_button_new (gettext ("Input template"),
                                     GTK_FILE_CHOOSER_ACTION_OPEN);
      window->id_input[i]
        = g_signal_connect_swapped (window->button_template[i],
                                    "selection-changed",
                                    (void (*)) window_template_experiment,
                                    (void *) (size_t) i);
      gtk_grid_attach (window->grid_experiment,
                       GTK_WIDGET (window->button_template[i]), 1, 3 + i, 3, 1);
    }
  window->frame_experiment =
    (GtkFrame *) gtk_frame_new (gettext ("Experiment"));
  gtk_container_add (GTK_CONTAINER (window->frame_experiment),
                     GTK_WIDGET (window->grid_experiment));

  // Creating the grid and attaching the widgets to the grid
  window->grid = (GtkGrid *) gtk_grid_new ();
  gtk_grid_attach (window->grid, GTK_WIDGET (window->grid_buttons), 0, 0, 6, 1);
  gtk_grid_attach (window->grid,
                   GTK_WIDGET (window->label_simulator), 0, 1, 1, 1);
  gtk_grid_attach (window->grid,
                   GTK_WIDGET (window->button_simulator), 1, 1, 1, 1);
  gtk_grid_attach (window->grid,
                   GTK_WIDGET (window->label_evaluator), 2, 1, 1, 1);
  gtk_grid_attach (window->grid,
                   GTK_WIDGET (window->button_evaluator), 3, 1, 1, 1);
  gtk_grid_attach (window->grid,
                   GTK_WIDGET (window->frame_algorithm), 0, 2, 2, 1);
  gtk_grid_attach (window->grid,
                   GTK_WIDGET (window->frame_variable), 2, 2, 2, 1);
  gtk_grid_attach (window->grid,
                   GTK_WIDGET (window->frame_experiment), 4, 2, 2, 1);
  gtk_container_add (GTK_CONTAINER (window->window), GTK_WIDGET (window->grid));

  // Setting the window logo
  window->logo = gdk_pixbuf_new_from_xpm_data (logo);
  gtk_window_set_icon (window->window, window->logo);

  // Showing the window
  gtk_widget_show_all (GTK_WIDGET (window->window));
  window_update ();
}

#endif

/**
 * \fn int cores_number()
 * \brief Function to obtain the cores number.
 * \return Cores number.
 */
int
cores_number ()
{
#ifdef G_OS_WIN32
  SYSTEM_INFO sysinfo;
  GetSystemInfo (&sysinfo);
  return sysinfo.dwNumberOfProcessors;
#else
  return (int) sysconf (_SC_NPROCESSORS_ONLN);
#endif
}

/**
 * \fn int main(int argn, char **argc)
 * \brief Main function.
 * \param argn
 * \brief Arguments number.
 * \param argc
 * \brief Arguments pointer.
 * \return 0 on success, >0 on error.
 */
int
main (int argn, char **argc)
{
#if HAVE_GTK
  int status;
  char *buffer;
  GtkApplication *application;
  xmlKeepBlanksDefault (0);
  setlocale (LC_ALL, "");
  setlocale (LC_NUMERIC, "C");
  buffer = g_get_current_dir ();
  bindtextdomain
    (PROGRAM_INTERFACE, g_build_filename (buffer, LOCALE_DIR, NULL));
  bind_textdomain_codeset (PROGRAM_INTERFACE, "UTF-8");
  textdomain (PROGRAM_INTERFACE);
  gtk_disable_setlocale ();
  application =
    gtk_application_new ("git.jburguete.calibrator", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (application, "activate", (void (*)) window_new, NULL);
  status = g_application_run (G_APPLICATION (application), argn, argc);
  g_object_unref (application);
  return status;
#else
#if HAVE_MPI
  // Starting MPI
  MPI_Init (&argn, &argc);
  MPI_Comm_size (MPI_COMM_WORLD, &ntasks);
  MPI_Comm_rank (MPI_COMM_WORLD, &calibrate->mpi_rank);
  printf ("rank=%d tasks=%d\n", calibrate->mpi_rank, ntasks);
#else
  ntasks = 1;
#endif
  // Checking syntax
  if (!(argn == 2 || (argn == 4 && !strcmp (argc[1], "-nthreads"))))
    {
      printf ("The syntax is:\ncalibrator [-nthreads x] data_file\n");
#if HAVE_MPI
      // Closing MPI
      MPI_Finalize ();
#endif
      return 1;
    }
  // Getting threads number
  if (argn == 2)
    nthreads = cores_number ();
  else
    nthreads = atoi (argc[2]);
  printf ("nthreads=%u\n", nthreads);
#if GLIB_MINOR_VERSION < 32
  g_thread_init (NULL);
  if (nthreads > 1)
    mutex = g_mutex_new ();
#endif
  // Starting pseudo-random numbers generator
  calibrate->rng = gsl_rng_alloc (gsl_rng_taus2);
  gsl_rng_set (calibrate->rng, DEFAULT_RANDOM_SEED);
  // Allowing spaces in the XML data file
  xmlKeepBlanksDefault (0);
  // Making calibration
  calibrate_new (argc[argn - 1]);
  // Freeing memory
  gsl_rng_free (calibrate->rng);
#if HAVE_MPI
  // Closing MPI
  MPI_Finalize ();
#endif
  return 0;
#endif
}
