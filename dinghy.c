/*
 * dinghy.c
 * Copyright (C) 2017 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the BSD-2-Clause license.
 */

#include <stdlib.h>
#include "dinghy.h"


static struct {
    gboolean version;
    gboolean print_appid;
    GStrv    arguments;
} s_options = { 0, };


static GOptionEntry s_cli_options[] =
{
    { "version", '\0', 0, G_OPTION_ARG_NONE, &s_options.version, "Print version and exit", NULL },
    { "print-appid", '\0', 0, G_OPTION_ARG_NONE, &s_options.print_appid, "Print application ID and exit", NULL },
    { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_FILENAME_ARRAY, &s_options.arguments, "", "[URL]" },
    { NULL }
};


static int
on_handle_local_options (GApplication *application,
                         GVariantDict *options,
                         gpointer      user_data)
{
    if (s_options.version) {
        g_print ("%s\n", DY_VERSION_STRING);
        return EXIT_SUCCESS;
    }
    if (s_options.print_appid) {
        const char *appid = g_application_get_application_id (application);
        if (appid) g_print ("%s\n", appid);
        return EXIT_SUCCESS;
    }

    const char *uri = NULL;
    if (!s_options.arguments) {
        if (!(uri = g_getenv ("DINGHY_URL"))) {
            g_printerr ("%s: URL not passed in the command line, and DINGHY_URL not set\n", g_get_prgname ());
            return EXIT_FAILURE;
        }
    } else if (g_strv_length (s_options.arguments) > 1) {
        g_printerr ("%s: Cannot load more than one URL.\n", g_get_prgname ());
        return EXIT_FAILURE;
    } else {
        uri = s_options.arguments[0];
    }

    g_autofree char *utf8_uri = NULL;
    g_autoptr(GFile) file = g_file_new_for_commandline_arg (uri);

    if (g_file_is_native (file) && g_file_query_exists (file, NULL)) {
        utf8_uri = g_file_get_uri (file);
    } else {
        g_autoptr(GError) error = NULL;
        utf8_uri = g_locale_to_utf8 (uri, -1, NULL, NULL, &error);
        if (!utf8_uri) {
            g_printerr ("%s: URI '%s' is invalid UTF-8: %s\n", g_get_prgname (), uri, error->message);
            return EXIT_FAILURE;
        }
    }

    g_strfreev (s_options.arguments);
    s_options.arguments = NULL;

    dy_launcher_set_home_uri (DY_LAUNCHER (application), utf8_uri);
    return -1;  /* Continue startup. */
}


static void
on_about_page (DyURIHandlerRequest *request,
               void                *user_data)
{
    const char data[] =
        "<html><head><title>Dinghy - About</title>"
        "<style type='text/css'>"
        "body { color: #888; font: menu; padding: 0 5em }"
        "p { text-align: center; font-size: 4em;"
        "  margin: 0.5em; padding: 1em; border: 2px solid #ccc;"
        "  border-radius: 7px; background: #fafafa }"
        "p > span { font-weight: bold; color: #666 }"
        "</style></head><body>"
        "<p><span>Dinghy</span> v" DY_VERSION_STRING "</p>"
        "</body></html>";
    dy_uri_handler_request_load_string (request, "text/html", data, -1);
}

static void
on_startup (GApplication *application,
            void         *user_data)
{
    g_autoptr(DyURIHandler) uri_handler = dy_uri_handler_new("dinghy");
    dy_uri_handler_register (uri_handler, "about", on_about_page, NULL);
    dy_uri_handler_attach (uri_handler, DY_LAUNCHER (application));
}


int
main (int argc, char *argv[])
{
    g_autoptr(GApplication) app = G_APPLICATION (dy_launcher_get_default ());
    g_application_add_main_option_entries (app, s_cli_options);
    g_signal_connect (app, "startup", G_CALLBACK (on_startup), NULL);
    g_signal_connect (app, "handle-local-options",
                      G_CALLBACK (on_handle_local_options), NULL);
    return g_application_run (app, argc, argv);
}
