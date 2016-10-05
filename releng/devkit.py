#!/usr/bin/env python

from __future__ import print_function
from collections import OrderedDict
from glob import glob
import os
import pipes
import re
import shutil
import subprocess
import sys
import tempfile

INCLUDE_PATTERN = re.compile("#include\s+[<\"](.*?)[>\"]")

DEVKITS = {
    "frida-gum": ("frida-gum-1.0", ("frida-1.0", "gum", "gum.h")),
    "frida-gumjs": ("frida-gumjs-1.0", ("frida-1.0", "gumjs", "gumscriptbackend.h")),
    "frida-core": ("frida-core-1.0", ("frida-1.0", "frida-core.h")),
}

def generate_devkit(kit, host, output_dir):
    package, umbrella_header = DEVKITS[kit]

    frida_root = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))

    env_rc = os.path.join(frida_root, "build", "frida-env-{}.rc".format(host))
    umbrella_header_path = os.path.join(frida_root, "build", "frida-" + host, "include", *umbrella_header)

    header_filename = kit + ".h"
    if not os.path.exists(umbrella_header_path):
        raise Exception("Header not found: {}".format(umbrella_header_path))
    header = generate_header(package, frida_root, env_rc, umbrella_header_path)
    with open(os.path.join(output_dir, header_filename), "w") as f:
        f.write(header)

    library_filename = "lib{}.a".format(kit)
    (library, extra_ldflags) = generate_library(package, env_rc)
    with open(os.path.join(output_dir, library_filename), "wb") as f:
        f.write(library)

    example_filename = kit + "-example.c"
    example = generate_example(example_filename, package, env_rc, kit, extra_ldflags)
    with open(os.path.join(output_dir, example_filename), "w") as f:
        f.write(example)

    return [header_filename, library_filename, example_filename]

def generate_header(package, frida_root, env_rc, umbrella_header_path):
    header_dependencies = subprocess.check_output(
        ["(. \"{rc}\" && $CPP $CFLAGS -M $($PKG_CONFIG --cflags {package}) \"{header}\")".format(rc=env_rc, package=package, header=umbrella_header_path)],
        shell=True).decode('utf-8')
    header_lines = header_dependencies.strip().split("\n")[1:]
    header_files = [line.rstrip("\\").strip() for line in header_lines]
    header_files = [header_file for header_file in header_files if header_file.startswith(frida_root)]

    devkit_header_lines = []
    umbrella_header = header_files[0]
    processed_header_files = set([umbrella_header])
    ingest_header(umbrella_header, header_files, processed_header_files, devkit_header_lines)
    return "".join(devkit_header_lines)

def ingest_header(header, all_header_files, processed_header_files, result):
    with open(header, "r") as f:
        for line in f:
            match = INCLUDE_PATTERN.match(line.strip())
            if match is not None:
                name = match.group(1)
                inline = False
                for other_header in all_header_files:
                    if other_header.endswith("/" + name):
                        inline = True
                        if not other_header in processed_header_files:
                            processed_header_files.add(other_header)
                            ingest_header(other_header, all_header_files, processed_header_files, result)
                        break
                if not inline:
                    result.append(line)
            else:
                result.append(line)

def generate_library(package, env_rc):
    library_flags = subprocess.check_output(
        ["(. \"{rc}\" && $PKG_CONFIG --static --libs {package})".format(rc=env_rc, package=package)],
        shell=True).decode('utf-8').strip().split(" ")
    library_dirs = infer_library_dirs(library_flags)
    library_names = infer_library_names(library_flags)
    library_paths, extra_flags = resolve_library_paths(library_names, library_dirs)
    extra_flags += infer_linker_flags(library_flags)

    combined_dir = tempfile.mkdtemp(prefix="devkit")
    object_names = set()

    for library_path in library_paths:
        scratch_dir = tempfile.mkdtemp(prefix="devkit")

        subprocess.check_output(
            ["(. \"{rc}\" && $AR x {library_path})".format(rc=env_rc, library_path=library_path)],
            shell=True,
            cwd=scratch_dir)
        for object_path in glob(os.path.join(scratch_dir, "*.o")):
            object_name = os.path.basename(object_path)
            while object_name in object_names:
                object_name = "_" + object_name
            object_names.add(object_name)
            shutil.move(object_path, os.path.join(combined_dir, object_name))

        shutil.rmtree(scratch_dir)

    library_path = os.path.join(combined_dir, "library.a")
    subprocess.check_output(
        ["(. \"{rc}\" && $AR rcs {library_path} {object_files} 2>/dev/null)".format(
            rc=env_rc,
            library_path=library_path,
            object_files=" ".join([pipes.quote(object_name) for object_name in object_names]))],
        shell=True,
        cwd=combined_dir)
    with open(library_path, "rb") as f:
        data = f.read()

    shutil.rmtree(combined_dir)

    return (data, extra_flags)

