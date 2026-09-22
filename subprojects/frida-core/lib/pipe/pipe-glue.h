#ifndef __FRIDA_PIPE_GLUE_H__
#define __FRIDA_PIPE_GLUE_H__

#include "frida-pipe.h"

#define FRIDA_TYPE_WINDOWS_PIPE_INPUT_STREAM (frida_windows_pipe_input_stream_get_type ())
#define FRIDA_TYPE_WINDOWS_PIPE_OUTPUT_STREAM (frida_windows_pipe_output_stream_get_type ())

G_DECLARE_FINAL_TYPE (FridaWindowsPipeInputStream, frida_windows_pipe_input_stream, FRIDA, WINDOWS_PIPE_INPUT_STREAM, GInputStream)
G_DECLARE_FINAL_TYPE (FridaWindowsPipeOutputStream, frida_windows_pipe_output_stream, FRIDA, WINDOWS_PIPE_OUTPUT_STREAM, GOutputStream)

#endif
