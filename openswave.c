/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "openswave.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#define PLUGIN_NAME        "CoolStar OpenSWave"
#define PLUGIN_DESCRIPTION PLUGIN_NAME " (An Open Source Implementation of Adobe Shockwave)"
#define PLUGIN_VERSION     "0.0.0.1"

static NPNetscapeFuncs* sBrowserFuncs = NULL;

typedef struct InstanceData {
  guint16 mode;
  NPP npp;
  NPWindow window;
  guint32 x, y;
  guint32 width, height;

  NPP instance;

  GtkWidget *container;
  GtkWidget *canvas;

  gboolean windowless;
} InstanceData;

static void
drawWindow(InstanceData* instanceData, GdkDrawable* gdkWindow)
{
  NPWindow window = instanceData->window;
  int x = window.x;
  int y = window.y;
  int width = window.width;
  int height = window.height;

  NPP npp = instanceData->npp;
  if (!npp)
    return;

  GdkGC* gdkContext = gdk_gc_new(gdkWindow);

  GdkColor col;
  col.red = 0;
  col.blue = 65535;
  col.green = 65535;
  gdk_gc_set_rgb_fg_color(gdkContext, &col);
  gdk_draw_rectangle(gdkWindow, gdkContext, TRUE, x, y, width, height);

  PangoContext* pangoContext = gdk_pango_context_get();
  PangoLayout* pangoTextLayout = pango_layout_new(pangoContext);
  pango_layout_set_width(pangoTextLayout, (width - 10) * PANGO_SCALE);
  pango_layout_set_text(pangoTextLayout, "Shockwave Content Here", -1);
  gdk_draw_layout(gdkWindow, gdkContext, x + 5, y + 5, pangoTextLayout);
  g_object_unref(pangoTextLayout);

  g_object_unref(gdkContext);
}

NP_EXPORT(NPError)
NP_Initialize(NPNetscapeFuncs* bFuncs, NPPluginFuncs* pFuncs)
{
  sBrowserFuncs = bFuncs;

  NPError err = NPERR_NO_ERROR;

  // Check the size of the provided structure based on the offset of the
  // last member we need.
  if (pFuncs->size < (offsetof(NPPluginFuncs, setvalue) + sizeof(void*)))
    return NPERR_INVALID_FUNCTABLE_ERROR;

  pFuncs->newp = NPP_New;
  pFuncs->destroy = NPP_Destroy;
  pFuncs->setwindow = NPP_SetWindow;
  pFuncs->newstream = NPP_NewStream;
  pFuncs->destroystream = NPP_DestroyStream;
  pFuncs->asfile = NPP_StreamAsFile;
  pFuncs->writeready = NPP_WriteReady;
  pFuncs->write = NPP_Write;
  pFuncs->print = NPP_Print;
  pFuncs->event = NPP_HandleEvent;
  pFuncs->urlnotify = NPP_URLNotify;
  pFuncs->getvalue = NPP_GetValue;
  pFuncs->setvalue = NPP_SetValue;

  return err;
}

NP_EXPORT(char*)
NP_GetPluginVersion()
{
  return PLUGIN_VERSION;
}

NP_EXPORT(const char*)
NP_GetMIMEDescription()
{
  return "application/x-director:dcr:Adobe Shockwave";
}

NP_EXPORT(NPError)
NP_GetValue(void* future, NPPVariable aVariable, void* aValue) {
  switch (aVariable) {
    case NPPVpluginNameString:
      *((char**)aValue) = PLUGIN_NAME;
      break;
    case NPPVpluginDescriptionString:
      *((char**)aValue) = PLUGIN_DESCRIPTION;
      break;
    default:
      return NPERR_INVALID_PARAM;
      break;
  }
  return NPERR_NO_ERROR;
}

NP_EXPORT(NPError)
NP_Shutdown()
{
  return NPERR_NO_ERROR;
}

NPError
NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved) {
  // Make sure we can render this plugin
  NPBool browserSupportsWindowless = false;
  sBrowserFuncs->getvalue(instance, NPNVSupportsWindowless, &browserSupportsWindowless);
  if (!browserSupportsWindowless) {
    printf("Windowless mode not supported by the browser\n");
    return NPERR_GENERIC_ERROR;
  }

  sBrowserFuncs->setvalue(instance, NPPVpluginWindowBool, (void*)false);

  // set up our our instance data
  InstanceData* instanceData = (InstanceData*)malloc(sizeof(InstanceData));
  if (!instanceData)
    return NPERR_OUT_OF_MEMORY_ERROR;
  memset(instanceData, 0, sizeof(InstanceData));
  instanceData->npp = instance;
  instance->pdata = instanceData;

  return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData** save) {
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  free(instanceData);
  return NPERR_NO_ERROR;
}

NPError
NPP_SetWindow(NPP instance, NPWindow* window) {
  InstanceData* instanceData = (InstanceData*)(instance->pdata);
  instanceData->window = *window;
  return NPERR_NO_ERROR;
}

NPError
NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype) {
  return NPERR_GENERIC_ERROR;
}

NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason) {
  return NPERR_GENERIC_ERROR;
}

int32_t
NPP_WriteReady(NPP instance, NPStream* stream) {
  return 0;
}

int32_t
NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer) {
  return 0;
}

void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname) {

}

void
NPP_Print(NPP instance, NPPrint* platformPrint) {

}

int16_t
NPP_HandleEvent(NPP instance, void* event) {
  InstanceData *instanceData = (InstanceData*)(instance->pdata);
  XEvent *nativeEvent = (XEvent*)event;

  if (nativeEvent->type != GraphicsExpose)
    return 0;

  XGraphicsExposeEvent *expose = &nativeEvent->xgraphicsexpose;
  instanceData->window.window = (void*)(expose->drawable);

  GdkNativeWindow nativeWinId = (XID)(instanceData->window.window);
  GdkDrawable* gdkWindow = GDK_DRAWABLE(gdk_window_foreign_new(nativeWinId));  
  drawWindow(instanceData, gdkWindow);
  g_object_unref(gdkWindow);

  return 1;
}

void
NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData) {

}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  return NPERR_GENERIC_ERROR;
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  return NPERR_GENERIC_ERROR;
}