def infer_library_dirs(flags):
    return [flag[2:] for flag in flags if flag.startswith("-L")]

def infer_library_names(flags):
    return [flag[2:] for flag in flags if flag.startswith("-l")]

def infer_linker_flags(flags):
    return [flag for flag in flags if flag.startswith("-Wl")]

def resolve_library_paths(names, dirs):
    paths = []
    flags = []
    for name in names:
        library_path = None
        for d in dirs:
            candidate = os.path.join(d, "lib{}.a".format(name))
            if os.path.exists(candidate):
                library_path = candidate
                break
        if library_path is not None:
            paths.append(library_path)
        else:
            flags.append("-l{}".format(name))
    return (list(set(paths)), flags)

def generate_example(filename, package, env_rc, library_name, extra_ldflags):
    cc = probe_env(env_rc, "echo $CC")
    cflags = probe_env(env_rc, "echo $CFLAGS")
    ldflags = probe_env(env_rc, "echo $LDFLAGS")

    (cflags, ldflags) = trim_flags(cflags, " ".join([" ".join(extra_ldflags), ldflags]))

    params = {
        "cc": cc,
        "cflags": cflags,
        "ldflags": ldflags,
        "source_filename": filename,
        "program_filename": os.path.splitext(filename)[0],
        "library_name": library_name
    }

    preamble = """\
/*
 * Compile with:
 *
 * %(cc)s %(cflags)s %(source_filename)s -o %(program_filename)s -L. -l%(library_name)s %(ldflags)s
 */""" % params

    if package == "frida-gum-1.0":
        return r"""%(preamble)s

#include "frida-gum.h"

#include <fcntl.h>
#include <unistd.h>

typedef struct _ExampleListener ExampleListener;
typedef enum _ExampleHookId ExampleHookId;

struct _ExampleListener
{
  GObject parent;

  guint num_calls;
};

enum _ExampleHookId
{
  EXAMPLE_HOOK_OPEN,
  EXAMPLE_HOOK_CLOSE
};

static void example_listener_iface_init (gpointer g_iface, gpointer iface_data);

#define EXAMPLE_TYPE_LISTENER (example_listener_get_type ())
G_DECLARE_FINAL_TYPE (ExampleListener, example_listener, EXAMPLE, LISTENER, GObject)
G_DEFINE_TYPE_EXTENDED (ExampleListener,
                        example_listener,
                        G_TYPE_OBJECT,
                        0,
                        G_IMPLEMENT_INTERFACE (GUM_TYPE_INVOCATION_LISTENER,
                            example_listener_iface_init))

int
main (int argc,
      char * argv[])
{
  GumInterceptor * interceptor;
  GumInvocationListener * listener;

  gum_init ();

  interceptor = gum_interceptor_obtain ();
  listener = g_object_new (EXAMPLE_TYPE_LISTENER, NULL);

  gum_interceptor_begin_transaction (interceptor);
  gum_interceptor_attach_listener (interceptor,
      GSIZE_TO_POINTER (gum_module_find_export_by_name (NULL, "open")),
      listener,
      GSIZE_TO_POINTER (EXAMPLE_HOOK_OPEN));
  gum_interceptor_attach_listener (interceptor,
      GSIZE_TO_POINTER (gum_module_find_export_by_name (NULL, "close")),
      listener,
      GSIZE_TO_POINTER (EXAMPLE_HOOK_CLOSE));
  gum_interceptor_end_transaction (interceptor);

  close (open ("/etc/hosts", O_RDONLY));
  close (open ("/etc/fstab", O_RDONLY));

  g_print ("[*] listener got %%u calls\n", EXAMPLE_LISTENER (listener)->num_calls);

  gum_interceptor_detach_listener (interceptor, listener);

  close (open ("/etc/hosts", O_RDONLY));
  close (open ("/etc/fstab", O_RDONLY));

  g_print ("[*] listener still has %%u calls\n", EXAMPLE_LISTENER (listener)->num_calls);

  g_object_unref (listener);
  g_object_unref (interceptor);

  return 0;
}

static void
example_listener_on_enter (GumInvocationListener * listener,
                           GumInvocationContext * ic)
{
  ExampleListener * self = EXAMPLE_LISTENER (listener);
  ExampleHookId hook_id = GUM_LINCTX_GET_FUNC_DATA (ic, ExampleHookId);

  switch (hook_id)
  {
    case EXAMPLE_HOOK_OPEN:
      g_print ("[*] open(\"%%s\")\n", gum_invocation_context_get_nth_argument (ic, 0));
      break;
    case EXAMPLE_HOOK_CLOSE:
      g_print ("[*] close(%%d)\n", (int) gum_invocation_context_get_nth_argument (ic, 0));
      break;
  }

  self->num_calls++;
}

static void
example_listener_on_leave (GumInvocationListener * listener,
                           GumInvocationContext * ic)
{
}

static void
example_listener_class_init (ExampleListenerClass * klass)
{
  (void) EXAMPLE_IS_LISTENER;
  (void) glib_autoptr_cleanup_ExampleListener;
}

static void
example_listener_iface_init (gpointer g_iface,
                             gpointer iface_data)
{
  GumInvocationListenerIface * iface = (GumInvocationListenerIface *) g_iface;

  iface->on_enter = example_listener_on_enter;
  iface->on_leave = example_listener_on_leave;
}

static void
example_listener_init (ExampleListener * self)
{
}
""" % { "preamble": preamble }
    elif package == "frida-gumjs-1.0":
        return r"""%(preamble)s

#include "frida-gumjs.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

static void on_message (GumScript * script, const gchar * message, GBytes * data, gpointer user_data);

int
main (int argc,
      char * argv[])
{
  GumScriptBackend * backend;
  GCancellable * cancellable = NULL;
  GError * error = NULL;
  GumScript * script;
  GMainContext * context;

  gum_init ();

  backend = gum_script_backend_obtain_duk ();

  script = gum_script_backend_create_sync (backend, "example",
      "Interceptor.attach(Module.findExportByName(null, \"open\"), {\n"
      "  onEnter: function (args) {\n"
      "    console.log(\"[*] open(\\\"\" + Memory.readUtf8String(args[0]) + \"\\\")\");\n"
      "  }\n"
      "});\n"
      "Interceptor.attach(Module.findExportByName(null, \"close\"), {\n"
      "  onEnter: function (args) {\n"
      "    console.log(\"[*] close(\" + args[0].toInt32() + \")\");\n"
      "  }\n"
      "});",
      cancellable, &error);
  g_assert (error == NULL);

  gum_script_set_message_handler (script, on_message, NULL, NULL);

  gum_script_load_sync (script, cancellable);

  close (open ("/etc/hosts", O_RDONLY));
  close (open ("/etc/fstab", O_RDONLY));

  context = g_main_context_get_thread_default ();
  while (g_main_context_pending (context))
    g_main_context_iteration (context, FALSE);

  gum_script_unload_sync (script, cancellable);

  g_object_unref (script);

  return 0;
}

static void
on_message (GumScript * script,
            const gchar * message,
            GBytes * data,
            gpointer user_data)
{
  JsonParser * parser;
  JsonObject * root;
  const gchar * type;

  parser = json_parser_new ();
  json_parser_load_from_data (parser, message, -1, NULL);
  root = json_node_get_object (json_parser_get_root (parser));

  type = json_object_get_string_member (root, "type");
  if (strcmp (type, "log") == 0)
  {
    const gchar * log_message;

    log_message = json_object_get_string_member (root, "payload");
    g_print ("%%s\n", log_message);
  }
  else
  {
    g_print ("on_message: %%s\n", message);
  }

  g_object_unref (parser);
}
""" % { "preamble": preamble }
    elif package == "frida-core-1.0":
        return r"""%(preamble)s

#include "frida-core.h"

#include <stdlib.h>
#include <string.h>

static void on_message (FridaScript * script, const gchar * message, GBytes * data, gpointer user_data);
static void on_signal (int signo);
static gboolean stop (gpointer user_data);

static GMainLoop * loop = NULL;

int
main (int argc,
      char * argv[])
{
  guint target_pid;
  FridaDeviceManager * manager;
  GError * error = NULL;
  FridaDeviceList * devices;
  gint num_devices, i;
  FridaDevice * local_device;
  FridaSession * session;

  if (argc != 2 || (target_pid = atoi (argv[1])) == 0)
  {
    g_printerr ("Usage: %%s <pid>\n", argv[0]);
    return 1;
  }

  frida_init ();

  loop = g_main_loop_new (NULL, TRUE);

  signal (SIGINT, on_signal);
  signal (SIGTERM, on_signal);

  manager = frida_device_manager_new ();

  devices = frida_device_manager_enumerate_devices_sync (manager, &error);
  g_assert (error == NULL);

  local_device = NULL;
  num_devices = frida_device_list_size (devices);
  for (i = 0; i != num_devices; i++)
  {
    FridaDevice * device = frida_device_list_get (devices, i);

    g_print ("[*] Found device: \"%%s\"\n", frida_device_get_name (device));

    if (frida_device_get_dtype (device) == FRIDA_DEVICE_TYPE_LOCAL)
      local_device = g_object_ref (device);

    g_object_unref (device);
  }
  g_assert (local_device != NULL);

  frida_unref (devices);
  devices = NULL;

  session = frida_device_attach_sync (local_device, target_pid, &error);
  if (error == NULL)
  {
    FridaScript * script;

    g_print ("[*] Attached\n");

    script = frida_session_create_script_sync (session, "example",
        "Interceptor.attach(Module.findExportByName(null, \"open\"), {\n"
        "  onEnter: function (args) {\n"
        "    console.log(\"[*] open(\\\"\" + Memory.readUtf8String(args[0]) + \"\\\")\");\n"
        "  }\n"
        "});\n"
        "Interceptor.attach(Module.findExportByName(null, \"close\"), {\n"
        "  onEnter: function (args) {\n"
        "    console.log(\"[*] close(\" + args[0].toInt32() + \")\");\n"
        "  }\n"
        "});",
        &error);
    g_assert (error == NULL);

    g_signal_connect (script, "message", G_CALLBACK (on_message), NULL);

    frida_script_load_sync (script, &error);
    g_assert (error == NULL);

    g_print ("[*] Script loaded\n");

    if (g_main_loop_is_running (loop))
      g_main_loop_run (loop);

    g_print ("[*] Stopped\n");

    frida_script_unload_sync (script, NULL);
    frida_unref (script);
    g_print ("[*] Unloaded\n");

    frida_session_detach_sync (session);
    frida_unref (session);
    g_print ("[*] Detached\n");
  }
  else
  {
    g_printerr ("Failed to attach: %%s\n", error->message);
    g_error_free (error);
  }

  frida_unref (local_device);

  frida_device_manager_close_sync (manager);
  frida_unref (manager);
  g_print ("[*] Closed\n");

  g_main_loop_unref (loop);

  return 0;
}

static void
on_message (FridaScript * script,
            const gchar * message,
            GBytes * data,
            gpointer user_data)
{
  JsonParser * parser;
  JsonObject * root;
  const gchar * type;

  parser = json_parser_new ();
  json_parser_load_from_data (parser, message, -1, NULL);
  root = json_node_get_object (json_parser_get_root (parser));

  type = json_object_get_string_member (root, "type");
  if (strcmp (type, "log") == 0)
  {
    const gchar * log_message;

    log_message = json_object_get_string_member (root, "payload");
    g_print ("%%s\n", log_message);
  }
  else
  {
    g_print ("on_message: %%s\n", message);
  }

  g_object_unref (parser);
}

static void
on_signal (int signo)
{
  g_idle_add (stop, NULL);
}

static gboolean
stop (gpointer user_data)
{
  g_main_loop_quit (loop);

  return FALSE;
}
""" % { "preamble": preamble }

