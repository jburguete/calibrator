/*
MPCOTool:
The Multi-Purposes Calibration and Optimization Tool. A software to perform
calibrations or optimizations of empirical parameters.

AUTHORS: Javier Burguete and Borja Latorre.

Copyright 2012-2016, AUTHORS.

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
 * \file variable.c
 * \brief Source file to define the variable data.
 * \authors Javier Burguete and Borja Latorre.
 * \copyright Copyright 2012-2016, all rights reserved.
 */
#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <libxml/parser.h>
#include <libintl.h>
#include <glib.h>
#include "utils.h"
#include "variable.h"

#define DEBUG 0                 ///< Macro to debug.

const char *format[NPRECISIONS] = {
  "%.0lf", "%.1lf", "%.2lf", "%.3lf", "%.4lf", "%.5lf", "%.6lf", "%.7lf",
  "%.8lf", "%.9lf", "%.10lf", "%.11lf", "%.12lf", "%.13lf", "%.14lf"
};                              ///< Array of C-strings with variable formats.

const double precision[NPRECISIONS] = {
  1., 0.1, 0.01, 1e-3, 1e-4, 1e-5, 1e-6, 1e-7, 1e-8, 1e-9, 1e-10, 1e-11, 1e-12,
  1e-13, 1e-14
};                              ///< Array of variable precisions.

/**
 * \fn void variable_new (Variable * variable)
 * \brief Function to create a new Variable struct.
 * \param variable
 * \brief Variable struct.
 */
void
variable_new (Variable * variable)
{
#if DEBUG
  fprintf (stderr, "variable_new: start\n");
#endif
  variable->name = NULL;
#if DEBUG
  fprintf (stderr, "variable_new: end\n");
#endif
}

/**
 * \fn void variable_free (Variable * variable)
 * \brief Function to free the memory of a Variable struct.
 * \param variable
 * \brief Variable struct.
 */
void
variable_free (Variable * variable)
{
#if DEBUG
  fprintf (stderr, "variable_free: start\n");
#endif
  g_free (variable->name);
#if DEBUG
  fprintf (stderr, "variable_free: end\n");
#endif
}

/**
 * \fn void variable_error (Variable * variable, char *message)
 * \brief Function to print a message error opening an Variable struct.
 * \param variable
 * \brief Variable struct.
 * \param message
 * \brief Error message.
 */
void
variable_error (Variable * variable, char *message)
{
  char buffer[64];
  if (!variable->name)
    snprintf (buffer, 64, "%s: %s", gettext ("Variable"), message);
  else
    snprintf (buffer, 64, "%s %s: %s", gettext ("Variable"), variable->name,
              message);
  error_message = g_strdup (buffer);
}

/**
 * \fn int variable_open (Variable * variable, xmlNode * node, \
 *   unsigned int algorithm, unsigned int nsteps)
 * \brief Function to open the variable file.
 * \param variable
 * \brief Variable struct.
 * \param node
 * \brief XML node.
 * \param algorithm
 * \brief Algorithm type.
 * \param nsteps
 * \brief Number of steps to do the direction search method.
 * \return 1 on success, 0 on error.
 */
int
variable_open (Variable * variable, xmlNode * node, unsigned int algorithm,
		       unsigned int nsteps)
{
  char *msg;
  int error_code;

#if DEBUG
  fprintf (stderr, "variable_open: start\n");
#endif

  variable->name = (char*) xmlGetProp (node, XML_NAME);
  if (!variable->name)
    {
      variable_error (variable, gettext ("no name"));
      goto exit_on_error;
    }
  if (xmlHasProp (node, XML_MINIMUM))
    {
      variable->rangemin = xml_node_get_float (node, XML_MINIMUM, &error_code);
      if (error_code)
        {
          variable_error (variable, gettext ("bad minimum"));
          goto exit_on_error;
        }
      variable->rangeminabs
        = xml_node_get_float_with_default (node, XML_ABSOLUTE_MINIMUM,
                                           -G_MAXDOUBLE, &error_code);
      if (error_code)
        {
          variable_error (variable, gettext ("bad absolute minimum"));
          goto exit_on_error;
        }
      if (variable->rangemin < variable->rangeminabs)
        {
          variable_error (variable, gettext ("minimum range not allowed"));
          goto exit_on_error;
        }
    }
  else
    {
      variable_error (variable, gettext ("no minimum range"));
      goto exit_on_error;
    }
  if (xmlHasProp (node, XML_MAXIMUM))
    {
      variable->rangemax
        = xml_node_get_float (node, XML_MAXIMUM, &error_code);
      if (error_code)
        {
          variable_error (variable, gettext ("bad maximum"));
          goto exit_on_error;
        }
      variable->rangemaxabs
        = xml_node_get_float_with_default (node, XML_ABSOLUTE_MAXIMUM,
                                           G_MAXDOUBLE, &error_code);
      if (error_code)
        {
          variable_error (variable, gettext ("bad absolute maximum"));
          goto exit_on_error;
        }
      if (variable->rangemax > variable->rangemaxabs)
        {
          variable_error (variable, gettext ("maximum range not allowed"));
          goto exit_on_error;
        }
	  if (variable->rangemax < variable->rangemin)
        {
          variable_error (variable, gettext ("bad range"));
          goto exit_on_error;
        }
	  if (variable->rangemaxabs < variable->rangeminabs)
        {
          variable_error (variable, gettext ("bad absolute range"));
          goto exit_on_error;
        }
    }
  else
    {
      variable_error (variable, gettext ("no maximum range"));
      goto exit_on_error;
    }
  variable->precision
    = xml_node_get_uint_with_default (node, XML_PRECISION,
                                      DEFAULT_PRECISION, &error_code);
  if (error_code || variable->precision >= NPRECISIONS)
    {
      variable_error (variable, gettext ("bad precision"));
      goto exit_on_error;
    }
  if (algorithm == ALGORITHM_SWEEP)
    {
      if (xmlHasProp (node, XML_NSWEEPS))
        {
          variable->nsweeps
            = xml_node_get_uint (node, XML_NSWEEPS, &error_code);
          if (error_code || !variable->nsweeps)
            {
              variable_error (variable, gettext ("bad sweeps"));
              goto exit_on_error;
            }
        }
      else
        {
          variable_error (variable, gettext ("no sweeps number"));
          goto exit_on_error;
        }
#if DEBUG
      fprintf (stderr, "variable_open: nsweeps=%u\n", variable->nsweeps);
#endif
    }
  if (algorithm == ALGORITHM_GENETIC)
    {
      // Obtaining bits representing each variable
      if (xmlHasProp (node, XML_NBITS))
        {
          variable->nbits = xml_node_get_uint (node, XML_NBITS, &error_code);
          if (error_code || !variable->nbits)
            {
              variable_error (variable, gettext ("invalid bits number"));
              goto exit_on_error;
            }
        }
      else
        {
          variable_error (variable, gettext ("no bits number"));
          goto exit_on_error;
        }
    }
  else if (nsteps)
    {
      variable->step
        = xml_node_get_float (node, XML_STEP, &error_code);
      if (error_code || variable->step < 0.)
        {
          variable_error (variable, gettext ("bad step size"));
          goto exit_on_error;
        }
    }

#if DEBUG
  fprintf (stderr, "variable_open: end\n");
#endif
  return 1;

exit_on_error:
  variable_free (variable);
#if DEBUG
  fprintf (stderr, "variable_open: end\n");
#endif
  return 0;
}