def probe_env(env_rc, command):
    return subprocess.check_output([
        "(. \"{rc}\" && PACKAGE_TARNAME=frida-devkit . $CONFIG_SITE && {command})".format(rc=env_rc, command=command)
    ], shell=True).decode('utf-8').strip()

def trim_flags(cflags, ldflags):
    trimmed_cflags = []
    trimmed_ldflags = []

    pending_cflags = cflags.split(" ")
    while len(pending_cflags) > 0:
        flag = pending_cflags.pop(0)
        if flag == "-include":
            pending_cflags.pop(0)
        else:
            trimmed_cflags.append(flag)

    trimmed_cflags = deduplicate(trimmed_cflags)
    existing_cflags = set(trimmed_cflags)

    pending_ldflags = ldflags.split(" ")
    while len(pending_ldflags) > 0:
        flag = pending_ldflags.pop(0)
        if flag in ("-arch", "-isysroot") and flag in existing_cflags:
            pending_ldflags.pop(0)
        else:
            trimmed_ldflags.append(flag)

    pending_ldflags = trimmed_ldflags
    trimmed_ldflags = []
    while len(pending_ldflags) > 0:
        flag = pending_ldflags.pop(0)

        raw_flags = []
        while flag.startswith("-Wl,"):
            raw_flags.append(flag[4:])
            if len(pending_ldflags) > 0:
                flag = pending_ldflags.pop(0)
            else:
                flag = None
                break
        if len(raw_flags) > 0:
            trimmed_ldflags.append("-Wl," + ",".join(raw_flags))

        if flag is not None and flag not in existing_cflags:
            trimmed_ldflags.append(flag)

    return (" ".join(trimmed_cflags), " ".join(trimmed_ldflags))

def deduplicate(items):
    return list(OrderedDict.fromkeys(items))


if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: {0} kit host outdir".format(sys.argv[0]), file=sys.stderr)
        sys.exit(1)

    kit = sys.argv[1]
    host = sys.argv[2]
    outdir = sys.argv[3]

    try:
        os.makedirs(outdir)
    except:
        pass

    generate_devkit(kit, host, outdir)
